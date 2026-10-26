#include "PatternTypes.h"

#include <cstring>

namespace pam
{
static const char* const kStyleNames[] = { "Rolling Bass", "Acid", "FM Lead", "Lead Synth", "Stabs", "Arpeggio", "Rhythmics" };

const char* styleName (GeneratorStyle s) noexcept
{
    const auto i = (int) s;
    return (i >= 0 && i < (int) GeneratorStyle::numStyles) ? kStyleNames[i] : "Acid";
}

GeneratorStyle styleFromName (const char* name) noexcept
{
    if (name != nullptr)
        for (int i = 0; i < (int) GeneratorStyle::numStyles; ++i)
            if (std::strcmp (name, kStyleNames[i]) == 0)
                return (GeneratorStyle) i;

    if (name != nullptr && std::strcmp (name, "Rhythmics / Stabs") == 0) // presets written before v0.7
        return GeneratorStyle::RhythmStab;

    return GeneratorStyle::Acid;
}

GeneratorStyle defaultStyleForChannel (int channelIndex) noexcept
{
    switch (channelIndex)
    {
        case 0:  return GeneratorStyle::Acid;
        case 1:  return GeneratorStyle::FMLead;
        case 2:  return GeneratorStyle::LeadSynth;
        default: return GeneratorStyle::RhythmStab;
    }
}
} // namespace pam
