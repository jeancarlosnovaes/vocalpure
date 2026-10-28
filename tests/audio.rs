use std::{
    alloc::{GlobalAlloc, Layout, System},
    cell::Cell,
};
use vocalpure::{
    denoiser::Denoiser,
    engine::{AudioEngine, AudioParameters},
    resampler::FrameResampler,
};

struct TrackingAllocator;
thread_local! {static TRACK:Cell<bool>=const {Cell::new(false)};static ALLOCS:Cell<usize>=const {Cell::new(0)};}
unsafe impl GlobalAlloc for TrackingAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if TRACK.try_with(|x| x.get()).unwrap_or(false) {
            let _ = ALLOCS.try_with(|x| x.set(x.get() + 1));
        }
        System.alloc(layout)
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        System.dealloc(ptr, layout)
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, size: usize) -> *mut u8 {
        if TRACK.try_with(|x| x.get()).unwrap_or(false) {
            let _ = ALLOCS.try_with(|x| x.set(x.get() + 1));
        }
        System.realloc(ptr, layout, size)
    }
}
#[global_allocator]
static ALLOCATOR: TrackingAllocator = TrackingAllocator;

fn noise(n: usize) -> Vec<[f32; 2]> {
    let mut state = 42u32;
    (0..n)
        .map(|_| {
            state = state.wrapping_mul(1664525).wrapping_add(1013904223);
            let x = ((state >> 8) as f32 / 16777216.0 - 0.5) * 0.16;
            [x, x]
        })
        .collect()
}
fn run(
    engine: &mut AudioEngine,
    input: &[[f32; 2]],
    p: AudioParameters,
    blocks: &[usize],
) -> Vec<[f32; 2]> {
    let mut result = input.to_vec();
    let mut offset = 0;
    let mut index = 0;
    while offset < result.len() {
        let end = (offset + blocks[index % blocks.len()]).min(result.len());
        index += 1;
        ALLOCS.with(|x| x.set(0));
        TRACK.with(|x| x.set(true));
        engine.begin_block(p);
        for sample in &mut result[offset..end] {
            engine.process_sample(sample, p.profile);
        }
        engine.end_block();
        TRACK.with(|x| x.set(false));
        assert_eq!(
            ALLOCS.with(|x| x.get()),
            0,
            "audio processing allocated Rust memory"
        );
        offset = end;
    }
    result
}
fn energy(input: &[[f32; 2]], start: usize) -> f64 {
    input[start..].iter().map(|x| (x[0] as f64).powi(2)).sum()
}

#[test]
fn aligned_dry_bypass_compare_gain_and_width() {
    for rate in [44100, 48000, 96000] {
        for channels in [1, 2] {
            let signal = noise(rate / 3);
            for mode in 0..3 {
                let p = AudioParameters {
                    reduction: if mode == 0 { 0.0 } else { 100.0 },
                    bypass: mode == 1,
                    compare: mode == 2,
                    ..Default::default()
                };
                let mut engine = AudioEngine::new(rate as f32, channels, p).unwrap();
                assert_eq!(
                    engine.latency,
                    match rate {
                        44100 => 1176,
                        48000 => 960,
                        _ => 2560,
                    }
                );
                let output = run(&mut engine, &signal, p, &[7, 32, 137, 512, 1024]);
                for (i, x) in output.iter().enumerate() {
                    assert_eq!(
                        x[0],
                        if i < engine.latency {
                            0.0
                        } else {
                            signal[i - engine.latency][0]
                        }
                    );
                    if channels == 2 {
                        assert_eq!(x[0], x[1]);
                    }
                }
            }
        }
    }
    for width in [0.0, 100.0, 200.0] {
        let p = AudioParameters {
            reduction: 0.0,
            width,
            input_db: 6.0,
            output_db: -3.0,
            ..Default::default()
        };
        let mut engine = AudioEngine::new(48000.0, 2, p).unwrap();
        let output = run(&mut engine, &vec![[0.1, -0.04]; 5000], p, &[137]);
        let gain = 10.0_f32.powf(3.0 / 20.0);
        let last = output.last().unwrap();
        assert!((last[0] - (0.03 + 0.07 * width / 100.0) * gain).abs() < 1e-6);
        assert!((last[1] - (0.03 - 0.07 * width / 100.0) * gain).abs() < 1e-6);
    }
}

#[test]
fn neural_suppression_blend_partition_and_reset() {
    for rate in [44100, 48000, 96000] {
        for channels in [1, 2] {
            let signal = noise(rate);
            let p = AudioParameters {
                reduction: 100.0,
                ..Default::default()
            };
            let mut a = AudioEngine::new(rate as f32, channels, p).unwrap();
            let wet = run(&mut a, &signal, p, &[7, 137, 1024]);
            let mut b = AudioEngine::new(rate as f32, channels, p).unwrap();
            assert_eq!(wet, run(&mut b, &signal, p, &[512]));
            assert!(wet.iter().flatten().all(|x| x.is_finite()));
            assert!((0.0..=1.0).contains(&a.meters.voice));
            ALLOCS.with(|x| x.set(0));
            TRACK.with(|x| x.set(true));
            a.reset(p);
            TRACK.with(|x| x.set(false));
            assert_eq!(ALLOCS.with(|x| x.get()), 0);
            assert_eq!(wet, run(&mut a, &signal, p, &[64, 256]));
            let dry_p = AudioParameters {
                reduction: 0.0,
                ..p
            };
            let mut dry_engine = AudioEngine::new(rate as f32, channels, dry_p).unwrap();
            let dry = run(&mut dry_engine, &signal, dry_p, &[137]);
            let ratio = energy(&wet, rate / 2) / energy(&dry, rate / 2);
            println!(
                "rate={rate} channels={channels} noise attenuation={:.2} dB",
                -10.0 * ratio.log10()
            );
            // Match the original acceptance contract: native-rate attenuation is
            // stronger; resampled, band-limited noise has a different spectrum.
            assert!(
                ratio < if rate == 48000 { 0.6 } else { 1.0 },
                "neural suppression at {rate}: {ratio}"
            );
            let half_p = AudioParameters {
                reduction: 50.0,
                ..p
            };
            let mut half_engine = AudioEngine::new(rate as f32, channels, half_p).unwrap();
            let half = run(&mut half_engine, &signal, half_p, &[32, 7, 512]);
            for i in 0..half.len() {
                assert!((half[i][0] - (dry[i][0] + wet[i][0]) * 0.5).abs() < 1e-7);
            }
        }
    }
}

#[test]
fn profile_cadence_strong_and_invalid_input() {
    let mut general = Denoiser::new().unwrap();
    let mut eco = Denoiser::new().unwrap();
    let mut strong = Denoiser::new().unwrap();
    let input = noise(480 * 100);
    let mut out = [0.0; 480];
    let mut strong_out = [0.0; 480];
    let mut difference = 0.0;
    for frame in input.chunks_exact(480) {
        let data: [f32; 480] = std::array::from_fn(|i| frame[i][0]);
        assert!(general.process(&data, &mut out, 0).is_some());
        assert!(eco.process(&data, &mut strong_out, 2).is_some());
        assert!(strong.process(&data, &mut strong_out, 1).is_some());
        difference += out
            .iter()
            .zip(strong_out)
            .map(|(x, y)| (x - y).abs())
            .sum::<f32>();
    }
    assert!(general.inference_count() > 80);
    assert!(eco.inference_count() <= general.inference_count() / 2 + 1);
    assert!(difference > 1e-4);
    let mut engine = AudioEngine::new(48000.0, 2, AudioParameters::default()).unwrap();
    let signal = vec![[f32::NAN, f32::INFINITY]; 2000];
    let result = run(&mut engine, &signal, AudioParameters::default(), &[512]);
    assert!(result.iter().flatten().all(|x| x.is_finite()));
    assert_eq!(engine.status, 3);
    assert!(AudioEngine::new(88200.0, 2, AudioParameters::default()).is_none());
    assert!(AudioEngine::new(48000.0, 3, AudioParameters::default()).is_none());
}

#[test]
fn resampler_delay_and_alias_rejection() {
    for rate in [44100, 96000] {
        let mut down = FrameResampler::new(rate, 48000);
        let mut up = FrameResampler::new(48000, rate);
        let mut result = Vec::new();
        let mut input = [0.0; 960];
        let mut middle = [0.0; 480];
        let mut output = [0.0; 960];
        for frame in 0..5 {
            input.fill(0.0);
            if frame == 0 {
                input[0] = 1.0;
            }
            down.process(&input, &mut middle);
            up.process(&middle, &mut output);
            result.extend_from_slice(&output[..rate as usize / 100]);
        }
        let peak = result
            .iter()
            .enumerate()
            .max_by(|a, b| a.1.abs().total_cmp(&b.1.abs()))
            .unwrap()
            .0;
        assert_eq!(peak, 2 * (rate as usize / 300));
    }
    let mut down = FrameResampler::new(96000, 48000);
    let mut input = [0.0; 960];
    let mut output = [0.0; 480];
    let mut energy = 0.0;
    for frame in 0..10 {
        for (i, x) in input.iter_mut().enumerate() {
            *x = (2.0 * std::f32::consts::PI * 30000.0 * (frame * 960 + i) as f32 / 96000.0).sin();
        }
        down.process(&input, &mut output);
        if frame > 1 {
            energy += output.iter().map(|x| x * x).sum::<f32>();
        }
    }
    assert!(energy / (8.0 * 480.0) < 1e-6);
}
