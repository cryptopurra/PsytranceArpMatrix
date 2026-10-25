#pragma once

#include <juce_core/juce_core.h>
#include "../Core/PatternTypes.h"

namespace pam
{
/** Optional performance settings stored next to the pattern in a preset. */
struct PresetMeta
{
    juce::String name, category, author = "Psytrance Arp Matrix", description;
    juce::String timeDivision = "1/16";
    float swing = 0.0f;        // 0..1
    float gateScale = 1.0f;    // 0.05..2
    bool  lockToScale = true;
    juce::String arpMode = "Step";
    juce::String velocityMode = "Step";
};

/** Pattern <-> JSON.

    {
      "format": "PsytranceArpMatrix", "version": 1,
      "name": "...", "category": "Acid_Grooves", "author": "...",
      "bpm": 142, "key": "F#", "rootNote": 6, "scale": "Phrygian",
      "length": 16, "timeDivision": "1/16", "swing": 0.0, "gate": 1.0, "lock": true,
      "channels": [
        { "index": 1, "enabled": true, "style": "Acid", "midiChannel": 1, "length": 16,
          "steps": [ { "note": 42, "velocity": 100, "gate": 0.5, "slide": false,
                       "accent": true, "active": true, "chord": 1 }, ... ] },
        ... 4 channels
      ]
    }

    `steps` holds `length` entries (16 for a one-bar pattern, up to 64). */
class PatternSerializer
{
public:
    static juce::var toVar (const Pattern& p, const PresetMeta& meta = {});
    static juce::String toJson (const Pattern& p, const PresetMeta& meta = {});

    /** Returns false (and leaves `p` untouched) if the JSON is not a valid pattern. */
    static bool fromVar (const juce::var& v, Pattern& p, PresetMeta* meta = nullptr);
    static bool fromJson (const juce::String& json, Pattern& p, PresetMeta* meta = nullptr);

    static juce::String timeDivisionName (TimeDivision d);
    static TimeDivision timeDivisionFromName (const juce::String& s);
    static juce::StringArray arpModeNames();
    static juce::StringArray velocityModeNames();
    static juce::StringArray variationStyleNames();
    static juce::StringArray holdModeNames();
    static juce::StringArray timeDivisionNames();
    static juce::StringArray styleNames();
};
} // namespace pam
