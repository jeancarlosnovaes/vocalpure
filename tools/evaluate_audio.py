"""Offline WAV checks; never uploads audio. Python 3 + NumPy.

extract: writes a 90 s source excerpt (unmodified PCM).
compare: checks alignment/levels for already latency-compensated rendered WAVs.
Energy-selected quiet windows are NOT ground-truth noise; no quality score is claimed.
"""
import argparse
import json
import wave
from pathlib import Path
import numpy as np

def load(path):
    with wave.open(str(path), 'rb') as f:
        rate, channels, width = f.getframerate(), f.getnchannels(), f.getsampwidth()
        raw = f.readframes(f.getnframes())
    if width == 2:
        data = np.frombuffer(raw, '<i2').astype(np.float64) / 32768
    elif width == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        x = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        data = ((x ^ 0x800000) - 0x800000).astype(np.float64) / 8388608
    else:
        raise ValueError('Expected PCM16 or PCM24 WAV')
    return data.reshape(-1, channels), rate

def extract(source, output, seconds):
    output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(source), 'rb') as f:
        metadata = dict(channels=f.getnchannels(), rate=f.getframerate(), frames=f.getnframes(),
                        duration_seconds=f.getnframes()/f.getframerate(), sample_width=f.getsampwidth())
        params = f.getparams()
        raw = f.readframes(int(seconds*f.getframerate()))
    if output.exists():
        raise FileExistsError(output)
    with wave.open(str(output), 'wb') as f:
        f.setparams(params)
        f.writeframes(raw)
    print(json.dumps(metadata, indent=2))

def compare(source, rendered):
    x, rate = load(source)
    y, out_rate = load(rendered)
    if rate != out_rate or x.shape != y.shape:
        raise ValueError('Rates or lengths differ; comparison must be time-aligned')
    n = int(rate*0.1)
    count = len(x)//n
    a = x[:count*n].reshape(count, -1)
    b = y[:count*n].reshape(count, -1)
    ex, ey = np.mean(a*a, axis=1), np.mean(b*b, axis=1)
    quiet = (ex <= np.quantile(ex, 0.2)) & (ex > 1e-12)
    active = ex >= np.quantile(ex, 0.6)
    def attenuation(mask):
        return float(10*np.log10((ex[mask].mean()+1e-20)/(ey[mask].mean()+1e-20))) if mask.any() else None
    return dict(rate=rate, channels=x.shape[1], samples=len(x), duration_seconds=len(x)/rate,
                output_peak_dbfs=float(20*np.log10(np.max(np.abs(y))+1e-20)),
                clipped_samples=int(np.sum(np.abs(y)>=0.99999)),
                overall_level_change_db=float(10*np.log10((np.mean(x*x)+1e-20)/(np.mean(y*y)+1e-20))),
                low_energy_windows_attenuation_db=attenuation(quiet),
                high_energy_windows_attenuation_db=attenuation(active),
                waveform_correlation=float(np.corrcoef(x.ravel(), y.ravel())[0,1]),
                note='Energy windows are not labelled speech/noise. No clean reference: SI-SDR/STOI improvement not claimed.')

def add_noise(source, output):
    """Controlled additional noise; source is only a reference, not assumed clean."""
    x, rate = load(source)
    x = x[:rate*30]
    rng = np.random.default_rng(2026)
    white = rng.standard_normal(x.shape)
    frequencies = np.fft.rfftfreq(len(x), 1/rate)
    spectrum = np.fft.rfft(white, axis=0)
    spectrum /= np.sqrt(1 + frequencies[:, None]/200)
    noise = np.fft.irfft(spectrum, n=len(x), axis=0)
    noise *= np.sqrt(np.mean(x*x)/(10*np.mean(noise*noise)))  # 10 dB added-noise SNR.
    mixed = x + noise
    if np.max(np.abs(mixed)) >= 1:
        raise ValueError('Mixture would clip; use a quieter source')
    if output.exists():
        raise FileExistsError(output)
    with wave.open(str(output), 'wb') as f:
        f.setnchannels(x.shape[1]); f.setsampwidth(2); f.setframerate(rate)
        f.writeframes(np.round(mixed*32767).astype('<i2').tobytes())
    print(json.dumps(dict(seed=2026, seconds=30, added_noise_snr_db=10,
                         note='Artificial additional coloured noise; original recording is not assumed clean.'), indent=2))

def inspect(path):
    peak = 0.0
    clipped = 0
    total = 0
    square_sum = 0.0
    with wave.open(str(path), 'rb') as f:
        rate, channels, width, frames = f.getframerate(), f.getnchannels(), f.getsampwidth(), f.getnframes()
        while raw := f.readframes(rate):
            if width == 2:
                x = np.frombuffer(raw, '<i2').astype(np.float64)/32768
            elif width == 3:
                b = np.frombuffer(raw, np.uint8).reshape(-1,3).astype(np.int32)
                v = b[:,0] | (b[:,1]<<8) | (b[:,2]<<16)
                x = ((v^0x800000)-0x800000).astype(np.float64)/8388608
            else:
                raise ValueError('Expected PCM16/24')
            peak = max(peak, float(np.max(np.abs(x))))
            clipped += int(np.sum(np.abs(x)>=0.99999))
            square_sum += float(np.sum(x*x))
            total += len(x)
    return dict(rate=rate, channels=channels, frames=frames, duration_seconds=frames/rate,
                peak_dbfs=20*np.log10(peak+1e-20), clipped_samples=clipped,
                rms_dbfs=10*np.log10(square_sum/max(total,1)+1e-20))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['extract', 'compare', 'add-noise', 'inspect'])
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path, nargs='?')
    parser.add_argument('--seconds', type=float, default=90)
    args = parser.parse_args()
    if args.mode == 'inspect':
        print(json.dumps(inspect(args.source), indent=2))
    elif args.output is None:
        parser.error('output is required for this mode')
    elif args.mode == 'extract':
        extract(args.source, args.output, args.seconds)
    elif args.mode == 'add-noise':
        add_noise(args.source, args.output)
    else:
        print(json.dumps(compare(args.source, args.output), indent=2))
