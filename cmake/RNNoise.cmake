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
