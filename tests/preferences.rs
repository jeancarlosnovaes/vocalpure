use vocalpure::preferences::{Language, Preferences, Theme};
#[test]
fn language_and_theme_round_trip_and_persist() {
    let folder = std::env::temp_dir().join(format!("vocalpure-prefs-{}", std::process::id()));
    let path = folder.join("preferences.ini");
    for language in [Language::English, Language::Portuguese] {
        for theme in [Theme::Dark, Theme::Light] {
            let p = Preferences { language, theme };
            assert_eq!(Preferences::decode(&p.encode()), p);
            p.save_to(&path).unwrap();
            assert_eq!(
                Preferences::decode(&std::fs::read_to_string(&path).unwrap()),
                p
            );
        }
    }
    assert_eq!(Preferences::decode("invalid data"), Preferences::default());
}
#[test]
fn translations_switch_both_directions_and_preserve_values() {
    let en = Preferences {
        language: Language::English,
        theme: Theme::Dark,
    };
    let pt = Preferences {
        language: Language::Portuguese,
        theme: Theme::Light,
    };
    assert_eq!(en.text("Configure seu áudio"), "Set up your audio");
    assert_eq!(pt.text("NOISE REDUCTION"), "REDUÇÃO DE RUÍDO");
    assert_eq!(
        pt.text("VOICE PROBABILITY  42%"),
        "PROBABILIDADE DE VOZ  42%"
    );
    assert_eq!(
        en.text("Line (3- USB AUDIO CODEC)"),
        "Line (3- USB AUDIO CODEC)"
    );
}

#[test]
fn windows_device_names_are_preserved_for_every_interface_and_language() {
    let names = [
        "Line (3- USB AUDIO CODEC)",
        "Focusrite USB Audio",
        "Microfone (Realtek Audio)",
        "Interface de estúdio",
        "General",
        "INPUT  Studio",
    ];
    for language in [Language::English, Language::Portuguese] {
        let prefs = Preferences {
            language,
            ..Default::default()
        };
        for name in names {
            assert_eq!(prefs.device_label(Some(name)), name);
        }
        assert!(!prefs.device_label(None).is_empty());
    }
}
#[test]
fn both_themes_have_readable_text_contrast() {
    fn luminance(c: nih_plug_egui::egui::Color32) -> f32 {
        let linear = |v: u8| {
            let v = v as f32 / 255.0;
            if v <= 0.04045 {
                v / 12.92
            } else {
                ((v + 0.055) / 1.055).powf(2.4)
            }
        };
        0.2126 * linear(c.r()) + 0.7152 * linear(c.g()) + 0.0722 * linear(c.b())
    }
    for theme in [Theme::Dark, Theme::Light] {
        let p = Preferences {
            theme,
            ..Default::default()
        }
        .palette();
        for (a, b) in [(p.text, p.panel), (p.muted, p.bg), (p.bg, p.accent)] {
            let a = luminance(a);
            let b = luminance(b);
            assert!((a.max(b) + 0.05) / (a.min(b) + 0.05) >= 4.5);
        }
    }
}
