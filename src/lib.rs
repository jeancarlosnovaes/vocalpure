pub mod denoiser;
mod editor;
pub mod engine;
pub mod preferences;
pub mod resampler;
mod spectrum;

use engine::{AudioEngine, AudioParameters};
use nih_plug::prelude::*;
use nih_plug_egui::EguiState;
use std::sync::{
    atomic::{AtomicU32, Ordering},
    Arc,
};

pub const PRESETS: [(&str, f32, i32); 8] = [
    ("Natural Voice", 35.0, 0),
    ("Podcast Clean", 65.0, 0),
    ("Studio Voice", 50.0, 0),
    ("Noisy Room", 85.0, 0),
    ("Outdoor Dialogue", 90.0, 0),
    ("Strong Noise Reduction", 100.0, 1),
    ("Gentle Cleaning", 20.0, 0),
    ("Low CPU", 65.0, 2),
];

#[derive(Params)]
pub struct VocalPureParams {
    #[persist = "editor-state"]
    pub editor_state: Arc<EguiState>,
    #[persist = "ui-preferences"]
    pub ui_preferences: std::sync::Mutex<preferences::Preferences>,
    #[id = "inputGain"]
    pub input: FloatParam,
    #[id = "outputGain"]
    pub output: FloatParam,
    #[id = "bypass"]
    pub bypass: BoolParam,
    #[id = "reduction"]
    pub reduction: FloatParam,
    #[id = "width"]
    pub width: FloatParam,
    #[id = "compare"]
    pub compare: BoolParam,
    #[id = "profile"]
    pub profile: IntParam,
}
impl Default for VocalPureParams {
    fn default() -> Self {
        Self {
            editor_state: EguiState::from_size(1000, 560),
            ui_preferences: std::sync::Mutex::new(preferences::Preferences::load()),
            input: FloatParam::new(
                "Input Gain",
                0.0,
                FloatRange::Linear {
                    min: -24.0,
                    max: 24.0,
                },
            )
            .with_unit(" dB"),
            output: FloatParam::new(
                "Output Gain",
                0.0,
                FloatRange::Linear {
                    min: -24.0,
                    max: 24.0,
                },
            )
            .with_unit(" dB"),
            bypass: BoolParam::new("Bypass", false).make_bypass(),
            reduction: FloatParam::new(
                "Noise Reduction",
                65.0,
                FloatRange::Linear {
                    min: 0.0,
                    max: 100.0,
                },
            )
            .with_unit(" %"),
            width: FloatParam::new(
                "Stereo Width",
                100.0,
                FloatRange::Linear {
                    min: 0.0,
                    max: 200.0,
                },
            )
            .with_unit(" %"),
            compare: BoolParam::new("Compare Original", false),
            profile: IntParam::new("RNNoise Profile", 0, IntRange::Linear { min: 0, max: 2 })
                .with_value_to_string(Arc::new(|v| {
                    ["General", "Strong", "Eco"][v.clamp(0, 2) as usize].into()
                })),
        }
    }
}
impl VocalPureParams {
    pub fn values(&self) -> AudioParameters {
        AudioParameters {
            input_db: self.input.value(),
            output_db: self.output.value(),
            bypass: self.bypass.value(),
            reduction: self.reduction.value(),
            width: self.width.value(),
            compare: self.compare.value(),
            profile: self.profile.value(),
        }
    }
}

#[derive(Default)]
pub(crate) struct Meters {
    input: AtomicF32,
    output: AtomicF32,
    voice: AtomicF32,
    attenuation: AtomicF32,
    status: AtomicU32,
    latency: AtomicU32,
    channels: AtomicU32,
}

pub struct VocalPure {
    params: Arc<VocalPureParams>,
    engine: Option<AudioEngine>,
    meters: Arc<Meters>,
    capture: spectrum::Capture,
}
impl Default for VocalPure {
    fn default() -> Self {
        Self {
            params: Arc::new(VocalPureParams::default()),
            engine: None,
            meters: Arc::new(Meters::default()),
            capture: spectrum::Capture::default(),
        }
    }
}
impl Plugin for VocalPure {
    const NAME: &'static str = "VocalPure";
    const VENDOR: &'static str = "VocalPure";
    const URL: &'static str = "https://github.com/jeancarlosnovaes/vocalpure";
    const EMAIL: &'static str = "";
    const VERSION: &'static str = env!("CARGO_PKG_VERSION");
    const AUDIO_IO_LAYOUTS: &'static [AudioIOLayout] = &[
        AudioIOLayout {
            main_input_channels: NonZeroU32::new(2),
            main_output_channels: NonZeroU32::new(2),
            ..AudioIOLayout::const_default()
        },
        AudioIOLayout {
            main_input_channels: NonZeroU32::new(1),
            main_output_channels: NonZeroU32::new(1),
            ..AudioIOLayout::const_default()
        },
    ];
    const SAMPLE_ACCURATE_AUTOMATION: bool = true;
    type SysExMessage = ();
    type BackgroundTask = ();
    fn params(&self) -> Arc<dyn Params> {
        self.params.clone()
    }
    fn editor(&mut self, _: AsyncExecutor<Self>) -> Option<Box<dyn Editor>> {
        editor::create(
            self.params.clone(),
            self.meters.clone(),
            self.capture.queue.clone(),
        )
    }
    fn initialize(
        &mut self,
        layout: &AudioIOLayout,
        config: &BufferConfig,
        context: &mut impl InitContext<Self>,
    ) -> bool {
        let channels = layout.main_input_channels.map_or(1, |v| v.get() as usize);
        self.engine = AudioEngine::new(config.sample_rate, channels, self.params.values());
        self.meters
            .channels
            .store(channels as u32, Ordering::Relaxed);
        if let Some(engine) = &self.engine {
            context.set_latency_samples(engine.latency as u32);
            self.meters
                .latency
                .store(engine.latency as u32, Ordering::Relaxed);
            self.meters.status.store(0, Ordering::Relaxed);
            self.capture.prepare(config.sample_rate, channels);
            true
        } else {
            self.meters.status.store(1, Ordering::Relaxed);
            false
        }
    }
    fn reset(&mut self) {
        if let Some(engine) = &mut self.engine {
            engine.reset(self.params.values());
        }
        self.capture.discard();
    }
    fn process(
        &mut self,
        buffer: &mut Buffer,
        _: &mut AuxiliaryBuffers,
        _: &mut impl ProcessContext<Self>,
    ) -> ProcessStatus {
        let p = self.params.values();
        let Some(engine) = &mut self.engine else {
            return ProcessStatus::Normal;
        };
        engine.begin_block(p);
        let capture = self.params.editor_state.is_open();
        if !capture {
            self.capture.discard();
        }
        for mut channels in buffer.iter_samples() {
            let mut sample = [0.0; 2];
            for (c, value) in channels.iter_mut().enumerate() {
                sample[c] = *value;
            }
            let dry = engine.process_sample(&mut sample, p.profile);
            for (c, value) in channels.iter_mut().enumerate() {
                *value = sample[c];
            }
            if capture {
                self.capture.push(dry, sample);
            }
        }
        let m = engine.end_block();
        self.meters.input.store(m.input, Ordering::Relaxed);
        self.meters.output.store(m.output, Ordering::Relaxed);
        self.meters.voice.store(m.voice, Ordering::Relaxed);
        self.meters
            .attenuation
            .store(m.attenuation_db, Ordering::Relaxed);
        self.meters.status.store(engine.status, Ordering::Relaxed);
        ProcessStatus::Normal
    }
}
impl ClapPlugin for VocalPure {
    const CLAP_ID: &'static str = "audio.vocalpure.rust";
    const CLAP_DESCRIPTION: Option<&'static str> = Some("Local RNNoise voice restoration");
    const CLAP_MANUAL_URL: Option<&'static str> = Some(Self::URL);
    const CLAP_SUPPORT_URL: Option<&'static str> = None;
    const CLAP_FEATURES: &'static [ClapFeature] = &[
        ClapFeature::AudioEffect,
        ClapFeature::Mono,
        ClapFeature::Stereo,
        ClapFeature::Restoration,
    ];
}
impl Vst3Plugin for VocalPure {
    // Separate identity: JUCE binary session state is not compatible with NIH-plug.
    const VST3_CLASS_ID: [u8; 16] = *b"VocalPureRust001";
    const VST3_SUBCATEGORIES: &'static [Vst3SubCategory] =
        &[Vst3SubCategory::Fx, Vst3SubCategory::Restoration];
}
nih_export_clap!(VocalPure);
nih_export_vst3!(VocalPure);
