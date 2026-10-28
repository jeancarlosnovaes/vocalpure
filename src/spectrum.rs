use crossbeam::queue::ArrayQueue;
use rustfft::{num_complex::Complex32, Fft, FftPlanner};
use std::sync::Arc;

pub const SIZE: usize = 2048;
#[derive(Clone, Copy)]
pub struct Frame {
    pub samples: [[f32; SIZE]; 4],
    pub rate: f32,
    pub channels: usize,
}

pub struct Capture {
    pending: Frame,
    position: usize,
    pub queue: Arc<ArrayQueue<Frame>>,
}
impl Default for Capture {
    fn default() -> Self {
        Self {
            pending: Frame {
                samples: [[0.0; SIZE]; 4],
                rate: 48000.0,
                channels: 2,
            },
            position: 0,
            queue: Arc::new(ArrayQueue::new(8)),
        }
    }
}
impl Capture {
    pub fn prepare(&mut self, rate: f32, channels: usize) {
        self.pending.rate = rate;
        self.pending.channels = channels;
        self.position = 0;
        while self.queue.pop().is_some() {}
    }
    pub fn discard(&mut self) {
        self.position = 0;
    }
    pub fn push(&mut self, dry: [f32; 2], wet: [f32; 2]) {
        for (c, x) in [dry[0], dry[1], wet[0], wet[1]].into_iter().enumerate() {
            self.pending.samples[c][self.position] = x;
        }
        self.position += 1;
        if self.position == SIZE {
            self.position = 0;
            let _ = self.queue.push(self.pending);
        }
    }
}

pub struct Analyzer {
    pub input: Vec<f32>,
    pub output: Vec<f32>,
    pub rate: f32,
    fft: Arc<dyn Fft<f32>>,
    work: Vec<Complex32>,
    scratch: Vec<Complex32>,
}
impl Default for Analyzer {
    fn default() -> Self {
        let fft = FftPlanner::new().plan_fft_forward(SIZE);
        let scratch = vec![Complex32::default(); fft.get_inplace_scratch_len()];
        Self {
            input: vec![-100.0; SIZE / 2 + 1],
            output: vec![-100.0; SIZE / 2 + 1],
            rate: 48000.0,
            fft,
            work: vec![Complex32::default(); SIZE],
            scratch,
        }
    }
}
impl Analyzer {
    pub fn analyze(&mut self, frame: &Frame) {
        self.rate = frame.rate;
        self.input.fill(0.0);
        self.output.fill(0.0);
        for path in 0..2 {
            for c in 0..frame.channels {
                for (i, value) in self.work.iter_mut().enumerate() {
                    // Hann normalized to unity DC gain, like the original JUCE window.
                    let window = 1.0 - (2.0 * std::f32::consts::PI * i as f32 / SIZE as f32).cos();
                    *value = Complex32::new(frame.samples[c + path * 2][i] * window, 0.0);
                }
                self.fft
                    .process_with_scratch(&mut self.work, &mut self.scratch);
                let power = if path == 0 {
                    &mut self.input
                } else {
                    &mut self.output
                };
                for (k, value) in power.iter_mut().enumerate() {
                    let scale = if k == 0 { 1.0 } else { 2.0 } / SIZE as f32;
                    *value += self.work[k].norm_sqr() * scale * scale / frame.channels as f32;
                }
            }
        }
        for value in self.input.iter_mut().chain(&mut self.output) {
            *value = (10.0 * value.max(1e-10).log10()).max(-100.0);
        }
    }
}
