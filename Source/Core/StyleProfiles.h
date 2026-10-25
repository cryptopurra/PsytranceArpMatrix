#pragma once

#include "PatternTypes.h"

namespace pam
{
/** Everything the generator needs to know about one style.

    The statistical part (onset / accent probabilities per 16th, interval and
    scale-degree distributions, gate, legato...) comes from StyleProfilesData.h,
    produced by tools/analyze_midi_library.py from a psytrance MIDI library.
    The musical rules (register, slide/accent policy, kick space...) are
    hand-tuned on top of it. */
struct StyleProfile
{
    GeneratorStyle style {};

    // --- learned statistics -----------------------------------------------
    std::array<float, 16> onsetProb {};
    std::array<float, 16> accentProb {};
    std::array<float, 25> intervalProb {}; // -12 .. +12 semitones
    std::array<float, 7>  degreeWeights {};
    float notesPerBar = 12.0f, legatoRatio = 0.05f, meanGateSteps = 0.9f;
    float polyRatio = 0.0f, rootRatio = 0.6f, meanSpan = 12.0f;
    int   filesAnalysed = 0;

    // --- rules ------------------------------------------------------------
    int   lowNote = 48, highNote = 72;  // generation register (before octave shift)
    float density = 1.0f;               // multiplier on onsetProb
    float slideProb = 0.0f;
    float octaveJumpProb = 0.0f;
    float minAccentProb = 0.0f;
    float accentVelocity = 120.0f, baseVelocity = 96.0f, velocitySpread = 10.0f;
    float gateMin = 0.3f, gateMax = 0.9f;
    bool  kickSpace = false;            // keep steps 1-5-9-13 free for the kick
    bool  offbeatBias = false;          // favour odd 16ths (counterpoint)
    bool  sustainToNext = false;        // extend notes up to the next onset (legato phrases)
    int   chordSize = 1;                // stacked thirds for stabs
    float pitchVariety = 1.0f;          // exponent on degree weights (<1 flattens the tonic dominance)
    float repeatScale = 1.0f;           // multiplier on the "same note again" interval probability
    float intervalBlend = 0.0f;         // 0 = learned intervals only, 1 = melodic prior only
    std::array<bool, 16> forcedAccents {};

    static const StyleProfile& get (GeneratorStyle s);
};
} // namespace pam
