# Validacao Fase 2 — 18/10/2026

## Build e regressao
Windows x64, MSVC 19.44, JUCE 8.0.6, RNNoise v0.1.1, CMake 3.31.6.
Release VST3/Standalone compilado. CTest aprovado (3.71 s na execucao registrada).
54 grupos da matriz + testes de mistura 50%, migracao, ganhos, bypass suave, sanitizacao e estereo.
Blocos 7/32/64/128/137/256/512/1024, inclusive chamadas vazias. Saida independe do particionamento.
Nenhuma alocacao C++ detectada durante processBlock nos testes. Nao e um teste global de malloc do sistema.

## Latencia medida por impulso
| Taxa | Declarada | Pico medido mono/estereo |
|---|---:|---:|
| 44.1 kHz | 1176 | 1176 |
| 48 kHz | 960 | 960 |
| 96 kHz | 2560 | 2560 |

Bypass e 0% produzem copia exata atrasada com ganhos em zero. Conversores sinc tambem foram
medidos isoladamente e o teste de rejeicao de alias de 30 kHz (96 -> 48 kHz) passou.
Trata-se de alinhamento algoritmico, nao latencia de placa de audio ou DAW.

## Ruido controlado e limitacoes descobertas
Teste deterministico: ruido uniforme, 3 s, energia avaliada apos 1 s, intensidade 100%.
48 kHz: 9.85/10.02 dB de atenuacao (mono/estereo com seeds sequenciais).
44.1 kHz: 2.32/1.24 dB. 96 kHz: 4.42/4.63 dB, incluindo banda removida pela reamostragem.
O modelo antigo e sensivel ao espectro do ruido; VAD chegou a ~0.86 em ruido puro reamostrado.
Logo, VAD e uma probabilidade estimada sujeita a falso positivo, e nao prova de fala.
Um limiar universal de qualidade usado inicialmente no teste falhou. A suite registra as medidas,
testa reducao forte no caminho nativo e exige ausencia de aumento de energia nos outros casos;
isso nao torna a qualidade entre taxas equivalente. Avaliar modelos mais recentes em fase posterior.

## Gravacao fornecida pelo usuario
Arquivo original: Tipos_de_Virus.wav, mono PCM16, 44.1 kHz, 73.640.633 amostras,
1669.8556 segundos (~27 min 50 s). Original apenas lido, sem modificacao.
Exportada versao completa a 65%, PCM24, compensada e com a mesma quantidade de amostras.
Trechos de 90 s disponiveis em 65% e 100%, alem do original.

No trecho original de 90 s, houve pouca alteracao de nivel: 0.004 dB global em 65%,
correlacao 0.99435; em 100%, -0.034 dB de atenuacao (leve aumento), correlacao 0.98670.
Zero amostras saturadas em ambas as versoes. Esses numeros nao comprovam melhora de qualidade
ou ausencia de artefatos perceptivos; audicao A/B humana permanece necessaria.

Teste adicional separado: primeiros 30 s com ruido colorido artificial acrescentado a SNR 10 dB,
seed 2026. A 100%, janelas de menor energia tiveram atenuacao de 14.00 dB e janelas de maior
energia 0.393 dB; sem clipping. Essas janelas nao sao rotulos ground-truth de fala/ruido.
Nenhuma afirmacao de SI-SDR/STOI/PESQ foi feita sem referencia limpa apropriada.

## Desempenho observado
Renderizacao completa com blocos de 512: tempo medio por callback 0.209 ms,
maximo 2.973 ms, custo acumulado equivalente a 1.80% da duracao do audio nesta maquina.
Nao confundir com leitura de CPU do DAW, percentagem de todos os nucleos ou garantia de deadline.
Processamento offline sem falha na exportacao. Nao houve teste de underrun de hardware.
Buffers pequenos e varias instancias precisam de profiling em tempo real (Fase 4).

## Interface e pendencias
Standalone abriu com controle de reducao e status do RNNoise/latencia/VAD, preservando estado salvo.
Sessoes antigas abrem com reducao 0%; para testar limpeza, desativar bypass e aumentar reduction.
Microfone no standalone permanece sob controle do usuario e pode estar inicialmente silenciado.
Sem validacao dentro de REAPER/FL Studio/Ableton, sem macOS testado, sem garantia de qualidade de canto.
Os criterios finais do produto continuam pendentes; esta e a entrega do motor da Fase 2.
