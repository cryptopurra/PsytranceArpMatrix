#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "PatternTypes.h"
#include "Scales.h"

namespace pam
{
struct ArpParams
{
    ArpMode       mode           = ArpMode::Step;
    VelocityMode  velocityMode   = VelocityMode::Step;
    HoldMode      holdMode       = HoldMode::Off;
    TimeDivision  division       = TimeDivision::Sixteenth;
    float swing          = 0.0f;  // 0..1 (delays every second step by up to 1/3 step)
    float gateScale      = 1.0f;  // global gate length multiplier ("% of time")
    int   octave         = 0;     // -3..+3
    int   octaveRange    = 1;     // 1..4 (arp modes)
    int   holdVelocity   = 100;   // velocity knob
    bool  lockToScale    = true;
    bool  keyFollow      = false; // held key transposes the sequencer channels (root key = no transposition)
    bool  playWithoutKeys = true; // pattern runs with transport even without held keys
    bool  mergeToChannel1 = false;// all channels out on MIDI ch 1 (single-instrument hosts)
    float internalBpm    = 142.0f;
    std::array<int, kNumChannels> channelTranspose {}; // per-channel pitch (semitones)
};

/** Real-time engine: turns the pattern + held keys + host position into MIDI.
    Audio-thread only, allocation free after prepare(). */
class ArpEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    struct Position
    {
        bool   hostPlaying = false;
        double ppq = 0.0;        // position at block start (quarters)
        double bpm = 142.0;
        bool   hasHostPosition = false;
        bool   allowInternalClock = true; // false inside a DAW: never play while the transport is stopped
        bool   isLooping = false;         // host cycle / loop markers active
        double loopStart = 0.0, loopEnd = 0.0;
    };

    /** Consumes the incoming MIDI (keys) and replaces it with the generated MIDI. */
    void process (juce::MidiBuffer& midi, int numSamples, const Position& pos,
                  const Pattern& pattern, const ArpParams& params);

    /** Sends note-offs for everything sounding + All Notes Off (bypass, panic, transport stop). */
    void stopAll (juce::MidiBuffer& out, const ArpParams& p);

    int currentStep (int channel) const noexcept { return playingStep[(size_t) channel].load(); }
    bool isRunning() const noexcept               { return running; }
    int lastKey() const noexcept                  { return lastKeyNote.load(); }
    /** Semitones the sequence is currently transposed by Key Follow (0 = none). */
    int currentTranspose() const noexcept         { return transposeNow.load(); }
    int heldKeyCount() const noexcept             { return heldCount.load(); }

private:
    struct Held { int note; int velocity; int order; };
    struct Sounding { int channel; int note; int samplesLeft; };

    void handleInput (const juce::MidiBuffer& in, const ArpParams& p);
    void triggerStep (juce::int64 globalStep, int sampleOffset, double samplesPerStep,
                      const Pattern& pattern, const ArpParams& p, juce::MidiBuffer& out);
    void noteOn (juce::MidiBuffer& out, int channel, int note, int velocity, int sampleOffset, int lengthSamples);
    void flushNoteOffs (juce::MidiBuffer& out, int numSamples);
    void allNotesOff (juce::MidiBuffer& out, int sampleOffset);
    void allSoundOff (juce::MidiBuffer& out, const ArpParams& p, int sampleOffset);
    int  arpNote (int channel, const Step& st, const Pattern& pattern, const ArpParams& p, int& keyVelocity);

    double sampleRate = 44100.0;
    std::array<Held, 32> held {};
    int numHeld = 0, physicallyDown = 0, orderCounter = 0;
    bool latchArmed = false;
    std::array<Sounding, 128> sounding {};
    int numSounding = 0;
    int minNoteOnOffset = 0;

    double internalPpq = 0.0;
    double lastPpqEnd = -1.0;
    juce::int64 lastTriggeredStep = -1;
    bool running = false;
    bool lastRunWasHost = false;
    juce::MidiBuffer outBuffer;
    std::array<int, kNumChannels> arpCounter {};
    std::array<std::atomic<int>, kNumChannels> playingStep {};
    std::atomic<int> lastKeyNote { -1 };
    std::atomic<int> transposeNow { 0 }, heldCount { 0 };
    juce::Random rng { 0x5eed };
};
} // namespace pam
