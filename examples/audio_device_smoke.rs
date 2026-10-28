//! Open the actual selected WASAPI devices, capture without retaining samples,
//! and send only silence to playback. No microphone audio is monitored or saved.
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use std::{
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc,
    },
    time::Duration,
};
fn main() {
    let host = cpal::default_host();
    let input = host
        .input_devices()
        .unwrap()
        .find(|d| d.name().unwrap_or_default().contains("USB AUDIO"))
        .expect("USB input");
    let output = host
        .output_devices()
        .unwrap()
        .find(|d| {
            d.name().unwrap_or_default().starts_with("Speakers")
                && d.name().unwrap_or_default().contains("USB AUDIO")
        })
        .expect("USB output");
    for rate in [44100, 48000, 96000] {
        for channels in [1, 2] {
            let config = cpal::StreamConfig {
                channels,
                sample_rate: cpal::SampleRate(rate),
                buffer_size: cpal::BufferSize::Default,
            };
            let captured = Arc::new(AtomicUsize::new(0));
            let played = Arc::new(AtomicUsize::new(0));
            let errors = Arc::new(AtomicUsize::new(0));
            let count = captured.clone();
            let error = errors.clone();
            let input_stream = input
                .build_input_stream(
                    &config,
                    move |data: &[f32], _| {
                        count.fetch_add(data.len(), Ordering::Relaxed);
                    },
                    move |_| {
                        error.fetch_add(1, Ordering::Relaxed);
                    },
                    None,
                )
                .unwrap();
            let count = played.clone();
            let error = errors.clone();
            let output_stream = output
                .build_output_stream(
                    &config,
                    move |data: &mut [f32], _| {
                        data.fill(0.0);
                        count.fetch_add(data.len(), Ordering::Relaxed);
                    },
                    move |_| {
                        error.fetch_add(1, Ordering::Relaxed);
                    },
                    None,
                )
                .unwrap();
            input_stream.play().unwrap();
            output_stream.play().unwrap();
            std::thread::sleep(Duration::from_millis(250));
            drop(output_stream);
            drop(input_stream);
            assert!(captured.load(Ordering::Relaxed) > 0);
            assert!(played.load(Ordering::Relaxed) > 0);
            assert_eq!(errors.load(Ordering::Relaxed), 0);
            println!("PASS: USB capture and silent playback at {rate} Hz, {channels} channel(s)");
        }
    }
}
