#pragma once

#include "../Core/MidiAnalyzer.h"
#include "../Core/PatternTypes.h"

namespace pam
{
/** "Import MIDI": turns a dropped/loaded MIDI file into matrix channels and runs
    the key/scale analyser on it.

    - notes are quantised to the 16th grid; up to 64 steps (4 bars) are kept
    - a file with several MIDI channels/tracks fills up to 4 matrix channels,
      otherwise the notes go to `targetChannel`
    - simultaneous notes become a stab chord (chord size), the top note is kept
    - overlapping consecutive notes become slides, loud notes become accents */
class MidiPatternImporter
{
public:
    struct Result
    {
        Pattern      pattern;
        KeyDetection key;
        int          channelsFilled = 0;
        int          notesImported = 0;
        juce::String error;
        bool ok() const noexcept { return error.isEmpty() && channelsFilled > 0; }
    };

    /** mergeIntoTarget: every note goes into targetChannel, even from multi-channel files. */
    static Result importNotes (const juce::Array<AnalyzedNote>& notes, const Pattern& base,
                               int targetChannel = 0, bool applyDetectedKey = true, bool mergeIntoTarget = false);
    static Result importFile (const juce::File& file, const Pattern& base,
                              int targetChannel = 0, bool applyDetectedKey = true, bool mergeIntoTarget = false);
};
} // namespace pam
