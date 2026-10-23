# Dependencias

Codigo original do VocalPure: GNU AGPLv3 (AGPL-3.0-only), autorizado pelo titular.
Nesta pre-release aberta, JUCE e usado sob AGPLv3 e os arquivos do VST3 SDK sob
a alternativa GPLv3 indicada em seus avisos, preservando licencas por arquivo.
Fontes correspondentes do projeto, JUCE e RNNoise acompanham os binarios na release.

- JUCE 8.0.6: licenciamento dual AGPLv3/comercial. Texto exato em external/JUCE/LICENSE.md;
  https://github.com/juce-framework/JUCE/blob/8.0.6/LICENSE.md. Distribuicao deve atender a uma modalidade.
- VST3 SDK incorporado ao JUCE: consultar licencas na copia exata de
  external/JUCE/modules/juce_audio_processors/format_types/VST3_SDK.
- CMake portatil (ferramenta de build, nao runtime): https://cmake.org/licensing/.
- Windows 0.4.2: runtime MSVC/C++ ligado estaticamente ao plugin, Standalone e
  instalador. Inspecao de imports nao encontrou DLLs VC++ redistribuiveis como
  dependencia; isso nao define os termos de distribuicao do projeto/dependencias.
- RNNoise v0.1.1 integrado, incluindo pesos originais em src/rnn_data.c sob o COPYING do repositorio.
  Commit 6cbfd53eb348a8d394e0757b4025c6ded34eb2b6; BSD-3-Clause no COPYING e avisos BSD por arquivo.
  https://github.com/xiph/rnnoise/tree/v0.1.1. Preservar COPYING e avisos do fonte ao distribuir.
  CMake gera copias com checks de alocacao, workspaces fixos para MSVC, cache de mascara
  a cada dois frames no Eco e profundidade ajustada no Strong; pesos nao modificados.
- Avaliacao offline opcional (nao incorporada ao VST3): DPDFNet Python 0.6.0 e pesos
  dpdfnet2_48khz_hr, Apache-2.0 conforme pacote/model card do autor.
  https://github.com/ceva-ip/DPDFNet e https://huggingface.co/Ceva-IP/DPDFNet.
  Revisao/hash/contrato em tools/onnx_model.json; nao ha redistribuicao dos pesos neste repositorio.
- ONNX Runtime CPU 1.31.0 (MIT) usado apenas pelo avaliador Python; ainda nao integrado
  ao plugin. https://github.com/microsoft/onnxruntime. Demais dependencias do ambiente
  de avaliacao listadas em tools/onnx-eval-requirements.txt; licencas/avisos nas respectivas
  pastas .dist-info do ambiente local. Preservar avisos caso sejam redistribuidas.
- Nenhum dataset de treinamento ou recurso comercial foi incorporado. Gravacoes do usuario em tests/output
  sao locais, excluidas do versionamento e nao devem acompanhar distribuicoes.

Esta lista nao concede licenca sobre dependencias. Preservar seus avisos ao distribuir.
