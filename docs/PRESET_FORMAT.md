# Formato preset / pattern (`.json`)

```json
{
  "format": "PsytranceArpMatrix",
  "version": 1,
  "name": "Resonant_Acid_142",
  "category": "Acid_Grooves",
  "author": "Psytrance Arp Matrix (factory)",
  "description": "303 line with accents on 6-13-14 and slides",
  "bpm": 142,
  "key": "F#",
  "rootNote": 6,
  "scale": "Phrygian",
  "length": 16,
  "timeDivision": "1/16",
  "swing": 0.0,
  "gate": 1.0,
  "lock": true,
  "arpMode": "Step",
  "velocityMode": "Step",
  "channels": [
    {
      "index": 1,
      "enabled": true,
      "style": "Acid",
      "midiChannel": 1,
      "length": 16,
      "steps": [
        { "note": 42, "velocity": 124, "gate": 0.52, "slide": false, "accent": true, "active": true, "chord": 1 }
      ]
    }
  ]
}
```

| Campo | Valori |
|---|---|
| `key` / `rootNote` | nome nota (`"F#"`, `"Gb"`) e/o pitch class 0-11; `rootNote` ha la precedenza |
| `scale` | uno dei nomi di `ScaleLibrary` (es. `"Phrygian Dominant"`, `"In Sen"`) |
| `style` | `Rolling Bass`, `Acid`, `FM Lead`, `Lead Synth`, `Rhythmics / Stabs` |
| `channels` | sempre 4 elementi; `enabled` = stato dello switch di attivazione |
| `steps` | `length` elementi (16 per una battuta, max 64). Se ce ne sono meno, vengono ripetuti in loop |
| `note` | numero di nota MIDI assoluto (0-127) |
| `velocity` | 1-127 |
| `gate` | frazione di step (0.05 – 4.0) |
| `slide` | legato verso la nota successiva (la nota dura 1.08 step) |
| `accent` | nota accentata (velocity alta) |
| `chord` | 1 = nota singola, 2-4 = terze sovrapposte nella scala (stab) |
| `timeDivision` | `1/4`, `1/8`, `1/8T`, `1/16`, `1/16T`, `1/32` |
| `swing`, `gate` | 0-1 e 0.05-2 (moltiplicatore globale) |

I campi mancanti prendono i valori di default, quindi si possono scrivere preset a mano con solo `scale`, `key` e `channels`.
