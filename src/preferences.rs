use nih_plug_egui::egui::{self, Color32};
use serde::{Deserialize, Serialize};
use std::{
    cell::Cell,
    fs, io,
    path::{Path, PathBuf},
    sync::atomic::{AtomicU64, Ordering},
};

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Language {
    English,
    #[default]
    Portuguese,
}
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Theme {
    #[default]
    Dark,
    Light,
}
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct Preferences {
    pub language: Language,
    pub theme: Theme,
}

pub struct Palette {
    pub bg: Color32,
    pub panel: Color32,
    pub control: Color32,
    pub border: Color32,
    pub text: Color32,
    pub muted: Color32,
    pub accent: Color32,
    pub selected: Color32,
    pub hover: Color32,
}
impl Preferences {
    pub fn palette(self) -> Palette {
        if self.theme == Theme::Dark {
            Palette {
                bg: Color32::from_rgb(10, 21, 29),
                panel: Color32::from_rgb(17, 35, 46),
                control: Color32::from_rgb(12, 28, 39),
                border: Color32::from_rgb(66, 92, 106),
                text: Color32::from_rgb(232, 245, 246),
                muted: Color32::from_rgb(157, 183, 195),
                accent: Color32::from_rgb(125, 232, 218),
                selected: Color32::from_rgb(24, 62, 69),
                hover: Color32::from_rgb(24, 53, 64),
            }
        } else {
            Palette {
                bg: Color32::from_rgb(240, 247, 248),
                panel: Color32::WHITE,
                control: Color32::from_rgb(248, 252, 252),
                border: Color32::from_rgb(153, 180, 190),
                text: Color32::from_rgb(20, 53, 65),
                muted: Color32::from_rgb(73, 99, 112),
                accent: Color32::from_rgb(8, 115, 123),
                selected: Color32::from_rgb(218, 243, 239),
                hover: Color32::from_rgb(227, 241, 243),
            }
        }
    }
    pub fn text(self, source: &str) -> String {
        for &(en, pt) in TRANSLATIONS {
            if source == en || source == pt {
                return match self.language {
                    Language::English => en,
                    Language::Portuguese => pt,
                }
                .into();
            }
        }
        for &(en, pt) in PREFIXES {
            for prefix in [en, pt] {
                if let Some(rest) = source.strip_prefix(prefix) {
                    let rest = if self.language == Language::Portuguese {
                        rest.replace(" samples latency  /  ", " amostras de latência  /  ")
                            .replace("No audio input", "Sem entrada de áudio")
                            .replace("Audio active", "Áudio ativo")
                    } else {
                        rest.replace(" amostras de latência  /  ", " samples latency  /  ")
                            .replace("Sem entrada de áudio", "No audio input")
                            .replace("Áudio ativo", "Audio active")
                    };
                    return format!(
                        "{}{}",
                        if self.language == Language::English {
                            en
                        } else {
                            pt
                        },
                        rest
                    );
                }
            }
        }
        source.into()
    }
    /// Device names are supplied by Windows, not by the UI translation table.
    pub fn device_label(self, name: Option<&str>) -> String {
        name.map(str::to_owned)
            .unwrap_or_else(|| self.text("No devices available"))
    }
    pub fn encode(self) -> String {
        format!(
            "language={}\ntheme={}\n",
            if self.language == Language::English {
                "en"
            } else {
                "pt"
            },
            if self.theme == Theme::Light {
                "light"
            } else {
                "dark"
            }
        )
    }
    pub fn decode(source: &str) -> Self {
        let mut p = Self::default();
        for line in source.lines() {
            match line.trim() {
                "language=en" => p.language = Language::English,
                "language=pt" => p.language = Language::Portuguese,
                "theme=light" => p.theme = Theme::Light,
                "theme=dark" => p.theme = Theme::Dark,
                _ => {}
            }
        }
        p
    }
    fn path() -> Option<PathBuf> {
        std::env::var_os("LOCALAPPDATA")
            .or_else(|| std::env::var_os("XDG_CONFIG_HOME"))
            .map(|p| PathBuf::from(p).join("VocalPure").join("preferences.ini"))
    }
    pub fn load() -> Self {
        Self::path()
            .and_then(|p| fs::read_to_string(p).ok())
            .map_or_else(Self::default, |s| Self::decode(&s))
    }
    pub fn save(self) -> io::Result<()> {
        self.save_to(&Self::path().ok_or_else(|| io::Error::other("No preferences directory"))?)
    }
    pub fn save_to(self, path: &Path) -> io::Result<()> {
        static SEQUENCE: AtomicU64 = AtomicU64::new(0);
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        let temp = path.with_extension(format!(
            "{}.{}.tmp",
            std::process::id(),
            SEQUENCE.fetch_add(1, Ordering::Relaxed)
        ));
        fs::write(&temp, self.encode())?;
        fs::rename(&temp, path)
    }
}
thread_local! {static CURRENT:Cell<Preferences>=Cell::new(Preferences::default());}
pub fn current() -> Preferences {
    CURRENT.with(Cell::get)
}
pub fn set_current(p: Preferences) {
    CURRENT.with(|c| c.set(p));
}
pub fn tr(source: &str) -> String {
    current().text(source)
}

pub fn controls(ui: &mut egui::Ui, p: &mut Preferences) -> bool {
    let before = *p;
    ui.horizontal(|ui| {
        egui::ComboBox::from_id_salt("ui-language")
            .width(94.0)
            .selected_text(if p.language == Language::English {
                "English"
            } else {
                "Português"
            })
            .show_ui(ui, |ui| {
                ui.selectable_value(&mut p.language, Language::English, "English");
                ui.selectable_value(&mut p.language, Language::Portuguese, "Português");
            });
        egui::ComboBox::from_id_salt("ui-theme")
            .width(80.0)
            .selected_text(p.text(if p.theme == Theme::Dark {
                "Dark"
            } else {
                "Light"
            }))
            .show_ui(ui, |ui| {
                let dark = p.text("Dark");
                let light = p.text("Light");
                ui.selectable_value(&mut p.theme, Theme::Dark, dark);
                ui.selectable_value(&mut p.theme, Theme::Light, light);
            });
    });
    if before != *p {
        let _ = p.save();
        set_current(*p);
        ui.ctx().request_repaint();
        true
    } else {
        false
    }
}

const PREFIXES: &[(&str, &str)] = &[
    ("VOICE PROBABILITY  ", "PROBABILIDADE DE VOZ  "),
    ("LEVEL CHANGE  ", "VARIAÇÃO DE NÍVEL  "),
    ("RNNoise ready  /  ", "RNNoise pronto  /  "),
    ("INPUT  ", "ENTRADA  "),
    ("OUTPUT  ", "SAÍDA  "),
    (
        "Could not open input: ",
        "Não foi possível abrir a entrada: ",
    ),
    (
        "Could not open output: ",
        "Não foi possível abrir a saída: ",
    ),
];
const TRANSLATIONS:&[(&str,&str)]=&[
    ("Dark","Escuro"),("Light","Claro"),("Settings","Ajustes"),("SETTINGS","AJUSTES"),("UNDO","DESFAZER"),
    ("LOCAL NEURAL AUDIO","ÁUDIO NEURAL LOCAL"),("AUDIO SETUP","CONFIGURAÇÃO DE ÁUDIO"),
    ("Set up your audio","Configure seu áudio"),("Choose how to open VocalPure.","Escolha como deseja abrir o VocalPure."),
    ("Process audio","Processar áudio"),("Use your input and headphones.","Use sua entrada e seus fones."),
    ("Explore the interface","Explorar interface"),("Try the controls without audio.","Conheça os controles, sem áudio."),
    ("A space to explore","Seu espaço para explorar"),
    ("Explore VocalPure's profiles, presets and controls before connecting audio.","Veja os perfis, presets e controles do VocalPure antes de conectar seu áudio."),
    ("No devices will be used","Nenhum dispositivo será utilizado"),("Explore all the settings","Você poderá conhecer todos os ajustes"),
    ("Audio processing is disabled in this preview","O processamento fica desligado nesta prévia"),
    ("AUDIO INPUT","ENTRADA DE ÁUDIO"),("AUDIO OUTPUT","SAÍDA DE ÁUDIO"),("SAMPLE RATE","TAXA DE AMOSTRAGEM"),("CHANNELS","CANAIS"),
    ("Stereo","Estéreo"),("No devices available","Nenhum dispositivo disponível"),
    ("The preview does not capture or play audio.","A prévia não captura nem reproduz áudio."),
    ("Use headphones to avoid microphone feedback.","Use fones de ouvido para evitar o retorno do microfone."),
    ("Explore VocalPure","Explorar VocalPure"),("Start VocalPure","Iniciar VocalPure"),
    ("Processing on your computer","Processamento no seu computador"),
    ("Select an audio input.","Selecione uma entrada de áudio."),("Select an audio output.","Selecione uma saída de áudio."),
    ("The input is no longer available.","A entrada não está mais disponível."),("The output is no longer available.","A saída não está mais disponível."),
    ("CUSTOM / SELECT PRESET","PERSONALIZADO / PRESET"),("COMPARE ORIGINAL","COMPARAR ORIGINAL"),("BYPASS","IGNORAR EFEITO"),
    ("RNNOISE PROFILE","PERFIL RNNOISE"),("General","Geral"),("Strong","Forte"),("Eco","Eco"),
    ("ANALYSIS","ANÁLISE"),("DUAL","ESTÉREO"),("WIDTH","LARGURA"),("MONO: NO EFFECT","MONO: SEM EFEITO"),
    ("ORIGINAL: INACTIVE","ORIGINAL: INATIVO"),("STEREO WIDTH","LARGURA ESTÉREO"),("NOISE REDUCTION","REDUÇÃO DE RUÍDO"),
    ("LIVE SPECTRUM","ESPECTRO AO VIVO"),("COMPARE","COMPARAR"),("INPUT","ENTRADA"),("OUTPUT","SAÍDA"),("IN","ENT."),("OUT","SAÍDA"),
    ("LISTENING TO ORIGINAL","OUVINDO O ORIGINAL"),("NO SIGNAL","SEM SINAL"),("Unsupported sample rate","Taxa não suportada"),
    ("Neural engine failed: aligned dry audio","Falha no motor neural: áudio original alinhado"),
    ("Invalid input samples removed","Amostras inválidas removidas"),("No audio input","Sem entrada de áudio"),("Audio active","Áudio ativo"),
    ("Natural Voice","Voz natural"),("Podcast Clean","Podcast limpo"),("Studio Voice","Voz de estúdio"),("Noisy Room","Ambiente ruidoso"),
    ("Outdoor Dialogue","Diálogo externo"),("Strong Noise Reduction","Redução intensa"),("Gentle Cleaning","Limpeza suave"),("Low CPU","Baixo consumo"),
    ("Input Gain","Ganho de entrada"),("Output Gain","Ganho de saída"),("Noise Reduction","Redução de ruído"),("Stereo Width","Largura estéreo"),
    ("Local RNNoise 0.1.1 processing. General, Strong and Eco use the same embedded model.","Processamento local com RNNoise 0.1.1. Geral, Forte e Eco usam o mesmo modelo incorporado."),
    ("44.1, 48 and 96 kHz. Latency: 20 ms at 48 kHz; 26.67 ms otherwise.","44,1, 48 e 96 kHz. Latência: 20 ms em 48 kHz; 26,67 ms nas outras taxas."),
    ("Drag the reduction dial to adjust. Shift: fine adjustment. Double-click: 65%.","Arraste o controle de redução. Shift: ajuste fino. Clique duplo: 65%."),
    ("Compare and Bypass play the aligned original. Width affects stereo only.","Comparar e Ignorar efeito reproduzem o original alinhado. A largura afeta apenas o estéreo."),
    ("Level change measures the whole signal, not isolated noise. VAD can give false positives.","A variação de nível mede o sinal completo, não o ruído isolado. A detecção de voz pode gerar falsos positivos."),
    ("This plugin has a separate identity. Existing JUCE sessions are not imported.","Este plugin tem identidade própria. Sessões antigas do JUCE não são importadas."),
];
