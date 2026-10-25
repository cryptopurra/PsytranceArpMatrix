# Architettura

```
            ┌────────────── message thread ──────────────┐        ┌──── audio thread ────┐
 GUI  ───►  PluginProcessor::editMatrix()                 │        │                      │
 params ──► parameterChanged → AsyncUpdater ──►  ChannelMatrix ──► publishPattern() ─(SpinLock, copia)─► ArpEngine::process()
                                                │   ├─ PatternGenerator (per canale)            │   ├─ held keys / hold modes
                                                │   ├─ InterlockMatrix                          │   ├─ clock host o interno
                                                │   └─ originals (per "Original")               │   ├─ swing, gate, slide, accordi
 Import MIDI ─► MidiPatternImporter ─► MidiAnalyzer (tonica/scala)                               │   └─ Lock Mode, velocity mode
 Drag MIDI  ◄── MultiMidiExporter (1 file per canale attivo)                                     └──────────────────────┘
 Preset     ◄─► PresetManager ◄─► PatternSerializer (JSON)
```

## Classi principali

| Classe | Thread | Ruolo |
|---|---|---|
| `Pattern` / `ChannelPattern` / `Step` | – | Stato completo, *trivially copyable* (array fissi 4×64) così può essere copiato all'audio thread senza allocazioni. |
| `ScaleLibrary` | qualsiasi | 36 scale + Custom, quantizzazione, conversione nota ↔ grado. |
| `MidiAnalyzer` | message | Rilevamento tonica/scala da note (istogramma pesato per durata e velocity, bonus tonica/nota più grave/prima nota/quinta, penalità per scale più grandi del materiale). Identico a `tools/scales.py`. |
| `RealtimeKeyTracker` | audio → UI | Istogramma lock-free (atomics) degli input da tastiera per *Auto Match*. |
| `StyleProfile` | – | Statistiche apprese (`StyleProfilesData.h`) + regole di ruolo (registro, slide, accenti forzati, kick space, varietà). |
| `InterlockMatrix` | message | Griglia booleana [canale][step]: pesi di onset per il canale che si sta generando e `resolveCollisions()` (max 3 voci/step, priorità Rolling Bass > Acid > Stabs > FM Lead > Lead Synth, accenti ed edit manuali protetti). |
| `PatternGenerator` | message | Generazione di **un** canale: ritmo → altezze → velocity/gate/slide/accordi → forma (domanda/risposta, battute derivate). Variazioni Neighbors/Contour/Answer/Octaves. |
| `ChannelMatrix` | message | Single Arp Mode, attivazione incrementale, Generate/Roll, edit, retune su cambio scala, Lock Mode. |
| `ArpEngine` | audio | Clock (host PPQ o interno), step con swing, note-off schedulati, modi arp sulle note tenute, Key Follow, velocity mode, merge su ch1. Nessuna allocazione dopo `prepare()`. |
| `MultiMidiExporter` | message | Render del pattern in MIDI: un file per canale attivo (tempo, 4/4, nome traccia, canale MIDI proprio) o un file multitraccia type-1. |
| `MidiPatternImporter` | message | MIDI → matrice: quantizzazione a 16°, voce superiore + numero di voci = accordo, legato → slide, velocity alta → accento; file multi-canale → più righe. |
| `PresetManager` | message | Cartelle, installazione preset di fabbrica (BinaryData), salvataggio rapido con timestamp, pulizia Drag_Cache. |

## Attivazione incrementale (requisito chiave)

```
initialiseDefault()          → solo ch1 enabled, generato
setChannelEnabled(c, true)   → buildMatrix(escludi c)  // canali già attivi
                               PatternGenerator::generate(c, …, matrix)
                               resolveCollisions()       // non tocca i canali a priorità più alta
setChannelEnabled(c, false)  → il canale resta in memoria ma non suona né viene esportato
generateAll()                → rigenera tutti gli attivi in ordine di priorità
```
Un canale con modifiche manuali (`userEdited`) viene ripristinato tale e quale quando lo si riattiva.

## Drag & Drop multi-traccia

`MidiDragSource::mouseDrag` → `PluginProcessor::createDragFiles()` scrive in `Sequences/Drag_Cache/<timestamp>/`
un file per canale attivo (`<Preset>_Ch2_FM_Lead_F#_142bpm.mid`) e chiama
`DragAndDropContainer::performExternalDragDropOfFiles()`. Numero di file = `Pattern::numEnabled()`.
La cartella è unica per ogni drag perché alcune DAW referenziano il file invece di copiarlo.

## Thread safety

- Tutto ciò che modifica la matrice gira sul message thread (GUI, `AsyncUpdater` per i parametri automatizzati).
- `publishPattern()` copia il `Pattern` in `pendingPattern` sotto `SpinLock`; l'audio thread fa `ScopedTryLock` e, se non riesce, continua con la copia precedente: nessun blocco.
- `ArpEngine` usa solo array fissi e un `MidiBuffer` pre-allocato.
