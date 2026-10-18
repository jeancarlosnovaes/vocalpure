# Dependencias

- JUCE 8.0.6: licenciamento dual AGPLv3/comercial. Texto exato em external/JUCE/LICENSE.md;
  https://github.com/juce-framework/JUCE/blob/8.0.6/LICENSE.md. Distribuicao deve atender a uma modalidade.
- VST3 SDK incorporado ao JUCE: consultar licencas na copia exata de
  external/JUCE/modules/juce_audio_processors/format_types/VST3_SDK.
- CMake portatil (ferramenta de build, nao runtime): https://cmake.org/licensing/.
- RNNoise v0.1.1 integrado, incluindo pesos originais em src/rnn_data.c sob o COPYING do repositorio.
  Commit 6cbfd53eb348a8d394e0757b4025c6ded34eb2b6; BSD-3-Clause no COPYING e avisos BSD por arquivo.
  https://github.com/xiph/rnnoise/tree/v0.1.1. Preservar COPYING e avisos do fonte ao distribuir.
  CMake gera copias com checks de alocacao e workspaces fixos para MSVC; pesos nao modificados.
- ONNX Runtime ainda nao integrado: https://github.com/microsoft/onnxruntime.
- Nenhum dataset de treinamento ou recurso comercial foi incorporado. Gravacoes do usuario em tests/output
  sao locais, excluidas do versionamento e nao devem acompanhar distribuicoes.

Esta lista nao concede licenca sobre dependencias. Preservar seus avisos ao distribuir.
