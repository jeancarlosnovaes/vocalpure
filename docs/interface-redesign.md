# Redesign visual a partir da referencia — 18/10/2026

Checkpoint anterior: `a8f1bb7` (VocalPure 0.3.0, antes do redesign).

Mudancas somente visuais em PluginEditor, Theme e SpectrumDisplay:
- Fundo ciano/turquesa com gradiente azul e textura de rede vetorial original.
- Barra superior escura com Undo, presets, Compare, informacoes e bypass.
- Grande controle circular com acabamento preto, interior de cinza a rosa e espectro radial ao vivo,
  miolo central discreto e marcador que acompanha o parametro real de reducao.
- Painel esquerdo compacto, identificacao RNNoise e indicador Mono/Dual (nao e um seletor ficticio).
- Espectro circular com curvas reais, preenchimento magenta/ciano, escala logaritmica e modos existentes.
- Medidores/ganhos em faixa inferior compacta; largura estereo, VAD e status preservados.

Marca e desenho originais VocalPure; nenhuma arte, logo ou modelo comercial da referencia incorporado.
A malha central representa 32 bandas logaritmicas do FFT real de entrada, suavizadas no thread
da interface. Nao representa o estado interno da rede neural. Silencio e ausencia de novos
quadros apagam a atividade; nao ha movimento aleatorio nem FFT adicional no callback de audio.
Reducao e ajustada por arraste, roda ou teclado; o campo numerico separado
foi removido para aproximar a composicao da referencia. Width e ganhos mantem campos numericos.

DSP, parametros, presets, persistencia, latencia e calculo FFT nao foram alterados.
Build Windows VST3/Standalone e regressao automatizada aprovados. Layout inspecionado no aplicativo.
O commit posterior de redesign pode ser revertido para retornar ao checkpoint; reconstruir os binarios
apos qualquer rollback. Dependencias e audios do usuario ficam fora do Git.

Refino do indicador: aba triangular magenta desenhada sob o aro, com sombra, gradiente
e entalhe radial escuro. Substitui a pequena seta, o ponto e o arco de progresso do
controle principal. A aba acompanha o valor real e tem espaco reservado em toda a rotacao.
Base anterior a este refino: `2a72bed`.

Segundo refino: aro ciano/rosa e seta com cor amostrada do mesmo gradiente na posicao
angular atual. Interior cinza em 0%, rosa em 100%, com brilho e geometria ligados ao
espectro de entrada. Circulo da porcentagem reduzido a metade do diametro anterior.
Titulos alinhados ao centro de cada controle; Reset representado por seta circular,
com nome acessivel e tooltip preservados. Base anterior: `963acad`.

Refino do miolo: porcentagem visual removida, disco interno neutro reduzido e concentrico
ao controle e ao espectro. Removido o contorno verde de hover. O valor continua disponivel
na acessibilidade e nos parametros do host. Base anterior: `dafbee4`.
