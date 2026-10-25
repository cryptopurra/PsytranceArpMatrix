#pragma once

#include <juce_core/juce_core.h>

namespace pam
{
/** A musical scale as a set of semitone intervals from the root. */
struct Scale
{
    juce::String name;
    juce::Array<int> intervals; // ascending, first is always 0

    bool contains (int semitoneFromRoot) const noexcept
    {
        return intervals.contains (((semitoneFromRoot % 12) + 12) % 12);
    }
    int size() const noexcept { return intervals.size(); }
};

/** All scales offered by the plugin (same list/order as the reference arp),
    plus a user-editable "Custom" scale at the end. */
class ScaleLibrary
{
public:
    static const ScaleLibrary& get();

    int size() const noexcept                     { return scales.size(); }
    const Scale& operator[] (int index) const     { return scales.getReference (juce::jlimit (0, scales.size() - 1, index)); }
    int indexOf (const juce::String& name) const;
    juce::StringArray names() const;

    int customIndex() const noexcept              { return scales.size() - 1; }

    /** Custom scale: bit n set = semitone n is in the scale (bit 0 always forced). */
    static void setCustomMask (int mask12);
    static int  getCustomMask() noexcept;

    //==============================================================================
    /** Nearest in-scale MIDI note (ties go down). */
    static int quantize (int midiNote, int root, const Scale& scale) noexcept;

    /** MIDI note for a (possibly negative / > size) scale degree relative to a base octave note. */
    static int degreeToNote (int degree, int root, const Scale& scale, int baseOctaveNote) noexcept;

    /** Inverse of degreeToNote for in-scale notes; out-of-scale notes are quantized first. */
    static int noteToDegree (int midiNote, int root, const Scale& scale, int baseOctaveNote) noexcept;

    static juce::String noteName (int pitchClass);
    static int pitchClassFromName (const juce::String& name); // "F#", "Gb", "a" ... -1 if invalid
    /** "F#3" -> 54 (C4 = 60, same naming as the UI). Without an octave the octave of
        `fallbackNote` is kept. Returns -1 if the text is not a note. */
    static int noteFromName (const juce::String& text, int fallbackNote);

private:
    ScaleLibrary();
    juce::Array<Scale> scales;
    static std::atomic<int> customMask;
};
} // namespace pam
