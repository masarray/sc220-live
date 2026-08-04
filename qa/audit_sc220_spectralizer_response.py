import math
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
GENERATED = ROOT / "generated" / "sc220-monitor" / "SC220MainComponent_Monitor.cpp"
SOURCE = GENERATED.read_text(encoding="utf-8") if GENERATED.exists() else ""
TELEMETRY = (ROOT / "live/ProgrammeBandTelemetry.h").read_text(encoding="utf-8")

SAMPLE_RATE = 48_000.0
CENTRES = np.array([
    50.0, 80.0, 125.0, 200.0, 315.0, 500.0, 800.0,
    1250.0, 2000.0, 3150.0, 5000.0, 8000.0, 12000.0, 16000.0,
])
SAMPLES = 4096


def band_q(index: int) -> float:
    centre = float(CENTRES[index])
    lower = (
        centre / math.sqrt(float(CENTRES[1]) / centre)
        if index == 0
        else math.sqrt(float(CENTRES[index - 1]) * centre)
    )
    upper = (
        centre * math.sqrt(centre / float(CENTRES[index - 1]))
        if index + 1 == len(CENTRES)
        else math.sqrt(centre * float(CENTRES[index + 1]))
    )
    return float(np.clip(centre / max(1.0, upper - lower), 0.55, 8.0))


def coefficients(index: int) -> tuple[float, float, float, float, float]:
    frequency = min(float(CENTRES[index]), SAMPLE_RATE * 0.42)
    q = band_q(index)
    omega = 2.0 * math.pi * frequency / SAMPLE_RATE
    alpha = math.sin(omega) / (2.0 * q)
    a0 = 1.0 + alpha
    return (
        alpha / a0,
        0.0,
        -alpha / a0,
        -2.0 * math.cos(omega) / a0,
        (1.0 - alpha) / a0,
    )


def filter_channel(signal: np.ndarray, index: int) -> np.ndarray:
    b0, b1, b2, a1, a2 = coefficients(index)
    output = np.zeros_like(signal)
    x1 = x2 = y1 = y2 = 0.0
    for sample_index, sample in enumerate(signal):
        value = b0 * sample + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, float(sample)
        y2, y1 = y1, value
        output[sample_index] = value
    return output


def analyse(left: np.ndarray, right: np.ndarray) -> np.ndarray:
    result = np.zeros(len(CENTRES), dtype=np.float64)
    for index in range(len(CENTRES)):
        filtered_left = filter_channel(left, index)
        filtered_right = filter_channel(right, index)
        stereo_energy = 0.5 * (filtered_left**2 + filtered_right**2)
        result[index] = math.sqrt(float(np.mean(stereo_energy)))
    return result


def tone(frequency: float) -> np.ndarray:
    time = np.arange(SAMPLES, dtype=np.float64) / SAMPLE_RATE
    return np.sin(2.0 * np.pi * frequency * time)


checks: list[tuple[str, bool, str]] = []


def check(name: str, condition: bool, detail: str) -> None:
    checks.append((name, bool(condition), detail))


frequencies = (80.0, 800.0, 8000.0)
peak_indices: list[int] = []
for frequency in frequencies:
    signal = tone(frequency)
    bands = analyse(signal, signal)
    peak = int(np.argmax(bands))
    peak_indices.append(peak)
    check(
        f"{frequency:g} Hz tone peaks at its fixed band",
        math.isclose(float(CENTRES[peak]), frequency, rel_tol=0.01),
        f"peak index={peak}, centre={CENTRES[peak]:.0f} Hz, Q={band_q(peak):.3f}",
    )

check(
    "bass mid and treble peaks increase left to right",
    peak_indices[0] < peak_indices[1] < peak_indices[2],
    f"indices={peak_indices}",
)

anti_left = tone(1250.0)
anti_right = -anti_left
mono_sum = 0.5 * (anti_left + anti_right)
anti_bands = analyse(anti_left, anti_right)
check(
    "anti-phase test collapses in mono",
    float(np.max(np.abs(mono_sum))) < 1.0e-12,
    f"mono peak={float(np.max(np.abs(mono_sum))):.3e}",
)
check(
    "stereo-energy filters retain anti-phase programme audio",
    float(np.max(anti_bands)) > 0.5,
    f"band peak={float(np.max(anti_bands)):.3f}",
)
check(
    "anti-phase tone peaks at 1.25 kHz band",
    int(np.argmax(anti_bands)) == 7,
    f"peak index={int(np.argmax(anti_bands))}",
)
check(
    "telemetry source contains proportional bands pink calibration and rhythm envelopes",
    all(token in TELEMETRY for token in (
        "bandCount = 14", "lowerBoundary", "upperBoundary",
        "nominal / bandwidth", "calculatePinkNoiseCalibration",
        "integrationPoints = 4096", "pinkCalibrationDb",
        "50.0f, 80.0f, 125.0f", "1250.0f", "8000.0f", "16000.0f",
        "processSample", "envelopePower", "attackSeconds",
        "releaseSeconds", "envelopeSeconds", "coefficient",
    ))
    and all(token not in TELEMETRY for token in (
        "slowPower", "transientDb", "transientEmphasis",
    )),
    "sample-rate-aware pink-flat rhythmic filter-bank contract",
)
check(
    "generated UI uses calibrated lively atomic 14-band contract",
    all(token in SOURCE for token in (
        "programmeBandTelemetry.read", "segmentCount = 16",
        "peakHoldFrames[band] = 14", "PROGRAM ANALYZER",
        "PINK-FLAT BANDS", "const float displayDb = rawDb[band]",
        "expandedDb = -38.0f", "positiveRise", "pulses[band]",
    )),
    "generated rhythmic pink-flat UI contract",
)
check(
    "FFT snapshot and history implementations are absent",
    all(token not in SOURCE + TELEMETRY for token in (
        "performFrequencyOnlyForwardTransform", "ProgrammeSpectrumSnapshot",
        "programmeSpectrumSnapshot", "PROGRAM IMPACT", "PC INPUT IMPACT",
        '"OLDER"', '"NOW"', "historySize = 32",
    )),
    "atomic calibrated bands only",
)

failed = [name for name, ok, _ in checks if not ok]
print("SC220 Live synthetic proportional rhythmic filter-bank response audit")
for name, ok, detail in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}: {detail}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
