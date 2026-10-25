#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Scales.h"

namespace pam
{
/** A note extracted from a MIDI file or live input (times in quarter notes). */
struct AnalyzedNote
{
    double start = 0.0, end = 0.25;
    int pitch = 60, velocity = 100, channel = 1;
};

struct KeyDetection
{
    int   rootNote   = 0;    // pitch class
    int   scaleIndex = 1;    // ScaleLibrary index
    float confidence = 0.0f; // fraction of weighted note mass inside the scale (0..1)
    bool  valid      = false;

    juce::String describe() const;
};

/** Key & scale detection.

    Algorithm (identical to tools/scales.py::detect_key so offline analysis and the
    plugin agree):
      1. duration- and velocity-weighted pitch-class histogram
      2. every (root, scale) pair is scored by the in-scale mass
      3. small bonuses for tonic weight, lowest note, first note and the fifth,
         a penalty for scales with more notes than the material uses, and a tiny
         tie-break toward typical psytrance modes. */
class MidiAnalyzer
{
public:
    static KeyDetection detect (const juce::Array<AnalyzedNote>& notes);
    static KeyDetection detectFromHistogram (const std::array<float, 12>& histogram,
                                             int lowestPitchClass, int firstPitchClass);

    /** Reads all note on/off pairs from every track of a MIDI file. */
    static juce::Array<AnalyzedNote> extractNotes (const juce::MidiFile& file);
    static juce::Array<AnalyzedNote> extractNotes (const juce::File& midiFile);
};

/** Real-time key tracker for incoming MIDI (keyboard input).
    Lock-free for the audio thread: it only writes atomics; the UI polls getDetection(). */
class RealtimeKeyTracker
{
public:
    void noteOn (int midiNote, int velocity) noexcept;
    void decay (float factor = 0.995f) noexcept; // call once per block
    void reset() noexcept;
    KeyDetection getDetection() const;           // message thread
    int notesSeen() const noexcept { return seen.load(); }

private:
    std::array<std::atomic<float>, 12> hist {};
    std::atomic<int> lowest { -1 }, first { -1 }, lowestNote { 128 }, seen { 0 };
};
} // namespace pam
