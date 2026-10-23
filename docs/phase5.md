# Fase 5A — avaliacao ONNX local

Em 22/10/2026 foi preparada e executada a referencia streaming do
**DPDFNet2 48 kHz HR**, comparada ao RNNoise General do VocalPure 0.4.0.
Esta etapa entrega ferramentas reproduziveis, verificacoes e WAVs locais.
**O VST3 continua usando RNNoise. ONNX ainda nao esta integrado ao plugin.**

## Modelo e contrato

- Fonte: [Ceva-IP/DPDFNet](https://github.com/ceva-ip/DPDFNet).
  [Pesos e model card](https://huggingface.co/Ceva-IP/DPDFNet/tree/dd6818d00f50c836fed43a6243ebe49116de5964),
  declarados Apache-2.0 pelo autor. O pacote Python tambem inclui Apache-2.0.
- Revisao dos pesos `dd6818d00f50c836fed43a6243ebe49116de5964`.
  SHA256 e configuracao em `tools/onnx_model.json`; verificados antes de carregar ORT.
- Referencia `dpdfnet==0.6.0`, ONNX Runtime CPU 1.31.0, uma thread intra/inter-op.
  Ambiente Windows x64 / Python 3.12.14, Ryzen 7 5700X.
- PCM float32 mono a 48 kHz, janela Vorbis de 960 amostras, salto de 480 (10 ms).
  FFT real sem normalizacao externa adicional; RI intercalado no ultimo eixo.
- Entrada `spec`, saida `spec_e`: float32 `[1,1,481,2]`.
  Entrada `state_in`, saida `state_out`: float32 `[56436]`, independentes por canal.
  Estado inicial vem dos metadados: 481 valores de normalizacao ERB, 96 de espectro,
  restante zero. Nao iniciar todo o vetor em zero nem reutilizar estados entre arquivos.
- O wrapper ONNX ja contem a escala do modelo. Shapes, tipos, metadados e estado
  inicial sao conferidos pelo avaliador; nao e carregador generico de modelos.

## Alinhamento e cauda

O caminho offline oficial descarta quatro saltos espectrais (40 ms) na reconstrucao.
O avaliador usa `StreamEnhancer` causal, acrescenta meia janela inicial de zeros,
alimenta zeros suficientes no final e retira 2400 amostras da saida concatenada:
480 da meia janela mais 1920 do contexto neural. Preserva exatamente a duracao.
Nao usa `flush()` de um salto como drenagem completa do contexto do modelo.

**Esse deslocamento de arquivo nao e a latencia a informar ao Resolve.** A disponibilidade
de uma janela e a fila de saida tambem entram no atraso do futuro adaptador C++.
O contrato devera contabilizar os quatro saltos internos, janela e agendamento; nao
reutilizar automaticamente os 20 ms do RNNoise nem os ~20 ms de preenchimento
inicial citados na documentacao publica como se fossem todo o atraso neural.
Nos dois trechos reais, a correlacao apos compensacao teve pico em zero, com
resolucao de seis amostras. Isso nao substitui teste de impulso/PDC no host.

## Verificacoes executadas

Particionamento 480 versus sequencia 32/147/512/1/960/77, reinicializacao e
extensao da cauda com mais silencio produziram diferenca maxima zero.
Silencio ficou em zero; comprimentos 0/1/479/480/481 foram preservados, sem NaN/Inf.
Tambem foi verificado um trecho de comprimento nao multiplo do salto.

Foram renderizados os 90 s originais e os 30 s com ruido colorido adicional
usados na Fase 4. A reamostragem 44.1 -> 48 kHz acontece uma vez, antes dos dois
motores; nao se usa o reamostrador por bloco do pacote Python. Ambos recebem
exatamente o mesmo WAV a 48 kHz. RNNoise: General, reducao 100%, ganhos 0 dB.
DPDFNet: supressao completa, sem limite de atenuacao, normalizacao ou limitador.
WAVs ONNX em float evitam clipping implicito. Nao houve amostras acima de 0 dBFS.

## Resultados e decisao

Na repeticao final, DPDFNet processou 90 s em 17.30 s e 30 s em 5.72 s.
Chamadas de 10 ms: media 1.90–1.92 ms, p99 4.29–4.39 ms, maximo 6.24 ms.
Na primeira rodada houve quatro chamadas acima de 10 ms, com maximo de 45.23 ms.
Isso inclui Python/FFT/buffering/ORT e interferencia do sistema; nao e benchmark
isolado do grafo nem certificacao de prazo de callback.
O renderizador RNNoise nativo consumiu cerca de 1.18–1.20% do tempo do audio
na repeticao, versus 19% da referencia Python ONNX. Comparacao de caminhos completos,
nao de implementacoes equivalentes. Dados das duas rodadas em `benchmarks/phase5-*.json`.

No trecho com ruido adicional, a atenuacao das janelas de menor energia foi
13.06 dB no DPDFNet e 14.18 dB no RNNoise; nas de maior energia, 0.36 e 0.40 dB.
Essas janelas nao sao rotulos de voz/ruido. A gravacao original nao e referencia
limpa: nao se declara melhoria SI-SDR/STOI, naturalidade ou superioridade auditiva.
Os arquivos ficam disponiveis para audicao A/B local.

**Decisao:** manter RNNoise como backend do VST3 nesta etapa. A referencia ONNX
aloca arrays por chamada; nao satisfaz o contrato sem alocacoes de `IDenoiser`.
Proxima subetapa: adaptador C++ com buffers/estados prealocados, equivalencia
numerica Python/C++, medicao de alocacoes e picos, reset, falhas e latencia total.
Depois, integrar selecao/transicao segura e validar reproducao e exportacao acelerada
no Resolve. Uma fila de worker nao pode perder blocos quando o host renderiza mais
rapido que o tempo real. Estereo requer dois estados e validacao de imagem/fase.

## Reproduzir

Com Python 3.12 x64, na raiz do projeto:

```powershell
./scripts/setup_onnx_eval.ps1 -Python 'C:/caminho/python.exe'
./tools/local/onnx-eval/Scripts/python.exe tools/evaluate_onnx.py `
  --model tools/local/onnx-models/dpdfnet2_48khz_hr.onnx `
  --output-dir tests/output/minha-avaliacao `
  --input 'C:/caminho/voz-mono.wav' `
  --rnnoise-renderer build/VocalPureTests_artefacts/Release/VocalPureTests.exe
```

Sem `--input`, executa apenas as verificacoes de streaming. A pasta de saida deve
ser nova; arquivos anteriores nao sao sobrescritos. O setup baixa dependencias e
pesos oficiais; a avaliacao nao acessa a rede. Sem upload de audio.
Modelo, ambiente e gravacoes ficam em pastas ignoradas pelo Git. Nenhum peso,
runtime ou gravacao pessoal acompanha o VST3 ou este commit.
