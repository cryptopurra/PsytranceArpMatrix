#pragma once

#include "PatternGenerator.h"

namespace pam
{
/** Owner of the 4-channel sequence matrix (message thread only).

    - Single Arp Mode (default): only channel 1 is enabled and generated.
    - Incremental activation: enabling another channel generates *only that
      channel*, complementary to the channels that are already on (Interlocking
      Matrix + call & response). Existing channels are never touched.
    - Generate / Roll regenerates every enabled channel in priority order.

    Every mutating call bumps getVersion() so editors/processors can resync. */
class ChannelMatrix
{
public:
    ChannelMatrix();

    //=== settings ================================================================
    void setKey (int rootPitchClass, int scaleIndex, bool retuneExisting = true);
    void setLockToScale (bool shouldLock);
    void setDensity (float d) noexcept              { density = juce::jlimit (0.25f, 1.5f, d); }
    void setLength (int newLength);
    void setChannelOctave (int channel, int octave) noexcept;
    void setSeed (juce::int64 seed)                  { rng.setSeed (seed); }

    int  getRoot() const noexcept                    { return pattern.rootNote; }
    int  getScaleIndex() const noexcept              { return pattern.scaleIndex; }
    bool isLocked() const noexcept                   { return lock; }
    int  getLength() const noexcept                  { return pattern.channels[0].length; }
    float getDensity() const noexcept                { return density; }

    //=== generation ==============================================================
    /** Resets to Single Arp Mode: channel 1 only, freshly generated. */
    void initialiseDefault();

    /** "Generate" / "Roll": regenerates every enabled channel (priority order). */
    void generateAll();

    /** Regenerates one channel against the others (they stay untouched). */
    void regenerateChannel (int channel);

    /** Dynamic activation switch. Enabling always generates a new part, complementary
        to the channels that are already on. */
    void setChannelEnabled (int channel, bool enabled);
    bool isChannelEnabled (int channel) const noexcept;
    juce::Array<int> getActiveChannels() const;

    void setChannelStyle (int channel, GeneratorStyle style, bool regenerate = true);

    /** Bar locks (bars of 16 steps): locked bars keep their notes through Roll / Generate,
        re-activation, variations and velocity / gate randomisation. */
    void setBarLocked (int channel, int bar, bool locked) noexcept;
    bool isBarLocked (int channel, int bar) const noexcept;
    bool anyBarLocked (int channel) const noexcept;

    /** Bar clipboard helpers (16 steps). Pasting beyond the current length extends it. */
    std::array<Step, 16> copyBar (int channel, int bar) const;
    void pasteBar (int channel, int bar, const std::array<Step, 16>& steps);
    void clearBar (int channel, int bar);
    void setBarMuted (int channel, int bar, bool muted);
    bool isBarMuted (int channel, int bar) const noexcept;

    /** Call & response role: 0 = automatic (first active channel asks), 1 = call, 2 = answer,
        3 = free (no question / answer: plain variations, no gap filling). */
    static constexpr int kRoleFree = 3;
    void setRole (int channel, int role) noexcept;
    int  getRole (int channel) const noexcept       { return roles[(size_t) juce::jlimit (0, kNumChannels - 1, channel)]; }
    bool isAnswer (int channel) const noexcept;     // effective role used by the generator
    void setChannelMidiChannel (int channel, int midiChannel);

    //=== editing ================================================================
    void setStep (int channel, int step, const Step& s);
    void applyVariation (int channel, VariationStyle style, float amount); // channel -1 = all enabled
    void restoreOriginal (int channel);                                     // channel -1 = all
    void randomizeVelocity (int channel);
    void randomizeGate (int channel);
    void clearChannel (int channel);

    //=== state ==================================================================
    const Pattern& getPattern() const noexcept       { return pattern; }
    void setPattern (const Pattern& p);              // preset load
    void setChannelPattern (int channel, const ChannelPattern& ch); // one channel (channel preset / MIDI drop)
    int  getVersion() const noexcept                 { return version; }

    GeneratorContext contextFor (int channel) const noexcept;

private:
    InterlockMatrix buildMatrix (int excludingChannel) const;
    void restoreLockedBars (const Pattern& before) noexcept;
    int  questionChannel() const noexcept;
    void touch() noexcept { ++version; }

    Pattern pattern;
    std::array<ChannelPattern, kNumChannels> originals {};
    std::array<bool, kNumChannels> everGenerated {};
    std::array<int, kNumChannels> octaves {};
    std::array<std::array<bool, kMaxSteps / 16>, kNumChannels> barLocks {};
    std::array<int, kNumChannels> roles {};
    bool lock = true;
    float density = 1.0f;
    int version = 0;
    juce::Random rng;
};
} // namespace pam
