# Bibliotecas de terceiros

O código do VocalPure Rust é distribuído sob AGPL-3.0-only, conforme `LICENSE`.
As bibliotecas mantêm suas próprias licenças e avisos.

- RNNoise 0.1.1, Xiph.Org e Jean-Marc Valin: licença BSD de três cláusulas.
  Código e modelo incorporado em `vendor/rnnoise`; texto em
  `vendor/rnnoise/COPYING`. O arquivo `build.rs` aplica as mesmas alterações
  locais de segurança, máscara Strong e cadência Eco do projeto C++, além de
  uma função de reinicialização que reutiliza os buffers da rede.
- NIH-plug e nih_plug_egui, Robbert van der Helm: licença ISC, em
  `vendor/nih-plug/LICENSE`. Fonte:
  https://github.com/robbert-vdh/nih-plug, revisão
  `de421011f41a6d10fc8c7a6084e4f4dee0143683`.
- vst3-sys e vst3-com: os avisos e as licenças estão no repositório da
  dependência, fixada em `Cargo.lock`:
  https://github.com/robbert-vdh/vst3-sys.
- egui, egui-baseview, baseview, rustfft, crossbeam, CPAL, midir e suas
  dependências: consulte as versões e fontes exatas em `Cargo.lock` e os
  respectivos arquivos de licença incluídos no código das dependências.

JUCE não é usado nem vinculado pelos binários Rust.
O teste de carregamento VST3 do projeto original usa JUCE como host externo.

CPAL 0.15.3 está incluído em vendor/cpal sob Apache-2.0. A cópia local habilita a conversão de taxa e canais do WASAPI compartilhado. O backend independente do NIH-plug foi ajustado para aceitar blocos variáveis e transferir quadros completos entre os dispositivos sem espera ativa. Essas alterações são locais ao VocalPure.
