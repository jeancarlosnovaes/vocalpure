use crate::{denoiser::Denoiser, resampler::FrameResampler};

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AudioParameters {
    pub input_db: f32,
    pub output_db: f32,
    pub bypass: bool,
    pub reduction: f32,
    pub width: f32,
    pub compare: bool,
    pub profile: i32,
}
impl Default for AudioParameters {
    fn default() -> Self {
        Self {
            input_db: 0.0,
            output_db: 0.0,
            bypass: false,
            reduction: 65.0,
            width: 100.0,
            compare: false,
            profile: 0,
        }
    }
}

#[derive(Default, Clone, Copy, Debug)]
pub struct MeterValues {
    pub input: f32,
    pub output: f32,
    pub voice: f32,
    pub attenuation_db: f32,
}

struct Ramp {
    value: f32,
    target: f32,
    step: f32,
    remaining: usize,
    length: usize,
}
impl Ramp {
    fn new(value: f32, rate: f32) -> Self {
        Self {
            value,
            target: value,
            step: 0.0,
            remaining: 0,
            length: (rate * 0.02).floor().max(1.0) as usize,
        }
    }
    fn target(&mut self, target: f32) {
        if self.target != target {
            self.target = target;
            self.remaining = self.length;
            self.step = (target - self.value) / self.length as f32;
        }
    }
    fn next(&mut self) -> f32 {
        if self.remaining > 0 {
            self.remaining -= 1;
            self.value = if self.remaining == 0 {
                self.target
            } else {
                self.value + self.step
            };
        }
        self.value
    }
}

struct Channel {
    model: Denoiser,
    down: Option<FrameResampler>,
    up: Option<FrameResampler>,
    input: [f32; 960],
    output: [f32; 960],
    neural_in: [f32; 480],
    neural_out: [f32; 480],
    raw_delay: Vec<f32>,
    gained_delay: Vec<f32>,
}

pub struct AudioEngine {
    channels: Vec<Channel>,
    pub latency: usize,
    frame_size: usize,
    frame_position: usize,
    delay_position: usize,
    input: Ramp,
    output: Ramp,
    active: Ramp,
    reduction: Ramp,
    width: Ramp,
    pub status: u32,
    pub meters: MeterValues,
    ready: bool,
    peak_in: f32,
    peak_out: f32,
    dry_energy: f64,
    wet_energy: f64,
}

fn db_gain(db: f32) -> f32 {
    10.0_f32.powf(db / 20.0)
}

impl AudioEngine {
    pub fn reset(&mut self, p: AudioParameters) {
        for s in &mut self.channels {
            s.model.reset();
            s.input.fill(0.0);
            s.output.fill(0.0);
            s.neural_in.fill(0.0);
            s.neural_out.fill(0.0);
            s.raw_delay.fill(0.0);
            s.gained_delay.fill(0.0);
            if let Some(down) = &mut s.down {
                down.clear();
            }
            if let Some(up) = &mut s.up {
                up.clear();
            }
        }
        self.frame_position = 0;
        self.delay_position = 0;
        self.ready = true;
        self.status = 0;
        self.meters = MeterValues::default();
        for (ramp, value) in [
            (&mut self.input, db_gain(p.input_db)),
            (&mut self.output, db_gain(p.output_db)),
            (
                &mut self.active,
                if p.bypass || p.compare { 0.0 } else { 1.0 },
            ),
            (&mut self.reduction, p.reduction / 100.0),
            (&mut self.width, p.width / 100.0),
        ] {
            ramp.value = value;
            ramp.target = value;
            ramp.remaining = 0;
        }
    }

    pub fn new(rate: f32, count: usize, p: AudioParameters) -> Option<Self> {
        if !(1..=2).contains(&count) || ![44100.0, 48000.0, 96000.0].contains(&rate) {
            return None;
        }
        let resample = rate != 48000.0;
        let frame_size = rate as usize / 100;
        let latency = 2 * frame_size
            + if resample {
                2 * (rate as usize / 300)
            } else {
                0
            };
        let mut channels = Vec::with_capacity(count);
        for _ in 0..count {
            channels.push(Channel {
                model: Denoiser::new()?,
                down: resample.then(|| FrameResampler::new(rate as u32, 48000)),
                up: resample.then(|| FrameResampler::new(48000, rate as u32)),
                input: [0.0; 960],
                output: [0.0; 960],
                neural_in: [0.0; 480],
                neural_out: [0.0; 480],
                raw_delay: vec![0.0; latency],
                gained_delay: vec![0.0; latency],
            });
        }
        Some(Self {
            channels,
            latency,
            frame_size,
            frame_position: 0,
            delay_position: 0,
            input: Ramp::new(db_gain(p.input_db), rate),
            output: Ramp::new(db_gain(p.output_db), rate),
            active: Ramp::new(if p.bypass || p.compare { 0.0 } else { 1.0 }, rate),
            reduction: Ramp::new(p.reduction / 100.0, rate),
            width: Ramp::new(p.width / 100.0, rate),
            status: 0,
            meters: MeterValues::default(),
            ready: true,
            peak_in: 0.0,
            peak_out: 0.0,
            dry_energy: 0.0,
            wet_energy: 0.0,
        })
    }

    pub fn begin_block(&mut self, p: AudioParameters) {
        self.input.target(db_gain(p.input_db));
        self.output.target(db_gain(p.output_db));
        self.active
            .target(if p.bypass || p.compare { 0.0 } else { 1.0 });
        self.reduction
            .target(if self.ready { p.reduction / 100.0 } else { 0.0 });
        self.width.target(p.width / 100.0);
        self.peak_in = 0.0;
        self.peak_out = 0.0;
        self.dry_energy = 0.0;
        self.wet_energy = 0.0;
    }

    /// A single mono/stereo sample. All storage is allocated in new().
    /// Returns aligned dry samples for the spectrum display.
    pub fn process_sample(&mut self, samples: &mut [f32; 2], profile: i32) -> [f32; 2] {
        let ig = self.input.next();
        let og = self.output.next();
        let on = self.active.next();
        let amount = self.reduction.next();
        let width = 1.0 + on * (self.width.next() - 1.0);
        let mut aligned = [0.0; 2];
        for (c, s) in self.channels.iter_mut().enumerate() {
            let raw = if samples[c].is_finite() {
                samples[c]
            } else {
                self.status = 3;
                0.0
            };
            self.peak_in = self.peak_in.max(raw.abs());
            let original = s.raw_delay[self.delay_position];
            let dry = s.gained_delay[self.delay_position];
            aligned[c] = original;
            s.raw_delay[self.delay_position] = raw;
            s.gained_delay[self.delay_position] = raw * ig;
            s.input[self.frame_position] = raw * ig;
            let wet = if self.ready {
                s.output[self.frame_position]
            } else {
                dry
            };
            let mixed = (dry + amount * (wet - dry)) * og;
            let out = original + on * (mixed - original);
            samples[c] = if out.is_finite() { out } else { original };
        }
        if self.channels.len() == 2 && width != 1.0 {
            let mid = (samples[0] + samples[1]) * 0.5;
            let side = (samples[0] - samples[1]) * 0.5 * width;
            samples[0] = mid + side;
            samples[1] = mid - side;
        }
        for c in 0..self.channels.len() {
            self.peak_out = self.peak_out.max(samples[c].abs());
            self.dry_energy += (aligned[c] as f64).powi(2);
            self.wet_energy += (samples[c] as f64).powi(2);
        }
        self.delay_position = (self.delay_position + 1) % self.latency;
        self.frame_position += 1;
        if self.frame_position == self.frame_size {
            self.frame_position = 0;
            if self.ready {
                self.run_frame(profile);
            }
        }
        aligned
    }

    fn run_frame(&mut self, profile: i32) {
        let mut vad = 0.0_f32;
        for s in &mut self.channels {
            if let Some(down) = &mut s.down {
                down.process(&s.input, &mut s.neural_in);
            } else {
                s.neural_in.copy_from_slice(&s.input[..480]);
            }
            let Some(probability) = s.model.process(&s.neural_in, &mut s.neural_out, profile)
            else {
                self.ready = false;
                self.status = 2;
                self.meters.voice = 0.0;
                return;
            };
            vad = vad.max(probability);
            if let Some(up) = &mut s.up {
                up.process(&s.neural_out, &mut s.output);
            } else {
                s.output[..480].copy_from_slice(&s.neural_out);
            }
        }
        self.meters.voice = vad;
    }

    pub fn end_block(&mut self) -> MeterValues {
        self.meters.input = self.peak_in;
        self.meters.output = self.peak_out;
        self.meters.attenuation_db = if self.dry_energy > 1e-12 {
            (10.0 * ((self.dry_energy + 1e-20) / (self.wet_energy + 1e-20)).log10()) as f32
        } else {
            0.0
        };
        self.meters
    }
}
