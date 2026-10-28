use crate::preferences::{self, Preferences, Theme};
use crate::{
    engine::AudioParameters,
    spectrum::{Analyzer, Frame, SIZE},
    Meters, VocalPureParams, PRESETS,
};
use crossbeam::queue::ArrayQueue;
use nih_plug::prelude::*;
use nih_plug_egui::{
    create_egui_editor,
    egui::{
        self, pos2, vec2, Align2, Color32, FontId, Id, Painter, Pos2, Rect, Response, Sense, Shape,
        Stroke,
    },
    resizable_window::ResizableWindow,
};
use std::{
    f32::consts::{PI, TAU},
    sync::{atomic::Ordering, Arc},
    time::{Duration, Instant},
};

const WHITE: Color32 = Color32::from_rgb(236, 255, 255);
const CYAN: Color32 = Color32::from_rgb(139, 225, 230);
const PINK: Color32 = Color32::from_rgb(235, 105, 226);
const DARK: Color32 = Color32::from_rgb(7, 60, 80);

struct UiState {
    undo: Option<AudioParameters>,
    analyzer: Analyzer,
    input: Vec<f32>,
    output: Vec<f32>,
    view: usize,
    last_frame: Instant,
    about: bool,
    meter_db: [f32; 2],
    selected: Option<usize>,
}
impl Default for UiState {
    fn default() -> Self {
        Self {
            undo: None,
            analyzer: Analyzer::default(),
            input: vec![-100.0; SIZE / 2 + 1],
            output: vec![-100.0; SIZE / 2 + 1],
            view: 0,
            last_frame: Instant::now(),
            about: false,
            meter_db: [-100.0; 2],
            selected: None,
        }
    }
}
fn set<P: Param>(setter: &ParamSetter, p: &P, value: P::Plain) {
    setter.begin_set_parameter(p);
    setter.set_parameter(p, value);
    setter.end_set_parameter(p);
}
fn apply(p: &VocalPureParams, setter: &ParamSetter, v: AudioParameters) {
    set(setter, &p.input, v.input_db);
    set(setter, &p.output, v.output_db);
    set(setter, &p.reduction, v.reduction);
    set(setter, &p.width, v.width);
    set(setter, &p.bypass, v.bypass);
    set(setter, &p.compare, v.compare);
    set(setter, &p.profile, v.profile);
}

// Preserve the original 1000 x 560 layout and proportions at any host size.
struct Canvas {
    preferences: Preferences,
    origin: Pos2,
    scale: f32,
    painter: Painter,
}
impl Canvas {
    fn ink(&self, color: Color32) -> Color32 {
        if color == WHITE || color == DARK {
            self.preferences.palette().text
        } else if (self.preferences.theme == Theme::Light
            && u16::from(color.r()) + u16::from(color.g()) + u16::from(color.b()) > 450)
            || color == Color32::from_rgb(168, 190, 204)
            || color == Color32::from_rgb(168, 220, 230)
        {
            self.preferences.palette().muted
        } else {
            color
        }
    }

    fn p(&self, x: f32, y: f32) -> Pos2 {
        self.origin + vec2(x, y) * self.scale
    }
    fn rect(&self, x: f32, y: f32, w: f32, h: f32) -> Rect {
        Rect::from_min_size(self.p(x, y), vec2(w, h) * self.scale)
    }
    fn text(&self, x: f32, y: f32, text: impl ToString, size: f32, color: Color32, align: Align2) {
        self.painter.text(
            self.p(x, y),
            align,
            self.preferences.text(&text.to_string()),
            FontId::proportional(size * self.scale),
            self.ink(color),
        );
    }
    fn line(&self, a: (f32, f32), b: (f32, f32), width: f32, color: Color32) {
        self.painter.line_segment(
            [self.p(a.0, a.1), self.p(b.0, b.1)],
            Stroke::new(width * self.scale, color),
        );
    }
    fn disc(&self, c: (f32, f32), radius: f32, centre: Color32, edge: Color32) {
        let (centre, edge) = if self.preferences.theme == Theme::Light {
            (
                mix(self.preferences.palette().panel, centre, 0.06),
                mix(self.preferences.palette().bg, edge, 0.03),
            )
        } else {
            (centre, edge)
        };
        let mut mesh = egui::Mesh::default();
        mesh.colored_vertex(self.p(c.0, c.1), centre);
        for i in 0..=128 {
            let a = i as f32 / 128.0 * TAU;
            mesh.colored_vertex(self.p(c.0 + radius * a.cos(), c.1 + radius * a.sin()), edge);
            if i > 0 {
                mesh.add_triangle(0, i, i + 1);
            }
        }
        self.painter.add(Shape::mesh(mesh));
    }
    fn rim(&self, c: (f32, f32), radius: f32, width: f32, left: Color32, right: Color32) {
        let mut mesh = egui::Mesh::default();
        for i in 0..=128 {
            let a = i as f32 / 128.0 * TAU;
            let color = mix(left, right, (a.cos() + 1.0) * 0.5);
            for r in [radius, radius + width] {
                mesh.colored_vertex(self.p(c.0 + r * a.cos(), c.1 + r * a.sin()), color);
            }
            if i > 0 {
                let k = i * 2;
                mesh.add_triangle(k - 2, k - 1, k);
                mesh.add_triangle(k - 1, k + 1, k);
            }
        }
        self.painter.add(Shape::mesh(mesh));
    }
}
fn mix(a: Color32, b: Color32, t: f32) -> Color32 {
    let t = t.clamp(0.0, 1.0);
    let a = a.to_array();
    let b = b.to_array();
    let c: [u8; 4] = std::array::from_fn(|i| (a[i] as f32 + (b[i] as f32 - a[i] as f32) * t) as u8);
    Color32::from_rgba_premultiplied(c[0], c[1], c[2], c[3])
}
fn background(c: &Canvas) {
    let mut mesh = egui::Mesh::default();
    for y in 0..=20 {
        for x in 0..=32 {
            let px = x as f32 / 32.0 * 1000.0;
            let py = 42.0 + y as f32 / 20.0 * 518.0;
            let axis = vec2(-260.0, 515.0);
            let t =
                ((vec2(px, py) - vec2(480.0, 45.0)).dot(axis) / axis.length_sq()).clamp(0.0, 1.0);
            let _base = mix(
                Color32::from_rgb(39, 212, 222),
                Color32::from_rgb(5, 100, 135),
                t,
            );
            let _purple = ((px - 420.0) / 580.0 * 0.6).clamp(0.0, 0.6)
                * ((py - 100.0) / 450.0).clamp(0.0, 1.0);
            mesh.colored_vertex(
                c.p(px, py),
                if c.preferences.theme == Theme::Dark {
                    mix(c.preferences.palette().bg, Color32::from_rgb(24, 49, 65), t)
                } else {
                    mix(
                        c.preferences.palette().bg,
                        Color32::from_rgb(211, 235, 235),
                        t,
                    )
                },
            );
            if x > 0 && y > 0 {
                let k = y * 33 + x;
                mesh.add_triangle(k - 34, k - 33, k - 1);
                mesh.add_triangle(k - 33, k, k - 1);
            }
        }
    }
    c.painter.add(Shape::mesh(mesh));
    let nodes = [
        (20.0, 160.0),
        (140.0, 130.0),
        (230.0, 205.0),
        (340.0, 105.0),
        (440.0, 170.0),
        (600.0, 95.0),
        (730.0, 170.0),
        (930.0, 110.0),
        (75.0, 350.0),
        (190.0, 440.0),
        (350.0, 360.0),
        (490.0, 460.0),
        (650.0, 390.0),
        (800.0, 460.0),
        (950.0, 330.0),
        (990.0, 220.0),
    ];
    for (i, &node) in nodes.iter().enumerate() {
        for offset in [3, 6] {
            c.line(
                node,
                nodes[(i + offset) % nodes.len()],
                0.7,
                Color32::from_rgba_unmultiplied(176, 244, 245, 25),
            );
        }
        c.painter.circle_filled(
            c.p(node.0, node.1),
            2.5 * c.scale,
            Color32::from_rgba_unmultiplied(217, 249, 255, 43),
        );
    }
    c.painter.add(Shape::convex_polygon(
        vec![
            c.p(445.0, 380.0),
            c.p(618.0, 230.0),
            c.p(905.0, 490.0),
            c.p(560.0, 490.0),
        ],
        Color32::from_rgba_unmultiplied(82, 21, 136, 55),
        Stroke::NONE,
    ));
    c.painter.rect_filled(
        c.rect(0.0, 0.0, 1000.0, 42.0),
        0.0,
        preferences::current().palette().panel,
    );
    c.painter.rect_filled(
        c.rect(0.0, 42.0, 1000.0, 2.0),
        0.0,
        Color32::from_rgb(123, 223, 228),
    );
    c.text(
        42.0,
        87.0,
        "V O C A L P U R E",
        28.0,
        DARK,
        Align2::LEFT_CENTER,
    );
    c.text(
        44.0,
        115.0,
        env!("CARGO_PKG_VERSION"),
        11.0,
        DARK,
        Align2::LEFT_CENTER,
    );
    c.text(
        814.0,
        87.0,
        "LOCAL NEURAL AUDIO",
        17.0,
        DARK,
        Align2::CENTER_CENTER,
    );
}
fn button(
    ui: &mut egui::Ui,
    c: &Canvas,
    id: &str,
    r: Rect,
    label: &str,
    enabled: bool,
) -> Response {
    let response = ui.interact(
        r,
        Id::new(id),
        if enabled {
            Sense::click()
        } else {
            Sense::hover()
        },
    );
    let fill = if response.hovered() && enabled {
        c.preferences.palette().hover
    } else {
        c.preferences.palette().panel
    };
    c.painter.rect_filled(r, 5.0 * c.scale, fill);
    c.painter.rect_stroke(
        r,
        5.0 * c.scale,
        Stroke::new(c.scale, c.preferences.palette().border),
        egui::StrokeKind::Inside,
    );
    c.painter.text(
        r.center(),
        Align2::CENTER_CENTER,
        c.preferences.text(label),
        FontId::proportional(14.0 * c.scale),
        if enabled {
            c.preferences.palette().text
        } else {
            c.preferences.palette().muted
        },
    );
    response
}
fn toggle(
    ui: &mut egui::Ui,
    c: &Canvas,
    id: &str,
    x: f32,
    w: f32,
    label: &str,
    param: &BoolParam,
    setter: &ParamSetter,
) {
    let response = ui.interact(c.rect(x, 7.0, w, 28.0), Id::new(id), Sense::click());
    c.painter.rect_stroke(
        c.rect(x + 1.0, 12.0, 17.0, 17.0),
        3.0 * c.scale,
        Stroke::new(c.scale, Color32::from_gray(132)),
        egui::StrokeKind::Inside,
    );
    if param.value() {
        c.line((x + 4.0, 20.0), (x + 8.0, 24.0), 2.0, CYAN);
        c.line((x + 8.0, 24.0), (x + 15.0, 15.0), 2.0, CYAN);
    }
    c.text(x + 24.0, 21.0, label, 13.0, WHITE, Align2::LEFT_CENTER);
    if response.clicked() {
        set(setter, param, !param.value());
    }
}
fn menu(
    ui: &mut egui::Ui,
    c: &Canvas,
    id: &str,
    r: Rect,
    label: &str,
    options: &[&str],
    pill: bool,
) -> Option<usize> {
    let response = ui.interact(r, Id::new(id), Sense::click());
    let popup_id = Id::new((id, "popup"));
    if pill {
        c.painter.rect_filled(r, r.height() * 0.5, CYAN);
        c.painter.rect_filled(
            r.shrink(4.0 * c.scale),
            r.height() * 0.45,
            preferences::current().palette().control,
        );
    } else {
        c.painter
            .rect_filled(r, 2.0 * c.scale, preferences::current().palette().control);
        c.painter.rect_stroke(
            r,
            2.0 * c.scale,
            Stroke::new(c.scale, Color32::from_rgb(54, 81, 98)),
            egui::StrokeKind::Inside,
        );
    }
    c.painter.text(
        if pill {
            r.center() - vec2(10.0 * c.scale, 0.0)
        } else {
            r.left_center() + vec2(6.0 * c.scale, 0.0)
        },
        if pill {
            Align2::CENTER_CENTER
        } else {
            Align2::LEFT_CENTER
        },
        c.preferences.text(label),
        FontId::proportional(if pill { 23.0 * c.scale } else { 14.0 * c.scale }),
        c.preferences.palette().text,
    );
    let a = r.right_center() - vec2(19.0 * c.scale, 0.0);
    c.painter.add(Shape::line(
        vec![
            a + vec2(-5.0, -3.0) * c.scale,
            a + vec2(0.0, 3.0) * c.scale,
            a + vec2(5.0, -3.0) * c.scale,
        ],
        Stroke::new(2.0 * c.scale, c.preferences.palette().text),
    ));
    if response.clicked() {
        ui.memory_mut(|m| m.toggle_popup(popup_id));
    }
    let mut selected = None;
    egui::popup_below_widget(
        ui,
        popup_id,
        &response,
        egui::PopupCloseBehavior::CloseOnClick,
        |ui| {
            for (i, label) in options.iter().enumerate() {
                if ui
                    .selectable_label(false, c.preferences.text(label))
                    .clicked()
                {
                    selected = Some(i);
                }
            }
        },
    );
    selected
}
fn parameter_interaction(
    ui: &mut egui::Ui,
    c: &Canvas,
    id: &str,
    r: Rect,
    param: &FloatParam,
    setter: &ParamSetter,
    enabled: bool,
    rotary: bool,
) -> Response {
    let response = ui.interact(
        r,
        Id::new(id),
        if enabled {
            Sense::click_and_drag()
        } else {
            Sense::hover()
        },
    );
    if enabled {
        if response.drag_started() {
            setter.begin_set_parameter(param);
        }
        if response.dragged() {
            let fine = ui.input(|i| i.modifiers.shift);
            let value = if rotary {
                let d = response.drag_motion();
                param.unmodulated_normalized_value()
                    + (d.x - d.y) / (if fine { 2400.0 } else { 240.0 } * c.scale)
            } else {
                response
                    .interact_pointer_pos()
                    .map_or(param.unmodulated_normalized_value(), |p| {
                        (p.x - r.left()) / r.width()
                    })
            };
            setter.set_parameter(param, param.preview_plain(value.clamp(0.0, 1.0)));
        }
        if response.drag_stopped() {
            setter.end_set_parameter(param);
        }
        if response.double_clicked() {
            set(setter, param, param.default_plain_value());
        }
        if response.clicked() {
            response.request_focus();
        }
        let scroll = if response.hovered() {
            ui.input(|i| i.smooth_scroll_delta.y)
        } else {
            0.0
        };
        if scroll != 0.0 {
            set(
                setter,
                param,
                param.preview_plain(
                    (param.unmodulated_normalized_value() + scroll * 0.001).clamp(0.0, 1.0),
                ),
            );
        }
        if response.has_focus() {
            let delta = ui.input(|i| {
                if i.key_pressed(egui::Key::ArrowUp) || i.key_pressed(egui::Key::ArrowRight) {
                    0.01
                } else if i.key_pressed(egui::Key::ArrowDown) || i.key_pressed(egui::Key::ArrowLeft)
                {
                    -0.01
                } else {
                    0.0
                }
            });
            if delta != 0.0 {
                set(
                    setter,
                    param,
                    param.preview_plain(
                        (param.unmodulated_normalized_value() + delta).clamp(0.0, 1.0),
                    ),
                );
            }
        }
    }
    response.on_hover_text(format!(
        "{}: {:.1}{}\n{}",
        c.preferences.text(param.name()),
        param.value(),
        param.unit(),
        c.preferences
            .text("Drag to adjust. Shift: fine. Double-click: default.")
    ))
}
fn radial_levels(state: &UiState) -> [f32; 32] {
    let top = 20000.0_f32.min(state.analyzer.rate * 0.5);
    std::array::from_fn(|i| {
        let lo = 20.0 * (top / 20.0).powf(i as f32 / 32.0);
        let hi = 20.0 * (top / 20.0).powf((i + 1) as f32 / 32.0);
        let a = ((lo * SIZE as f32 / state.analyzer.rate) as usize).clamp(1, SIZE / 2);
        let b = ((hi * SIZE as f32 / state.analyzer.rate) as usize).clamp(a, SIZE / 2);
        ((state.input[a..=b].iter().copied().fold(-100.0, f32::max) + 72.0) / 66.0).clamp(0.0, 1.0)
    })
}
fn rotary(
    c: &Canvas,
    centre: (f32, f32),
    radius: f32,
    position: f32,
    main: bool,
    levels: &[f32; 32],
    enabled: bool,
) {
    let angle = (-0.75 + position * 1.5) * PI;
    let dir = vec2(angle.sin(), -angle.cos());
    let tangent = vec2(angle.cos(), angle.sin());
    let centre_pos = vec2(centre.0, centre.1);
    let rim_l = Color32::from_rgb(100, 185, 197);
    let rim_r = Color32::from_rgb(237, 34, 211);
    if main {
        let tip = centre_pos + dir * radius * 1.2;
        let left = centre_pos + dir * radius * 0.7 - tangent * radius * 0.7;
        let right = centre_pos + dir * radius * 0.7 + tangent * radius * 0.7;
        let vertices = [left, tip, right];
        c.painter.add(Shape::convex_polygon(
            vertices.iter().map(|p| c.p(p.x + 4.0, p.y + 7.0)).collect(),
            Color32::from_black_alpha(65),
            Stroke::NONE,
        ));
        c.painter.add(Shape::convex_polygon(
            vertices.iter().map(|p| c.p(p.x, p.y)).collect(),
            mix(rim_l, rim_r, (angle.sin() + 1.0) * 0.5),
            Stroke::NONE,
        ));
        let a = centre_pos + dir * radius * 0.96;
        let b = centre_pos + dir * radius * 1.175;
        c.line(
            (a.x, a.y),
            (b.x, b.y),
            radius * 0.03,
            Color32::from_rgb(39, 16, 46),
        );
    }
    c.painter.circle_filled(
        c.p(centre.0 + 4.0, centre.1 + 7.0),
        (radius + 6.0) * c.scale,
        Color32::from_black_alpha(45),
    );
    c.rim(centre, radius, if main { 5.0 } else { 3.0 }, rim_l, rim_r);
    c.disc(
        centre,
        radius,
        Color32::from_rgb(8, 19, 23),
        Color32::from_rgb(8, 19, 23),
    );
    let activity = levels.iter().copied().fold(0.0, f32::max);
    let core = mix(
        Color32::from_rgb(121, 123, 131),
        Color32::from_rgb(239, 35, 218),
        position,
    );
    let bright = 0.32 + activity * 0.68;
    let core_dim = Color32::from_rgb(
        (core.r() as f32 * bright) as u8,
        (core.g() as f32 * bright) as u8,
        (core.b() as f32 * bright) as u8,
    );
    c.disc(
        centre,
        radius - if main { 10.0 } else { 5.0 },
        if main {
            core_dim
        } else {
            Color32::from_rgb(35, 70, 87)
        },
        Color32::from_rgb(9, 13, 23),
    );
    if main {
        let points: Vec<_> = levels
            .iter()
            .enumerate()
            .map(|(i, level)| {
                let a = i as f32 * TAU / 32.0;
                let r = radius * (0.22 + 0.54 * level);
                (centre.0 + a.sin() * r, centre.1 + a.cos() * r)
            })
            .collect();
        let color =
            Color32::from_rgba_unmultiplied(core.r(), core.g(), core.b(), (activity * 140.0) as u8);
        for (i, &point) in points.iter().enumerate() {
            c.line(point, points[(i + 1) % 32], 1.1, color);
            c.line(point, points[(i + 4) % 32], 0.7, color);
            c.painter
                .circle_filled(c.p(point.0, point.1), 1.7 * c.scale, color);
        }
        c.disc(
            centre,
            radius * 0.14,
            Color32::from_rgb(41, 42, 46),
            Color32::from_rgb(9, 12, 17),
        );
        c.rim(
            centre,
            radius * 0.14,
            0.7,
            mix(core, Color32::BLACK, 0.7),
            mix(core, Color32::BLACK, 0.7),
        );
    } else {
        let points: Vec<_> = (0..=64)
            .map(|i| {
                let a = -0.75 * PI + (angle + 0.75 * PI) * i as f32 / 64.0;
                c.p(centre.0 + a.sin() * radius, centre.1 - a.cos() * radius)
            })
            .collect();
        c.painter.add(Shape::line(
            points,
            Stroke::new(
                4.0 * c.scale,
                if enabled {
                    Color32::from_rgb(201, 132, 230)
                } else {
                    Color32::from_gray(80)
                },
            ),
        ));
        c.painter.circle_filled(
            c.p(centre.0 + dir.x * radius, centre.1 + dir.y * radius),
            5.0 * c.scale,
            if enabled {
                Color32::from_rgb(198, 255, 243)
            } else {
                Color32::from_gray(110)
            },
        );
    }
}
fn value_box(
    ui: &mut egui::Ui,
    c: &Canvas,
    id: &str,
    r: Rect,
    param: &FloatParam,
    setter: &ParamSetter,
    range: std::ops::RangeInclusive<f32>,
    suffix: &str,
    enabled: bool,
) {
    c.painter
        .rect_filled(r, 0.0, preferences::current().palette().control);
    let mut value = param.value();
    ui.push_id(id, |ui| {
        ui.add_enabled_ui(enabled, |ui| {
            let response = ui.put(
                r,
                egui::DragValue::new(&mut value)
                    .range(range)
                    .speed(0.1)
                    .fixed_decimals(if suffix == " %" { 1 } else { 2 })
                    .suffix(suffix),
            );
            if response.changed() {
                set(setter, param, value);
            }
        });
    });
    c.painter.rect_stroke(
        r,
        0.0,
        Stroke::new(c.scale, CYAN),
        egui::StrokeKind::Outside,
    );
}
fn gain(ui: &mut egui::Ui, c: &Canvas, id: &str, x: f32, param: &FloatParam, setter: &ParamSetter) {
    let track = c.rect(x + 9.0, 499.0, 66.0, 5.0);
    c.painter
        .rect_filled(track, 3.0 * c.scale, Color32::from_rgb(38, 60, 76));
    parameter_interaction(
        ui,
        c,
        id,
        c.rect(x + 4.0, 492.0, 76.0, 18.0),
        param,
        setter,
        true,
        false,
    );
    c.painter.circle_filled(
        pos2(
            track.left() + track.width() * param.unmodulated_normalized_value(),
            track.center().y,
        ),
        6.0 * c.scale,
        Color32::from_rgb(66, 166, 201),
    );
    value_box(
        ui,
        c,
        &format!("{id}-value"),
        c.rect(x, 512.0, 80.0, 22.0),
        param,
        setter,
        -24.0..=24.0,
        " dB",
        true,
    );
}
fn meter(c: &Canvas, x: f32, label: &str, db: f32) {
    c.text(
        x,
        508.0,
        format!("{label}  {db:.1} dBFS"),
        11.0,
        Color32::from_rgb(193, 209, 221),
        Align2::LEFT_CENTER,
    );
    let track = c.rect(x, 520.0, 187.0, 9.0);
    c.painter
        .rect_filled(track, 3.0 * c.scale, Color32::from_rgb(38, 60, 76));
    let fraction = ((db + 60.0) / 60.0).clamp(0.0, 1.0);
    if fraction > 0.0 {
        c.painter.rect_filled(
            Rect::from_min_size(track.min, vec2(track.width() * fraction, track.height())),
            3.0 * c.scale,
            if db >= 0.0 {
                Color32::from_rgb(255, 101, 130)
            } else {
                Color32::from_rgb(83, 221, 201)
            },
        );
    }
}
fn spectrum(c: &Canvas, state: &UiState) {
    c.text(
        860.0,
        191.0,
        "LIVE SPECTRUM",
        14.0,
        WHITE,
        Align2::CENTER_CENTER,
    );
    c.painter.circle_filled(
        c.p(863.0, 330.0),
        114.0 * c.scale,
        Color32::from_black_alpha(50),
    );
    c.rim(
        (860.0, 324.0),
        106.0,
        3.0,
        Color32::from_rgb(179, 177, 223),
        Color32::from_rgb(180, 63, 189),
    );
    c.disc(
        (860.0, 324.0),
        106.0,
        Color32::from_rgb(10, 16, 24),
        Color32::from_rgb(10, 16, 24),
    );
    let plot = c.rect(790.0, 258.0, 140.0, 132.0);
    let top = 20000.0_f32.min(state.analyzer.rate * 0.5);
    let x_for = |f: f32| plot.left() + (f / 20.0).ln() / (top / 20.0).ln() * plot.width();
    for db in [-24.0, -48.0, -72.0, -96.0] {
        let y = plot.top() - db / 100.0 * plot.height();
        c.painter.line_segment(
            [pos2(plot.left(), y), pos2(plot.right(), y)],
            Stroke::new(c.scale, Color32::from_rgb(38, 55, 67)),
        );
    }
    for (f, label) in [(100.0, "100"), (1000.0, "1k"), (10000.0, "10k")] {
        let x = x_for(f);
        c.painter.line_segment(
            [pos2(x, plot.top()), pos2(x, plot.bottom())],
            Stroke::new(c.scale, Color32::from_rgb(38, 55, 67)),
        );
        c.painter.text(
            pos2(x, plot.bottom() + 12.0 * c.scale),
            Align2::CENTER_CENTER,
            label,
            FontId::proportional(10.0 * c.scale),
            c.preferences.palette().muted,
        );
    }
    for (path, values, color) in [
        (1, &state.input, PINK),
        (2, &state.output, Color32::from_rgb(164, 255, 245)),
    ] {
        if state.view != 0 && state.view != path {
            continue;
        }
        let points: Vec<_> = (0..140)
            .map(|px| {
                let lo = 20.0 * (top / 20.0).powf(px as f32 / 140.0);
                let hi = 20.0 * (top / 20.0).powf((px + 1) as f32 / 140.0);
                let a = ((lo * SIZE as f32 / state.analyzer.rate) as usize).clamp(1, SIZE / 2);
                let b = ((hi * SIZE as f32 / state.analyzer.rate) as usize).clamp(a, SIZE / 2);
                let db = values[a..=b].iter().copied().fold(-100.0, f32::max);
                pos2(
                    plot.left() + px as f32 * c.scale,
                    plot.top() + (-db / 100.0).clamp(0.0, 1.0) * plot.height(),
                )
            })
            .collect();
        c.painter
            .add(Shape::line(points, Stroke::new(1.6 * c.scale, color)));
    }
    for (x, label, color) in [
        (790.0, "IN", PINK),
        (853.0, "OUT", Color32::from_rgb(164, 255, 245)),
    ] {
        c.painter.circle_filled(c.p(x, 452.0), 2.5 * c.scale, color);
        c.text(x + 10.0, 452.0, label, 10.0, WHITE, Align2::LEFT_CENTER);
    }
}

pub(crate) fn create(
    params: Arc<VocalPureParams>,
    meters: Arc<Meters>,
    queue: Arc<ArrayQueue<Frame>>,
) -> Option<Box<dyn Editor>> {
    let window = params.editor_state.clone();
    create_egui_editor(
        params.editor_state.clone(),
        UiState::default(),
        |ctx, _| {
            let mut visuals = egui::Visuals::dark();
            visuals.panel_fill = preferences::current().palette().panel;
            visuals.widgets.inactive.bg_fill = preferences::current().palette().control;
            visuals.widgets.hovered.bg_fill = preferences::current().palette().hover;
            visuals.selection.bg_fill = Color32::from_rgb(33, 103, 117);
            visuals.override_text_color = Some(WHITE);
            ctx.set_visuals(visuals);
        },
        move |ctx, setter, state| {
            let mut prefs = *params
                .ui_preferences
                .lock()
                .unwrap_or_else(|p| p.into_inner());
            preferences::set_current(prefs);
            let mut visuals = if prefs.theme == Theme::Dark {
                egui::Visuals::dark()
            } else {
                egui::Visuals::light()
            };
            visuals.panel_fill = prefs.palette().bg;
            visuals.window_fill = prefs.palette().panel;
            visuals.override_text_color = Some(prefs.palette().text);
            visuals.widgets.inactive.bg_fill = prefs.palette().control;
            visuals.selection.bg_fill = prefs.palette().selected;
            ctx.set_visuals(visuals);
            let mut latest = None;
            for _ in 0..8 {
                if let Some(frame) = queue.pop() {
                    latest = Some(frame);
                } else {
                    break;
                }
            }
            if let Some(frame) = latest {
                state.analyzer.analyze(&frame);
                state.last_frame = Instant::now();
                for (value, next) in state.input.iter_mut().zip(&state.analyzer.input) {
                    *value += 0.4 * (next - *value);
                }
                for (value, next) in state.output.iter_mut().zip(&state.analyzer.output) {
                    *value += 0.4 * (next - *value);
                }
            }
            let stale = state.last_frame.elapsed() > Duration::from_millis(500);
            if stale {
                state.input.fill(-100.0);
                state.output.fill(-100.0);
            }
            let peak_in = if stale {
                0.0
            } else {
                meters.input.load(Ordering::Relaxed)
            };
            let peak_out = if stale {
                0.0
            } else {
                meters.output.load(Ordering::Relaxed)
            };
            for (db, peak) in state.meter_db.iter_mut().zip([peak_in, peak_out]) {
                *db = (20.0 * peak.max(1e-5).log10()).max(*db - 1.5);
            }
            let p = params.values();
            let dry = p.bypass || p.compare;
            let stereo = meters.channels.load(Ordering::Relaxed) == 2;
            ResizableWindow::new("vocalpure-original")
                .min_size(vec2(800.0, 448.0))
                .show(ctx, window.as_ref(), |ui| {
                    let area = ui.max_rect();
                    let scale = (area.width() / 1000.0).min(area.height() / 560.0);
                    let origin = area.center() - vec2(500.0, 280.0) * scale;
                    let c = Canvas {
                        preferences: prefs,
                        origin,
                        scale,
                        painter: ui.painter().with_clip_rect(Rect::from_min_size(
                            origin,
                            vec2(1000.0, 560.0) * scale,
                        )),
                    };
                    background(&c);
                    if button(
                        ui,
                        &c,
                        "undo",
                        c.rect(16.0, 7.0, 82.0, 28.0),
                        "UNDO",
                        state.undo.is_some(),
                    )
                    .clicked()
                    {
                        if let Some(v) = state.undo.take() {
                            apply(&params, setter, v);
                            state.selected = None;
                        }
                    }
                    let selected =
                        state
                            .selected
                            .map(|i| &PRESETS[i])
                            .filter(|(_, reduction, profile)| {
                                (*reduction - p.reduction).abs() < 0.1
                                    && *profile == p.profile
                                    && (p.width - 100.0).abs() < 0.1
                                    && p.input_db.abs() < 0.01
                                    && p.output_db.abs() < 0.01
                            });
                    if selected.is_none() {
                        state.selected = None;
                    }
                    let names: Vec<_> = PRESETS.iter().map(|v| v.0).collect();
                    if let Some(i) = menu(
                        ui,
                        &c,
                        "presets",
                        c.rect(112.0, 7.0, 330.0, 28.0),
                        selected.map_or("CUSTOM / SELECT PRESET", |v| v.0),
                        &names,
                        false,
                    ) {
                        state.undo = Some(p);
                        state.selected = Some(i);
                        apply(
                            &params,
                            setter,
                            AudioParameters {
                                reduction: PRESETS[i].1,
                                profile: PRESETS[i].2,
                                ..Default::default()
                            },
                        );
                    }
                    toggle(
                        ui,
                        &c,
                        "compare",
                        473.0,
                        225.0,
                        "COMPARE ORIGINAL",
                        &params.compare,
                        setter,
                    );
                    if button(
                        ui,
                        &c,
                        "info",
                        c.rect(732.0, 7.0, 74.0, 28.0),
                        "SETTINGS",
                        true,
                    )
                    .clicked()
                    {
                        state.about = true;
                    }
                    toggle(
                        ui,
                        &c,
                        "bypass",
                        849.0,
                        140.0,
                        "BYPASS",
                        &params.bypass,
                        setter,
                    );
                    let reset = button(ui, &c, "reset", c.rect(131.0, 155.0, 52.0, 46.0), "", true);
                    let points: Vec<_> = (0..=48)
                        .map(|i| {
                            let a = -0.65 + i as f32 / 48.0 * 5.35;
                            c.p(157.0 + a.sin() * 13.0, 178.0 - a.cos() * 13.0)
                        })
                        .collect();
                    c.painter.add(Shape::line(
                        points,
                        Stroke::new(2.6 * scale, prefs.palette().text),
                    ));
                    c.painter.add(Shape::convex_polygon(
                        vec![c.p(145.0, 162.0), c.p(144.0, 171.0), c.p(153.0, 170.0)],
                        prefs.palette().text,
                        Stroke::NONE,
                    ));
                    if reset.clicked() {
                        state.undo = Some(p);
                        state.selected = None;
                        apply(&params, setter, AudioParameters::default());
                    }
                    c.text(
                        157.5,
                        230.0,
                        "RNNOISE PROFILE",
                        17.0,
                        WHITE,
                        Align2::CENTER_CENTER,
                    );
                    if let Some(profile) = menu(
                        ui,
                        &c,
                        "profile",
                        c.rect(40.0, 252.0, 235.0, 57.0),
                        ["General", "Strong", "Eco"][p.profile.clamp(0, 2) as usize],
                        &["General", "Strong", "Eco"],
                        true,
                    ) {
                        set(setter, &params.profile, profile as i32);
                    }
                    c.text(112.0, 343.0, "ANALYSIS", 15.0, WHITE, Align2::CENTER_CENTER);
                    c.painter
                        .rect_filled(c.rect(40.0, 366.0, 144.0, 52.0), 25.0 * scale, CYAN);
                    c.painter.rect_filled(
                        c.rect(44.0, 370.0, 136.0, 44.0),
                        22.0 * scale,
                        preferences::current().palette().control,
                    );
                    c.text(
                        112.0,
                        392.0,
                        if stereo { "DUAL" } else { "MONO" },
                        20.0,
                        WHITE,
                        Align2::CENTER_CENTER,
                    );
                    c.text(243.0, 343.0, "WIDTH", 15.0, WHITE, Align2::CENTER_CENTER);
                    parameter_interaction(
                        ui,
                        &c,
                        "width",
                        c.rect(208.0, 360.0, 70.0, 72.0),
                        &params.width,
                        setter,
                        stereo && !dry,
                        true,
                    );
                    rotary(
                        &c,
                        (243.0, 394.0),
                        29.0,
                        p.width / 200.0,
                        false,
                        &[0.0; 32],
                        stereo && !dry,
                    );
                    value_box(
                        ui,
                        &c,
                        "width-value",
                        c.rect(203.0, 437.0, 80.0, 22.0),
                        &params.width,
                        setter,
                        0.0..=200.0,
                        " %",
                        stereo && !dry,
                    );
                    c.text(
                        243.0,
                        471.0,
                        if !stereo {
                            "MONO: NO EFFECT"
                        } else if dry {
                            "ORIGINAL: INACTIVE"
                        } else {
                            "STEREO WIDTH"
                        },
                        10.0,
                        WHITE,
                        Align2::CENTER_CENTER,
                    );
                    c.text(
                        524.0,
                        122.0,
                        "NOISE REDUCTION",
                        15.0,
                        WHITE,
                        Align2::CENTER_CENTER,
                    );
                    parameter_interaction(
                        ui,
                        &c,
                        "reduction",
                        c.rect(353.0, 142.0, 342.0, 310.0),
                        &params.reduction,
                        setter,
                        true,
                        true,
                    );
                    rotary(
                        &c,
                        (524.0, 296.0),
                        141.0,
                        p.reduction / 100.0,
                        true,
                        &radial_levels(state),
                        true,
                    );
                    c.text(
                        524.0,
                        479.0,
                        format!(
                            "VOICE PROBABILITY  {:.0}%",
                            if stale {
                                0.0
                            } else {
                                meters.voice.load(Ordering::Relaxed) * 100.0
                            }
                        ),
                        13.0,
                        WHITE,
                        Align2::CENTER_CENTER,
                    );
                    spectrum(&c, state);
                    if let Some(view) = menu(
                        ui,
                        &c,
                        "spectrum-view",
                        c.rect(780.0, 459.0, 165.0, 26.0),
                        ["COMPARE", "INPUT", "OUTPUT"][state.view],
                        &["COMPARE", "INPUT", "OUTPUT"],
                        false,
                    ) {
                        state.view = view;
                    }
                    c.painter.rect_filled(
                        c.rect(20.0, 489.0, 960.0, 51.0),
                        12.0 * scale,
                        preferences::current().palette().panel,
                    );
                    meter(&c, 37.0, "INPUT", state.meter_db[0]);
                    meter(&c, 365.0, "OUTPUT", state.meter_db[1]);
                    gain(ui, &c, "input-gain", 245.0, &params.input, setter);
                    gain(ui, &c, "output-gain", 573.0, &params.output, setter);
                    c.text(
                        825.0,
                        515.0,
                        if dry {
                            "LISTENING TO ORIGINAL".into()
                        } else if peak_in < 1e-7 {
                            "NO SIGNAL".into()
                        } else {
                            format!(
                                "LEVEL CHANGE  {:.1} dB",
                                meters.attenuation.load(Ordering::Relaxed)
                            )
                        },
                        13.0,
                        Color32::from_rgb(168, 190, 204),
                        Align2::CENTER_CENTER,
                    );
                    let status = match meters.status.load(Ordering::Relaxed) {
                        1 => "Unsupported sample rate".into(),
                        2 => "Neural engine failed: aligned dry audio".into(),
                        3 => "Invalid input samples removed".into(),
                        _ => format!(
                            "RNNoise ready  /  {} samples latency  /  {}",
                            meters.latency.load(Ordering::Relaxed),
                            if peak_in < 1e-7 {
                                "No audio input"
                            } else {
                                "Audio active"
                            }
                        ),
                    };
                    c.text(
                        30.0,
                        550.0,
                        status,
                        12.0,
                        Color32::from_rgb(168, 220, 230),
                        Align2::LEFT_CENTER,
                    );
                });
            egui::Window::new(prefs.text("Settings")).id(Id::new("vocalpure-settings"))
                .open(&mut state.about).default_width(480.0).show(ctx,|ui| {
                    if preferences::controls(ui,&mut prefs) {
                        *params.ui_preferences.lock().unwrap_or_else(|p|p.into_inner())=prefs;
                    }
                    ui.separator();
                    ui.label(format!("VocalPure {} / AGPL-3.0-only",env!("CARGO_PKG_VERSION")));
                    for text in [
                        "Local RNNoise 0.1.1 processing. General, Strong and Eco use the same embedded model.",
                        "44.1, 48 and 96 kHz. Latency: 20 ms at 48 kHz; 26.67 ms otherwise.",
                        "Drag the reduction dial to adjust. Shift: fine adjustment. Double-click: 65%.",
                        "Compare and Bypass play the aligned original. Width affects stereo only.",
                        "Level change measures the whole signal, not isolated noise. VAD can give false positives.",
                        "This plugin has a separate identity. Existing JUCE sessions are not imported.",
                    ] {ui.label(prefs.text(text));}
                });
            ctx.request_repaint_after(Duration::from_millis(33));
        },
    )
}
