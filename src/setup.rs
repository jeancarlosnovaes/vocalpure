use baseview::{Size, WindowOpenOptions, WindowScalePolicy};
use cpal::traits::{DeviceTrait, HostTrait};
use egui_baseview::{EguiWindow, GraphicsConfig};
use std::sync::{Arc, Mutex};
use vocalpure::preferences::Preferences;

#[path = "setup_view.rs"]
mod view;

struct Setup {
    preferences: Preferences,
    inputs: Vec<String>,
    outputs: Vec<String>,
    input: usize,
    output: usize,
    rate: u32,
    channels: usize,
    preview: bool,
    error: String,
    result: Arc<Mutex<Option<Vec<String>>>>,
}

pub fn choose_audio() -> Option<Vec<String>> {
    let host = cpal::default_host();
    let inputs = host
        .input_devices()
        .map(|devices| devices.filter_map(|d| d.name().ok()).collect())
        .unwrap_or_default();
    let outputs = host
        .output_devices()
        .map(|devices| devices.filter_map(|d| d.name().ok()).collect())
        .unwrap_or_default();
    let result = Arc::new(Mutex::new(None));
    let setup = Setup {
        preferences: Preferences::load(),
        inputs,
        outputs,
        input: 0,
        output: 0,
        rate: 48000,
        channels: 2,
        preview: false,
        error: String::new(),
        result: result.clone(),
    };
    EguiWindow::open_blocking(
        WindowOpenOptions {
            title: "VocalPure".into(),
            size: Size::new(720.0, 720.0),
            scale: WindowScalePolicy::SystemScaleFactor,
            gl_config: Some(Default::default()),
        },
        GraphicsConfig::default(),
        setup,
        |ctx, _, _| view::configure(ctx),
        |ctx, queue, s| view::show(ctx, queue, s),
    );
    let args = result.lock().unwrap().take();
    args
}

impl Setup {
    fn arguments(&self) -> Result<Vec<String>, String> {
        let mut args = vec!["VocalPure".into()];
        if self.preview {
            args.extend(["--backend".into(), "dummy".into()]);
            return Ok(args);
        }
        let input = self
            .inputs
            .get(self.input)
            .ok_or("Selecione uma entrada de áudio.")?;
        let output = self
            .outputs
            .get(self.output)
            .ok_or("Selecione uma saída de áudio.")?;
        let host = cpal::default_host();
        let input_device = host
            .input_devices()
            .map_err(|e| e.to_string())?
            .find(|d| d.name().as_deref().is_ok_and(|name| name == input))
            .ok_or("A entrada não está mais disponível.")?;
        let output_device = host
            .output_devices()
            .map_err(|e| e.to_string())?
            .find(|d| d.name().as_deref().is_ok_and(|name| name == output))
            .ok_or("A saída não está mais disponível.")?;
        let config = cpal::StreamConfig {
            channels: self.channels as u16,
            sample_rate: cpal::SampleRate(self.rate),
            buffer_size: cpal::BufferSize::Default,
        };
        // Open both streams without starting capture or playback. This asks
        // Windows to validate the real configuration instead of guessing from
        // the native mix format list. No device settings are changed.
        let _input_probe = input_device
            .build_input_stream(&config, |_: &[f32], _| {}, |_| {}, None)
            .map_err(|e| format!("Não foi possível abrir a entrada: {e}"))?;
        let _output_probe = output_device
            .build_output_stream(&config, |data: &mut [f32], _| data.fill(0.0), |_| {}, None)
            .map_err(|e| format!("Não foi possível abrir a saída: {e}"))?;
        #[cfg(target_os = "windows")]
        args.extend(["--backend".into(), "wasapi".into()]);
        args.extend([
            "--input-device".into(),
            input.clone(),
            "--output-device".into(),
            output.clone(),
            "--sample-rate".into(),
            self.rate.to_string(),
            "--audio-layout".into(),
            (if self.channels == 2 { 1 } else { 2 }).to_string(),
        ]);
        Ok(args)
    }
}
