VocalPure - pacote interno de teste para Windows x64

Conteudo:
  VST3/VocalPure.vst3 : plugin completo (uma pasta, nao apenas o arquivo interno).
  Standalone/VocalPure.exe : aplicativo independente.
  package.json : versao, commit e hashes dos arquivos.
  Licenses : avisos do projeto e das dependencias.

Instalacao no Resolve:
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
  Pacote de teste interno, sem assinatura e sem instalador automatico.
  Nao e uma declaracao de conclusao da homologacao no Resolve nem de liberacao
  comercial. Modalidade de licenciamento/distribuicao publica ainda pendente.

Projeto e documentacao: https://github.com/jeancarlosnovaes/vocalpure
