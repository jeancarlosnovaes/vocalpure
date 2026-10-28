use std::f64::consts::PI;

/// Causal 128-tap Blackman-windowed sinc, with 1/300 s delay per stage.
pub struct FrameResampler {
    input_count: usize,
    output_count: usize,
    samples: [f32; 1920],
    starts: Vec<usize>,
    weights: Vec<f32>,
}

impl FrameResampler {
    pub fn new(from: u32, to: u32) -> Self {
        assert!([44100, 48000, 96000].contains(&from));
        assert!([44100, 48000, 96000].contains(&to));
        let input_count = (from / 100) as usize;
        let output_count = (to / 100) as usize;
        let mut result = Self {
            input_count,
            output_count,
            samples: [0.0; 1920],
            starts: vec![0; output_count],
            weights: vec![0.0; output_count * 128],
        };
        let cutoff = 0.49 * (to as f64 / from as f64).min(1.0);
        for j in 0..output_count {
            let centre = j as f64 * from as f64 / to as f64 - (from / 300) as f64;
            let start = centre.floor() as isize - 63;
            result.starts[j] = (input_count as isize + start) as usize;
            assert!(result.starts[j] + 128 <= input_count * 2);
            let mut sum = 0.0;
            for k in 0..128 {
                let x = (start + k as isize) as f64 - centre;
                let u = x / 64.0;
                let window = 0.42 + 0.5 * (PI * u).cos() + 0.08 * (2.0 * PI * u).cos();
                let coefficient = if x.abs() < 1e-12 {
                    2.0 * cutoff
                } else {
                    (2.0 * PI * cutoff * x).sin() / (PI * x)
                } * window;
                result.weights[j * 128 + k] = coefficient as f32;
                sum += coefficient;
            }
            for weight in &mut result.weights[j * 128..(j + 1) * 128] {
                *weight /= sum as f32;
            }
        }
        result
    }

    pub fn clear(&mut self) {
        self.samples.fill(0.0);
    }

    pub fn process(&mut self, input: &[f32], output: &mut [f32]) {
        assert!(input.len() >= self.input_count && output.len() >= self.output_count);
        self.samples[self.input_count..2 * self.input_count]
            .copy_from_slice(&input[..self.input_count]);
        for (j, out) in output.iter_mut().take(self.output_count).enumerate() {
            let source = &self.samples[self.starts[j]..self.starts[j] + 128];
            *out = self.weights[j * 128..(j + 1) * 128]
                .iter()
                .zip(source)
                .map(|(a, b)| a * b)
                .sum();
        }
        self.samples
            .copy_within(self.input_count..2 * self.input_count, 0);
    }
}
