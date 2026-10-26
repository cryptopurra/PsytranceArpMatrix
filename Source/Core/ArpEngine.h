#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "PatternTypes.h"
#include "Scales.h"

namespace pam
{
struct ArpParams
{
    // per matrix channel: step length, key arp mode, velocity source and hold behaviour
    std::array<ArpMode, kNumChannels>      mode         { ArpMode::Step, ArpMode::Step, ArpMode::Step, ArpMode::Step };
    std::array<VelocityMode, kNumChannels> velocityMode { VelocityMode::Step, VelocityMode::Step, VelocityMode::Step, VelocityMode::Step };
    std::array<HoldMode, kNumChannels>     holdMode     { HoldMode::Off, HoldMode::Off, HoldMode::Off, HoldMode::Off };
    std::array<TimeDivision, kNumChannels> division     { TimeDivision::Sixteenth, TimeDivision::Sixteenth,
                                                          TimeDivision::Sixteenth, TimeDivision::Sixteenth };
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
    std::array<int, kNumChannels> channelTranspose {}; // per-channel transposition (semitones, octave x 12)
    std::array<VoiceMode, kNumChannels> voiceMode { VoiceMode::Poly, VoiceMode::Poly, VoiceMode::Poly, VoiceMode::Poly };
    std::array<int, kNumChannels> glide { -1, -1, -1, -1 };
    std::array<bool, kNumChannels> channelMuted {};      // per-channel MUTE (non destructive) // portamento time sent as CC5 (0..127), -1 = don't send
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
    struct Sounding { int channel; int note; int samplesLeft; int source = -1; bool slide = false; };

    void handleInput (const juce::MidiBuffer& in, const ArpParams& p);
    struct KeySet { std::array<Held, 32> held {}; int num = 0; };
    void triggerChannel (int channel, juce::int64 channelStep, int sampleOffset, double samplesPerStep,
                         const Pattern& pattern, const ArpParams& p, juce::MidiBuffer& out);
    void noteOn (juce::MidiBuffer& out, int channel, int note, int velocity, int sampleOffset, int lengthSamples,
                 int source = -1, bool slide = false);
    void sendGlideControllers (juce::MidiBuffer& out, const Pattern& pattern, const ArpParams& p);
    void flushNoteOffs (juce::MidiBuffer& out, int numSamples);
    void allNotesOff (juce::MidiBuffer& out, int sampleOffset);
    void allSoundOff (juce::MidiBuffer& out, const ArpParams& p, int sampleOffset);
    int  arpNote (int channel, const KeySet& keys, const Step& st, const Pattern& pattern, const ArpParams& p, int& keyVelocity);
    int  maxHeld (const ArpParams& p) const noexcept;

    double sampleRate = 44100.0;
    // held keys as seen by each HOLD mode (Off / Latch / Clean Hold), so every channel can use its own
    std::array<KeySet, 3> keySets {};
    int physicallyDown = 0, orderCounter = 0;
    bool latchArmed = false;
    std::array<Sounding, 128> sounding {};
    int numSounding = 0;
    int minNoteOnOffset = 0;

    double internalPpq = 0.0;
    double lastPpqEnd = -1.0;
    int smoothBlocks = 0;            // blocks since the continuous clock was (re)synced
    std::array<juce::int64, kNumChannels> lastTriggeredStep { -1, -1, -1, -1 };
    bool running = false;
    bool lastRunWasHost = false;
    juce::MidiBuffer outBuffer;
    std::array<int, kNumChannels> arpCounter {};
    std::array<std::atomic<int>, kNumChannels> playingStep {};
    std::atomic<int> lastKeyNote { -1 };
    std::atomic<int> transposeNow { 0 }, heldCount { 0 };
    std::array<int, kNumChannels> lastGlideSent { -2, -2, -2, -2 };
    juce::Random rng { 0x5eed };
};
} // namespace pam
