# Validacao

**Fase 4:** ver `phase4.md` e `benchmarks/phase4-results.md`. Perfis RNNoise reais,
reamostrador comparado com referencia anterior, metade das inferencias Eco,
estereo/anti-fase/canal silencioso por taxa, blocos variaveis, migracao e 30 s simulados
com trocas. Audios reais exportados para comparacao; superioridade perceptual nao declarada.

**Fase 3:** suite expandida com calibracao FFT, fila visual saturada, invariancia do audio com captura,
width/mono sum, Compare, presets e schema 3. Conferencia visual e detalhes em `phase3.md`.

**Atualizacao Fase 2:** a suite agora testa RNNoise real. Transparencia significa sinal original
atrasado conforme PDC quando reduction=0/bypass. Ver docs/phase2.md e docs/validation-phase2.md.
A matriz inclui 3 s de ruido por taxa/canal, invariancia por particionamento, mistura 50%, impulso,
filtro antialiasing, estado/migracao v1, reset neural, sanitizacao e polaridade/crosstalk estereo.
Um contador intercepta new/new[] durante callbacks; malloc/calloc do backend C foram revisados
no fonte, mas nao interceptados por esse contador. Perfis reais de DAW ainda pendentes.

## Registro da Fase 1 (historico)

`ctest --test-dir build -C Release --output-on-failure` executa o processador real.
Matriz: 44100/48000/96000 Hz, 1/2 canais, blocos 0/7/32/64/128/137/256/512/1024.
Verifica transparencia exata, ganho conhecido, convergencia/crossfade bypass, bypass do host,
serializacao/restauracao, estado invalido, reset, medidores, latencia zero e rejeicao de layout assimetrico.
Processamento da fase 1 usa apenas buffers do host, suavizadores e atomicos lock-free; sem IO ou locks.
Teste de alocacao instrumental, profiling, soak test e validacao VST3 completa ficam pendentes.

## Roteiro manual de host
1. Abrir VST3 no DAW; inserir antes de outros efeitos em faixa com WAV conhecido.
2. Em 0 dB, renderizar e comparar por null test contra original, sem normalizacao/dither.
3. Automatizar ganhos e bypass; verificar ausencia de descontinuidades.
4. Salvar projeto com valores distintos, fechar/reabrir, verificar restauracao.
5. Alternar mono/estereo, taxas e buffers; repetir abertura/fechamento da interface.
6. Standalone: selecionar hardware, usar fones, habilitar entrada, verificar audio e medidores.

REAPER, FL Studio e Ableton: compatibilidade NAO validada. Compilar nao equivale a testar em DAW.
Nao existem avaliacoes auditivas de denoising nesta fase. Nas proximas etapas, usar gravacoes licenciadas
com ventilador/trafego/chuva/vozes, A/B alinhado, SI-SDR/STOI e CPU/latencia medidos;
PESQ/DNSMOS somente apos verificar licencas. Separar treino/validacao/teste por locutor e fonte de ruido.
