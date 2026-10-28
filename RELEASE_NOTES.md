# VocalPure 0.5.1 para Windows

O pacote inclui o aplicativo independente, o VST3 e o CLAP de 64 bits.
O instalador permite escolher os componentes, as pastas dos plugins e a criação
de um atalho na área de trabalho. Também inclui desinstalador e atalho no menu Iniciar.

O processamento aceita mono e estéreo em 44,1, 48 e 96 kHz. O modo compartilhado
do Windows adapta as taxas da entrada e da saída. A tela inicial tem seletores
de dispositivos, modo de prévia, ícones vetoriais e layout sem rolagem.
O editor mantém o desenho original, oito presets, Compare, Bypass e Undo.

A identidade do plugin atual é diferente da versão C++ original. Projetos e presets binários do JUCE não são importados automaticamente.
Após instalar, refaça a busca de plugins no editor de áudio.

O instalador e o aplicativo não têm assinatura digital. ONNX e Single Analysis
continuam fora desta versão. A validação automatizada de áudio e do VST3 não
substitui testes de reprodução e exportação no seu editor.

O código do aplicativo usa AGPL-3.0-only. O código-fonte correspondente acompanha
o instalador em um arquivo ZIP separado. O RNNoise permanece como biblioteca C
incorporada; as demais bibliotecas mantêm suas próprias licenças.

A versão 0.5.1 remove o sufixo Rust dos nomes exibidos e dos arquivos dos plugins, adiciona inglês e português e oferece temas claro e escuro. O instalador solicita elevação para as pastas compartilhadas e verifica a permissão de gravação antes de copiar os plugins. Idioma, tema e contraste têm testes automatizados.
