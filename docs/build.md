# Compilacao

Windows x64: Visual Studio 2022, workload Desktop development with C++, Windows SDK, Git e CMake >=3.22.
Obter JUCE: `git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git external/JUCE`.
Versao fixada: commit `51a8a6d7aeae7326956d747737ccf1575e61e209`.
RNNoise: `git clone --depth 1 --branch v0.1.1 https://github.com/xiph/rnnoise.git external/rnnoise`.
Commit fixado `6cbfd53eb348a8d394e0757b4025c6ded34eb2b6`; pesos incorporados, sem download no runtime.
Executar `./scripts/build_windows.ps1`. Alternativa com CMake no PATH:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Artefatos: `build/VocalPure_artefacts/Release/VST3/VocalPure.vst3` (pasta inteira) e
`build/VocalPure_artefacts/Release/Standalone/VocalPure.exe`.
Para instalar, copiar manualmente a pasta .vst3 para `C:/Program Files/Common Files/VST3` e
solicitar nova varredura no DAW. Isso pode exigir permissao de administrador.
No standalone, abrir Audio/MIDI Settings, selecionar dispositivo, canais de entrada e saida.
O wrapper JUCE pode iniciar com entrada silenciada para evitar feedback; usar fones e habilitar entrada.
O processamento e offline/local; rede e usada apenas para obter dependencias de desenvolvimento.

macOS: Xcode/SDK e CMake; `bash scripts/build_macos.sh` configura universal Intel/Apple Silicon,
VST3/AU/Standalone. Essa plataforma ainda nao foi testada. Assinatura/notarizacao nao configuradas.
`./scripts/package_windows.ps1` gera um pacote interno Windows x64 com VST3 completo,
Standalone, assistente de instalacao/backup/restauracao, instrucoes e hashes apos a regressao.
O instalador requer permissao de administrador; nao e executado pelo script de build/pacote.
MSVC usa runtime estatico para evitar dependencia do Visual C++ Redistributable.
Detalhes e pendencias em `phase6.md`. ONNX Runtime/Python nao sao dependencias do plugin.
