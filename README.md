# VocalPure 0.5.1

Versão Rust do VocalPure 0.4.3, com plugin VST3, CLAP e aplicativo Windows.
Este repositório contém o aplicativo, os plugins e os scripts para gerar o instalador da versão atual.

O motor de áudio, a reamostragem, os controles e a interface foram reescritos
em Rust. O RNNoise 0.1.1 continua em C, integrado por uma interface pequena e
compilado junto com o programa. O modelo neural e as modificações dos perfis
General, Strong e Eco são os mesmos da versão anterior. Não há dependência
de JUCE nesta versão.

## Download

[Baixar o instalador para Windows x64](https://github.com/jeancarlosnovaes/vocalpure/releases/download/v0.5.1/VocalPure-0.5.1-Windows-x64-Setup.exe).

O instalador ainda não tem assinatura digital. Por isso, o Windows pode mostrar fornecedor desconhecido. A instalação solicita permissão de administrador para gravar os plugins nas pastas compartilhadas.

## Usar

Os arquivos compilados ficam em `dist/`:

- `VocalPure.exe`: aplicativo independente.
- `VocalPure.vst3`: pasta do plugin VST3 de 64 bits.
- `VocalPure.clap`: plugin CLAP de 64 bits.

O instalador fica em releases/VocalPure-0.5.1-Windows-x64-Setup.exe. Ele permite escolher aplicativo, VST3 e CLAP, cria atalhos e inclui desinstalador. O instalador solicita permissão de administrador para instalar os plugins compartilhados em C:\Program Files\Common Files. Os locais dos plugins aparecem no assistente. Se já houver um binário VST3 com o mesmo nome no destino, uma cópia é guardada na pasta Backups do aplicativo antes da substituição.

Para instalar manualmente o VST3, copie a pasta inteira para
`C:\Program Files\Common Files\VST3` e refaça a busca de plugins no host.
Essa cópia pode exigir permissão de administrador. A compilação não instala
arquivos no sistema.

Ao abrir `VocalPure.exe` com dois cliques, uma janela permite escolher a entrada,
a saída, a taxa e os canais, ou examinar somente a interface. O aplicativo
valida a abertura dos dispositivos antes de iniciar. No Windows, entrada e saída podem usar taxas nativas diferentes: o modo compartilhado adapta o áudio à taxa escolhida no VocalPure. O tamanho dos blocos fica a cargo do Windows.
Também é possível iniciar pela linha de comando. Para listar as entradas:

```powershell
./dist/VocalPure.exe --backend wasapi --input-device ""
```

Depois, selecione o nome exato apresentado na lista:

```powershell
./dist/VocalPure.exe --backend wasapi --input-device "NOME DA ENTRADA" --sample-rate 48000
```

Use `--help` para ver as opções de saída, canais e tamanho do bloco.
Para examinar a interface sem usar dispositivos de áudio:

```powershell
./dist/VocalPure.exe --backend dummy
```

## Controles e áudio

Os sete parâmetros mantêm os identificadores anteriores: `inputGain`,
`outputGain`, `bypass`, `reduction`, `width`, `compare` e `profile`.

Ganhos de entrada e saída: -24 a +24 dB. Redução: 0 a 100%, padrão 65%.
Largura estéreo: 0 a 200%, padrão 100%. General é o perfil padrão.
Compare e Bypass reproduzem o original alinhado no tempo, sem os ganhos e
sem ajuste de largura. Redução em 0% mantém os ganhos e o atraso.
As transições de ganho, redução, largura e Compare/Bypass duram 20 ms.

| Taxa | Atraso informado ao host | Tempo |
| --- | ---: | ---: |
| 44.100 Hz | 1.176 amostras | 26,667 ms |
| 48.000 Hz | 960 amostras | 20 ms |
| 96.000 Hz | 2.560 amostras | 26,667 ms |

Entrada e saída aceitam um ou dois canais. Outras taxas são recusadas na
inicialização. O modelo trabalha em quadros de 480 amostras a 48 kHz.
A reamostragem usa o mesmo filtro sinc causal de 128 coeficientes do original.
O processamento e a reinicialização não alocam memória Rust durante o áudio.

A interface inclui oito presets, Undo de preset/reset, Compare, Bypass,
medidores, probabilidade de voz e espectro real de entrada e saída.
O espectro usa FFT de 2.048 amostras fora da rotina de áudio; quadros visuais
são descartados quando a fila está cheia. A mudança de nível mede o sinal
inteiro, não apenas o ruído. A probabilidade de voz pode gerar falsos positivos.

## Compilar e testar

Requisitos: Rust com ferramenta MSVC de 64 bits, Visual Studio Build Tools
com C++ e Windows SDK. O compilador C é necessário para o RNNoise.
A primeira compilação baixa as dependências registradas em `Cargo.lock`.

```powershell
./scripts/build_windows.ps1
```

O script executa os testes, compila em modo otimizado e monta `dist/`, com
licenças e hashes SHA256. Para trabalhar apenas no plugin:

```powershell
cargo build --locked --release --no-default-features
```

Os testes de áudio verificam atraso, mistura, largura, ganhos, redução neural,
perfis, rejeição de valores inválidos, reinicialização e independência do tamanho
dos blocos. Também verificam alocações Rust durante o processamento e a rejeição
de aliasing do filtro de reamostragem.

## Compatibilidade e limites

O plugin mantém a identidade VST3 da implementação atual. Ela é diferente da versão C++ original.
Sessões e presets binários salvos pelo JUCE não são importados automaticamente.
Os ajustes da versão Rust são salvos e restaurados pelo host.
A interface em egui preserva o desenho original: fundo turquesa, controle central de redução, espectro circular e disposição dos controles. O desenho é vetorial e acompanha o tamanho da janela.

O instalador Windows desta versão é criado com Inno Setup 6.7 ou superior. Para gerar novamente, execute ./scripts/build_installer.ps1. O compilador pode ser informado com -IsccPath. O instalador e o aplicativo ainda não têm assinatura digital.
ONNX e Single Analysis continuam fora do plugin, como no original.
A validação automatizada não substitui testes de reprodução e exportação no
DaVinci Resolve ou em outras DAWs.

O framework NIH-plug está incluído em `vendor/nih-plug`, revisão
`de421011f41a6d10fc8c7a6084e4f4dee0143683`. O RNNoise original está em
`vendor/rnnoise`; `build.rs` aplica as correções de segurança e os perfis durante
a compilação. As dependências transitivas estão fixadas em `Cargo.lock`.

Código original sob AGPL-3.0-only, conforme `LICENSE`.
Consulte `THIRD_PARTY_NOTICES.md` para as bibliotecas usadas.


A integração WASAPI local habilita a conversão de formato do Windows e divide os blocos recebidos em trechos limitados ao tamanho aceito pelo motor. A fila entre entrada e saída preserva os canais e não espera dentro das rotinas de áudio. Se você alterar a taxa nas configurações do Windows com o aplicativo aberto, feche e reabra o VocalPure.

O teste do instalador usa uma compilação com identidade separada e pastas isoladas dentro de target/. Para reproduzir, execute ./scripts/build_installer.ps1 -SkipBuild -TestBuild e ./scripts/test_installer.ps1. Para empacotar o código-fonte, execute ./scripts/package_source.ps1. Consulte RELEASE_NOTES.md para os limites desta versão.

## Idioma e aparência

A versão 0.5.1 oferece português e inglês, além dos temas claro e escuro. As opções ficam na tela inicial e no botão Ajustes do plugin. As escolhas são lembradas entre aberturas e também são salvas no estado da sessão pelo host. Os nomes dos dispositivos são exibidos exatamente como o Windows os fornece.

O instalador também oferece português e inglês. Para corrigir uma tentativa anterior que parou com acesso negado, cancele o instalador antigo e abra o instalador 0.5.1, aceitando a solicitação de administrador do Windows.
