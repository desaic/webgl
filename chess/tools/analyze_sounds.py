# Lists the percussive hits in a recording: when they start, how loud, how long they ring, and a
# rough brightness (spectral centroid). Used to pick the chess move/capture/castle/promotion sounds.
#
#   python tools/analyze_sounds.py ignore/wood_table.wav
import subprocess
import sys

import numpy as np

FFMPEG = r"D:\Program Files (x86)\FFmpeg\ffmpeg.exe"
SR = 48000

path = sys.argv[1]
raw = subprocess.run(
    [FFMPEG, "-v", "error", "-i", path, "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"],
    capture_output=True, check=True,
).stdout
x = np.frombuffer(raw, dtype=np.float32)
print("%.2f s, peak %.1f dBFS" % (len(x) / SR, 20 * np.log10(np.abs(x).max() + 1e-12)))

# short-time energy envelope (2 ms hop), then onsets = sharp rises well above the local floor
hop = SR // 500
frames = len(x) // hop
env = np.sqrt((x[: frames * hop].reshape(frames, hop) ** 2).mean(axis=1))
db = 20 * np.log10(env + 1e-9)
floor = np.percentile(db, 20)
print("noise floor about %.1f dB" % floor)

onsets = []
last = -1e9
for i in range(5, frames - 5):
    rise = db[i] - db[max(0, i - 6): i - 1].mean()
    if rise > 12 and db[i] > floor + 18 and (i - last) * hop / SR > 0.12:
        # walk to the local maximum
        j = i
        while j + 1 < frames and db[j + 1] >= db[j]:
            j += 1
        onsets.append((i, j))
        last = i

print("%d hits" % len(onsets))
print(" #   start     peak dB  ring(-30dB)  centroid Hz  next hit in")
for n, (i, j) in enumerate(onsets):
    t0 = i * hop / SR
    peak = db[j]
    k = j
    while k + 1 < frames and db[k + 1] > peak - 30:
        k += 1
    ring = (k - j) * hop / SR
    seg = x[i * hop: i * hop + SR // 10]
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
    freqs = np.fft.rfftfreq(len(seg), 1 / SR)
    cent = (spec * freqs).sum() / (spec.sum() + 1e-9)
    nxt = (onsets[n + 1][0] - i) * hop / SR if n + 1 < len(onsets) else float("nan")
    print("%2d  %7.3f  %7.1f  %9.3f  %11.0f  %8.2f" % (n, t0, peak, ring, cent, nxt))
