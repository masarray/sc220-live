from __future__ import annotations

from pathlib import Path
import math
import random
import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def fixed_fft(signal: np.ndarray) -> np.ndarray:
    n = int(signal.size)
    stages = int(math.log2(n))
    data = np.zeros(n, dtype=np.complex128)
    data.real = signal
    reversed_indices = []
    for index in range(n):
        value = index
        result = 0
        for _ in range(stages):
            result = (result << 1) | (value & 1)
            value >>= 1
        reversed_indices.append(result)
    for index, other in enumerate(reversed_indices):
        if other > index:
            data[index], data[other] = data[other], data[index]
    length = 2
    while length <= n:
        half = length // 2
        for block in range(0, n, length):
            for offset in range(half):
                twiddle = np.exp(-2j * np.pi * offset / length)
                even = data[block + offset]
                odd = twiddle * data[block + offset + half]
                data[block + offset] = even + odd
                data[block + offset + half] = even - odd
        length *= 2
    return data


random.seed(0x4D617341)
lines = [
    "SONKUPIK LIVE v0.8.2 FIXED FFT REFERENCE CHECK",
    "================================================",
]
passed = True
for n in (256, 512):
    signal = np.array([random.uniform(-1.0, 1.0) for _ in range(n)], dtype=np.float64)
    actual = fixed_fft(signal)
    reference = np.fft.fft(signal)
    forward_error = float(np.max(np.abs(actual - reference)))
    restored = np.fft.ifft(actual).real
    inverse_error = float(np.max(np.abs(restored - signal)))
    ok = forward_error < 1.0e-10 and inverse_error < 1.0e-12
    passed &= ok
    lines.append(f"N={n}: forward max abs error={forward_error:.3e}; inverse={inverse_error:.3e}; {'PASS' if ok else 'FAIL'}")

lines.append("")
lines.append("The C++ implementation uses float rather than this double-precision reference,")
lines.append("so production error is expected to be normal single-precision rounding.")
report = "\n".join(lines) + "\n"
(ROOT / "qa" / "fixed_fft_reference_v0.8.2.txt").write_text(report, encoding="utf-8")
print(report)
raise SystemExit(0 if passed else 1)
