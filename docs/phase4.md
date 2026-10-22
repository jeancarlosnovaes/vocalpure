# Fase 4 — desempenho e perfis RNNoise (0.4.0)

## Processamento

- General: configuracao anterior do RNNoise 0.1.1, com inferencia a cada frame nao silencioso de 10 ms.
- Strong: mesmo modelo e pesos; ganhos por banda elevados a potencia 1.5 antes da suavizacao
  original. A profundidade entra/sai em dois frames (20 ms). Pode atenuar componentes vocais.
- Eco: inferencia em frames alternados nao silenciosos (20 ms), reutilizando a ultima mascara
  e VAD. FFT, extracao de caracteristicas, pitch e overlap-add continuam em todos os frames
  de 10 ms. Silencio invalida o cache. Nao ha descarte de frames de audio nem nova latencia.
  As GRUs passam a receber uma sequencia temporal subamostrada: ataques de fala e qualidade
  podem mudar. Nao e um novo modelo treinado nem garantia de qualidade equivalente.

Perfis mudam em limites de frame, sem carregar modelos, alocar, reiniciar estado ou ler arquivos
no callback. A suavizacao original de mascara e o overlap-add permanecem ativos; a troca de
cadencia no Eco nao tem um segundo motor paralelo de crossfade. Sao configuracoes do mesmo
modelo, apresentadas explicitamente como perfis RNNoise. Single Analysis segue pendente.

## Otimizacoes e memoria

O FIR de 128 taps agora usa uma janela contigua de historico + entrada. Remove a escolha
entre dois buffers de cada multiplicacao, mantendo coeficientes, cutoff e atraso.
O teste compara com uma copia congelada do reamostrador anterior (tolerancia 2e-6).
Essa troca aumenta o workspace fixo em ate 15 KiB no estereo; nao se afirma reducao de memoria.
Ponteiros dos canais sao obtidos uma vez por bloco. Nao foi adicionado trabalho de FFT na
thread de audio. O cache neural e fixo e pequeno; os pesos continuam os mesmos.

## Parametros e interface

Novo parametro automatizavel `profile`: 0 General, 1 Strong, 2 Eco. IDs existentes preservados.
Schema 4: sessoes anteriores recebem General; valores de reducao e demais ajustes sao mantidos.
O seletor no painel esquerdo tem funcao real. O oitavo preset Low CPU seleciona Eco;
Strong Noise Reduction agora seleciona Strong. Reset e Undo de preset/reset incluem o perfil.
O resto do desenho aprovado permanece, incluindo miolo sem porcentagem e espectro real.

## Medicoes

Ver [resultados](benchmarks/phase4-results.md), CSVs e [metricas de audio](benchmarks/phase4-audio-metrics.json).
Windows x64 / Ryzen 7 5700X. Mediana do custo por duracao de audio, sem renderizacao da UI:
Eco economizou aproximadamente 13–27% frente a General nesta carga sintetica.
Nao e a porcentagem total de CPU mostrada pelo Gerenciador de Tarefas.
Alguns callbacks em blocos de 32 amostras excederam o orcamento; nao prometer operacao sem
glitches em buffers curtos. Tempo offline nao mede underruns de hardware.

Foram exportados localmente tres WAVs alinhados de 90 s a 65% e tres de 30 s a 100%
com ruido controlado. Nenhum apresentou clipping. Nos segmentos de menor energia da mistura
de 30 s, a queda de nivel foi 14.00 dB General, 15.38 dB Strong e 13.00 dB Eco.
Esses segmentos nao sao rotulos de ruido: isso nao e uma metrica de qualidade perceptual.
Nao ha referencia limpa, avaliacao auditiva conclusiva nem superioridade comprovada de um perfil.
A comparacao com um WAV antigo General mostrou diferenca maxima de 0.001205; nao se declara
identidade bit a bit com exportacoes historicas. Atraso e tolerancia do FIR foram verificados.

## Validacao automatizada

Regressao anterior + diferencas reais entre perfis, contagem de inferencias Eco (50/100),
blocos variaveis, igualdade/oposicao de fase e canal silencioso em 44.1/48/96 kHz,
dry alinhado, troca de perfis ao longo de 30 s simulados, estado/migracao/reset e ausencia
de new/new[] no callback. O backend C continua sem malloc/calloc na funcao de processamento
por revisao do fonte; o contador de testes nao intercepta o alocador C.

## Reproducao

Compilar com `scripts/build_windows.ps1`. O executavel de testes aceita:

```text
VocalPureTests.exe --benchmark results.csv
VocalPureTests.exe --render source.wav output.wav 65 0
VocalPureTests.exe --render source.wav output.wav 65 1
VocalPureTests.exe --render source.wav output.wav 65 2
```

O ultimo argumento e o perfil; o renderizador recusa sobrescrever WAVs existentes.
`tools/summarize_benchmark.py baseline.csv current.csv report.md` gera a tabela de desempenho.
Audios do usuario ficam somente em `tests/output`, fora do Git.

Validacao em DAW e audicao comparativa permanecem separadas dos testes de motor.
O Resolve 21 foi aberto em um projeto novo `VocalPure Validation 0.4.0`; a tentativa
de importacao foi cancelada apos falha da automacao do seletor de arquivos. Nenhum
carregamento do VST3, reproducao, exportacao ou restauracao de sessao foi validado no host.
No standalone foram conferidos layout, abertura do seletor e troca real para Eco.
ONNX e modelos adicionais pertencem a Fase 5 e nao foram integrados.
