#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pam
{
//==============================================================================
static constexpr int kNumChannels = 4;   // matrix channels (tracks)
static constexpr int kMaxSteps    = 64;  // 4 pages of 16 steps
static constexpr int kStepsPerBar = 16;  // 4/4 grid in 16ths

//==============================================================================
/** Generator style of a channel. Each matrix channel has a default style
    (Ch1 Acid, Ch2 FM Lead, Ch3 Lead Synth, Ch4 Stabs) but it can be changed. */
enum class GeneratorStyle : int
{
    RollingBass = 0,  // off-beat rolling psy bass (kick space on 1-5-9-13)
    Acid,             // TB-303 style 16ths, accents, slides, octave jumps
    FMLead,           // fast incisive arp / FM lead, off-beat counterpoint
    LeadSynth,        // hypnotic evolving lead, longer legato notes
    RhythmStab,       // short chord stabs, accents on tension points
    numStyles
};

/** How held keys are turned into notes (copied from the reference arp modes). */
enum class ArpMode : int
{
    Up = 0, Down, UpDown, AsPlayed, Random, Chord, Melody, EpicMotif, Step, numModes
};

/** Where the outgoing velocity comes from. */
enum class VelocityMode : int
{
    Key = 0,   // velocity of the played key
    Hold,      // fixed velocity knob
    Step,      // per-step velocity
    StepKey,   // step velocity scaled by key velocity
    StepHold,  // step velocity scaled by velocity knob
    numModes
};

/** Melody variation algorithms (Melody Var page). */
enum class VariationStyle : int
{
    Neighbors = 0, // swap notes with neighbouring scale degrees
    Contour,       // keep up/down contour, re-pick pitches
    Answer,        // second half answers the first, resolving to tonic
    Octaves,       // octave displacement
    numStyles
};

/** How a channel voices its notes on the synth (classic mono / legato / glide playing). */
enum class VoiceMode : int
{
    Poly = 0,  // chords allowed, notes may overlap (slide steps overlap the next note)
    Mono,      // one note at a time: chords -> single note, each note cuts the previous one,
               // only slide steps overlap (303 style: glide on slides with a legato synth)
    Legato,    // one note at a time, every note tied to the next (continuous legato / glide)
    numModes
};

enum class HoldMode : int
{
    Off = 0,    // notes play only while keys are held
    Latch,      // keep playing after release, new keys add to the set
    CleanHold,  // keep playing after release, a new chord replaces the old one
    numModes
};

enum class TimeDivision : int
{
    Quarter = 0, Eighth, EighthTriplet, Sixteenth, SixteenthTriplet, ThirtySecond, numDivisions
};

/** Length of one step in quarter notes. */
inline double stepLengthInQuarters (TimeDivision d) noexcept
{
    switch (d)
    {
        case TimeDivision::Quarter:          return 1.0;
        case TimeDivision::Eighth:           return 0.5;
        case TimeDivision::EighthTriplet:    return 1.0 / 3.0;
        case TimeDivision::Sixteenth:        return 0.25;
        case TimeDivision::SixteenthTriplet: return 1.0 / 6.0;
        case TimeDivision::ThirtySecond:     return 0.125;
        case TimeDivision::numDivisions:     break;
    }
    return 0.25;
}

//==============================================================================
/** One cell of the sequence grid. Trivially copyable so whole patterns can be
    handed to the audio thread by value. */
struct Step
{
    int8_t  note     = 60;    // absolute MIDI note number
    uint8_t velocity = 100;   // 1..127
    float   gate     = 0.5f;  // length as a fraction of a step (0.05 .. 4.0)
    bool    active   = false; // note on / off
    bool    slide    = false; // legato / glide into the next note
    bool    accent   = false; // accented note (+velocity)
    uint8_t chord    = 1;     // 1 = single note, 2..4 = stacked scale thirds (stabs)

    bool operator== (const Step& o) const noexcept
    {
        return note == o.note && velocity == o.velocity && std::memcmp (&gate, &o.gate, sizeof (float)) == 0 && active == o.active
            && slide == o.slide && accent == o.accent && chord == o.chord;
    }
};

struct ChannelPattern
{
    bool           enabled    = false;
    GeneratorStyle style      = GeneratorStyle::Acid;
    int            midiChannel = 1;    // 1..16 output channel
    int            length     = 16;    // 1..kMaxSteps
    bool           userEdited = false; // manual edits are preserved on re-enable
    std::array<Step, kMaxSteps> steps {};

    int countActive() const noexcept
    {
        int n = 0;
        for (int i = 0; i < length; ++i)
            n += steps[(std::size_t) i].active ? 1 : 0;
        return n;
    }
};

/** The full matrix state: musical context + 4 channels. */
struct Pattern
{
    double bpm        = 142.0;
    int    rootNote   = 6;   // pitch class 0..11 (6 = F#)
    int    scaleIndex = 5;   // index into ScaleLibrary (5 = Phrygian)
    std::array<ChannelPattern, kNumChannels> channels {};

    int numEnabled() const noexcept
    {
        int n = 0;
        for (auto& c : channels)
            n += c.enabled ? 1 : 0;
        return n;
    }
};

const char* styleName (GeneratorStyle s) noexcept;
GeneratorStyle styleFromName (const char* name) noexcept;
GeneratorStyle defaultStyleForChannel (int channelIndex) noexcept;

} // namespace pam
