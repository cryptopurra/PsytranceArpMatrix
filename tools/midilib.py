"""Minimal, dependency-free Standard MIDI File reader/writer (stdlib only).

Used by the offline tools in this folder. It is NOT used by the plugin itself
(the plugin uses juce::MidiFile).
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import List


@dataclass
class Note:
    start: int      # ticks
    end: int        # ticks
    pitch: int
    velocity: int
    channel: int


@dataclass
class MidiData:
    ppq: int
    notes: List[Note] = field(default_factory=list)
    tempo_bpm: float = 0.0


def _read_varlen(data: bytes, pos: int):
    value = 0
    while True:
        b = data[pos]
        pos += 1
        value = (value << 7) | (b & 0x7F)
        if not b & 0x80:
            return value, pos


def read_midi(path: str) -> MidiData:
    with open(path, "rb") as fh:
        data = fh.read()
    if data[:4] != b"MThd":
        raise ValueError("not a MIDI file")
    hdr_len = struct.unpack(">I", data[4:8])[0]
    _fmt, ntracks, division = struct.unpack(">HHH", data[8:14])
    if division & 0x8000:
        raise ValueError("SMPTE time division not supported")
    out = MidiData(ppq=division)
    pos = 8 + hdr_len
    for _ in range(ntracks):
        if pos + 8 > len(data):
            break
        if data[pos:pos + 4] != b"MTrk":
            # skip unknown chunk
            ln = struct.unpack(">I", data[pos + 4:pos + 8])[0]
            pos += 8 + ln
            continue
        ln = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        trk = data[pos + 8:pos + 8 + ln]
        pos += 8 + ln
        t = 0
        p = 0
        running = 0
        active = {}
        while p < len(trk):
            delta, p = _read_varlen(trk, p)
            t += delta
            status = trk[p]
            if status & 0x80:
                p += 1
                if status < 0xF0:
                    running = status
            else:
                status = running
            if status == 0xFF:
                mtype = trk[p]
                mlen, p = _read_varlen(trk, p + 1)
                if mtype == 0x51 and mlen == 3 and out.tempo_bpm == 0.0:
                    us = (trk[p] << 16) | (trk[p + 1] << 8) | trk[p + 2]
                    if us:
                        out.tempo_bpm = 60_000_000 / us
                p += mlen
                if mtype == 0x2F:
                    break
                continue
            if status in (0xF0, 0xF7):
                slen, p = _read_varlen(trk, p)
                p += slen
                continue
            kind = status & 0xF0
            ch = status & 0x0F
            if kind in (0xC0, 0xD0):
                p += 1
                continue
            d1, d2 = trk[p], trk[p + 1]
            p += 2
            if kind == 0x90 and d2 > 0:
                active.setdefault((ch, d1), []).append((t, d2))
            elif kind == 0x80 or (kind == 0x90 and d2 == 0):
                stack = active.get((ch, d1))
                if stack:
                    st, vel = stack.pop(0)
                    out.notes.append(Note(st, max(t, st + 1), d1, vel, ch))
        for (ch, pitch), stack in active.items():
            for st, vel in stack:
                out.notes.append(Note(st, st + out.ppq // 4, pitch, vel, ch))
    out.notes.sort(key=lambda n: (n.start, n.pitch))
    return out


def _varlen(value: int) -> bytes:
    buf = [value & 0x7F]
    value >>= 7
    while value:
        buf.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(buf))


def write_midi(path: str, tracks: List[List[Note]], ppq: int = 96, bpm: float = 142.0,
               names: List[str] | None = None) -> None:
    """Write a type-1 MIDI file with one track per list of notes."""
    chunks = []
    for i, notes in enumerate(tracks):
        events = []
        if i == 0:
            us = int(60_000_000 / bpm)
            events.append((0, 0, b"\xFF\x51\x03" + us.to_bytes(3, "big")))
        if names and i < len(names):
            nm = names[i].encode("ascii", "replace")
            events.append((0, 0, b"\xFF\x03" + _varlen(len(nm)) + nm))
        for n in notes:
            events.append((n.start, 2, bytes([0x90 | n.channel, n.pitch, n.velocity])))
            events.append((n.end, 1, bytes([0x80 | n.channel, n.pitch, 0])))
        events.sort(key=lambda e: (e[0], e[1]))
        body = b""
        last = 0
        for t, _, ev in events:
            body += _varlen(t - last) + ev
            last = t
        body += b"\x00\xFF\x2F\x00"
        chunks.append(b"MTrk" + struct.pack(">I", len(body)) + body)
    hdr = b"MThd" + struct.pack(">IHHH", 6, 1, len(chunks), ppq)
    with open(path, "wb") as fh:
        fh.write(hdr + b"".join(chunks))
