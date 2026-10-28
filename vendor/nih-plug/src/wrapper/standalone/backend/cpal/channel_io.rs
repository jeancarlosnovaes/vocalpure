//! Bounded, frame-aligned transfer between independent device callbacks.

pub fn push_frame(
    queue: &mut rtrb::Producer<f32>,
    samples: impl ExactSizeIterator<Item = f32>,
) -> bool {
    if queue.slots() < samples.len() {
        return false;
    }
    for sample in samples {
        let _ = queue.push(sample);
    }
    true
}

pub fn read_frames(
    queue: &mut rtrb::Consumer<f32>,
    channels: usize,
    output: &mut [Vec<f32>],
    frames: usize,
) {
    for n in 0..frames {
        if channels > 0 && queue.slots() >= channels {
            for c in 0..channels {
                let sample = queue.pop().unwrap_or(0.0);
                if let Some(destination) = output.get_mut(c) {
                    destination[n] = sample;
                }
            }
            for channel in output.iter_mut().skip(channels) {
                channel[n] = 0.0;
            }
        } else {
            for channel in output.iter_mut() {
                channel[n] = 0.0;
            }
        }
    }
}
