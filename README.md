# Psytrance Arp Matrix

Arpeggiatore / sequencer MIDI multi-canale (VST3 · AU · Standalone) per Psytrance Full-On, Progressive e Twilight.
Scritto in C++17 con [JUCE 8](https://juce.com).

- **Single Arp Mode di default**: all'apertura (o con *Reset*) viene generato un solo pattern sul Canale 1.
- **Attivazione incrementale**: accendendo un altro canale viene generata *solo quella traccia*, complementare a quelle già attive (Interlocking Matrix + Call & Response). I canali già presenti non vengono toccati.
- **Matrice a 4 canali × 16 step (fino a 64)**: Ch1 Acid · Ch2 FM Lead · Ch3 Lead Synth · Ch4 Rhythmics/Stabs (lo stile di ogni canale è cambiabile, c'è anche *Rolling Bass*).
- **Analisi MIDI**: tonica + scala da file MIDI importati o dalla tastiera (Auto Match), con **Lock Mode** che forza tutto nella scala.
- **36 scale + Custom** (Ionian … Lydian Augmented, In Sen, Hirajoshi, Phrygian Dominant, Hungarian Minor, Persian, Bebop…).
- **Modi arp**: Up, Down, Up/Down, As Played, Random, Chord, Melody, Epic Motif, Step · **Velocity mode**: Key, Hold, Step, Step+Key, Step+Hold · **Variazioni**: Neighbors, Contour, Answer, Octaves · Hold: Off/Latch/Clean Hold · Time/Sync 1/4 … 1/32 (anche terzine), Swing, Gate, Octave, Oct Range.
- **Drag & Drop Multi-MIDI**: trascina l'icona *DRAG MIDI* sulla timeline → **un file MIDI per ogni canale attivo** (1 in Single Arp Mode, fino a 4), ciascuno sul proprio canale MIDI e con nome traccia. Opzione *1 FILE* per un unico MIDI multitraccia.
- **Preset JSON** organizzati per categoria + salvataggio con un clic in `Sequences/User_Exported`.

## Struttura su disco

Al primo avvio il plugin crea (in `Documenti/`) e popola con i preset di fabbrica:

```
PsytranceArpMatrix/
├── Presets/
│   ├── Bassline_Rolling/   FullOn_Roll_01.json, Prog_Bass_Sub.json, …
│   ├── Acid_Grooves/       Resonant_Acid_142.json, Squelch_Pattern_A.json, …
│   ├── Melodic_Leads/      Hypnotic_FM_Lead.json, Epic_Phrygian_Arp.json, …
│   └── Stabs_Rhythmics/    Offbeat_Stabs_01.json, Perc_Stab_Matrix.json, …
└── Sequences/
    ├── User_Exported/      ← "SAVE" (JSON) e "SAVE MIDI" (sottocartella MIDI/)
    └── Drag_Cache/         ← file creati per il drag & drop (pulizia automatica dopo 24h)
```

La stessa struttura è in [`Resources/PsytranceArpMatrix`](Resources/PsytranceArpMatrix) e viene pubblicata come ZIP (`PsytranceArpMatrix_Presets.zip`) a ogni release. Il formato JSON è descritto in [docs/PRESET_FORMAT.md](docs/PRESET_FORMAT.md).

## Compilare

Requisiti: CMake ≥ 3.22, un compilatore C++17 (Visual Studio 2022, Xcode 14+, GCC 11+/Clang 14+). JUCE 8.0.15 viene scaricato automaticamente.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure   # unit test headless
```

Su Linux servono: `libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev`.

Gli eseguibili finiscono in `build/PsytranceArpMatrix_artefacts/Release/{VST3,AU,Standalone}`. Copia il `.vst3` in
`C:\Program Files\Common Files\VST3` (Windows) o `~/Library/Audio/Plug-Ins/VST3` (macOS).

Opzioni CMake utili: `-DPAM_JUCE_PATH=/percorso/JUCE` (JUCE locale), `-DPAM_PLUGIN_FORMATS=VST3`, `-DPAM_AU_MIDI_FX=ON` (AU come MIDI FX per Logic).

La CI di GitHub Actions ([.github/workflows/build.yml](.github/workflows/build.yml)) compila Windows/macOS (universal)/Linux, esegue i test e [pluginval](https://github.com/Tracktion/pluginval) e, su un tag `v*`, crea una Release con i binari e lo ZIP dei preset.

## Uso nella DAW

Il plugin è uno strumento che **emette MIDI** (audio silenzioso). Ogni canale della matrice esce sul proprio canale MIDI (1-4); con *MERGE CH1* tutto esce sul canale 1.

| DAW | Come instradare il MIDI |
|---|---|
| Cubase | Traccia strumento con Psytrance Arp Matrix → nelle tracce MIDI dei synth imposta *Input = Psytrance Arp Matrix* e filtra il canale 1-4 |
| Ableton Live | Traccia MIDI con il plugin → sulle tracce dei synth *MIDI From = traccia del plugin / Psytrance Arp Matrix*, canale 1-4 |
| FL Studio | Wrapper → *MIDI output port*, synth sullo stesso port; un canale per synth |
| Logic | Compila con `-DPAM_AU_MIDI_FX=ON` e usa lo slot MIDI FX (un solo synth), oppure il drag & drop |

Senza tasti premuti il pattern segue il transport (*FREE RUN*). Tenendo premuti dei tasti: con *KEY FOLLOW* la sequenza viene trasposta, e il Canale 1 diventa un arpeggiatore sulle note tenute secondo il *MODE* (Up, Down, Chord, Melody, Epic Motif…), usando il ritmo/gate/velocity del pattern. In Standalone i tasti avviano un clock interno al BPM del preset.

**Griglia**: clic = step on/off · trascina su/giù = nota (per gradi di scala) · Shift+trascina = velocity · Alt+clic = accento · tasto destro = accent, slide, accordo (stab 2-4 note), gate, ottava, clear.
Trascinando un `.mid` sopra una riga, il file viene analizzato (tonica/scala) e caricato in quel canale; un MIDI multi-canale riempie più righe.

## Come funziona la generazione

Dettagli in [docs/ARCHITETTURA.md](docs/ARCHITETTURA.md). In breve:

1. **Ritmo** – per ogni 16° si estrae con probabilità `onsetProb[step]` dello stile (statistiche apprese), modificata da regole di ruolo (spazio per la cassa nel rolling bass, off-beat per l'FM lead, punti di tensione 4-8-12-16 per gli stab) e dai pesi della **Rhythmic Interlocking Matrix** (i canali melodici riempiono i vuoti degli altri; al massimo 3 voci per step, il canale a priorità più bassa viene silenziato).
2. **Altezze** – *constraint satisfaction* sulle note in scala del registro dello stile: `peso = grado(scala) × probabilità(intervallo)`, con enfasi su tonica, terza e quinta giusta.
3. **Forma** – la battuta 1 è la *domanda*; le battute successive sono variazioni/risposte (la risposta chiude sulla tonica, la domanda resta aperta sulla quinta).

### Statistiche dagli MIDI di riferimento

`Source/Core/StyleProfilesData.h` è generato da `tools/analyze_midi_library.py` (solo Python standard) a partire da una libreria MIDI locale. Il file contiene **solo statistiche aggregate** (probabilità per step, accenti, istogrammi di intervalli e gradi): nessuna sequenza dei pacchetti viene copiata nel repository e **i file MIDI non vanno committati** (licenze dei sample pack). Le cartelle con licenze che vietano l'uso in software generativi sono escluse per default (`--exclude`).

```bash
python3 -I tools/analyze_midi_library.py /percorso/libreria_midi \
    --json build/style_profiles.json --header Source/Core/StyleProfilesData.h
```

I preset di fabbrica sono generati dall'algoritmo stesso del plugin (`PamPresetBuilder Resources/PsytranceArpMatrix`), quindi sono materiale originale.

### La tua libreria MIDI come preset privati

Per avere i tuoi loop (sample pack acquistati inclusi) come preset caricabili nel plugin, convertili **in locale**:

```bash
python3 -I tools/import_midi_library.py "C:/percorso/libreria_midi"
# -> Documenti/PsytranceArpMatrix/Presets/Library/<Categoria>/<Pack>/<nome>.json
```

Ogni file viene quantizzato a 16° (max 64 step), ne viene rilevata tonica/scala, lo stile è scelto dal nome della cartella (bass/acid/arp/lead/stab) e i MIDI multi-canale riempiono più canali della matrice. Il Lock è disattivato in questi preset, così le note restano identiche all'originale. Nel menu PRESETS compaiono come `Library ▸ Categoria ▸ Pack`. Sono solo per uso personale: non vanno committati né condivisi (licenze dei pack). Singoli file si possono anche trascinare nel plugin e salvare con *SAVE AS*.

## Struttura del progetto

```
Source/
  Core/       PatternTypes, Scales, MidiAnalyzer, StyleProfiles(+Data), InterlockMatrix,
              PatternGenerator, ChannelMatrix, ArpEngine        ← nessuna dipendenza GUI
  IO/         PatternSerializer (JSON), PresetManager, MultiMidiExporter, MidiPatternImporter
  Plugin/     PluginProcessor, PluginEditor, FactoryPresets
  UI/         StepGridComponent, ChannelStripComponent, MidiDragSource
Tests/        CoreTests.cpp (juce::UnitTest, headless)
tools/        PresetBuilder.cpp, analyze_midi_library.py, midilib.py, scales.py
Resources/    PsytranceArpMatrix/ (preset di fabbrica, incorporati nel binario)
```

## Licenza

Codice sorgente: MIT (vedi [LICENSE](LICENSE)). I binari includono JUCE: vanno distribuiti sotto AGPLv3 oppure con una licenza JUCE (anche Personal/Starter gratuita).
