#include "ChannelMatrix.h"

#include <algorithm>

namespace pam
{
ChannelMatrix::ChannelMatrix()
{
    for (int c = 0; c < kNumChannels; ++c)
    {
        auto& ch = pattern.channels[(size_t) c];
        ch.style = defaultStyleForChannel (c);
        ch.midiChannel = c + 1;
        ch.length = 16;
        ch.enabled = false;
    }
    octaves = { 0, 0, 0, 0 };
}

GeneratorContext ChannelMatrix::contextFor (int c) const noexcept
{
    GeneratorContext ctx;
    ctx.rootNote = pattern.rootNote;
    ctx.scaleIndex = pattern.scaleIndex;
    ctx.lockToScale = lock;
    ctx.density = density;
    ctx.octaveShift = octaves[(size_t) juce::jlimit (0, kNumChannels - 1, c)];
    ctx.isResponse = isAnswer (c);
    return ctx;
}

int ChannelMatrix::questionChannel() const noexcept
{
    // the first enabled melodic-capable channel asks, the others answer
    for (int c = 0; c < kNumChannels; ++c)
        if (pattern.channels[(size_t) c].enabled)
            return c;
    return 0;
}

InterlockMatrix ChannelMatrix::buildMatrix (int excluding) const
{
    InterlockMatrix m;
    m.clear();
    for (int c = 0; c < kNumChannels; ++c)
        if (c != excluding && pattern.channels[(size_t) c].enabled)
            m.add (c, pattern.channels[(size_t) c]);
    return m;
}

//==============================================================================
void ChannelMatrix::initialiseDefault()
{
    for (int c = 0; c < kNumChannels; ++c)
    {
        auto& ch = pattern.channels[(size_t) c];
        ch.enabled = (c == 0);
        ch.userEdited = false;
        everGenerated[(size_t) c] = false;
        barLocks[(size_t) c].fill (false);
    }
    regenerateChannel (0);
}

void ChannelMatrix::generateAll()
{
    const Pattern before = pattern;
    // generate in priority order so the groove channels shape the melodic ones
    juce::Array<int> order = getActiveChannels();
    std::stable_sort (order.begin(), order.end(), [this] (int a, int b)
    {
        return InterlockMatrix::priority (pattern.channels[(size_t) a].style)
             > InterlockMatrix::priority (pattern.channels[(size_t) b].style);
    });

    // channels regenerate against the ones already rebuilt in this pass only
    std::array<bool, kNumChannels> wasEnabled {};
    for (int c = 0; c < kNumChannels; ++c)
    {
        wasEnabled[(size_t) c] = pattern.channels[(size_t) c].enabled;
        pattern.channels[(size_t) c].enabled = false;
    }

    for (int c : order)
    {
        pattern.channels[(size_t) c].enabled = true;
        pattern.channels[(size_t) c].userEdited = false;
        regenerateChannel (c);
    }

    for (int c = 0; c < kNumChannels; ++c)
        pattern.channels[(size_t) c].enabled = wasEnabled[(size_t) c];

    InterlockMatrix::resolveCollisions (pattern);
    restoreLockedBars (before);
    touch();
}

void ChannelMatrix::setBarLocked (int c, int bar, bool locked) noexcept
{
    if (c >= 0 && c < kNumChannels && bar >= 0 && bar < kMaxSteps / 16)
        barLocks[(size_t) c][(size_t) bar] = locked;
    touch();
}

bool ChannelMatrix::isBarLocked (int c, int bar) const noexcept
{
    return c >= 0 && c < kNumChannels && bar >= 0 && bar < kMaxSteps / 16 && barLocks[(size_t) c][(size_t) bar];
}

bool ChannelMatrix::anyBarLocked (int c) const noexcept
{
    for (int b = 0; b < kMaxSteps / 16; ++b)
        if (isBarLocked (c, b))
            return true;
    return false;
}

std::array<Step, 16> ChannelMatrix::copyBar (int c, int bar) const
{
    std::array<Step, 16> out {};
    if (c < 0 || c >= kNumChannels || bar < 0 || bar >= kMaxSteps / 16)
        return out;
    for (int i = 0; i < 16; ++i)
        out[(size_t) i] = pattern.channels[(size_t) c].steps[(size_t) (bar * 16 + i)];
    return out;
}

void ChannelMatrix::pasteBar (int c, int bar, const std::array<Step, 16>& steps)
{
    if (c < 0 || c >= kNumChannels || bar < 0 || bar >= kMaxSteps / 16)
        return;
    if (getLength() < (bar + 1) * 16)
        setLength ((bar + 1) * 16);
    auto& ch = pattern.channels[(size_t) c];
    for (int i = 0; i < 16; ++i)
        ch.steps[(size_t) (bar * 16 + i)] = steps[(size_t) i];
    ch.userEdited = true;
    touch();
}

void ChannelMatrix::clearBar (int c, int bar)
{
    if (c < 0 || c >= kNumChannels || bar < 0 || bar >= kMaxSteps / 16)
        return;
    auto& ch = pattern.channels[(size_t) c];
    for (int i = 0; i < 16; ++i)
        ch.steps[(size_t) (bar * 16 + i)].active = false;
    ch.userEdited = true;
    touch();
}

void ChannelMatrix::setBarMuted (int c, int bar, bool muted)
{
    if (c >= 0 && c < kNumChannels && bar >= 0 && bar < kMaxSteps / 16)
    {
        pattern.channels[(size_t) c].barMuted[(size_t) bar] = muted;
        originals[(size_t) c].barMuted[(size_t) bar] = muted;
    }
    touch();
}

bool ChannelMatrix::isBarMuted (int c, int bar) const noexcept
{
    return c >= 0 && c < kNumChannels && bar >= 0 && bar < kMaxSteps / 16 && pattern.channels[(size_t) c].barMuted[(size_t) bar];
}

void ChannelMatrix::setRole (int c, int role) noexcept
{
    if (c >= 0 && c < kNumChannels)
        roles[(size_t) c] = juce::jlimit (0, 2, role);
    touch();
}

bool ChannelMatrix::isAnswer (int c) const noexcept
{
    const int r = getRole (c);
    if (r == 1) return false;
    if (r == 2) return true;
    // automatic: an explicit "call" channel asks, otherwise the first active channel asks
    for (int i = 0; i < kNumChannels; ++i)
        if (roles[(size_t) i] == 1 && pattern.channels[(size_t) i].enabled)
            return true;
    return c != questionChannel();
}

void ChannelMatrix::restoreLockedBars (const Pattern& before) noexcept
{
    for (int c = 0; c < kNumChannels; ++c)
        for (int b = 0; b < kMaxSteps / 16; ++b)
            if (barLocks[(size_t) c][(size_t) b])
                for (int s = b * 16; s < b * 16 + 16; ++s)
                    pattern.channels[(size_t) c].steps[(size_t) s] = before.channels[(size_t) c].steps[(size_t) s];
}

void ChannelMatrix::regenerateChannel (int c)
{
    if (c < 0 || c >= kNumChannels)
        return;

    const Pattern before = pattern; // locked bars survive any regeneration

    auto& ch = pattern.channels[(size_t) c];
    const auto matrix = buildMatrix (c);
    auto fresh = PatternGenerator::generate (c, ch.style, ch.length, contextFor (c), matrix, rng);

    fresh.enabled = ch.enabled;
    fresh.midiChannel = ch.midiChannel;
    fresh.barMuted = ch.barMuted;
    fresh.userEdited = false;
    ch = fresh;
    everGenerated[(size_t) c] = true;

    if (pattern.numEnabled() > 1)
        InterlockMatrix::resolveCollisions (pattern);
    restoreLockedBars (before);
    originals[(size_t) c] = pattern.channels[(size_t) c];
    touch();
}

void ChannelMatrix::setChannelEnabled (int c, bool enabled)
{
    if (c < 0 || c >= kNumChannels)
        return;

    auto& ch = pattern.channels[(size_t) c];
    if (ch.enabled == enabled)
        return;

    ch.enabled = enabled;
    if (enabled)
        regenerateChannel (c);   // always a fresh part, complementary to whatever is already on
    touch();
}

bool ChannelMatrix::isChannelEnabled (int c) const noexcept
{
    return c >= 0 && c < kNumChannels && pattern.channels[(size_t) c].enabled;
}

juce::Array<int> ChannelMatrix::getActiveChannels() const
{
    juce::Array<int> r;
    for (int c = 0; c < kNumChannels; ++c)
        if (pattern.channels[(size_t) c].enabled)
            r.add (c);
    return r;
}

void ChannelMatrix::setChannelStyle (int c, GeneratorStyle style, bool regenerate)
{
    if (c < 0 || c >= kNumChannels)
        return;

    pattern.channels[(size_t) c].style = style;
    if (regenerate && pattern.channels[(size_t) c].enabled)
        regenerateChannel (c);
    touch();
}

void ChannelMatrix::setChannelMidiChannel (int c, int midiChannel)
{
    if (c >= 0 && c < kNumChannels)
        pattern.channels[(size_t) c].midiChannel = juce::jlimit (1, 16, midiChannel);
    touch();
}

void ChannelMatrix::setChannelOctave (int c, int octave) noexcept
{
    if (c >= 0 && c < kNumChannels)
        octaves[(size_t) c] = juce::jlimit (-3, 3, octave);
}

//==============================================================================
void ChannelMatrix::setKey (int root, int scaleIndex, bool retuneExisting)
{
    root = ((root % 12) + 12) % 12;
    scaleIndex = juce::jlimit (0, ScaleLibrary::get().size() - 1, scaleIndex);
    if (root == pattern.rootNote && scaleIndex == pattern.scaleIndex)
        return;

    const auto& lib = ScaleLibrary::get();
    if (retuneExisting)
        for (int c = 0; c < kNumChannels; ++c)
        {
            PatternGenerator::retune (pattern.channels[(size_t) c], pattern.rootNote, lib[pattern.scaleIndex], root, lib[scaleIndex]);
            PatternGenerator::retune (originals[(size_t) c], pattern.rootNote, lib[pattern.scaleIndex], root, lib[scaleIndex]);
        }

    pattern.rootNote = root;
    pattern.scaleIndex = scaleIndex;
    touch();
}

void ChannelMatrix::setLockToScale (bool shouldLock)
{
    lock = shouldLock;
    if (lock)
        for (auto& ch : pattern.channels)
            PatternGenerator::lockToScale (ch, pattern.rootNote, ScaleLibrary::get()[pattern.scaleIndex]);
    touch();
}

void ChannelMatrix::setLength (int newLength)
{
    newLength = juce::jlimit (1, kMaxSteps, newLength);
    for (int c = 0; c < kNumChannels; ++c)
    {
        auto& ch = pattern.channels[(size_t) c];
        const int old = juce::jmax (1, ch.length);
        // extending: continue the existing loop so the groove carries on
        for (int s = old; s < newLength; ++s)
            ch.steps[(size_t) s] = ch.steps[(size_t) (s % old)];
        ch.length = newLength;

        auto& o = originals[(size_t) c];
        const int oldO = juce::jmax (1, o.length);
        for (int s = oldO; s < newLength; ++s)
            o.steps[(size_t) s] = o.steps[(size_t) (s % oldO)];
        o.length = newLength;
    }
    touch();
}

//==============================================================================
void ChannelMatrix::setStep (int c, int step, const Step& s)
{
    if (c < 0 || c >= kNumChannels || step < 0 || step >= kMaxSteps)
        return;

    auto& ch = pattern.channels[(size_t) c];
    ch.steps[(size_t) step] = s;
    if (lock)
        ch.steps[(size_t) step].note = (int8_t) ScaleLibrary::quantize (s.note, pattern.rootNote,
                                                                        ScaleLibrary::get()[pattern.scaleIndex]);
    ch.userEdited = true;
    touch();
}

void ChannelMatrix::applyVariation (int c, VariationStyle style, float amount)
{
    const Pattern before = pattern;
    for (int i = 0; i < kNumChannels; ++i)
        if ((c < 0 && pattern.channels[(size_t) i].enabled) || i == c)
        {
            auto& ch = pattern.channels[(size_t) i];
            ch = originals[(size_t) i].length > 0 && everGenerated[(size_t) i] && ! ch.userEdited
                   ? [&] { auto o = originals[(size_t) i]; o.enabled = ch.enabled; o.midiChannel = ch.midiChannel; return o; }()
                   : ch;
            PatternGenerator::applyVariation (ch, style, amount, contextFor (i), rng);
        }
    restoreLockedBars (before);
    touch();
}

void ChannelMatrix::restoreOriginal (int c)
{
    for (int i = 0; i < kNumChannels; ++i)
        if ((c < 0 || i == c) && everGenerated[(size_t) i])
        {
            auto& ch = pattern.channels[(size_t) i];
            const bool en = ch.enabled;
            const int mc = ch.midiChannel;
            ch = originals[(size_t) i];
            ch.enabled = en;
            ch.midiChannel = mc;
        }
    touch();
}

void ChannelMatrix::randomizeVelocity (int c)
{
    const Pattern before = pattern;
    for (int i = 0; i < kNumChannels; ++i)
        if ((c < 0 && pattern.channels[(size_t) i].enabled) || i == c)
            PatternGenerator::randomizeVelocity (pattern.channels[(size_t) i], rng);
    restoreLockedBars (before);
    touch();
}

void ChannelMatrix::randomizeGate (int c)
{
    const Pattern before = pattern;
    for (int i = 0; i < kNumChannels; ++i)
        if ((c < 0 && pattern.channels[(size_t) i].enabled) || i == c)
            PatternGenerator::randomizeGate (pattern.channels[(size_t) i], rng);
    restoreLockedBars (before);
    touch();
}

void ChannelMatrix::clearChannel (int c)
{
    if (c < 0 || c >= kNumChannels)
        return;
    for (auto& st : pattern.channels[(size_t) c].steps)
        st.active = false;
    pattern.channels[(size_t) c].userEdited = true;
    touch();
}

void ChannelMatrix::setPattern (const Pattern& p)
{
    pattern = p;
    for (int c = 0; c < kNumChannels; ++c)
    {
        originals[(size_t) c] = p.channels[(size_t) c];
        everGenerated[(size_t) c] = true;
        pattern.channels[(size_t) c].userEdited = false;
    }
    touch();
}
} // namespace pam
