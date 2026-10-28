#![cfg(feature = "standalone")]
#[path = "../vendor/nih-plug/src/wrapper/standalone/backend/cpal/channel_io.rs"]
mod channel_io;

#[test]
fn interleaved_capture_preserves_left_and_right_across_packet_sizes() {
    let (mut producer, mut consumer) = rtrb::RingBuffer::new(32);
    let mut output = vec![vec![0.0; 8]; 2];
    for n in 0..10 {
        assert!(channel_io::push_frame(
            &mut producer,
            [n as f32, 100.0 + n as f32].into_iter()
        ));
    }
    let mut offset = 0;
    for frames in [1, 4, 2, 3] {
        channel_io::read_frames(&mut consumer, 2, &mut output, frames);
        for n in 0..frames {
            assert_eq!(output[0][n], (offset + n) as f32);
            assert_eq!(output[1][n], 100.0 + (offset + n) as f32);
        }
        offset += frames;
    }
}

#[test]
fn full_queue_drops_whole_frames_and_empty_queue_outputs_silence() {
    let (mut producer, mut consumer) = rtrb::RingBuffer::new(3);
    assert!(channel_io::push_frame(
        &mut producer,
        [1.0, 2.0].into_iter()
    ));
    assert!(!channel_io::push_frame(
        &mut producer,
        [3.0, 4.0].into_iter()
    ));
    let mut output = vec![vec![-1.0; 4]; 2];
    channel_io::read_frames(&mut consumer, 2, &mut output, 4);
    assert_eq!(output[0], vec![1.0, 0.0, 0.0, 0.0]);
    assert_eq!(output[1], vec![2.0, 0.0, 0.0, 0.0]);
    assert!(channel_io::push_frame(
        &mut producer,
        [5.0, 6.0].into_iter()
    ));
    channel_io::read_frames(&mut consumer, 2, &mut output, 1);
    assert_eq!(output[0][0], 5.0);
    assert_eq!(output[1][0], 6.0);
}
