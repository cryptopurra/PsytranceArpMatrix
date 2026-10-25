// Builds the factory preset library (Resources/Presets/**.json) with the plugin's own
// generator, so every factory pattern is original material produced by the same
// algorithm the user gets in the plugin.
//
//   PamPresetBuilder <outputDir>
//
#include <juce_core/juce_core.h>
#include <iostream>
#include <vector>
#include "../Source/Core/ChannelMatrix.h"
#include "../Source/IO/PresetManager.h"

using namespace pam;

namespace
{
struct ChannelSpec
{
    GeneratorStyle style;
    int octave = 0;
};

struct PresetSpec
{
    const char* category;
    const char* name;
    const char* description;
    const char* key;
    const char* scale;
    double bpm;
    int length;
    float density;
    std::vector<ChannelSpec> channels; // enabled channels, in matrix order
    juce::int64 seed;
    float swing = 0.0f;
};

/** Generates a few candidates and keeps the one whose density is closest to the style's
    learned notes-per-bar figure (avoids degenerate rolls in the factory set). */
Pattern build (const PresetSpec& spec)
{
    Pattern best;
    double bestScore = 1.0e9;

    for (int attempt = 0; attempt < 12; ++attempt)
    {
        ChannelMatrix m;
        m.setSeed (spec.seed * 101 + attempt);
        m.setKey (ScaleLibrary::pitchClassFromName (spec.key), ScaleLibrary::get().indexOf (spec.scale), false);
        m.setLockToScale (true);
        m.setDensity (spec.density);
        m.setLength (spec.length);

        for (int c = 0; c < kNumChannels; ++c)
            m.setChannelStyle (c, c < (int) spec.channels.size() ? spec.channels[(size_t) c].style
                                                                 : defaultStyleForChannel (c), false);
        for (int c = 0; c < (int) spec.channels.size(); ++c)
            m.setChannelOctave (c, spec.channels[(size_t) c].octave);

        m.initialiseDefault();                          // Single Arp Mode: channel 1
        for (int c = 1; c < (int) spec.channels.size(); ++c)
            m.setChannelEnabled (c, true);              // incremental activation

        auto p = m.getPattern();
        p.bpm = spec.bpm;

        double score = 0.0;
        for (int c = 0; c < (int) spec.channels.size(); ++c)
        {
            const auto& ch = p.channels[(size_t) c];
            const double perBar = ch.countActive() * 16.0 / ch.length;
            const double target = StyleProfile::get (ch.style).notesPerBar
                                  * StyleProfile::get (ch.style).density * spec.density;
            score += std::abs (perBar - target);
        }

        if (score < bestScore)
        {
            bestScore = score;
            best = p;
        }
    }
    return best;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PamPresetBuilder <outputDir>\n";
        return 1;
    }

    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    using S = GeneratorStyle;

    const std::vector<PresetSpec> specs = {
        // ---- Bassline_Rolling ---------------------------------------------------
        { "Bassline_Rolling", "FullOn_Roll_01", "Full-on rolling bass, kick space on every beat", "F#", "Phrygian", 145, 16, 1.0f, { { S::RollingBass } }, 11 },
        { "Bassline_Rolling", "Prog_Bass_Sub", "Progressive sub bass, slightly sparser roll", "D", "Natural Minor / Aeolian", 138, 16, 0.85f, { { S::RollingBass } }, 12 },
        { "Bassline_Rolling", "FullOn_Roll_02_Hungarian", "Rolling bass with fifth/octave pickups", "E", "Hungarian Minor", 145, 32, 1.0f, { { S::RollingBass } }, 13 },
        { "Bassline_Rolling", "Twilight_Roll_PhrygDom", "Dark twilight roll", "G", "Phrygian Dominant", 148, 16, 1.0f, { { S::RollingBass } }, 14 },
        { "Bassline_Rolling", "Roll_Plus_Acid_Matrix", "Bass + acid answer", "F", "Phrygian", 143, 32, 1.0f, { { S::RollingBass }, { S::Acid, 1 } }, 15 },

        // ---- Acid_Grooves -------------------------------------------------------
        { "Acid_Grooves", "Resonant_Acid_142", "303 line with accents on 6-13-14 and slides", "F#", "Phrygian", 142, 16, 1.0f, { { S::Acid } }, 21 },
        { "Acid_Grooves", "Squelch_Pattern_A", "Busy squelchy acid, octave jumps", "A", "Phrygian Dominant", 145, 32, 1.1f, { { S::Acid } }, 22 },
        { "Acid_Grooves", "Acid_InSen_Hypno", "Japanese In Sen acid loop", "C#", "In Sen", 140, 16, 0.9f, { { S::Acid } }, 23 },
        { "Acid_Grooves", "Acid_Harmonic_Drive", "Harmonic minor drive", "G#", "Harmonic Minor", 146, 32, 1.0f, { { S::Acid } }, 24 },
        { "Acid_Grooves", "Acid_vs_FM_Call_Response", "Acid question, FM lead answer", "F#", "Phrygian", 144, 32, 1.0f, { { S::Acid }, { S::FMLead } }, 25 },

        // ---- Melodic_Leads ------------------------------------------------------
        { "Melodic_Leads", "Hypnotic_FM_Lead", "Fast off-beat FM lead", "E", "Phrygian", 142, 16, 1.0f, { { S::FMLead } }, 31 },
        { "Melodic_Leads", "Epic_Phrygian_Arp", "FM arp with a legato lead answering in the gaps", "F#", "Phrygian Dominant", 145, 32, 1.0f, { { S::FMLead }, { S::LeadSynth } }, 32 },
        { "Melodic_Leads", "Twilight_Lead_Hungarian", "Evolving twilight lead", "D", "Hungarian Minor", 148, 64, 1.0f, { { S::LeadSynth } }, 33 },
        { "Melodic_Leads", "Prog_Dorian_Hypno", "Progressive dorian hypnotic arp", "A", "Dorian", 138, 32, 0.9f, { { S::FMLead } }, 34 },
        { "Melodic_Leads", "Goa_Double_Harmonic", "Goa-style double harmonic lead", "E", "Double Harmonic Major", 146, 32, 1.0f, { { S::LeadSynth } }, 35 },

        // ---- Stabs_Rhythmics ----------------------------------------------------
        { "Stabs_Rhythmics", "Offbeat_Stabs_01", "Chord stabs on tension points 4-8-12-16", "F#", "Natural Minor / Aeolian", 142, 16, 1.0f, { { S::RhythmStab } }, 41 },
        { "Stabs_Rhythmics", "Perc_Stab_Matrix", "Full 4-channel matrix: acid, FM lead, lead, stabs", "F#", "Phrygian", 143, 32, 1.0f, { { S::Acid }, { S::FMLead }, { S::LeadSynth }, { S::RhythmStab } }, 42 },
        { "Stabs_Rhythmics", "Stab_Pump_Minor_Penta", "Pentatonic stabs", "G", "Minor Pentatonic", 140, 16, 1.1f, { { S::RhythmStab } }, 43 },
        { "Stabs_Rhythmics", "Bass_Stab_Groove", "Rolling bass with stab punctuation", "D", "Phrygian", 145, 16, 1.0f, { { S::RollingBass }, { S::RhythmStab } }, 44 },
        { "Stabs_Rhythmics", "FullOn_Matrix_4ch", "Bass, acid, FM lead and stabs", "A", "Harmonic Minor", 146, 32, 1.0f, { { S::RollingBass }, { S::Acid, 1 }, { S::FMLead }, { S::RhythmStab } }, 45 },
    };

    PresetManager pm (out);
    pm.ensureFolderStructure();
    int written = 0;

    for (auto& spec : specs)
    {
        auto pattern = build (spec);
        PresetMeta meta;
        meta.name = spec.name;
        meta.description = spec.description;
        meta.swing = spec.swing;
        meta.author = "Psytrance Arp Matrix (factory)";
        auto f = pm.savePreset (pattern, meta, spec.category);
        if (f.existsAsFile())
        {
            ++written;
            std::cout << f.getRelativePathFrom (out) << "  ch=" << pattern.numEnabled() << "\n";
        }
    }

    // keep the empty export folder in git / zip
    pm.getUserExportDir().getChildFile (".gitkeep").replaceWithText ("");
    pm.getDragCacheDir().deleteRecursively();
    std::cout << written << " presets written to " << out.getFullPathName() << "\n";
    return written == (int) specs.size() ? 0 : 2;
}
