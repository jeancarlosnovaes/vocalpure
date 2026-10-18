# Fase 2 — RNNoise real

## Modelo e contrato
RNNoise v0.1.1, commit `6cbfd53eb348a8d394e0757b4025c6ded34eb2b6`, pesos originais incorporados em
`src/rnn_data.c` (SHA256 `63476f37a6cbac1202344671fa2a33bfdbccde78e23ecb9f29baa430b66f27a4`).
Fonte oficial: https://github.com/xiph/rnnoise/tree/v0.1.1. A versao fixa inclui pesos no repositorio;
nao exige rede, download de pesos ou instalacao de Python para executar o plugin.
Modelo historico, nao a versao mais recente: escolhido como baseline reproduzivel, nao como promessa de melhor qualidade.

IDenoiser define capacidades, prepare/reset e processFrame. RNNoise recebe 480 amostras mono a 48 kHz,
em escala PCM16 float. O adaptador recebe float normalizado e multiplica por 32768; a saida divide por 32768.
Somente a entrada neural e limitada a [-1,1] para evitar extrapolacao extrema; o dry nao recebe limitador.
RNNoise mantem GRUs, memoria espectral, pitch, filtro passa-altas e overlap-add. VAD vem do modelo,
nao do medidor de volume. Trata fala; pode modificar respiracao, canto, timbre e vozes distantes.

## Pipeline e alinhamento
Entrada -> ganho suavizado -> frames de 10 ms -> sinc para 48 kHz (quando preciso) -> RNNoise -> sinc
para taxa do host -> mistura com dry atrasado -> ganho de saida -> crossfade com original atrasado.

`reduction` (ID estavel, 0..100, padrao 65) controla `dry + reduction/100 * (neural - dry)`.
0% conserva o sinal com ganhos aplicados e latencia compensada. Bypass conserva o original, sem ganhos,
com a mesma latencia. Suavizadores de 20 ms operam por amostra, compartilhados entre canais.
O motor continua processando durante bypass/0%, preservando o estado neural; bypass nao economiza CPU.

| Taxa | Frame host | Frame + overlap RNNoise | Reamostragem | Latencia declarada |
|---|---:|---:|---:|---:|
| 44.1 kHz | 441 | 882 amostras | 294 amostras | 1176 (26.667 ms) |
| 48 kHz | 480 | 960 amostras | nenhuma | 960 (20 ms) |
| 96 kHz | 960 | 1920 amostras | 640 amostras | 2560 (26.667 ms) |

O primeiro frame e coletado antes de ser emitido; a reconstrucao RNNoise acrescenta um frame.
Cada conversor sinc causal tem atraso exato de 1/300 s, com kernel Blackman de 128 taps e fases
precalculadas. Esta escolha facilita alinhamento inteiro nas tres taxas e prioriza verificacao;
otimizacao de latencia futura pode usar delay fracionario com testes adicionais.
A meta de ate 20 ms NAO e atendida em 44.1/96 kHz nesta versao.
O atraso de dispositivo e do host e adicional; custo de CPU nao e latencia algoritmica.
Taxas diferentes dessas tres resultam em passagem original, latencia zero e aviso na interface.

## Estereo e seguranca
Cada canal possui estado neural independente (Dual). Nao ha downmix: sinais em oposicao nao se cancelam
na analise. Canais identicos e opostos, ausencia de vazamento e atraso comum sao testados.
Ainda pode haver mudanca de imagem quando os canais tem conteudo/ruido diferente; Single/Width ficam para fase 3/4.

Alocacoes e preparacao de tabelas/estados ocorrem em prepareToPlay. A inicializacao global lazy do RNNoise
e antecipada sob call_once fora do callback; processFrame nao usa locks. Buffers do frame e delays sao
prealocados. Nao ha arquivo, rede, troca de modelo nem espera em processBlock. Inferencia sincrona, sem worker.
Uma falha numerica do modelo desativa o backend ate novo prepare e usa dry alinhado; entrada NaN/Inf e sanitizada.
Checks de malloc/calloc e substituicao de VLAs por arrays de stack limitados sao gerados pelo CMake em copias
do fonte upstream. Os tamanhos RNNoise fixos cabem nos workspaces de 2048 elementos. Checkout original intacto.

## Estado e limites
Schema 2 adiciona reduction. Sessoes schema 1 migradas recebem 0% para nao adquirir limpeza inesperada;
o host passa a receber a nova latencia. Ganhos e bypass mantem IDs. Reset restaura 65%, 0 dB e bypass desligado.
ONNX, perfis General/Strong/Eco, espectro e presets avancados ainda nao implementados.
O percentual e mistura, nao uma medicao de porcentagem de ruido removido.

## Reproducao
`git clone --depth 1 --branch v0.1.1 https://github.com/xiph/rnnoise.git external/rnnoise`
Executar `./scripts/build_windows.ps1`. O teste console tambem renderiza WAV com compensacao do atraso:

```powershell
./build/VocalPureTests_artefacts/Release/VocalPureTests.exe --render C:/audio/entrada.wav C:/audio/saida.wav 65
```

Saida WAV PCM24, mesma taxa/canais/duracao, delay removido para comparacao. Nao sobrescreve arquivos existentes.
`tools/evaluate_audio.py` extrai trechos e calcula niveis com Python+NumPy. Sem referencia limpa, nao calcula
melhora de SI-SDR/STOI/PESQ. Atenuacao em janelas de pouca energia NAO equivale a medicao isolada de ruido.
