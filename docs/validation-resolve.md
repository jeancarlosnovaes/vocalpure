# Validacao no DaVinci Resolve — 22/10/2026

Ambiente: Windows x64, DaVinci Resolve 21 (executavel ProductVersion 21.1.1.10),
VocalPure VST3 0.4.0, codigo `42b4d5d`. Projeto local `VocalPure Validation 0.4.0`.

## Verificado diretamente no host

- O projeto existente abriu com VocalPure na faixa Audio 1, slot 1.
- Editor personalizado carregou com versao 0.4.0, perfil Strong, ganhos de 0 dB,
  entrada MONO e largura desabilitada conforme o layout.
- Reproducao real da faixa alimentou medidores de entrada/saida, VAD e os espectros
  central/lateral. Indicador RNNoise ready / Audio active, 960 amostras reportadas.
  Esse valor corresponde a 20 ms em 48 kHz; nao e uma medicao da compensacao do host.
- Compare Original foi ativado durante a reproducao: status LISTENING TO ORIGINAL,
  curvas de entrada/saida sobrepostas; desativar retomou as curvas distintas.
- Bypass foi ativado durante uma segunda reproducao: status de original e curvas
  sobrepostas. O estado de bypass tambem se refletiu no indicador de efeito do Resolve.
- Preset Low CPU aplicado no host selecionou Eco e reposicionou o controle principal
  para o valor do preset (65%, conforme definicao do codigo). Ganhos 0 dB, largura 100%,
  Compare/Bypass desligados. Reproducao continuou com dados reais.
- Transporte parado e projeto salvo sem indicador Edited, antes de encerrar o Resolve.
- Resolve encerrado completamente e iniciado de novo. O projeto foi reaberto pelo
  Project Manager. VocalPure restaurou Eco, a mesma posicao do controle principal,
  ganhos 0 dB, largura 100% e Compare/Bypass desligados.
  O nome visual do preset voltou a CUSTOM / SELECT PRESET: esse rotulo e estado local
  do editor, enquanto os parametros foram restaurados. Nao foi lido um valor numerico
  de reducao do host; a conferência foi visual a partir do preset conhecido.
- O projeto de validacao ficou salvo com Eco. A tentativa de retornar ao ajuste inicial
  Strong foi interrompida pela deteccao de interacao do usuario; nao se declara essa
  restauracao concluida. A gravacao e os efeitos da faixa foram preservados.

## Limites desta validacao

Observacao funcional de interface e fluxo de audio. Nao foi feita captura de loopback,
audicao conclusiva, null test de PDC, medicao de glitches/underruns, escrita de automacao
na timeline ou matriz estereo/taxas/buffers dentro do Resolve. Exportacao no host ainda
nao foi testada. Os testes automatizados do motor sao documentados separadamente.
Nenhuma gravacao ou arquivo de projeto foi incluido no Git.
