#!/usr/bin/env python3
"""Convert YOUR OWN MIDI library into private Psytrance Arp Matrix presets.

The presets are written to your local preset folder (default:
~/Documents/PsytranceArpMatrix/Presets/Library/<Category>/<Pack>/<name>.json)
and are meant for personal use only: never commit them or the MIDI files to a
public repository, sample-pack licences usually forbid redistribution.

Usage:
    python3 -I tools/import_midi_library.py <library_dir> [--out <Presets dir>] [--zip out.zip]

Every file is quantised to 16ths (up to 64 steps = 4 bars), its key/scale is
detected, and the style is chosen from the folder/file name (bass, acid, arp,
lead, stab). Multi-channel MIDI files fill up to 4 matrix channels.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import zipfile
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analyze_midi_library import categorise  # noqa: E402
from midilib import read_midi  # noqa: E402
from scales import NOTE_NAMES, SCALES, detect_key  # noqa: E402

CATEGORY_FOLDER = {
    "bass": "Bassline_Rolling",
    "acid": "Acid_Grooves",
    "arp": "Melodic_Arps",
    "lead": "Melodic_Leads",
    "stab": "Stabs_Rhythmics",
}
STYLE = {
    "bass": "Rolling Bass",
    "acid": "Acid",
    "arp": "FM Lead",
    "lead": "Lead Synth",
    "stab": "Rhythmics / Stabs",
}
DEFAULT_STYLES = ["Acid", "FM Lead", "Lead Synth", "Rhythmics / Stabs"]
MAX_STEPS = 64


def sanitise(name: str) -> str:
    name = re.sub(r"[^A-Za-z0-9#\-_. ]+", "", name).strip().replace(" ", "_")
    name = re.sub(r"_+", "_", name)
    return name[:60] or "Untitled"


def empty_step():
    return {"note": 60, "velocity": 100, "gate": 0.5, "slide": False,
            "accent": False, "active": False, "chord": 1}


def convert(path: str, category: str):
    md = read_midi(path)
    notes = md.notes
    if not notes:
        return None
    step_ticks = md.ppq / 4.0
    first_bar = int(min(n.start for n in notes) // (md.ppq * 4)) * md.ppq * 4

    root, si, fit = detect_key(notes)
    by_channel = defaultdict(list)
    for n in notes:
        by_channel[n.channel].append(n)

    channels = []
    groups = list(by_channel.values())[:4]
    last_step = 0
    for g in groups:
        vels = sorted(n.velocity for n in g)
        med = vels[len(vels) // 2]
        dynamic = vels[-1] - vels[0] >= 12
        steps = [empty_step() for _ in range(MAX_STEPS)]
        voices = [0] * MAX_STEPS
        for n in g:
            s = int(round((n.start - first_bar) / step_ticks))
            if s < 0 or s >= MAX_STEPS:
                continue
            voices[s] += 1
            st = steps[s]
            if st["active"] and n.pitch < st["note"]:
                continue  # keep the top voice
            st.update(note=n.pitch, velocity=max(1, min(127, n.velocity)),
                      gate=round(max(0.05, min(4.0, (n.end - n.start) / step_ticks)), 3),
                      accent=bool(dynamic and n.velocity >= med + 10), active=True)
            last_step = max(last_step, s)
        for s in range(MAX_STEPS):
            steps[s]["chord"] = max(1, min(4, voices[s]))
            if steps[s]["active"] and steps[s]["gate"] > 1.0 and s + 1 < MAX_STEPS and steps[s + 1]["active"]:
                steps[s]["slide"] = True
        channels.append(steps)

    length = min(MAX_STEPS, max(16, (last_step // 16 + 1) * 16))
    out_channels = []
    for c in range(4):
        if c < len(channels):
            style = STYLE[category] if c == 0 else DEFAULT_STYLES[c]
            out_channels.append({"index": c + 1, "enabled": True, "style": style,
                                 "midiChannel": c + 1, "length": length,
                                 "steps": channels[c][:length]})
        else:
            out_channels.append({"index": c + 1, "enabled": False, "style": DEFAULT_STYLES[c],
                                 "midiChannel": c + 1, "length": length,
                                 "steps": [empty_step() for _ in range(16)]})

    bpm = round(md.tempo_bpm, 2) if 100 <= md.tempo_bpm <= 200 else 142.0
    return {
        "format": "PsytranceArpMatrix", "version": 1,
        "name": os.path.splitext(os.path.basename(path))[0].strip(),
        "category": CATEGORY_FOLDER[category],
        "author": "Imported from local library (personal use)",
        "description": f"Detected {NOTE_NAMES[root]} {SCALES[si][0]} ({round(fit * 100)}% in scale)",
        "bpm": bpm, "key": NOTE_NAMES[root], "rootNote": root, "scale": SCALES[si][0],
        "length": length, "timeDivision": "1/16", "swing": 0.0, "gate": 1.0,
        "lock": False,  # keep the original notes exactly as in the MIDI file
        "arpMode": "Step", "velocityMode": "Step",
        "channels": out_channels,
    }


def pack_name(library: str, path: str) -> str:
    rel = os.path.relpath(os.path.dirname(path), library).replace("\\", "/")
    parts = [p for p in rel.split("/") if p and p != "."]
    # deepest meaningful folder, prefixed by its parent when the name is generic
    name = parts[-1] if parts else "Library"
    if len(parts) >= 2 and len(name) < 20:
        name = parts[-2] + "_" + name
    return sanitise(name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("library")
    ap.add_argument("--out", default=os.path.join(os.path.expanduser("~"), "Documents",
                                                  "PsytranceArpMatrix", "Presets"))
    ap.add_argument("--zip", default=None, help="also write a zip (paths relative to the Presets folder)")
    ap.add_argument("--exclude", action="append", default=[], help="path substring to skip (repeatable)")
    args = ap.parse_args()

    library = os.path.abspath(args.library)
    written, skipped = defaultdict(int), defaultdict(int)
    zf = zipfile.ZipFile(args.zip, "w", zipfile.ZIP_DEFLATED) if args.zip else None
    used = set()

    for dirpath, _dirs, files in os.walk(library):
        for f in sorted(files):
            if not f.lower().endswith((".mid", ".midi")):
                continue
            full = os.path.join(dirpath, f)
            low = full.lower().replace("\\", "/")
            if any(e.lower() in low for e in args.exclude):
                skipped["excluded"] += 1
                continue
            cat = categorise(low)
            if cat is None and not any(k in low for k in ("/scales/", "/chords/")):
                cat = "lead"  # melodic material without a recognisable folder name
            if cat is None:
                skipped["uncategorised"] += 1
                continue
            try:
                preset = convert(full, cat)
            except Exception:
                skipped["unreadable"] += 1
                continue
            if preset is None:
                skipped["empty"] += 1
                continue

            rel_dir = os.path.join("Library", CATEGORY_FOLDER[cat], pack_name(library, full))
            base = sanitise(preset["name"])
            rel = os.path.join(rel_dir, base + ".json")
            i = 2
            while rel in used:
                rel = os.path.join(rel_dir, f"{base}_{i}.json")
                i += 1
            used.add(rel)

            text = json.dumps(preset, indent=1)
            if zf:
                zf.writestr(rel.replace("\\", "/"), text)
            else:
                dest = os.path.join(args.out, rel)
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                with open(dest, "w", encoding="utf-8", newline="\n") as fh:
                    fh.write(text)
            written[CATEGORY_FOLDER[cat]] += 1

    if zf:
        zf.close()
    print("written:", dict(written), "total", sum(written.values()))
    print("skipped:", dict(skipped))


if __name__ == "__main__":
    main()
