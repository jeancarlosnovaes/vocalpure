fn bg() -> Color32 {
    preferences::current().palette().bg
}
fn panel() -> Color32 {
    preferences::current().palette().panel
}
fn border() -> Color32 {
    preferences::current().palette().border
}
fn text() -> Color32 {
    preferences::current().palette().text
}
fn muted() -> Color32 {
    preferences::current().palette().muted
}
fn accent() -> Color32 {
    preferences::current().palette().accent
}
use super::Setup;
use egui_baseview::{
    egui::{
        self, pos2, vec2, Align2, Color32, FontId, Margin, Rect, RichText, Sense, Stroke,
        TextStyle, TextWrapMode,
    },
    Queue,
};
use vocalpure::preferences::{self, tr, Theme};

pub(super) fn configure(ctx: &egui::Context) {
    let mut style = (*ctx.style()).clone();
    style
        .text_styles
        .insert(TextStyle::Body, FontId::proportional(15.0));
    style
        .text_styles
        .insert(TextStyle::Button, FontId::proportional(15.0));
    style
        .text_styles
        .insert(TextStyle::Small, FontId::proportional(12.0));
    style
        .text_styles
        .insert(TextStyle::Heading, FontId::proportional(30.0));
    style.spacing.item_spacing = vec2(12.0, 8.0);
    style.spacing.button_padding = vec2(14.0, 10.0);
    style.spacing.interact_size.y = 40.0;
    style.visuals = if preferences::current().theme == Theme::Dark {
        egui::Visuals::dark()
    } else {
        egui::Visuals::light()
    };
    style.visuals.panel_fill = bg();
    style.visuals.window_fill = panel();
    style.visuals.extreme_bg_color = preferences::current().palette().control;
    style.visuals.override_text_color = Some(text());
    style.visuals.selection.bg_fill = preferences::current().palette().selected;
    style.visuals.selection.stroke = Stroke::new(1.0_f32, accent());
    for widgets in [
        &mut style.visuals.widgets.inactive,
        &mut style.visuals.widgets.noninteractive,
    ] {
        widgets.bg_fill = preferences::current().palette().control;
        widgets.weak_bg_fill = preferences::current().palette().control;
        widgets.bg_stroke = Stroke::new(1.0_f32, border());
        widgets.corner_radius = 7.into();
        widgets.fg_stroke = Stroke::new(1.0_f32, text());
    }
    style.visuals.widgets.hovered.bg_fill = preferences::current().palette().hover;
    style.visuals.widgets.hovered.weak_bg_fill = preferences::current().palette().hover;
    style.visuals.widgets.hovered.bg_stroke = Stroke::new(1.0_f32, accent());
    style.visuals.widgets.hovered.corner_radius = 7.into();
    style.visuals.widgets.active.bg_fill = preferences::current().palette().selected;
    style.visuals.widgets.active.corner_radius = 7.into();
    ctx.set_style(style);
}

fn wave(ui: &egui::Ui, rect: Rect, color: Color32) {
    for (i, height) in [10.0, 21.0, 32.0, 16.0, 26.0, 12.0, 20.0]
        .iter()
        .enumerate()
    {
        let x = rect.left() + 6.0 + i as f32 * 7.0;
        ui.painter().line_segment(
            [
                pos2(x, rect.center().y - height * 0.5),
                pos2(x, rect.center().y + height * 0.5),
            ],
            Stroke::new(3.0_f32, color),
        );
    }
}

fn mode(ui: &mut egui::Ui, selected: bool, title: &str, subtitle: &str) -> egui::Response {
    let (rect, response) = ui.allocate_exact_size(vec2(ui.available_width(), 72.0), Sense::click());
    let fill = if selected {
        preferences::current().palette().selected
    } else if response.hovered() {
        preferences::current().palette().hover
    } else {
        panel()
    };
    ui.painter().rect_filled(rect, 10.0, fill);
    ui.painter().rect_stroke(
        rect,
        10.0,
        Stroke::new(
            if selected { 1.5_f32 } else { 1.0_f32 },
            if selected { accent() } else { border() },
        ),
        egui::StrokeKind::Inside,
    );
    let centre = pos2(rect.left() + 22.0, rect.top() + 25.0);
    ui.painter().circle_stroke(
        centre,
        7.0,
        Stroke::new(1.5_f32, if selected { accent() } else { muted() }),
    );
    if selected {
        ui.painter().circle_filled(centre, 3.5, accent());
    }
    ui.painter().text(
        pos2(rect.left() + 40.0, rect.top() + 25.0),
        Align2::LEFT_CENTER,
        tr(title),
        FontId::proportional(16.0),
        text(),
    );
    ui.painter().text(
        pos2(rect.left() + 18.0, rect.top() + 55.0),
        Align2::LEFT_CENTER,
        tr(subtitle),
        FontId::proportional(13.0),
        muted(),
    );
    response
}

fn devices(
    ui: &mut egui::Ui,
    id: &str,
    label: &str,
    names: &[String],
    selected: &mut usize,
) -> bool {
    ui.label(RichText::new(tr(label)).size(13.0).color(muted()));
    let mut changed = false;
    let text = preferences::current().device_label(names.get(*selected).map(String::as_str));
    egui::ComboBox::from_id_salt(id)
        .width(ui.available_width())
        .wrap_mode(TextWrapMode::Truncate)
        .selected_text(RichText::new(&text).size(16.0))
        .show_ui(ui, |ui| {
            for (i, name) in names.iter().enumerate() {
                changed |= ui.selectable_value(selected, i, name).changed();
            }
        })
        .response
        .on_hover_text(&text);
    changed
}

pub(super) fn show(ctx: &egui::Context, queue: &mut Queue, s: &mut Setup) {
    preferences::set_current(s.preferences);
    configure(ctx);
    egui::CentralPanel::default().frame(egui::Frame::NONE.fill(bg()).inner_margin(Margin::same(24))).show(ctx,|ui| {
            ui.horizontal(|ui| {
                let (logo,_) = ui.allocate_exact_size(vec2(54.0,42.0),Sense::hover());
                wave(ui,logo,accent());
                ui.vertical(|ui| {
                    ui.label(RichText::new(tr("V O C A L P U R E")).size(19.0).color(text()));
                    ui.label(RichText::new(tr("ÁUDIO NEURAL LOCAL")).size(10.0).color(accent()));
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center),|ui| {
                    preferences::controls(ui, &mut s.preferences);
                });
            });
            ui.add_space(12.0);
            ui.label(RichText::new(tr("Configure seu áudio")).size(29.0).strong().color(text()));
            ui.label(RichText::new(tr("Escolha como deseja abrir o VocalPure.")).color(muted()));
            ui.add_space(9.0);
            ui.columns(2,|cols| {
                if mode(&mut cols[0],!s.preview,"Processar áudio","Use sua entrada e seus fones.").clicked() {s.preview=false;s.error.clear();}
                if mode(&mut cols[1],s.preview,"Explorar interface","Conheça os controles, sem áudio.").clicked() {s.preview=true;s.error.clear();}
            });
            ui.add_space(9.0);
            egui::Frame::NONE.fill(panel()).stroke(Stroke::new(1.0_f32,border())).corner_radius(12).inner_margin(Margin::same(18)).show(ui,|ui| {
                ui.set_min_width((ui.available_width()).max(0.0));
                if s.preview {
                    ui.set_min_height(220.0);
                    ui.add_space(12.0);
                    ui.label(RichText::new(tr("Seu espaço para explorar")).size(20.0).color(text()));
                    ui.add_space(8.0);
                    ui.label(RichText::new(tr("Veja os perfis, presets e controles do VocalPure antes de conectar seu áudio.")).color(muted()));
                    ui.add_space(21.0);
                    for label in ["Nenhum dispositivo será utilizado", "Você poderá conhecer todos os ajustes", "O processamento fica desligado nesta prévia"] {
                        ui.horizontal(|ui| {
                            let (dot,_) = ui.allocate_exact_size(vec2(10.0,18.0),Sense::hover());
                            ui.painter().circle_filled(dot.center(),2.5,accent());
                            ui.label(tr(label));
                        });
                    }
                } else {
                    let mut changed=devices(ui,"input","ENTRADA DE ÁUDIO",&s.inputs,&mut s.input);
                    ui.add_space(2.0);
                    changed |= devices(ui,"output","SAÍDA DE ÁUDIO",&s.outputs,&mut s.output);
                    ui.add_space(4.0);
                    ui.columns(2,|cols| {
                        cols[0].label(RichText::new(tr("TAXA DE AMOSTRAGEM")).size(12.0).color(muted()));
                        egui::ComboBox::from_id_salt("rate").width(cols[0].available_width()).selected_text(tr(match s.rate {44100=>"44,1 kHz",96000=>"96 kHz",_=>"48 kHz"}))
                            .show_ui(&mut cols[0],|ui| {for (rate,label) in [(44100,"44,1 kHz"),(48000,"48 kHz"),(96000,"96 kHz")] {changed |= ui.selectable_value(&mut s.rate,rate,tr(label)).changed();}});
                        cols[1].label(RichText::new(tr("CANAIS")).size(12.0).color(muted()));
                        egui::ComboBox::from_id_salt("channels").width(cols[1].available_width()).selected_text(tr(if s.channels==1 {"Mono"} else {"Estéreo"}))
                            .show_ui(&mut cols[1],|ui| {changed |= ui.selectable_value(&mut s.channels,1,"Mono").changed();changed |= ui.selectable_value(&mut s.channels,2,tr("Estéreo")).changed();});
                    });
                    if changed {s.error.clear();}
                }
            });
            ui.add_space(7.0);
            ui.label(RichText::new(tr(if s.preview {"A prévia não captura nem reproduz áudio."} else {"Use fones de ouvido para evitar o retorno do microfone."})).size(13.0).color(muted()));
            ui.add_space(8.0);
            if !s.error.is_empty() {
                egui::Frame::NONE.fill(if preferences::current().theme==Theme::Dark {Color32::from_rgb(57,29,38)} else {Color32::from_rgb(255,233,232)}).corner_radius(8).inner_margin(Margin::same(12)).show(ui,|ui| {
                    ui.set_max_width(ui.available_width());
                    ui.label(RichText::new(tr(&s.error)).color(if preferences::current().theme==Theme::Dark {Color32::from_rgb(255,180,182)} else {Color32::from_rgb(150,36,42)}));
                });
                ui.add_space(4.0);
            }
            let available=s.preview || (!s.inputs.is_empty() && !s.outputs.is_empty());
            let action=if s.preview {"Explorar VocalPure"} else {"Iniciar VocalPure"};
            let button=egui::Button::new(RichText::new(tr(action)).size(17.0).strong().color(bg())).fill(accent()).corner_radius(9);
            let response=ui.add_enabled(available,button.min_size(vec2(ui.available_width(),48.0)));
            // Draw icons as vector strokes so missing font glyphs cannot turn
            // arrows into empty squares on Windows.
            let centre=response.rect.right_center()-vec2(25.0,0.0);
            let icon_color=if available {bg()} else {muted()};
            ui.painter().line_segment([centre-vec2(7.0,0.0),centre+vec2(7.0,0.0)],Stroke::new(2.0_f32,icon_color));
            ui.painter().add(egui::Shape::line(vec![centre+vec2(1.0,-6.0),centre+vec2(7.0,0.0),centre+vec2(1.0,6.0)],Stroke::new(2.0_f32,icon_color)));
            if response.clicked() {
                match s.arguments() {
                    Ok(args)=>{*s.result.lock().unwrap()=Some(args);queue.close_window();}
                    Err(error)=>s.error=error,
                }
            }
            ui.add_space(9.0);
            ui.horizontal(|ui| {
                ui.label(RichText::new(tr("Processamento no seu computador")).size(12.0).color(muted()));
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center),|ui| {
                    ui.label(RichText::new(format!("v{}",env!("CARGO_PKG_VERSION"))).size(12.0).color(muted()));
                });
            });
    });
}
