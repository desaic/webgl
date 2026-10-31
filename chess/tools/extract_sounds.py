# Cuts the chess sound effects out of the wood-table recording.
#
#   python tools/extract_sounds.py        (writes web/public/sounds/<name>.wav)
#
# tools/analyze_sounds.py lists every hit in the recording with its time, level and brightness;
# pick other hits by editing SOUNDS below (start/end in seconds into the source file).
import os
import subprocess
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "ignore", "wood_table.wav")  # git-ignored source recording
OUT = os.path.join(HERE, "..", "web", "public", "sounds")
FFMPEG = r"D:\Program Files (x86)\FFmpeg\ffmpeg.exe"
SR = 44100

# name: (start s, end s, peak level in dBFS after normalising)
SOUNDS = {
    "move":      (15.300, 15.520, -9.0),   # one mellow tap
    "capture":   (7.460, 7.750, -3.0),     # the loudest, sharpest hit in the file
    "castle":    (4.734, 5.300, -6.0),     # two taps 0.23 s apart: king, then rook
    "promotion": (0.222, 0.600, -6.0),     # two quick taps 0.12 s apart
}
FADE_IN = 0.002
FADE_OUT = 0.06

raw = subprocess.run(
    [FFMPEG, "-v", "error", "-i", SRC, "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"],
    capture_output=True, check=True,
).stdout
x = np.frombuffer(raw, dtype=np.float32)

os.makedirs(OUT, exist_ok=True)
for name, (t0, t1, peak_db) in SOUNDS.items():
    seg = x[int(t0 * SR): int(t1 * SR)].copy()
    seg -= seg.mean()  # remove any DC offset so the cut does not click
    n_in, n_out = int(FADE_IN * SR), int(FADE_OUT * SR)
    seg[:n_in] *= np.linspace(0, 1, n_in)
    seg[-n_out:] *= np.linspace(1, 0, n_out) ** 2
    seg *= 10 ** (peak_db / 20) / (np.abs(seg).max() + 1e-12)
    pcm = (np.clip(seg, -1, 1) * 32767).astype("<i2")
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print("%-10s %.3f-%.3f s  %.2f s  %d KB" % (name, t0, t1, len(seg) / SR, os.path.getsize(path) // 1024))
