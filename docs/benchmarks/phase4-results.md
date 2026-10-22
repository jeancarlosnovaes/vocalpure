# Callback timing results

Offline synthetic audio, Windows x64, AMD Ryzen 7 5700X.
Median CPU percentage across blocks 32/64/128/256/512/1024, spectrum capture off.
CPU percentage is callback time / audio duration on one processing thread, not whole-machine CPU.

| Hz | Channels | Previous General | General | Strong | Eco | Eco saving vs General |
|---:|---:|---:|---:|---:|---:|---:|
| 44100 | 1 | 1.792% | 1.744% | 1.707% | 1.397% | 19.9% |
| 44100 | 2 | 3.561% | 3.407% | 3.389% | 2.764% | 18.9% |
| 48000 | 1 | 1.134% | 1.141% | 1.141% | 0.841% | 26.3% |
| 48000 | 2 | 2.265% | 2.252% | 2.256% | 1.650% | 26.7% |
| 96000 | 1 | 2.212% | 2.062% | 2.060% | 1.786% | 13.4% |
| 96000 | 2 | 4.369% | 4.090% | 4.077% | 3.471% | 15.2% |

## Callback budget

Some inference bursts exceed the budget of short host buffers. These are offline timing overruns,
not measured device underruns. They prevent a blanket guarantee of glitch-free operation at 32/64 samples.
The benchmark excludes GUI drawing and FFT cost; capture=1 measures only the audio-side queue cost.

| Block | Calls over budget | Measured calls | Worst callback (microseconds) |
|---:|---:|---:|---:|
| 32 | 500 | 70536 | 546.1 |
| 64 | 0 | 35268 | 463.3 |
| 128 | 0 | 17640 | 543.2 |
| 256 | 0 | 8808 | 597.5 |
| 512 | 0 | 4404 | 849.8 |
| 1024 | 0 | 2208 | 1155.3 |

One baseline run and two optimized runs were recorded; this report uses the second optimized run.
Timing varies with background load. Results describe this machine and this synthetic workload only.
No listening-quality ranking is implied.
