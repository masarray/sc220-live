import math
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
GENERATED = ROOT / "generated" / "sc220-monitor" / "SC220MainComponent_Monitor.cpp"
SOURCE = GENERATED.read_text(encoding="utf-8") if GENERATED.exists() else ""

SAMPLE_RATE = 48_000.0
FFT_SIZE = 512
BAND_COUNT = 24
LOW_FREQUENCY = 35.0
HIGH_FREQUENCY = 16_000.0


def analyse(left: np.ndarray, right: np.ndarray) -> np.ndarray:
    window = np.hanning(FFT_SIZE)
    left_magnitude = np.abs(np.fft.rfft(left * window))
    right_magnitude = np.abs(np.fft.rfft(right * window))
    result = np.zeros(BAND_COUNT, dtype=np.float64)

    for band in range(BAND_COUNT):
        t0 = band / BAND_COUNT
        t1 = (band + 1) / BAND_COUNT
        frequency0 = LOW_FREQUENCY * (HIGH_FREQUENCY / LOW_FREQUENCY) ** t0
        frequency1 = LOW_FREQUENCY * (HIGH_FREQUENCY / LOW_FREQUENCY) ** t1
        bin0 = int(np.clip(round(frequency0 * FFT_SIZE / SAMPLE_RATE), 1, FFT_SIZE // 2 - 1))
        bin1 = int(np.clip(round(frequency1 * FFT_SIZE / SAMPLE_RATE), bin0 + 1, FFT_SIZE // 2))
        stereo_power = 0.5 * (
            left_magnitude[bin0:bin1] ** 2 + right_magnitude[bin0:bin1] ** 2
        )
        result[band] = math.sqrt(float(np.mean(stereo_power))) if stereo_power.size else 0.0

    return result


def band_centre(index: int) -> float:
    t = (index + 0.5) / BAND_COUNT
    return LOW_FREQUENCY * (HIGH_FREQUENCY / LOW_FREQUENCY) ** t


def tone(frequency: float, phase: float = 0.0) -> np.ndarray:
    time = np.arange(FFT_SIZE, dtype=np.float64) / SAMPLE_RATE
    return np.sin(2.0 * np.pi * frequency * time + phase)


checks: list[tuple[str, bool, str]] = []


def check(name: str, condition: bool, detail: str) -> None:
    checks.append((name, bool(condition), detail))


frequencies = (187.5, 1_500.0, 7_500.0)
peak_indices: list[int] = []
for frequency in frequencies:
    signal = tone(frequency)
    bands = analyse(signal, signal)
    peak = int(np.argmax(bands))
    peak_indices.append(peak)
    centre = band_centre(peak)
    ratio = max(centre, frequency) / min(centre, frequency)
    check(
        f"{frequency:g} Hz tone maps near its logarithmic band",
        ratio < 1.9,
        f"peak band={peak}, centre={centre:.1f} Hz, ratio={ratio:.2f}",
    )

check(
    "bass, mid, and treble peaks increase from left to right",
    peak_indices[0] < peak_indices[1] < peak_indices[2],
    f"indices={peak_indices}",
)

anti_left = tone(1_500.0)
anti_right = -anti_left
mono_sum = 0.5 * (anti_left + anti_right)
anti_bands = analyse(anti_left, anti_right)
check(
    "anti-phase stereo collapses in mono as the test intends",
    float(np.max(np.abs(mono_sum))) < 1.0e-12,
    f"mono peak={float(np.max(np.abs(mono_sum))):.3e}",
)
check(
    "stereo-energy spectralizer still detects anti-phase audio",
    float(np.max(anti_bands)) > 10.0,
    f"stereo spectral peak={float(np.max(anti_bands)):.3f}",
)
check(
    "anti-phase tone lands in the same band as in-phase tone",
    int(np.argmax(anti_bands)) == peak_indices[1],
    f"anti-phase band={int(np.argmax(anti_bands))}, reference={peak_indices[1]}",
)

check(
    "generated UI uses the audited 512-point 24-band contract",
    all(token in SOURCE for token in (
        "fftOrder = 9",
        "bandCount = 24",
        "stereoPower += 0.5f",
        "segmentCount = 15",
        "peakHoldFrames[band] = 15",
        "PROGRAM SPECTRUM",
    )),
    "generated source contract",
)
check(
    "time-history direction labels are absent",
    all(token not in SOURCE for token in (
        "PROGRAM IMPACT",
        "PC INPUT IMPACT",
        '"OLDER"',
        '"NOW"',
        "historySize = 32",
    )),
    "fixed frequency positions only",
)

failed = [name for name, ok, _ in checks if not ok]
print("SC220 Live synthetic fixed-frequency spectralizer audit")
for name, ok, detail in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}: {detail}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
