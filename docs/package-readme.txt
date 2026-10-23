VocalPure - pre-release de teste para Windows x64

Conteudo:
  VST3/VocalPure.vst3 : plugin completo (uma pasta, nao apenas o arquivo interno).
  Standalone/VocalPure.exe : aplicativo independente.
  Instalar VocalPure.exe : instala/atualiza o VST3 ou restaura o backup anterior.
  package.json : versao, commit e hashes dos arquivos.
  Licenses : avisos do projeto e das dependencias.

Instalacao com assistente:
1. Extraia TODO o ZIP para uma pasta. Nao execute apenas o EXE dentro do ZIP.
2. Feche o Resolve e outros editores de audio.
3. Abra Instalar VocalPure.exe. O Windows solicita permissao de administrador
   porque o destino e a pasta de plugins compartilhada do sistema.
4. Escolha Instalar / atualizar. O instalador confere os hashes do VST3 e, se
   houver uma versao anterior, guarda um backup verificado antes de substituir.
5. Reabra o Resolve e confira a versao do plugin na interface.
6. Para voltar, feche os editores, abra novamente o instalador e escolha
   Restaurar versao anterior. O backup fica em C:/ProgramData/VocalPure/Backups
   na configuracao padrao do Windows. Cada substituicao conserva o estado anterior.
   A primeira instalacao, sem versao preexistente, nao cria backup anterior.
7. O aplicativo Standalone fica na pasta do pacote; nao e instalado pelo assistente.

Instalacao manual alternativa:
1. Feche o Resolve antes de substituir um plugin carregado.
2. Se ja houver VocalPure instalado, guarde uma copia da pasta antiga em outro
   local, fora das pastas de varredura VST3, para poder voltar a essa versao.
3. Copie a pasta INTEIRA VST3/VocalPure.vst3 deste pacote para:
   C:/Program Files/Common Files/VST3/VocalPure.vst3
   O Windows pode pedir permissao de administrador.
4. Reabra o Resolve. Confira a lista de plugins de audio nas preferencias e
   refaca a varredura se necessario. Adicione VocalPure como efeito na faixa
   de audio ou no clip desejado. Confira a versao na interface.
5. Para voltar: feche o Resolve e restaure a pasta antiga que voce guardou.

Uso:
  Noise Reduction: intensidade de limpeza. Comece em 65% e ajuste ouvindo.
  Compare Original / Bypass: escute o original alinhado no tempo.
  General: perfil normal. Strong: supressao mais profunda. Eco: menor consumo,
  com possiveis alteracoes nos ataques da fala.
  Width: abertura estereo. 0% fecha em mono; 100% conserva a abertura original;
  200% duplica o sinal lateral. Nao cria estereo a partir de uma voz mono.
  Entrada mono: MONO: NO EFFECT. Canais L/R iguais tambem nao mudam.
  Compare/Bypass ativos: ORIGINAL: INACTIVE; o ajuste Width e preservado.
  Para narracao mono, mantenha Width em 100%. Valores acima de 100% podem
  elevar picos; confira o medidor de saida. Nao ha limitador oculto.

O plugin processa o audio que recebe do host, incluindo arquivos e clips.
Nao se limita ao microfone e nao captura automaticamente todo o audio do PC.
No Standalone, selecione dispositivo/canais em Audio/MIDI Settings e use fones.

Escopo deste pacote:
  RNNoise local; sem Python, ONNX ou download de modelos durante o uso.
  Taxas: 44.1, 48 e 96 kHz. Mono/estereo. Atraso: 20 ms em 48 kHz e
  26.67 ms nas outras duas taxas, alem do atraso do dispositivo/host.
  Pre-release, com assistente de instalacao sem assinatura digital.
  Plugin, Standalone e instalador compilados com runtime C++ estatico.
  Homologacao do assistente em Windows limpo ainda pendente.
  Codigo original sob GNU AGPLv3 (AGPL-3.0-only). Fontes correspondentes completos
  acompanham a release no GitHub; terceiros preservam suas respectivas licencas.

Projeto e documentacao: https://github.com/jeancarlosnovaes/vocalpure
