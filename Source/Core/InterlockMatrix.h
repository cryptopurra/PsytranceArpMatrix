#pragma once

#include "PatternTypes.h"

namespace pam
{
/** Rhythmic Interlocking Matrix.

    A boolean occupancy grid [channel][step] built from the channels that already
    exist. When a channel is (re)generated it asks the matrix how welcome an onset
    is at each step; after generation resolveCollisions() enforces hard limits so
    the groove stays clean when several channels are active.

    Priorities (high -> low): Rolling Bass / Acid (the groove), Rhythm Stabs on
    tension points, FM Lead, Lead Synth. */
class InterlockMatrix
{
public:
    void clear() noexcept;

    /** Registers an enabled channel's onsets. */
    void add (int channelIndex, const ChannelPattern& ch) noexcept;

    bool occupied (int channelIndex, int step) const noexcept;
    int  onsetsAt (int step, int excludingChannel = -1) const noexcept;
    bool anyMelodicAt (int step, int excludingChannel) const noexcept;
    bool anyChannel() const noexcept;

    /** Multiplier (0..~1.6) for the onset probability of `style` at `step`,
        given what is already in the matrix. Implements call & response:
        melodic channels fill the gaps left by other melodic channels. */
    float onsetWeight (int channelIndex, GeneratorStyle style, int step) const noexcept;

    /** Hard post-pass: at most `maxVoices` simultaneous onsets per step; the lowest
        priority non-accented note is muted first. Returns number of muted steps. */
    static int resolveCollisions (Pattern& p, int maxVoices = 3) noexcept;

    static int priority (GeneratorStyle s) noexcept;
    static bool isMelodic (GeneratorStyle s) noexcept
    {
        return s == GeneratorStyle::FMLead || s == GeneratorStyle::LeadSynth;
    }

private:
    std::array<std::array<bool, kMaxSteps>, kNumChannels> grid {};
    std::array<GeneratorStyle, kNumChannels> styles {};
    std::array<bool, kNumChannels> present {};
    std::array<int, kNumChannels> lengths {};
};
} // namespace pam
