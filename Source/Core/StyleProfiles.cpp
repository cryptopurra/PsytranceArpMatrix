#include "StyleProfiles.h"
#include "StyleProfilesData.h"

#include <algorithm>

namespace pam
{
static StyleProfile makeProfile (GeneratorStyle s)
{
    StyleProfile p;
    p.style = s;

    // the new styles reuse the statistics of the closest analysed category
    const GeneratorStyle dataStyle = s == GeneratorStyle::Arpeggio ? GeneratorStyle::FMLead
                                   : s == GeneratorStyle::Rhythmic ? GeneratorStyle::RhythmStab
                                                                   : s;
    for (auto& d : data::kProfiles)
    {
        if (d.style != dataStyle)
            continue;

        std::copy (std::begin (d.onsetProb),     std::end (d.onsetProb),     p.onsetProb.begin());
        std::copy (std::begin (d.accentProb),    std::end (d.accentProb),    p.accentProb.begin());
        std::copy (std::begin (d.intervalProb),  std::end (d.intervalProb),  p.intervalProb.begin());
        std::copy (std::begin (d.degreeWeights), std::end (d.degreeWeights), p.degreeWeights.begin());
        p.notesPerBar   = d.notesPerBar;
        p.legatoRatio   = d.legatoRatio;
        p.meanGateSteps = d.meanGateSteps;
        p.polyRatio     = d.polyRatio;
        p.rootRatio     = d.rootRatio;
        p.meanSpan      = d.meanSpan;
        p.filesAnalysed = d.filesAnalysed;
    }

    switch (s)
    {
        case GeneratorStyle::RollingBass:
            p.lowNote = 33; p.highNote = 45;
            p.kickSpace = true;
            p.gateMin = 0.55f; p.gateMax = 0.8f;
            p.baseVelocity = 100.0f; p.velocitySpread = 6.0f;
            break;

        case GeneratorStyle::Acid:
            p.lowNote = 36; p.highNote = 60;
            p.slideProb = 0.18f;
            p.octaveJumpProb = 0.22f;
            p.pitchVariety = 0.6f;
            p.repeatScale = 0.6f;
            p.intervalBlend = 0.35f;
            p.minAccentProb = 0.08f;
            p.gateMin = 0.35f; p.gateMax = 0.75f;
            p.baseVelocity = 92.0f; p.accentVelocity = 124.0f;
            break;

        case GeneratorStyle::FMLead:
            p.lowNote = 60; p.highNote = 84;
            p.offbeatBias = true;
            p.slideProb = 0.12f; // fast glissandi
            p.octaveJumpProb = 0.18f;
            p.pitchVariety = 0.35f;
            p.repeatScale = 0.4f;
            p.intervalBlend = 0.45f;
            p.minAccentProb = 0.05f;
            p.gateMin = 0.3f; p.gateMax = 0.6f;
            break;

        case GeneratorStyle::LeadSynth:
            p.lowNote = 60; p.highNote = 79;
            p.density = 0.7f;
            p.slideProb = 0.3f;
            p.sustainToNext = true;
            p.pitchVariety = 0.3f;
            p.repeatScale = 0.3f;
            p.intervalBlend = 0.6f;
            p.gateMin = 0.9f; p.gateMax = 2.0f;
            p.baseVelocity = 90.0f; p.velocitySpread = 8.0f;
            break;

        case GeneratorStyle::RhythmStab:
            p.lowNote = 55; p.highNote = 67;
            p.density = 0.45f;
            p.chordSize = 3;
            p.pitchVariety = 0.25f;
            p.repeatScale = 0.7f;
            p.intervalBlend = 0.6f;
            p.gateMin = 0.2f; p.gateMax = 0.4f;
            p.accentVelocity = 127.0f;
            for (int i : { 3, 7, 11, 15 })  // steps 4, 8, 12, 16: tension points
                p.forcedAccents[(size_t) i] = true;
            break;

        case GeneratorStyle::Arpeggio:
            p.lowNote = 57; p.highNote = 84;
            p.density = 1.15f;           // nearly every 16th
            p.minAccentProb = 0.04f;
            p.gateMin = 0.35f; p.gateMax = 0.6f;
            p.baseVelocity = 94.0f; p.velocitySpread = 8.0f;
            for (int i : { 0, 4, 8, 12 })
                p.forcedAccents[(size_t) i] = false;
            break;

        case GeneratorStyle::Rhythmic:
            p.lowNote = 48; p.highNote = 67;
            p.density = 0.8f;
            p.offbeatBias = true;
            p.pitchVariety = 0.9f;       // root-heavy: a rhythm, not a melody
            p.octaveJumpProb = 0.12f;    // occasional octave for movement
            p.repeatScale = 1.0f;
            p.intervalBlend = 0.2f;
            p.gateMin = 0.15f; p.gateMax = 0.45f;
            p.minAccentProb = 0.12f;
            p.accentVelocity = 122.0f; p.baseVelocity = 86.0f; p.velocitySpread = 14.0f;
            break;

        case GeneratorStyle::numStyles:
            break;
    }
    return p;
}

const StyleProfile& StyleProfile::get (GeneratorStyle s)
{
    static const std::array<StyleProfile, (size_t) GeneratorStyle::numStyles> profiles = []
    {
        std::array<StyleProfile, (size_t) GeneratorStyle::numStyles> a;
        for (int i = 0; i < (int) GeneratorStyle::numStyles; ++i)
            a[(size_t) i] = makeProfile ((GeneratorStyle) i);
        return a;
    }();

    const int idx = std::clamp ((int) s, 0, (int) GeneratorStyle::numStyles - 1);
    return profiles[(size_t) idx];
}
} // namespace pam
