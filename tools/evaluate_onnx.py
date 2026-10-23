"""Local DPDFNet streaming evaluation. NOT an audio-callback implementation.

Requires the isolated environment described in docs/phase5.md. Never downloads
models or uploads audio. The pinned model hash is checked before ORT loads it.
Only native 48 kHz is passed to StreamEnhancer: its per-chunk resampler is not
used. Whole-file resampling happens once, outside the timed streaming section.
"""
import argparse
import hashlib
import importlib.metadata
import json
import math
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import correlate, correlation_lags, resample_poly
from dpdfnet import StreamEnhancer

MANIFEST = json.loads(Path(__file__).with_name("onnx_model.json").read_text())
RATE = MANIFEST["sample_rate"]
HOP = MANIFEST["hop_samples"]
# One initial analysis half-window plus the model's four spectral hops.
# This is the shift in concatenated output, not a host latency declaration.
OUTPUT_SHIFT = HOP * (1 + MANIFEST["spectral_delay_hops"])


def new_stream(model):
    digest = hashlib.sha256(model.read_bytes()).hexdigest()
    if digest != MANIFEST["sha256"]:
        raise ValueError("Unrecognised model SHA256; refusing to infer")
    if importlib.metadata.version("dpdfnet") != "0.6.0":
        raise ValueError("This evaluator is validated with dpdfnet 0.6.0")
    stream = StreamEnhancer(model=MANIFEST["name"], onnx_path=model)
    runtime = stream._runtime  # Pinned upstream internals, evaluation only.
    session = runtime.session
    meta = session.get_modelmeta().custom_metadata_map
    expected = {"sample_rate": RATE, "n_fft": 960, "hop_length": HOP,
                "window_length": 960, "freq_bins": 481}
    for key, value in expected.items():
        if int(meta.get(key, -1)) != value:
            raise ValueError(f"Unexpected model metadata {key}: {meta.get(key)}")
    if meta.get("window_type") != "vorbis":
        raise ValueError("Expected Vorbis analysis/synthesis window")
    ins, outs = session.get_inputs(), session.get_outputs()
    if len(ins) != 2 or len(outs) != 2:
        raise ValueError("Expected exactly two inputs and outputs")
    if ins[0].shape != [1, 1, 481, 2] or outs[0].shape != [1, 1, 481, 2]:
        raise ValueError("Unexpected spectrum tensor shape")
    size = int(meta["state_size"])
    if ins[1].shape != [size] or outs[1].shape != [size]:
        raise ValueError("Unexpected recurrent state tensor shape")
    if any(t.type != "tensor(float)" for t in ins + outs):
        raise ValueError("Expected float32 tensors")
    if runtime.init_state.size != size or not np.isfinite(runtime.init_state).all():
        raise ValueError("Invalid initial state")
    contract = dict(inputs=[dict(name=t.name, shape=t.shape, dtype=t.type) for t in ins],
                    outputs=[dict(name=t.name, shape=t.shape, dtype=t.type) for t in outs],
                    metadata=meta, providers=session.get_providers())
    return stream, contract


def stream_render(stream, audio, chunks=(HOP,)):
    """Full tail, causal zero padding, same output length as mono input.

    No upstream flush(): its single-hop drain does not drain model context.
    No attenuation limit, output normalisation, limiting, or chunk resampling.
    """
    if audio.ndim != 1 or not np.isfinite(audio).all():
        raise ValueError("Expected finite mono samples")
    if not chunks or any(int(n) != n or n < 1 for n in chunks):
        raise ValueError("Chunk sizes must be positive integers")
    stream.reset()
    # Last real sample must traverse both the neural delay and overlap-add.
    padded = np.pad(audio.astype(np.float32), (HOP, OUTPUT_SHIFT + 2 * HOP))
    padded = np.pad(padded, (0, (-len(padded)) % HOP))
    output, durations = [], []
    offset = index = 0
    start = time.perf_counter()
    while offset < len(padded):
        n = min(chunks[index % len(chunks)], len(padded) - offset)
        before = time.perf_counter_ns()
        block = stream.process(padded[offset:offset+n], sample_rate=RATE)
        elapsed = (time.perf_counter_ns() - before) / 1e6
        # At 480-sample input each nonempty call executes exactly one inference.
        if len(block):
            durations.append(elapsed)
            output.append(block)
        offset += n
        index += 1
    wall = time.perf_counter() - start
    joined = np.concatenate(output) if output else np.zeros(0, np.float32)
    if len(joined) < OUTPUT_SHIFT + len(audio):
        raise RuntimeError("Incomplete neural tail")
    result = joined[OUTPUT_SHIFT:OUTPUT_SHIFT+len(audio)]
    if not np.isfinite(result).all():
        raise RuntimeError("Non-finite neural output")
    # Exclude first 20 nonempty calls from warm-frame statistics.
    warm = np.asarray(durations[20:] or durations)
    timing = dict(wall_seconds=wall, audio_seconds=len(audio)/RATE,
                  wall_realtime_factor=wall/(len(audio)/RATE) if len(audio) else None,
                  nonempty_calls=len(durations), warm_excluded_calls=min(20, len(durations)),
                  mean_call_ms=float(warm.mean()), p99_call_ms=float(np.quantile(warm, .99)),
                  max_call_ms=float(warm.max()),
                  calls_over_10ms=int(np.sum(warm > 10)), chunks=list(chunks))
    return result, timing


def check(stream):
    rng = np.random.default_rng(317)
    # Non-hop length and nonzero tail expose dropped final audio.
    t = np.arange(RATE + 173) / RATE
    x = (.06*np.sin(2*np.pi*223*t) + .015*rng.standard_normal(len(t))).astype(np.float32)
    fixed, _ = stream_render(stream, x)
    varied, _ = stream_render(stream, x, (32, 147, 512, 1, 960, 77))
    repeated, _ = stream_render(stream, x)
    extended, _ = stream_render(stream, np.pad(x, (0, RATE//10)))
    partition_error = float(np.max(np.abs(fixed-varied)))
    reset_error = float(np.max(np.abs(fixed-repeated)))
    tail_error = float(np.max(np.abs(fixed-extended[:len(x)])))
    silence, _ = stream_render(stream, np.zeros(RATE, np.float32))
    if max(partition_error, reset_error, tail_error) > 2e-6 or np.max(np.abs(silence)) > 1e-7:
        raise RuntimeError("Streaming invariance/reset/silence checks failed")
    # Very short/empty files still must retain their exact length, without NaNs.
    for n in (0, 1, 479, 480, 481):
        y, _ = stream_render(stream, x[:n])
        if len(y) != n:
            raise RuntimeError("Output length mismatch")
    return dict(partition_max_abs_error=partition_error, reset_max_abs_error=reset_error,
                extended_tail_max_abs_error=tail_error,
                silence_peak=float(np.max(np.abs(silence))), partial_tail_samples=173,
                tested_chunk_sizes=[32, 147, 512, 1, 960, 77],
                short_lengths=[0, 1, 479, 480, 481], passed=True)


def metrics(x, y):
    if len(x) != len(y) or not np.isfinite(y).all():
        raise ValueError("Outputs must have matching lengths and finite samples")
    # Waveform correlation is an alignment/level diagnostic, NOT quality.
    a, b = x[::6].astype(np.float64), y[::6].astype(np.float64)
    lag = correlation = None
    if np.std(x) > 1e-12 and np.std(y) > 1e-12:
        c = correlate(b, a, mode="full", method="fft")
        lags = correlation_lags(len(b), len(a))
        region = np.abs(lags) <= RATE//6//10
        lag = int(lags[region][np.argmax(c[region])])*6
        correlation = float(np.corrcoef(x, y)[0, 1])
    n = RATE//10
    ax = x[:len(x)//n*n].reshape(-1, n)
    by = y[:len(y)//n*n].reshape(-1, n)
    ex, ey = np.mean(ax.astype(np.float64)**2, axis=1), np.mean(by.astype(np.float64)**2, axis=1)
    def attenuation(mask):
        return float(10*np.log10((ex[mask].mean()+1e-20)/(ey[mask].mean()+1e-20)))
    return dict(samples=len(y), peak_dbfs=float(20*np.log10(np.max(np.abs(y))+1e-20)),
                samples_at_or_above_full_scale=int(np.sum(np.abs(y) >= 1)),
                rms_change_db=float(10*np.log10((np.mean(x.astype(np.float64)**2)+1e-20)/
                                               (np.mean(y.astype(np.float64)**2)+1e-20))),
                low_energy_attenuation_db=attenuation(ex <= np.quantile(ex, .2)),
                high_energy_attenuation_db=attenuation(ex >= np.quantile(ex, .6)),
                residual_lag_samples_6_sample_resolution=lag,
                waveform_correlation=correlation)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--input", type=Path, action="append", default=[])
    parser.add_argument("--rnnoise-renderer", type=Path)
    args = parser.parse_args()
    if len({p.stem.casefold() for p in args.input}) != len(args.input):
        raise ValueError("Input filenames must have different stems")
    if args.output_dir.exists():
        raise FileExistsError("Choose a new output directory; evaluation never overwrites a run")
    stream, contract = new_stream(args.model)
    report = dict(model=MANIFEST, contract=contract, environment=dict(
        platform=platform.platform(), python=platform.python_version(),
        packages={name: importlib.metadata.version(name) for name in
                  ("dpdfnet", "onnxruntime", "numpy", "scipy", "soundfile")}),
        concatenated_output_shift_samples=OUTPUT_SHIFT,
        checks=check(stream), renders=[], limitations=[
            "Offline Python reference; not a real-time VST3 backend or DAW latency measurement.",
            "No clean speech reference. Energy attenuation/correlation are not perceptual quality scores.",
            "Timing is wall time including Python FFT/buffering/ORT, not callback allocation validation.",
            "Model output shift is pinned to this model; must be validated in the future C++ adapter."])
    args.output_dir.mkdir(parents=True)
    for source in args.input:
        x, sr = sf.read(source, dtype="float32", always_2d=True)
        if x.shape[1] != 1:
            raise ValueError("Evaluation requires mono; stereo must use independent model states")
        x = x[:, 0]
        if len(x) < sr//10 or not np.isfinite(x).all():
            raise ValueError("Evaluation input must contain at least 100 ms of finite audio")
        if sr != RATE:
            divisor = math.gcd(sr, RATE)
            x = resample_poly(x, RATE//divisor, sr//divisor).astype(np.float32)
        base = args.output_dir / source.stem
        input_path = base.with_name(base.name + "-input48.wav")
        neural_path = base.with_name(base.name + "-DPDFNet.wav")
        # Float WAV keeps peaks without implicit clipping/normalisation.
        sf.write(input_path, x, RATE, subtype="FLOAT")
        y, timing = stream_render(stream, x)
        sf.write(neural_path, y, RATE, subtype="FLOAT")
        entry = dict(source_name=source.name, source_rate=sr, seconds=len(x)/RATE,
                     dpdfnet=dict(metrics=metrics(x, y), timing=timing))
        if args.rnnoise_renderer:
            dest = base.with_name(base.name + "-RNNoise.wav")
            native = subprocess.run([str(args.rnnoise_renderer.resolve()), "--render",
                str(input_path.resolve()), str(dest.resolve()), "100", "0"],
                check=True, capture_output=True, text=True)
            r, rate = sf.read(dest, dtype="float32")
            if rate != RATE:
                raise ValueError("RNNoise renderer changed rate")
            entry["rnnoise"] = dict(metrics=metrics(x, r), renderer_output=native.stdout.strip())
        report["renders"].append(entry)
        print(json.dumps(entry, indent=2), flush=True)
    (args.output_dir / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False)+"\n")
    print(f"Checks passed. Report: {args.output_dir / 'report.json'}")


if __name__ == "__main__":
    main()
