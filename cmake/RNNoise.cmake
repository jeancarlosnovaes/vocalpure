set(RN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/rnnoise")
if(NOT EXISTS "${RN_DIR}/src/rnn_data.c")
  message(FATAL_ERROR "Clone xiph/rnnoise v0.1.1 into external/rnnoise")
endif()
# Build a safety-patched copy: upstream v0.1.1 misses allocation checks.
file(READ "${RN_DIR}/src/denoise.c" rn_source)
string(REPLACE "  return 0;\n}\n\nDenoiseState *rnnoise_create"
  "  if (!st->rnn.vad_gru_state || !st->rnn.noise_gru_state || !st->rnn.denoise_gru_state) return -1;\n  return 0;\n}\n\nDenoiseState *rnnoise_create" rn_source "${rn_source}")
string(REPLACE "  rnnoise_init(st, model);"
  "  if (!st) return NULL;\n  if (rnnoise_init(st, model) != 0) { rnnoise_destroy(st); return NULL; }" rn_source "${rn_source}")
# Local extensions to the pinned backend: real gain shaping and reduced neural
# cadence. Original API remains the General path; FFT/pitch/synthesis run every frame.
function(rn_patch before after)
  string(FIND "${rn_source}" "${before}" offset)
  if(offset EQUAL -1)
    message(FATAL_ERROR "Pinned RNNoise source changed: profile patch anchor missing")
  endif()
  string(REPLACE "${before}" "${after}" patched "${rn_source}")
  set(rn_source "${patched}" PARENT_SCOPE)
endfunction()
rn_patch("  RNNState rnn;" "  RNNState rnn;\n  float vp_g[NB_BANDS], vp_vad;\n  int vp_cache_valid, vp_skip;\n  unsigned long vp_inferences;")
rn_patch("float rnnoise_process_frame(DenoiseState *st, float *out, const float *in) {"
  "float vocalpure_rnnoise_process_frame(DenoiseState *st, float *out, const float *in, int eco, float strong) {")
rn_patch("    compute_rnn(&st->rnn, g, &vad_prob, features);"
  "    if (!eco || !st->vp_cache_valid || !st->vp_skip) {\n      compute_rnn(&st->rnn, g, &vad_prob, features);\n      RNN_COPY(st->vp_g, g, NB_BANDS); st->vp_vad = vad_prob;\n      st->vp_cache_valid = 1; st->vp_skip = eco ? 1 : 0; st->vp_inferences++;\n    } else {\n      RNN_COPY(g, st->vp_g, NB_BANDS); vad_prob = st->vp_vad; st->vp_skip = 0;\n    }")
rn_patch("  silence = compute_frame_features(st, X, P, Ex, Ep, Exp, features, x);"
  "  silence = compute_frame_features(st, X, P, Ex, Ep, Exp, features, x);\n  if (silence) { st->vp_cache_valid = 0; st->vp_skip = 0; }")
rn_patch("    pitch_filter(X, P, Ex, Ep, Exp, g);"
  "    pitch_filter(X, P, Ex, Ep, Exp, g);\n    if (strong > 0) for (i=0;i<NB_BANDS;i++) g[i] = powf(MAX16(0, g[i]), 1.f + .5f*strong);")
string(APPEND rn_source "\nfloat rnnoise_process_frame(DenoiseState *st, float *out, const float *in) { return vocalpure_rnnoise_process_frame(st, out, in, 0, 0); }\nunsigned long vocalpure_rnnoise_inferences(const DenoiseState *st) { return st->vp_inferences; }\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-denoise.c" "${rn_source}")
# MSVC does not support C variable-length arrays. RNNoise's fixed frame/pitch
# dimensions are <=1728; use bounded stack workspaces, not heap allocations.
file(READ "${RN_DIR}/src/pitch.c" rn_pitch)
foreach(declaration "x_lp4[len>>2]" "y_lp4[lag>>2]" "xcorr[max_pitch>>1]" "yy_lookup[maxperiod+1]")
  string(REGEX REPLACE "\\[.*" "" array_name "${declaration}")
  string(REPLACE "${declaration}" "${array_name}[2048]" rn_pitch "${rn_pitch}")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-pitch.c" "${rn_pitch}")
file(READ "${RN_DIR}/src/celt_lpc.c" rn_lpc)
string(REPLACE "xx[n]" "xx[2048]" rn_lpc "${rn_lpc}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-lpc.c" "${rn_lpc}")
add_library(vocalpure_rnnoise STATIC "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-denoise.c"
  "${RN_DIR}/src/rnn.c" "${RN_DIR}/src/rnn_data.c" "${RN_DIR}/src/rnn_reader.c"
  "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-pitch.c" "${RN_DIR}/src/kiss_fft.c" "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-lpc.c")
set_target_properties(vocalpure_rnnoise PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(vocalpure_rnnoise PUBLIC "${RN_DIR}/include" PRIVATE "${RN_DIR}/src")
target_compile_definitions(vocalpure_rnnoise PRIVATE _USE_MATH_DEFINES)
if(MSVC)
  target_compile_options(vocalpure_rnnoise PRIVATE /O2 /W0)
else()
  target_link_libraries(vocalpure_rnnoise PRIVATE m)
endif()
