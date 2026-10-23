# Fase 6 — pre-release Windows 0.4.3

Atualizacao 0.4.3: VST3 anuncia `Fx|Restoration`; categoria conferida pelo teste
que carrega o bundle real. Para atualizar a lista no Resolve, reinstalar a nova
versao e refazer a varredura. O host pode manter a categoria anterior em cache.
Codigo original disponibilizado sob AGPL-3.0-only por autorizacao do titular.

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
  codigo do plugin. Varredura encontra VocalPure 0.4.2, seguida de 18 instancias
  para 3 taxas x 2 layouts x 3 valores de Width. Verifica serializacao/restauracao,
  parametros exportados, audio em reducao 0% e atraso reportado 960/1176/2560 amostras.
  Isso testa o bundle e o wrapper; nao substitui a compensacao de atraso do Resolve.
- CTest: `foundation` e `vst3_smoke` passaram (cerca de 6 s no ambiente local).

## Pacote reproduzivel

`./scripts/package_windows.ps1` compila, exige regressao aprovada e gera uma pasta
e um ZIP em `dist/`, com identificacao de versao e horario. `-SkipBuild` usa os
binarios ja compilados, mas ainda exige os testes. Versao dos dois executaveis
e conferida contra CMake antes de empacotar.

Inclui o bundle VST3 inteiro, Standalone, instalador nativo, guia em portugues, avisos/licencas
disponiveis das dependencias e manifesto de arquivos com SHA256. O script confere
contagem, tamanho e hash dos arquivos dentro do ZIP final e gera SHA256 do ZIP.
Nao inclui Python, ONNX, testes, gravacoes do usuario ou credenciais. O manifesto
registra commit e se havia alteracoes nao commitadas. Os pacotes nao entram no Git.

Esse e um **pacote interno de teste, sem assinatura digital**. O empacotamento
nao instala/substitui arquivos do sistema nem publica release no GitHub.
O assistente `Instalar VocalPure.exe` e executado separadamente pelo usuario, apos
extrair o ZIP. Exige administrador e oferece instalar/atualizar ou restaurar.
Instalacao automatica/manual e retorno constam em `package-readme.txt`.

## Instalador e runtime na 0.4.2

O assistente instala apenas o VST3 na pasta Common Files/VST3, obtida pela API de
pastas conhecidas do Windows. Standalone continua portatil. Nao fecha aplicativos,
nao altera suas configuracoes e nao possui desinstalacao automatica nesta versao.
Os backups ficam na pasta conhecida ProgramData/VocalPure/Backups, fora da varredura.

Antes de substituir: confere o manifesto SHA256 do payload (integridade, nao prova
de autenticidade), rejeita arquivos ausentes/extras, caminhos relativos invalidos,
links/junctions e arquivo em uso. Copia para uma pasta temporaria e verifica os bytes.
Se ha versao anterior, gera e verifica um backup completo. A substituicao usa renomeacoes
entre pastas irmas; se o commit falhar, retorna a pasta anterior. O retorno automatico
foi testado por uma falha injetada entre essas renomeacoes. Prefixo e pai resolvido
sao conferidos antes de cada limpeza recursiva; nao ha remocao generica de pastas.
Um mutex impede duas transacoes simultaneas na mesma sessao Windows. Backups nao sao apagados automaticamente.

`installer_transactions` passou com primeira instalacao, atualizacao, backup exato,
restauracao, arquivos antigos removidos da nova versao (preservados no backup), falha
injetada, hash conhecido SHA256, payload corrompido/incompleto/extra, caminhos duplicados,
traversal, arquivo bloqueado, concorrencia e destino com espacos/Unicode. Testes criam
pastas novas em `build/installer-tests`; nao instalam no sistema. Durante empacotamento,
o mesmo core confere o bundle real do pacote em modo somente leitura.

Plugin, Standalone e instalador passaram a usar CRT estatico. Inspecao com dumpbin
nao encontrou dependencias de VCRUNTIME/MSVCP/UCRT redistribuivel nos tres binarios;
restam DLLs do sistema Windows. Isso nao substitui homologacao em uma maquina limpa.
O dialogo/UAC do instalador nao foi exercitado nesta rodada. O usuario informou ja
ter testado no Resolve e solicitou nao usar o aplicativo; nao foram inferidos resultados
especificos de exportacao, PDC ou automacao a partir desse relato.
Falha de energia, encerramento forcado no meio da troca e instalacoes a partir de
sessoes Windows diferentes nao foram homologados. A recuperacao testada cobre
erros retornados pelo sistema e a falha de commit injetada, mantendo o backup.

## Pendencias para concluir a Fase 6

- Registrar o escopo do teste informado pelo usuario; a validacao automatizada anterior no Resolve e da 0.4.0.
- Exportacao real de timeline, preservacao de inicio/cauda e null test de PDC no host.
- Automacao de timeline, taxas/layouts e buffers no Resolve, incluindo estereo real.
- Homologacao do assistente/UAC e retorno em maquina Windows limpa, sem ferramentas de desenvolvimento.
- Preservar os avisos e fontes correspondentes completos na distribuicao AGPLv3.
- Assinatura de codigo e pacote publico apos os itens anteriores.
- macOS/AU continua sem compilacao ou homologacao neste ambiente Windows.

Veja `validation-resolve.md` para a evidencia anterior e `phase5.md` para ONNX.
