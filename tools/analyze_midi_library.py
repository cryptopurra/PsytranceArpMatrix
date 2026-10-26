#!/usr/bin/env python3
"""Analyse a local psytrance MIDI library and produce *aggregate* style profiles.

Only statistics are extracted (step onset probabilities, accent maps, interval and
scale-degree histograms, gate lengths...). No note sequence from the library is copied
into the output, so the generated profile header can be committed to the repository
without redistributing anybody's MIDI files.

Usage:
    python3 -I tools/analyze_midi_library.py <library_dir> \
        --json build/style_profiles.json --header Source/Core/StyleProfilesData.h

Folders whose licence forbids use in generative software can be skipped with
--exclude (case-insensitive substring match on the path).
"""
from __future__ import annotations

import argparse
import json
import os
import statistics
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from midilib import read_midi  # noqa: E402
from scales import SCALES, detect_key  # noqa: E402

DEFAULT_EXCLUDE = [
    "freak audio",     # licence forbids use in generative applications
    "tunecraft",       # licence forbids use as part of another product
    "/chords/", "/scales/", "midi hits",   # reference material, not patterns
    "penta arp", "midi modificati",
]

# Ordered: first match wins.
CATEGORY_RULES = [
    ("acid",  ["acid"]),
    ("bass",  ["bass", "deepsy", "rolling", "groove"]),
    ("rhythmic", ["rhythmic", "perc", "step and hold"]),
    ("stab",  ["stab", "hit"]),
    ("lead",  ["lead", "melod", "riff", "goa", "stepsy", "melody", "cyber flows"]),
    ("arp",   ["arp", "seq", "arpsy", "freaky", "triom", "spectrosense", "outerverse",
               "dimensions", "serum", "function", "dm7", "dimension", "edm"]),
]

CATEGORIES = ["bass", "acid", "arp", "lead", "stab"]


def categorise(path: str) -> str | None:
    p = path.lower()
    for cat, keys in CATEGORY_RULES:
        if any(k in p for k in keys):
            return cat
    return None


def analyse_file(path: str):
    md = read_midi(path)
    if len(md.notes) < 4:
        return None
    step_ticks = md.ppq / 4.0
    notes = md.notes
    last_step = int(round(max(n.start for n in notes) / step_ticks))
    bars = max(1, last_step // 16 + 1)
    root, si, fit = detect_key(notes)
    ivs = SCALES[si][1]

    onsets = [0] * 16
    accents = [0] * 16
    by_start = defaultdict(list)
    for n in notes:
        s = int(round(n.start / step_ticks))
        by_start[s].append(n)
    vels = [n.velocity for n in notes]
    med = statistics.median(vels)
    vel_spread = (max(vels) - min(vels)) if vels else 0
    for s, group in by_start.items():
        onsets[s % 16] += 1
        if vel_spread >= 12 and max(g.velocity for g in group) >= med + 10:
            accents[s % 16] += 1

    # monophonic top line for melodic stats
    starts = sorted(by_start)
    line = [max(by_start[s], key=lambda n: n.pitch) for s in starts]
    intervals = Counter()
    legato = 0
    for a, b in zip(line, line[1:]):
        d = max(-12, min(12, b.pitch - a.pitch))
        intervals[d] += 1
        if a.end > b.start + step_ticks * 0.1:
            legato += 1
    gates = [min(4.0, (n.end - n.start) / step_ticks) for n in notes]
    poly = sum(1 for s in starts if len({n.pitch for n in by_start[s]}) >= 2)

    degree = Counter()
    for n in notes:
        rel = (n.pitch - root) % 12
        degree[ivs.index(rel) if rel in ivs else -1] += 1

    lowest = min(n.pitch for n in notes)
    return {
        "bars": bars,
        "onsets": onsets,
        "accents": accents,
        "events": len(starts),
        "intervals": intervals,
        "legato": legato,
        "pairs": max(1, len(line) - 1),
        "gate": statistics.mean(gates),
        "poly": poly,
        "degree": degree,
        "scale": SCALES[si][0],
        "fit": fit,
        "root_ratio": degree.get(0, 0) / len(notes),
        "span": max(n.pitch for n in notes) - lowest,
    }


def aggregate(results):
    out = {}
    for cat in CATEGORIES:
        rs = results.get(cat, [])
        if not rs:
            continue
        bars = sum(r["bars"] for r in rs)
        # average of per-file probabilities, so one very long file cannot dominate
        onset = [statistics.mean(min(1.0, r["onsets"][i] / r["bars"]) for r in rs) for i in range(16)]
        acc = [sum(r["accents"][i] for r in rs) / max(1e-9, sum(r["onsets"][i] for r in rs))
               for i in range(16)]
        iv = Counter()
        for r in rs:
            iv.update(r["intervals"])
        tot_iv = sum(iv.values()) or 1
        deg = Counter()
        for r in rs:
            deg.update(r["degree"])
        tot_deg = sum(deg.values()) or 1
        out[cat] = {
            "files": len(rs),
            "onsetProb": [round(min(1.0, x), 3) for x in onset],
            "accentProb": [round(x, 3) for x in acc],
            "notesPerBar": round(statistics.median(r["events"] / r["bars"] for r in rs), 2),
            "intervalHist": {str(k): round(iv.get(k, 0) / tot_iv, 4) for k in range(-12, 13)},
            "legatoRatio": round(sum(r["legato"] for r in rs) / sum(r["pairs"] for r in rs), 3),
            "meanGateSteps": round(statistics.mean(r["gate"] for r in rs), 3),
            "polyRatio": round(sum(r["poly"] for r in rs) / max(1, sum(r["events"] for r in rs)), 3),
            "degreeWeights": [round(deg.get(i, 0) / tot_deg, 4) for i in range(7)],
            "outOfScale": round(deg.get(-1, 0) / tot_deg, 4),
            "rootRatio": round(statistics.mean(r["root_ratio"] for r in rs), 3),
            "meanSpan": round(statistics.mean(r["span"] for r in rs), 2),
            "scales": dict(Counter(r["scale"] for r in rs).most_common(8)),
        }
    return out


def write_header(profiles, path):
    role_of = {"bass": "RollingBass", "acid": "Acid", "arp": "FMLead",
               "lead": "LeadSynth", "stab": "RhythmStab"}

    def arr(xs):
        return "{ " + ", ".join(f"{x:.3f}f" for x in xs) + " }"

    lines = [
        "// AUTO-GENERATED by tools/analyze_midi_library.py — do not edit by hand.",
        "// Aggregate statistics only: no note sequences from the analysed library are stored.",
        "#pragma once",
        "#include \"PatternTypes.h\"",
        "",
        "namespace pam::data",
        "{",
        "struct StyleProfileData",
        "{",
        "    GeneratorStyle style;",
        "    float onsetProb[16];",
        "    float accentProb[16];",
        "    float intervalProb[25];  // index 0 == -12 semitones ... 24 == +12",
        "    float degreeWeights[7];",
        "    float notesPerBar, legatoRatio, meanGateSteps, polyRatio, rootRatio, meanSpan;",
        "    int filesAnalysed;",
        "};",
        "",
        f"inline constexpr StyleProfileData kProfiles[] = {{",
    ]
    for cat in CATEGORIES:
        if cat not in profiles:
            continue
        p = profiles[cat]
        ivs = [p["intervalHist"][str(k)] for k in range(-12, 13)]
        lines.append(f"    {{ GeneratorStyle::{role_of[cat]},")
        lines.append(f"      {arr(p['onsetProb'])},")
        lines.append(f"      {arr(p['accentProb'])},")
        lines.append(f"      {arr(ivs)},")
        lines.append(f"      {arr(p['degreeWeights'])},")
        lines.append(f"      {p['notesPerBar']:.3f}f, {p['legatoRatio']:.3f}f, {p['meanGateSteps']:.3f}f, "
                     f"{p['polyRatio']:.3f}f, {p['rootRatio']:.3f}f, {p['meanSpan']:.3f}f, {p['files']} }},")
    lines += ["};", "} // namespace pam::data", ""]
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("library")
    ap.add_argument("--json", default=None)
    ap.add_argument("--header", default=None)
    ap.add_argument("--exclude", action="append", default=None,
                    help="path substring to skip (repeatable); replaces the default list")
    args = ap.parse_args()
    excludes = [e.lower() for e in (args.exclude or DEFAULT_EXCLUDE)]

    results = defaultdict(list)
    skipped = Counter()
    for dirpath, _dirs, files in os.walk(args.library):
        for f in files:
            if not f.lower().endswith((".mid", ".midi")):
                continue
            full = os.path.join(dirpath, f)
            low = full.lower().replace("\\", "/")
            if any(e in low for e in excludes):
                skipped["excluded"] += 1
                continue
            cat = categorise(low)
            if cat == "rhythmic":
                cat = "stab"  # one statistical profile for stabs and rhythmics
            if cat is None:
                skipped["uncategorised"] += 1
                continue
            try:
                r = analyse_file(full)
            except Exception:  # corrupt / exotic files
                skipped["unreadable"] += 1
                continue
            if r:
                results[cat].append(r)
    profiles = aggregate(results)
    summary = {c: profiles[c]["files"] for c in profiles}
    print("analysed:", summary, "skipped:", dict(skipped), file=sys.stderr)
    if args.json:
        os.makedirs(os.path.dirname(os.path.abspath(args.json)), exist_ok=True)
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(profiles, fh, indent=2)
    if args.header:
        write_header(profiles, args.header)


if __name__ == "__main__":
    main()
