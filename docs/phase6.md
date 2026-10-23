# Fase 6 — candidato interno Windows 0.4.1

Iniciada em 22/10/2026, por solicitacao do usuario. Esta etapa prepara a versao
RNNoise existente para testes de distribuicao. Nao depende de disponibilizar ONNX:
a integracao C++ da Fase 5 segue pendente e nenhum modelo ONNX aparece no VST3.
**A Fase 6 completa ainda nao foi concluida.**

## Width: funcionamento e clareza na interface

Width ajusta o componente lateral do estereo (diferenca L/R). Em 0%, L/R passam
a ser a media dos canais; em 100%, a abertura e preservada; em 200%, o componente
lateral dobra. A soma mono e preservada. Isso nao cria largura a partir de mono,
nem de dois canais identicos. Acima de 100% pode elevar picos; nao ha limitador.

Na validacao anterior no Resolve, o plugin recebia **um canal mono**, portanto
Width estava corretamente desabilitado. Uma faixa estereo contendo a mesma voz
nos dois canais tambem nao apresenta mudanca audivel. Para narracao mono, 100%
e o ajuste indicado; abertura artificial de mono exigiria outro efeito.

Na 0.4.1, uma indicacao abaixo do controle explica a disponibilidade:

- `MONO: NO EFFECT`: plugin recebe um canal, controle desabilitado.
- `STEREO WIDTH`: dois canais, controle disponivel; efeito depende da diferenca L/R.
- `ORIGINAL: INACTIVE`: Compare/Bypass ativo, controle desabilitado para deixar claro
  que o original esta sendo ouvido. O valor e preservado e volta a atuar ao sair.

Nenhuma alteracao no algoritmo ou nos identificadores dos parametros. Sessoes
anteriores mantem compatibilidade de estado. Versao na interface vem do build.
O preparo do pacote identificou e corrigiu tambem um recurso Windows desatualizado:
o arquivo binario ainda informava 0.1.0, apesar da versao nova no plugin. A dependencia
do gerador de recursos agora acompanha os metadados de versao em builds incrementais.

## Verificado nesta versao

- Compilacao Release x64: VST3, Standalone, testes de motor e host de teste VST3.
- Regressao completa existente passou, incluindo RNNoise, perfis, reamostragem,
  estado, espectros e instrumentacao de alocacoes C++ no callback.
- Width em 44.1/48/96 kHz, valores 0/100/200%, mono, estereo identico e estereo
  com frequencias/amplitudes diferentes. Saida conforme referencia mid/side,
  soma mono preservada e Compare/Bypass restaurando o original alinhado.
- Disponibilidade/texto do controle em mono, estereo, Compare e Bypass.
- Imagens do editor geradas pelo JUCE e inspecionadas em 1000x560 e 800x448,
  com estados mono/estereo/Compare. Arquivos locais em `tests/output/phase6-ui`.
- **VST3 exportado carregado de verdade**, por um host minimo JUCE separado do
  codigo do plugin. Varredura encontra VocalPure 0.4.1, seguida de 18 instancias
  para 3 taxas x 2 layouts x 3 valores de Width. Verifica serializacao/restauracao,
  parametros exportados, audio em reducao 0% e atraso reportado 960/1176/2560 amostras.
  Isso testa o bundle e o wrapper; nao substitui a compensacao de atraso do Resolve.
- CTest: `foundation` e `vst3_smoke` passaram (cerca de 6 s no ambiente local).

## Pacote reproduzivel

`./scripts/package_windows.ps1` compila, exige regressao aprovada e gera uma pasta
e um ZIP em `dist/`, com identificacao de versao e horario. `-SkipBuild` usa os
binarios ja compilados, mas ainda exige os testes. Versao dos dois executaveis
e conferida contra CMake antes de empacotar.

Inclui o bundle VST3 inteiro, Standalone, guia em portugues, avisos/licencas
disponiveis das dependencias e manifesto de arquivos com SHA256. O script confere
contagem, tamanho e hash dos arquivos dentro do ZIP final e gera SHA256 do ZIP.
Nao inclui Python, ONNX, testes, gravacoes do usuario ou credenciais. O manifesto
registra commit e se havia alteracoes nao commitadas. Os pacotes nao entram no Git.

Esse e um **pacote interno de teste, sem assinatura e sem instalador automatico**.
Nao instala/substitui arquivos do sistema, nem publica release no GitHub.
Instalacao manual e retorno a versao anterior constam em `package-readme.txt`.

## Pendencias para concluir a Fase 6

- Repetir a homologacao da 0.4.1 no Resolve; a validacao anterior e da 0.4.0.
- Exportacao real de timeline, preservacao de inicio/cauda e null test de PDC no host.
- Automacao de timeline, taxas/layouts e buffers no Resolve, incluindo estereo real.
- Instalacao e retorno de versao em ambiente limpo; verificar dependencias de runtime
  em Windows sem as ferramentas de desenvolvimento deste computador.
- Escolha de modalidade de licenciamento pelo titular e revisao da distribuicao
  completa. O LICENSE do projeto ainda nao define essa modalidade; copiar avisos
  para o ZIP nao equivale a liberar uma distribuicao publica/comercial.
- Instalador definitivo, assinatura de codigo e pacote publico apos os itens anteriores.
- macOS/AU continua sem compilacao ou homologacao neste ambiente Windows.

Veja `validation-resolve.md` para a evidencia anterior e `phase5.md` para ONNX.
