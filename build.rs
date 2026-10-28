use std::{env, fs, path::PathBuf};

fn replace(source: &mut String, before: &str, after: &str) {
    assert!(
        source.contains(before),
        "RNNoise patch anchor missing: {before}"
    );
    *source = source.replace(before, after);
}

fn main() {
    println!("cargo:rerun-if-changed=assets/vocalpure.ico");
    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        winresource::WindowsResource::new()
            .set_icon("assets/vocalpure.ico")
            .set("ProductName", "VocalPure")
            .set("CompanyName", "VocalPure")
            .set("FileDescription", "VocalPure - audio neural local")
            .compile()
            .expect("Unable to compile Windows resources");
    }
    let root = PathBuf::from("vendor/rnnoise");
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    println!("cargo:rerun-if-changed=vendor/rnnoise");
    println!("cargo:rerun-if-changed=build.rs");
    let mut denoise = fs::read_to_string(root.join("src/denoise.c"))
        .unwrap()
        .replace("\r\n", "\n");
    replace(&mut denoise, "  return 0;\n}\n\nDenoiseState *rnnoise_create",
        "  if (!st->rnn.vad_gru_state || !st->rnn.noise_gru_state || !st->rnn.denoise_gru_state) return -1;\n  return 0;\n}\n\nDenoiseState *rnnoise_create");
    replace(&mut denoise, "  rnnoise_init(st, model);",
        "  if (!st) return NULL;\n  if (rnnoise_init(st, model) != 0) { rnnoise_destroy(st); return NULL; }");
    replace(&mut denoise, "  RNNState rnn;", "  RNNState rnn;\n  float vp_g[NB_BANDS], vp_vad;\n  int vp_cache_valid, vp_skip;\n  unsigned long vp_inferences;");
    replace(&mut denoise, "float rnnoise_process_frame(DenoiseState *st, float *out, const float *in) {",
        "float vocalpure_rnnoise_process_frame(DenoiseState *st, float *out, const float *in, int eco, float strong) {");
    replace(&mut denoise, "    compute_rnn(&st->rnn, g, &vad_prob, features);",
        "    if (!eco || !st->vp_cache_valid || !st->vp_skip) {\n      compute_rnn(&st->rnn, g, &vad_prob, features);\n      RNN_COPY(st->vp_g, g, NB_BANDS); st->vp_vad = vad_prob;\n      st->vp_cache_valid = 1; st->vp_skip = eco ? 1 : 0; st->vp_inferences++;\n    } else {\n      RNN_COPY(g, st->vp_g, NB_BANDS); vad_prob = st->vp_vad; st->vp_skip = 0;\n    }");
    replace(&mut denoise, "  silence = compute_frame_features(st, X, P, Ex, Ep, Exp, features, x);",
        "  silence = compute_frame_features(st, X, P, Ex, Ep, Exp, features, x);\n  if (silence) { st->vp_cache_valid = 0; st->vp_skip = 0; }");
    replace(&mut denoise, "    pitch_filter(X, P, Ex, Ep, Exp, g);",
        "    pitch_filter(X, P, Ex, Ep, Exp, g);\n    if (strong > 0) for (i=0;i<NB_BANDS;i++) g[i] = powf(MAX16(0, g[i]), 1.f + .5f*strong);");
    denoise.push_str("\nfloat rnnoise_process_frame(DenoiseState *st, float *out, const float *in) { return vocalpure_rnnoise_process_frame(st, out, in, 0, 0); }\nunsigned long vocalpure_rnnoise_inferences(const DenoiseState *st) { return st->vp_inferences; }\n");
    denoise.push_str("\nvoid vocalpure_rnnoise_reset(DenoiseState *st) {\n  RNNState saved = st->rnn;\n  memset(st, 0, sizeof(*st));\n  st->rnn = saved;\n  memset(saved.vad_gru_state, 0, sizeof(float)*saved.model->vad_gru_size);\n  memset(saved.noise_gru_state, 0, sizeof(float)*saved.model->noise_gru_size);\n  memset(saved.denoise_gru_state, 0, sizeof(float)*saved.model->denoise_gru_size);\n}\n");
    fs::write(out.join("denoise.c"), denoise).unwrap();
    let mut pitch = fs::read_to_string(root.join("src/pitch.c")).unwrap();
    for (before, after) in [
        ("x_lp4[len>>2]", "x_lp4[2048]"),
        ("y_lp4[lag>>2]", "y_lp4[2048]"),
        ("xcorr[max_pitch>>1]", "xcorr[2048]"),
        ("yy_lookup[maxperiod+1]", "yy_lookup[2048]"),
    ] {
        replace(&mut pitch, before, after);
    }
    fs::write(out.join("pitch.c"), pitch).unwrap();
    let mut lpc = fs::read_to_string(root.join("src/celt_lpc.c")).unwrap();
    replace(&mut lpc, "xx[n]", "xx[2048]");
    fs::write(out.join("celt_lpc.c"), lpc).unwrap();
    let mut build = cc::Build::new();
    build
        .include(root.join("include"))
        .include(root.join("src"))
        .define("_USE_MATH_DEFINES", None)
        .opt_level(2)
        .warnings(false)
        .file(out.join("denoise.c"))
        .file(out.join("pitch.c"))
        .file(out.join("celt_lpc.c"));
    for file in ["rnn.c", "rnn_data.c", "rnn_reader.c", "kiss_fft.c"] {
        build.file(root.join("src").join(file));
    }
    build.compile("vocalpure_rnnoise");
    if env::var("CARGO_CFG_TARGET_FAMILY").as_deref() == Ok("unix") {
        println!("cargo:rustc-link-lib=m");
    }
}
