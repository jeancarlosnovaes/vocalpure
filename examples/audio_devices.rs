use cpal::traits::{DeviceTrait, HostTrait};
fn main() {
    let host = cpal::default_host();
    for (input, label) in [(true, "INPUT"), (false, "OUTPUT")] {
        let devices = if input {
            host.input_devices()
        } else {
            host.output_devices()
        };
        for device in devices.unwrap() {
            println!("{label}: {}", device.name().unwrap_or_default());
            let configs: Vec<_> = if input {
                device.supported_input_configs().unwrap().collect()
            } else {
                device.supported_output_configs().unwrap().collect()
            };
            for config in configs {
                println!(
                    "  {} channels, {}..{} Hz, {:?}, {:?}",
                    config.channels(),
                    config.min_sample_rate().0,
                    config.max_sample_rate().0,
                    config.buffer_size(),
                    config.sample_format()
                );
            }
        }
    }
}
