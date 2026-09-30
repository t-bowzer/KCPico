"""Synthesized audio preview (optional ``preview`` extra).

Generates simple drum/tone waveforms with numpy and plays them with
sounddevice, so rhythm and bass patterns can be auditioned without any external
MIDI device. Both dependencies are optional: importing this module fails
gracefully, and the UI falls back to a silent animated cursor.
"""

from __future__ import annotations

from typing import Optional

try:
    import numpy as _np
except ImportError:  # pragma: no cover
    _np = None

try:
    import sounddevice as _sd
except ImportError:  # pragma: no cover
    _sd = None

SAMPLE_RATE = 44100


def available() -> bool:
    return _np is not None and _sd is not None


# --- waveform helpers ------------------------------------------------------

def _envelope(n: int, attack: int, release: int, sample_rate: int = SAMPLE_RATE):
    if _np is None:
        return None
    env = _np.ones(n, dtype=_np.float32)
    if attack > 0:
        env[:attack] = _np.linspace(0.0, 1.0, attack)
    if release > 0:
        env[-release:] = _np.linspace(1.0, 0.0, release)
    return env


def _kick(duration: float, sample_rate: int = SAMPLE_RATE):
    n = int(duration * sample_rate)
    t = _np.arange(n) / sample_rate
    freq = _np.linspace(150.0, 40.0, n)
    wave = _np.sin(2.0 * _np.pi * _np.cumsum(freq) / sample_rate)
    return (wave * _envelope(n, 5, n // 4, sample_rate)).astype(_np.float32)


def _snare(duration: float, sample_rate: int = SAMPLE_RATE):
    n = int(duration * sample_rate)
    noise = _np.random.default_rng(0).uniform(-1.0, 1.0, n).astype(_np.float32)
    tone = _np.sin(2.0 * _np.pi * 180.0 * _np.arange(n) / sample_rate)
    return (0.6 * noise + 0.4 * tone) * _envelope(n, 5, n // 2, sample_rate)


def _hihat(duration: float, sample_rate: int = SAMPLE_RATE):
    n = int(duration * sample_rate)
    rng = _np.random.default_rng(1)
    noise = rng.uniform(-1.0, 1.0, n).astype(_np.float32)
    return noise * _envelope(n, 2, n // 3, sample_rate)


def _tone(midi_note: int, duration: float, sample_rate: int = SAMPLE_RATE):
    n = int(duration * sample_rate)
    freq = 440.0 * (2.0 ** ((midi_note - 69) / 12.0))
    t = _np.arange(n) / sample_rate
    wave = _np.sin(2.0 * _np.pi * freq * t)
    return (wave * _envelope(n, 10, n // 4, sample_rate)).astype(_np.float32)


_DRUM_GENERATORS = {
    36: _kick,   # kick
    38: _snare,  # snare
    42: _hihat,  # hihat
    46: _hihat,  # open hat
    37: _snare,  # rimshot
    39: _snare,  # clap
    49: _hihat,  # crash
    51: _hihat,  # ride
    61: _tone,   # bongo (pitched)
    62: _tone,   # conga lo
    63: _tone,   # conga hi
    75: _hihat,  # clave
    82: _hihat,  # shaker
}


def _step_duration_seconds(tempo_bpm: float) -> float:
    return 60.0 / (tempo_bpm * 4.0)  # 4 steps per beat


def synthesize_rhythm_bar(rhythm, tempo_bpm: float):
    """Render one bar of a RhythmPattern to a float32 numpy array."""
    if _np is None:
        return None
    steps = max(rhythm.steps_per_bar, 1)
    step_s = _step_duration_seconds(tempo_bpm)
    # A little extra tail so the last hit's release isn't clipped.
    bar = _np.zeros(int(steps * step_s * SAMPLE_RATE) + SAMPLE_RATE // 8,
                    dtype=_np.float32)
    for step in range(steps):
        start = int(step * step_s * SAMPLE_RATE)
        for track in rhythm.tracks:
            if step >= len(track.pattern) or track.pattern[step] <= 0:
                continue
            velocity = track.pattern[step]
            if velocity == 1:
                velocity = 100
            gen = _DRUM_GENERATORS.get(track.note, _hihat)
            if gen is _tone:
                hit = gen(track.note, step_s * 2.0)
            else:
                hit = gen(step_s * 2.0)
            amp = 0.3 * (velocity / 127.0)
            end = min(start + len(hit), len(bar))
            if end > start:
                bar[start:end] += hit[: end - start] * amp
    return bar


def synthesize_bass_bar(bass, tempo_bpm: float, root_midi: int = 48):
    """Render one bar of a BassPattern to a float32 numpy array.

    Chord-degree codes are resolved against a simple major-triad blueprint
    (root, major 3rd, perfect 5th, major 6th), which is enough to audition the
    pattern's rhythm and contour.
    """
    if _np is None:
        return None
    blueprint = {0: 0, 1: 4, 2: 7, 3: 9}  # degree -> semitone offset
    steps = max(bass.steps_per_bar, 1)
    step_s = _step_duration_seconds(tempo_bpm)
    bar = _np.zeros(int(steps * step_s * SAMPLE_RATE) + SAMPLE_RATE // 8,
                    dtype=_np.float32)
    for step in range(steps):
        degree = bass.steps[step] if step < len(bass.steps) else -1
        if degree < 0:
            continue
        sustain = bass.sustain_steps[step] if step < len(bass.sustain_steps) else 0
        dur_steps = max(sustain, 1) if sustain > 0 else 1
        start = int(step * step_s * SAMPLE_RATE)
        midi = root_midi + blueprint.get(degree, 0)
        hit = _tone(midi, step_s * dur_steps)
        end = min(start + len(hit), len(bar))
        if end > start:
            bar[start:end] += hit[: end - start] * 0.5
    return bar


def play(array, block: bool = True) -> bool:
    """Play a synthesized buffer. Returns False if playback is unavailable."""
    if _sd is None or array is None:
        return False
    try:
        _sd.play(array, SAMPLE_RATE)
        if block:
            _sd.wait()
        return True
    except Exception:
        return False


def stop() -> None:
    if _sd is not None:
        try:
            _sd.stop()
        except Exception:
            pass
