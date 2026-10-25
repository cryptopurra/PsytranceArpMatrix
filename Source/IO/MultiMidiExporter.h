#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/PatternTypes.h"

namespace pam
{
/** Renders the matrix to Standard MIDI Files for Drag & Drop / Save MIDI.

    Routing rule: one MIDI file (or one track) per *currently active* channel.
    Single Arp Mode -> 1 file; all channels on -> 4 files. Each file carries the
    tempo, a 4/4 time signature, a track name ("Ch2 FM Lead") and the notes on the
    channel's own MIDI channel, so the DAW creates one ready-to-route track each. */
class MultiMidiExporter
{
public:
    struct Options
    {
        int    bars        = 0;      // 0 = pattern length rounded up to whole bars
        int    ppq         = 960;
        TimeDivision division = TimeDivision::Sixteenth;
        float  swing       = 0.0f;   // 0..1, same curve as the realtime engine
        float  gateScale   = 1.0f;
        int    octave      = 0;
        bool   lockToScale = true;
        juce::String baseName = "PsyArp";
    };

    /** Notes of one channel, timestamps in ticks (ppq = opts.ppq). */
    static juce::MidiMessageSequence buildChannelSequence (const Pattern& p, int channel, const Options& o);

    /** A complete single-channel MIDI file (tempo + time signature + name + notes). */
    static juce::MidiFile buildChannelFile (const Pattern& p, int channel, const Options& o);

    /** One type-1 file, one track per active channel (for hosts that split tracks on import). */
    static juce::MidiFile buildMultiTrackFile (const Pattern& p, const Options& o);

    /** Writes one .mid per active channel into `dir`. Returns the written files (in channel order). */
    static juce::Array<juce::File> exportActiveChannels (const Pattern& p, const Options& o, const juce::File& dir);

    /** Writes the multitrack variant. */
    static juce::File exportMultiTrack (const Pattern& p, const Options& o, const juce::File& dir);

    static juce::String trackName (const Pattern& p, int channel);
    static int totalBars (const Pattern& p, const Options& o);

private:
    static void addHeader (juce::MidiMessageSequence& seq, const Pattern& p, const Options& o,
                           const juce::String& name, bool withTempo);
    static bool writeFile (const juce::MidiFile& mf, const juce::File& f);
};
} // namespace pam
