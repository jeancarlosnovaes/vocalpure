# VocalPure 0.4.3 — Fase 6 em validacao

Codigo original licenciado sob GNU AGPLv3 (AGPL-3.0-only), conforme LICENSE.
Pre-release Windows sem assinatura digital; homologacao do assistente em Windows limpo pendente.
Categoria VST3: `Fx|Restoration`. Reinstale e refaca a varredura de plugins no host para atualizar a categoria.

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
- [Validacao basica no DaVinci Resolve](docs/validation-resolve.md)
- [Fase 5A: avaliacao ONNX e comparacao local](docs/phase5.md)
- [Fase 6: candidato Windows, Width e distribuicao](docs/phase6.md)

Width ajusta a diferenca entre os canais estereo: 0% mono, 100% abertura original,
200% dobro do sinal lateral. Nao altera entrada mono nem dois canais identicos.
A interface informa quando o controle esta inativo por mono ou Compare/Bypass.
Pacote interno Windows: `./scripts/package_windows.ps1`; arquivos em `dist/`,
com manifesto, verificacao SHA256 e assistente `Instalar VocalPure.exe`.
O assistente instala/atualiza o VST3 com backup e permite restaurar a versao anterior.
Sem publicacao de release. Plugin, Standalone e instalador usam runtime C++ estatico.

Carregamento, reproducao mono, Compare/Bypass e restauracao de ajustes verificados no Resolve 21.
DPDFNet ONNX de 48 kHz avaliado separadamente, com WAVs e medicoes locais; ainda nao integrado ao VST3.
Ainda pendentes: exportacao e validacao completa em DAW, Single Analysis, integracao ONNX e distribuicao final.
Nao ha controles ficticios de perfis neurais. Documentos phase1 registram a etapa historica anterior.
