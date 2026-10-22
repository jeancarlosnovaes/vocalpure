# VocalPure 0.4.0 — Fase 4

Plugin C++20/JUCE com **RNNoise real e processamento local**, VST3/Standalone mono/estereo.
Reducao 0..100%, ganhos -24..+24 dB, transicoes suaves, bypass alinhado, VAD neural,
medidores reais, reset, estado de sessao e interface redimensionavel.
Interface com espectro real, oito presets, Compare, largura estereo e Undo de preset/reset.
Perfis reais do mesmo RNNoise: General (padrao), Strong (mascara mais profunda), Eco
(metade das chamadas neurais, com possiveis mudancas em ataques e naturalidade da voz).
Parametros: `inputGain`, `outputGain`, `bypass`, `reduction`, `width`, `compare`, `profile`.
Padrao: 0 dB, 0 dB, desligado, 65%, 100%, desligado, General.
0% conserva o original com ganhos e atraso; bypass conserva original atrasado sem ganhos.
Latencia: 20 ms em 48 kHz; 26.667 ms em 44.1/96 kHz. A qualidade depende da gravacao.

## Compilar
No PowerShell: `./scripts/build_windows.ps1` (Visual Studio 2022 com C++, SDK Windows e CMake).
Dependencias: JUCE 8.0.6 em `external/JUCE`, RNNoise v0.1.1 em `external/rnnoise`.
Veja [instrucoes](docs/build.md) e [contrato neural](docs/phase2.md).
O script compila e executa testes, sem instalar o plugin no sistema.

## Documentacao
- [Arquitetura, contratos e plano](docs/architecture.md)
- [Compilacao e instalacao](docs/build.md)
- [Testes e limites](docs/testing.md)
- [Dependencias/licencas](THIRD_PARTY_NOTICES.md)
- [Fase 2: pipeline, latencia e limites](docs/phase2.md)
- [Fase 3: interface, espectro e validacao](docs/phase3.md)
- [Fase 4: perfis, desempenho e limites](docs/phase4.md)
- [Medicoes locais de CPU](docs/benchmarks/phase4-results.md)

Ainda pendentes: validacao completa em DAW, Single Analysis, ONNX e distribuicao final.
Nao ha controles ficticios de perfis neurais. Documentos phase1 registram a etapa historica anterior.
