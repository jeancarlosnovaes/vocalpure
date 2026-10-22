"""Summarize local offline callback timings. No audio or network access."""
import argparse
import csv
import statistics
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("baseline", type=Path)
parser.add_argument("current", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
with args.baseline.open() as f:
    baseline = list(csv.DictReader(f))
with args.current.open() as f:
    current = list(csv.DictReader(f))

lines = ["# Callback timing results", "", "Offline synthetic audio, Windows x64, AMD Ryzen 7 5700X.",
         "Median CPU percentage across blocks 32/64/128/256/512/1024, spectrum capture off.",
         "CPU percentage is callback time / audio duration on one processing thread, not whole-machine CPU.", "",
         "| Hz | Channels | Previous General | General | Strong | Eco | Eco saving vs General |",
         "|---:|---:|---:|---:|---:|---:|---:|"]
for rate in (44100, 48000, 96000):
    for channels in (1, 2):
        def median(rows, profile):
            return statistics.median(float(row["cpu_percent"]) for row in rows
                                     if int(row["rate"]) == rate and int(row["channels"]) == channels
                                     and int(row["profile"]) == profile and row["capture"] == "0")
        old = median(baseline, 0)
        general, strong, eco = (median(current, profile) for profile in range(3))
        lines.append(f"| {rate} | {channels} | {old:.3f}% | {general:.3f}% | {strong:.3f}% | {eco:.3f}% | {100*(1-eco/general):.1f}% |")

lines += ["", "## Callback budget", "",
          "Some inference bursts exceed the budget of short host buffers. These are offline timing overruns,",
          "not measured device underruns. They prevent a blanket guarantee of glitch-free operation at 32/64 samples.",
          "The benchmark excludes GUI drawing and FFT cost; capture=1 measures only the audio-side queue cost.", "",
          "| Block | Calls over budget | Measured calls | Worst callback (microseconds) |",
          "|---:|---:|---:|---:|"]
for block in (32, 64, 128, 256, 512, 1024):
    rows = [row for row in current if int(row["block"]) == block]
    lines.append(f"| {block} | {sum(int(row['over_budget']) for row in rows)} | {sum(int(row['calls']) for row in rows)} | {max(float(row['max_us']) for row in rows):.1f} |")
lines += ["", "One baseline run and two optimized runs were recorded; this report uses the second optimized run.",
          "Timing varies with background load. Results describe this machine and this synthetic workload only.",
          "No listening-quality ranking is implied.", ""]
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text("\n".join(lines), encoding="utf-8")
print(args.output)
