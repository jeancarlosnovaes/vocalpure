use std::{
    ffi::{c_float, c_int, c_ulong, c_void},
    ptr::NonNull,
    sync::OnceLock,
};

unsafe extern "C" {
    fn rnnoise_create(model: *mut c_void) -> *mut c_void;
    fn rnnoise_destroy(state: *mut c_void);
    fn vocalpure_rnnoise_reset(state: *mut c_void);
    fn vocalpure_rnnoise_process_frame(
        state: *mut c_void,
        out: *mut c_float,
        input: *const c_float,
        eco: c_int,
        strong: c_float,
    ) -> c_float;
    fn vocalpure_rnnoise_inferences(state: *const c_void) -> c_ulong;
}

pub struct Denoiser {
    state: NonNull<c_void>,
    scaled: [f32; 480],
    cleaned: [f32; 480],
    strong: f32,
}

// Each instance owns its state and only processes it through &mut self. Global
// FFT/window tables are initialized serially before any audio thread runs.
unsafe impl Send for Denoiser {}

impl Denoiser {
    pub fn new() -> Option<Self> {
        static WARM: OnceLock<bool> = OnceLock::new();
        if !*WARM.get_or_init(|| unsafe {
            let state = rnnoise_create(std::ptr::null_mut());
            if state.is_null() {
                return false;
            }
            let mut output = [0.0; 480];
            vocalpure_rnnoise_process_frame(
                state,
                output.as_mut_ptr(),
                [0.0; 480].as_ptr(),
                0,
                0.0,
            );
            rnnoise_destroy(state);
            true
        }) {
            return None;
        }
        Some(Self {
            state: NonNull::new(unsafe { rnnoise_create(std::ptr::null_mut()) })?,
            scaled: [0.0; 480],
            cleaned: [0.0; 480],
            strong: 0.0,
        })
    }

    pub fn process(
        &mut self,
        input: &[f32; 480],
        output: &mut [f32; 480],
        profile: i32,
    ) -> Option<f32> {
        for (scaled, sample) in self.scaled.iter_mut().zip(input) {
            if !sample.is_finite() {
                return None;
            }
            *scaled = sample.clamp(-1.0, 1.0) * 32768.0;
        }
        let target = if profile == 1 { 1.0 } else { 0.0 };
        self.strong += (target - self.strong).clamp(-0.5, 0.5);
        let vad = unsafe {
            vocalpure_rnnoise_process_frame(
                self.state.as_ptr(),
                self.cleaned.as_mut_ptr(),
                self.scaled.as_ptr(),
                (profile == 2) as i32,
                self.strong,
            )
        };
        if !vad.is_finite() || self.cleaned.iter().any(|x| !x.is_finite()) {
            return None;
        }
        for (out, sample) in output.iter_mut().zip(self.cleaned) {
            *out = sample / 32768.0;
        }
        Some(vad.clamp(0.0, 1.0))
    }

    pub fn inference_count(&self) -> u64 {
        unsafe { vocalpure_rnnoise_inferences(self.state.as_ptr()) as u64 }
    }

    pub fn reset(&mut self) {
        unsafe {
            vocalpure_rnnoise_reset(self.state.as_ptr());
        }
        self.strong = 0.0;
        self.scaled.fill(0.0);
        self.cleaned.fill(0.0);
    }
}

impl Drop for Denoiser {
    fn drop(&mut self) {
        unsafe { rnnoise_destroy(self.state.as_ptr()) }
    }
}
