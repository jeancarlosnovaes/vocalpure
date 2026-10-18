# Redesign visual a partir da referencia — 18/10/2026

Checkpoint anterior: `a8f1bb7` (VocalPure 0.3.0, antes do redesign).

Mudancas somente visuais em PluginEditor, Theme e SpectrumDisplay:
- Fundo ciano/turquesa com gradiente azul e textura de rede vetorial original.
- Barra superior escura com Undo, presets, Compare, informacoes e bypass.
- Grande controle circular com acabamento preto, halo magenta, malha geometrica estatica,
  percentual central e marcador que acompanha o parametro real de reducao.
- Painel esquerdo compacto, identificacao RNNoise e indicador Mono/Dual (nao e um seletor ficticio).
- Espectro circular com curvas reais, preenchimento magenta/ciano, escala logaritmica e modos existentes.
- Medidores/ganhos em faixa inferior compacta; largura estereo, VAD e status preservados.

Marca e desenho originais VocalPure; nenhuma arte, logo ou modelo comercial da referencia incorporado.
A malha central e decoracao estatica, nao uma visualizacao do estado de uma rede neural.
Reducao e ajustada por arraste, roda ou teclado, com percentual no centro; o campo numerico separado
foi removido para aproximar a composicao da referencia. Width e ganhos mantem campos numericos.

DSP, parametros, presets, persistencia, latencia e calculo FFT nao foram alterados.
Build Windows VST3/Standalone e regressao automatizada aprovados. Layout inspecionado no aplicativo.
O commit posterior de redesign pode ser revertido para retornar ao checkpoint; reconstruir os binarios
apos qualquer rollback. Dependencias e audios do usuario ficam fora do Git.

Refino do indicador: aba triangular magenta desenhada sob o aro, com sombra, gradiente
e entalhe radial escuro. Substitui a pequena seta, o ponto e o arco de progresso do
controle principal. A aba acompanha o valor real e tem espaco reservado em toda a rotacao.
Base anterior a este refino: `2a72bed`.
