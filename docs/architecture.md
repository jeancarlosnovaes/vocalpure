# Arquitetura e plano

**Estado atual: Fase 4 no motor; Fase 5A avaliada; Fase 6 com candidato RNNoise 0.4.2 e instalador interno.**
Veja `phase4.md` para o motor e `phase5.md` para contrato, resultados e limites do
modelo candidato. `phase6.md` registra o candidato interno e pendencias de distribuicao.
ONNX ainda fora do VST3. O restante registra o plano original.

## Decisoes
C++20, JUCE 8.0.6 (commit 51a8a6d7aeae7326956d747737ccf1575e61e209) e CMake >=3.22.
JUCE fornece wrappers VST3/Standalone/AU, APVTS e componentes acessiveis. O motor e separado do host.
Nao ha modelo neural na fase 1. RNNoise e a proposta para fase 2: backend real de fala,
48 kHz, frames de 480 amostras; nao e separador garantido de vozes nem modelo especializado em canto.
ONNX Runtime sera integrado na fase 5, somente com contrato e pesos validados.

## Pipeline
Fase 1: host -> ganho de entrada suavizado -> ganho de saida suavizado -> crossfade com original -> host.
Medidores de pico observam entrada original e saida real. Latencia algoritmica: zero amostras.
Futuro: host -> ganho -> reamostragem 48 kHz -> frames -> RNNoise -> reconstrucao/reamostragem ->
mistura com dry atrasado -> width -> ganho -> bypass alinhado -> host.
Buffers e estados serao alocados em prepare, nunca em processBlock. Arquivos/modelos carregados fora do callback.
UI recebe escalares atomicos; espectro futuro usara FIFO SPSC prealocada e FFT fora do callback.

## Contratos e estrutura
- src/PluginProcessor: layouts mono/estereo simetricos, APVTS, persistencia XML versionada e host.
- src/audio/AudioEngine.h: prepare(sampleRate, parametros), process(buffer, parametros); sem IO ou locks.
- src/PluginEditor: interface de fundacao, attachments e timer de 30 Hz; nenhuma chamada da thread de audio para UI.
- tests: testes reais do processador, incluindo blocos e restauracao.
- docs e scripts: reproducao, limites e compilacao.
- Futuro IDenoiser: prepare/reset fora do callback; processFrame com spans prealocados,
  capacidades (taxa, frame, canais, latencia, estados) e resultado VAD/erro. Sem alocacao em processFrame.
- Futuro ONNX: documentar nomes, shapes, normalizacao, estados recorrentes, lookahead e reconstrucao
  por modelo. Nao existe adaptador universal seguro.

## Fases e criterios
1. Fundacao: compilar VST3/Standalone, testar transparencia, ganhos, bypass, estado e layouts; executar e validar host.
2. RNNoise: licencas de fonte e pesos, resampling, frames, dry alinhado, intensidade, medidas e audicao A/B.
3. UI: knob, espectro real, presets, VAD do modelo e HiDPI; nenhum controle decorativo.
4. Desempenho: medir picos/medio, transicoes e estereo; Eco com economia mensuravel, nao apenas nome.
5. ONNX: modelo streaming licenciado, validacao numerica Python/C++, benchmark e comparacao auditiva.
6. Distribuicao: regressao, hosts, instaladores e assinatura.

## Riscos e validacao
Meta de 20 ms ainda nao comprovada; incluir frame, lookahead e resampling ao informar host.
Latencia algoritmica difere da latencia de dispositivo/host e do tempo de computacao.
Single Analysis deve preservar side e evitar cancelamento de entradas em oposicao de fase; Dual exige avaliar imagem estereo.
Mudancas de modelo exigem preparacao externa e transicao segura. Nao habilitar perfis sem comportamento real.
Ganhos de +24 dB podem exceder 0 dBFS; nao aplicar limitador oculto.
Dependencias e modelos exigem revisao de licencas antes da distribuicao comercial.
Sem amostras/hosts testados, nao declarar qualidade perceptual nem compatibilidade validada.
