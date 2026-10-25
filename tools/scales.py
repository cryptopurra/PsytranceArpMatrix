"""Scale table shared by the offline tools. Mirrors Source/Core/Scales.cpp."""

SCALES = [
    ("Major / Ionian",            [0, 2, 4, 5, 7, 9, 11]),
    ("Natural Minor / Aeolian",   [0, 2, 3, 5, 7, 8, 10]),
    ("Harmonic Minor",            [0, 2, 3, 5, 7, 8, 11]),
    ("Melodic Minor",             [0, 2, 3, 5, 7, 9, 11]),
    ("Dorian",                    [0, 2, 3, 5, 7, 9, 10]),
    ("Phrygian",                  [0, 1, 3, 5, 7, 8, 10]),
    ("Lydian",                    [0, 2, 4, 6, 7, 9, 11]),
    ("Mixolydian",                [0, 2, 4, 5, 7, 9, 10]),
    ("Locrian",                   [0, 1, 3, 5, 6, 8, 10]),
    ("Major Pentatonic",          [0, 2, 4, 7, 9]),
    ("Minor Pentatonic",          [0, 3, 5, 7, 10]),
    ("Minor Blues",               [0, 3, 5, 6, 7, 10]),
    ("Major Blues",               [0, 2, 3, 4, 7, 9]),
    ("Whole Tone",                [0, 2, 4, 6, 8, 10]),
    ("Diminished Whole-Half",     [0, 2, 3, 5, 6, 8, 9, 11]),
    ("Diminished Half-Whole",     [0, 1, 3, 4, 6, 7, 9, 10]),
    ("Chromatic",                 list(range(12))),
    ("Phrygian Dominant",         [0, 1, 4, 5, 7, 8, 10]),
    ("Lydian Dominant",           [0, 2, 4, 6, 7, 9, 10]),
    ("Double Harmonic Major",     [0, 1, 4, 5, 7, 8, 11]),
    ("Hungarian Minor",           [0, 2, 3, 6, 7, 8, 11]),
    ("Persian",                   [0, 1, 4, 5, 6, 8, 11]),
    ("Spanish Eight Tone",        [0, 1, 3, 4, 5, 6, 8, 10]),
    ("In Sen",                    [0, 1, 5, 7, 10]),
    ("Hirajoshi",                 [0, 2, 3, 7, 8]),
    ("Iwato",                     [0, 1, 5, 6, 10]),
    ("Egyptian Pentatonic",       [0, 2, 5, 7, 10]),
    ("Neapolitan Minor",          [0, 1, 3, 5, 7, 8, 11]),
    ("Neapolitan Major",          [0, 1, 3, 5, 7, 9, 11]),
    ("Enigmatic",                 [0, 1, 4, 6, 8, 10, 11]),
    ("Altered / Super Locrian",   [0, 1, 3, 4, 6, 8, 10]),
    ("Prometheus",                [0, 2, 4, 6, 9, 10]),
    ("Major Bebop",               [0, 2, 4, 5, 7, 8, 9, 11]),
    ("Dominant Bebop",            [0, 2, 4, 5, 7, 9, 10, 11]),
    ("Minor Bebop",               [0, 2, 3, 4, 5, 7, 9, 10]),
    ("Lydian Augmented",          [0, 2, 4, 6, 8, 9, 11]),
]

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# Scales the detector prefers on ties (typical psytrance modes).
PSY_PRIORITY = ["Natural Minor / Aeolian", "Phrygian", "Harmonic Minor", "Phrygian Dominant",
                "Hungarian Minor", "Dorian", "Minor Pentatonic", "Double Harmonic Major"]


def scale_index(name: str) -> int:
    return [s[0] for s in SCALES].index(name)


def detect_key(notes, candidates=None):
    """Return (root, scale_index, confidence) using a weighted pitch-class histogram.

    Mirrors MidiAnalyzer::detect in C++: duration-weighted histogram, extra weight on the
    lowest and the first note (likely tonic), penalty for out-of-scale mass and for
    scales larger than needed.
    """
    if not notes:
        return 0, 1, 0.0
    hist = [0.0] * 12
    for n in notes:
        hist[n.pitch % 12] += max(1, n.end - n.start) ** 0.5 * (n.velocity / 100.0 + 0.5)
    lowest = min(notes, key=lambda n: n.pitch).pitch % 12
    first = notes[0].pitch % 12
    total = sum(hist) or 1.0
    used = sum(1 for h in hist if h > 0)
    cand = candidates if candidates is not None else range(len(SCALES))
    best = None
    for si in cand:
        name, ivs = SCALES[si]
        if name == "Chromatic":
            continue
        for root in range(12):
            pcs = {(root + i) % 12 for i in ivs}
            inside = sum(hist[p] for p in pcs) / total
            score = inside
            score -= 0.015 * max(0, len(ivs) - used)          # prefer the smallest fitting scale
            score += 0.06 * (hist[root] / total)               # tonic emphasis
            score += 0.04 if root == lowest else 0.0
            score += 0.02 if root == first else 0.0
            score += 0.03 * (hist[(root + 7) % 12] / total) if 7 in ivs else 0.0
            if name in PSY_PRIORITY:
                score += 0.004 * (len(PSY_PRIORITY) - PSY_PRIORITY.index(name))
            if best is None or score > best[0]:
                best = (score, root, si, inside)
    return best[1], best[2], best[3]
