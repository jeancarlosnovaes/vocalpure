# Fase 3 — interface e visualizacao real (18/10/2026)

VocalPure 0.3.0: interface JUCE original em azul-petroleo, ciano e roxo. Tamanho inicial 1000x560;
redimensionamento proporcional de 800x448 a 1500x840. Desenho vetorial, sem bitmaps de baixa resolucao,
com foco/hover, tooltips, controles acessiveis por teclado e entrada numerica nos sliders.

## Recursos entregues
- Knob central de reducao com percentual, anel em gradiente, mouse/roda, ajuste fino e retorno a 65%.
- Sete presets reais: Natural Voice 35%, Podcast Clean 65%, Studio Voice 50%, Noisy Room 85%,
  Outdoor Dialogue 90%, Strong Noise Reduction 100%, Gentle Cleaning 20%.
  Todos restauram ganhos a 0 dB, largura 100%, bypass/Compare desligados. Sao ajustes do mesmo modelo.
- Reset e Undo de um nivel para o ultimo preset/reset. Undo de gestos/automacao continua a cargo do DAW.
- Width automatizavel (0..200%, ID `width`, padrao 100). Mid/side: M=(L+R)/2, S=(L-R)/2,
  L'=M+w*S, R'=M-w*S. Preserva soma mono; desabilitado na UI para mono. Alargamento pode elevar picos.
- Compare automatizavel (ID `compare`) alterna para original alinhado e volta sem alterar ajustes.
  Como bypass, inclui os ganhos e largura no que e desligado. Estado neural permanece ativo.
- Medidores de pico dos blocos e indicador VAD do RNNoise. LEVEL CHANGE compara energia da entrada
  alinhada com a saida efetiva, incluindo ganhos, mistura e width. **Nao e ruido isolado removido.**
- Informacoes do motor, avisos de falha/taxa nao suportada e ausencia de entrada.

## Espectro
Fila SPSC prealocada com 7 snapshots utilizaveis, cada um com 2048 amostras por canal/caminho.
Produtor: thread de audio; consumidor: timer da interface a 30 Hz. Sem espera, mutex, IO ou FFT no callback.
Fila cheia descarta snapshots novos. Interface consome ate o ultimo completo e faz apenas uma analise
por tick. Captura desabilitada quando editor fechado; frames parciais antigos sao descartados.

FFT 2048, janela Hann normalizada, escala logaritmica 20 Hz..min(20 kHz, Nyquist), amplitude -100..0 dBFS.
Entrada: original atrasado pela latencia do plugin; saida: sinal realmente entregue, incluindo Compare.
Estereo combina potencias (media de L/R) para nao ocultar audio em oposicao de fase.
Input/Output/Compare sao modos de visualizacao, sem mudar o audio. Curvas roxa/ciano possuem suavizacao
visual; nao representam mascara neural nem espectrograma. Sem dados, display e limpo apos 0.5 s.
O analisador da UI nao e instrumento de medicao certificado; bins e janela limitam resolucao.

## Estado
Schema 3 inclui width e compare. Sessoes anteriores recebem width=100 e compare=false.
Schema 1 continua migrando reduction=0. IDs existentes e parametros da Fase 2 preservados.
Presets persistem seus valores; o nome selecionado e local a janela, reabertura pode mostrar CUSTOM.
View do espectro e historico de Undo tambem sao preferencias locais da janela.

## Validacao
Release VST3/Standalone compilados; suite anterior mais:
- Captura habilitada versus desabilitada produz audio identico, sem alocacoes C++ no callback.
- FIFO: ordem e saturacao; descarta sem interromper processamento.
- FFT: seno em bin conhecido a -6.02 dBFS; saida a -12.04 dBFS; canais em oposicao nao se cancelam.
- Width 0/100/200, preservacao da soma mono, Compare voltando ao original sem alterar parametros.
- Sete presets e restauracao dos novos parametros.
- Inspecao visual do standalone em 1000x560 e 800x448, com espectro e medidores de entrada real.
- Natural Voice mudou reducao para 35% e ganhos para zero; Undo restaurou 65% e ganho anterior.

## Limites da fase
General/Strong/Eco, Single Analysis e Low CPU nao foram adicionados: requerem processamento distinto
e validacao de desempenho na Fase 4. Seven presets de intensidade nao sao sete modelos neurais.
Nao ha medidor CPU ficticio nem promessa de reducao em dB baseada apenas em volume.
Nao testado em DAW/macOS ou monitores fisicos com diferentes escalas HiDPI. Perfil do audio e latencias
da Fase 2 permanecem; a aceitação final do produto requer os testes de host e qualidade ainda pendentes.
