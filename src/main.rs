#![cfg_attr(
    all(target_os = "windows", not(debug_assertions)),
    windows_subsystem = "windows"
)]

mod setup;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let args = if args.len() == 1 {
        match setup::choose_audio() {
            Some(args) => args,
            None => return,
        }
    } else {
        args
    };
    if !nih_plug::prelude::nih_export_standalone_with_args::<vocalpure::VocalPure, _>(args) {
        std::process::exit(1);
    }
}
