#include "PatternGenerator.h"
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <vector>

namespace pam
{
namespace
{
    int wrapPc (int x) noexcept { return ((x % 12) + 12) % 12; }

    /** Lowest note >= low whose pitch class is the root. */
    int rootInRegister (int root, int low) noexcept
    {
        int n = low;
        while (wrapPc (n - root) != 0)
            ++n;
        return n;
    }

    template <typename Weights>
    int weightedChoice (const Weights& w, int count, juce::Random& rng)
    {
        float total = 0.0f;
        for (int i = 0; i < count; ++i)
            total += w[(size_t) i];

        if (total <= 0.0f)
            return rng.nextInt (juce::jmax (1, count));

        float r = rng.nextFloat() * total;
        for (int i = 0; i < count; ++i)
        {
            r -= w[(size_t) i];
            if (r <= 0.0f)
                return i;
        }
        return count - 1;
    }

    /** Octave-folds a note into [low, high] (pitch class, hence scale membership, is kept). */
    int foldIntoRange (int note, int low, int high) noexcept
    {
        while (note < low && note + 12 <= 127)  note += 12;
        while (note > high && note - 12 >= 0)   note -= 12;
        return note;
    }

    uint8_t clampVel (float v) noexcept { return (uint8_t) juce::jlimit (1, 127, juce::roundToInt (v)); }
}

//==============================================================================
float PatternGenerator::degreeWeight (int semis, const Scale& scale, const StyleProfile& prof) noexcept
{
    const int rel = wrapPc (semis);
    const int idx = scale.intervals.indexOf (rel);
    if (idx < 0)
        return 0.0f;

    float w;
    if (scale.size() == 7)
        w = prof.degreeWeights[(size_t) idx];
    else if (rel == 0)
        w = prof.degreeWeights[0];
    else
        w = (1.0f - prof.degreeWeights[0]) / (float) juce::jmax (1, scale.size() - 1);

    // constraint satisfaction: favour the pillars of a (minor) psy key
    if (rel == 0)                    w *= 1.25f;
    else if (rel == 3 || rel == 4)   w *= 1.6f;  // third
    else if (rel == 7)               w *= 1.7f;  // perfect fifth
    else if (rel == 1)               w *= 1.15f; // phrygian b2 colour

    return juce::jmax (0.01f, std::pow (juce::jmax (0.001f, w), prof.pitchVariety));
}

int PatternGenerator::pickNote (int prev, int low, int high, const GeneratorContext& ctx,
                                const StyleProfile& prof, juce::Random& rng, bool allowOctaveJump)
{
    const auto& scale = ScaleLibrary::get()[ctx.scaleIndex];

    if (allowOctaveJump && rng.nextFloat() < prof.octaveJumpProb)
    {
        const int up = prev + 12, down = prev - 12;
        const bool upOk = up <= high + 12, downOk = down >= low;
        if (upOk && (! downOk || rng.nextBool())) return up;
        if (downOk) return down;
    }

    // chromatic passing tone when Lock Mode is off
    if (! ctx.lockToScale && rng.nextFloat() < 0.04f)
        return juce::jlimit (low, high, prev + (rng.nextBool() ? 1 : -1));

    std::array<float, 64> weights {};
    std::array<int, 64> notes {};
    int count = 0;

    for (int n = low; n <= high && count < 64; ++n)
    {
        const int rel = n - ctx.rootNote;
        if (! scale.contains (rel))
            continue;

        const int leap = juce::jlimit (-12, 12, n - prev);
        // learned leap distribution blended with a stepwise-motion prior
        static constexpr float prior[13] = { 0.08f, 0.12f, 0.14f, 0.12f, 0.10f, 0.10f, 0.04f,
                                             0.10f, 0.05f, 0.04f, 0.04f, 0.02f, 0.07f };
        const float learned = prof.intervalProb[(size_t) (leap + 12)];
        float iv = (1.0f - prof.intervalBlend) * learned
                 + prof.intervalBlend * prior[std::abs (leap)] * 0.5f + 0.01f;
        if (leap == 0)
            iv *= prof.repeatScale;
        float w = degreeWeight (rel, scale, prof) * iv;
        if (std::abs (n - prev) > 12)
            w *= 0.05f;

        notes[(size_t) count] = n;
        weights[(size_t) count] = w;
        ++count;
    }

    if (count == 0)
        return ScaleLibrary::quantize (prev, ctx.rootNote, scale);

    return notes[(size_t) weightedChoice (weights, count, rng)];
}

void PatternGenerator::finalisePhrase (ChannelPattern& ch, int from, int to, const GeneratorContext& ctx,
                                       const StyleProfile& prof, bool resolveToTonic)
{
    // last active step of the phrase: answer resolves on the tonic, question stays open (5th)
    for (int s = to - 1; s >= from; --s)
    {
        auto& st = ch.steps[(size_t) s];
        if (! st.active)
            continue;

        const int target = resolveToTonic ? 0 : 7;
        int best = st.note, bestDist = 128;
        for (int n = prof.lowNote - 12; n <= prof.highNote + 12; ++n)
            if (wrapPc (n - ctx.rootNote) == target && std::abs (n - st.note) < bestDist)
            {
                best = n;
                bestDist = std::abs (n - st.note);
            }

        const auto& scale = ScaleLibrary::get()[ctx.scaleIndex];
        if (scale.contains (target))
            st.note = (int8_t) juce::jlimit (0, 127, best);
        break;
    }
}

//==============================================================================
ChannelPattern PatternGenerator::generateArpeggio (int c, int length, const GeneratorContext& ctx,
                                                   const InterlockMatrix& matrix, juce::Random& rng)
{
    const auto& prof = StyleProfile::get (GeneratorStyle::Arpeggio);
    const auto& scale = ScaleLibrary::get()[ctx.scaleIndex];
    const int n = juce::jmax (1, scale.size());

    ChannelPattern ch;
    ch.style = GeneratorStyle::Arpeggio;
    ch.length = juce::jlimit (1, kMaxSteps, length);
    ch.midiChannel = c + 1;

    // chord roots per bar (scale degrees): typical psytrance movements i / VI / VII / iv
    static const int progressions[][4] = { { 0, 0, 5, 6 }, { 0, 5, 6, 0 }, { 0, 3, 4, 0 }, { 0, 0, 0, 0 },
                                           { 0, 6, 5, 6 }, { 0, 1, 0, 6 } };
    const auto& prog = progressions[rng.nextInt ((int) std::size (progressions))];

    // shapes over the chord tones (0 = root, 1 = 3rd, 2 = 5th, 3 = octave, 4 = 10th, 5 = 12th)
    static const std::vector<std::vector<int>> shapes = {
        { 0, 1, 2, 3 },                    // up
        { 3, 2, 1, 0 },                    // down
        { 0, 1, 2, 3, 2, 1 },              // up / down
        { 0, 2, 1, 3 },                    // broken
        { 0, 2, 3, 2 },                    // root - fifth - octave pendulum
        { 0, 1, 2, 3, 4, 5, 4, 3, 2, 1 },  // two-octave sweep
        { 3, 0, 2, 0, 1, 0 },              // pedal on the root
    };
    const auto& shape = shapes[(size_t) rng.nextInt ((int) shapes.size())];
    static const int toneDegrees[] = { 0, 2, 4, 7, 9, 11 }; // in scale steps (7 = octave for 7-note scales)

    const int low = juce::jlimit (0, 108, prof.lowNote + ctx.octaveShift * 12);
    int base = low;
    while (((base - ctx.rootNote) % 12 + 12) % 12 != 0)
        ++base; // root note at or above the register bottom

    int idx = 0;
    for (int s = 0; s < ch.length; ++s)
    {
        const int bar = s / 16, inBar = s % 16;
        float prob = juce::jlimit (0.0f, 0.98f, prof.onsetProb[(size_t) inBar] * prof.density * ctx.density
                                                * matrix.onsetWeight (c, GeneratorStyle::Arpeggio, inBar));
        if (inBar % 4 == 0)
            prob = juce::jmax (prob, 0.9f); // keep the pulse on the beats
        auto& st = ch.steps[(size_t) s];
        st.active = rng.nextFloat() < prob;
        if (! st.active)
            continue;

        const int tone = shape[(size_t) (idx++ % (int) shape.size())];
        const int deg = prog[bar % 4] + toneDegrees[tone] * n / 7; // scale-size aware
        int note = ScaleLibrary::degreeToNote (deg, ctx.rootNote, scale, base);
        if (ctx.lockToScale)
            note = ScaleLibrary::quantize (note, ctx.rootNote, scale);
        st.note = (int8_t) juce::jlimit (0, 127, note);
        st.accent = inBar % 4 == 0 && rng.nextFloat() < 0.35f;
        st.velocity = clampVel (st.accent ? prof.accentVelocity
                                          : prof.baseVelocity + (rng.nextFloat() * 2.0f - 1.0f) * prof.velocitySpread);
        st.gate = prof.gateMin + rng.nextFloat() * (prof.gateMax - prof.gateMin);
        st.chord = 1;
    }
    return ch;
}

ChannelPattern PatternGenerator::generate (int c, GeneratorStyle style, int length,
                                           const GeneratorContext& ctx, const InterlockMatrix& matrix,
                                           juce::Random& rng)
{
    if (style == GeneratorStyle::Arpeggio)
        return generateArpeggio (c, length, ctx, matrix, rng);

    const auto& prof = StyleProfile::get (style);
    const auto& scale = ScaleLibrary::get()[ctx.scaleIndex];

    ChannelPattern ch;
    ch.style = style;
    ch.length = juce::jlimit (1, kMaxSteps, length);
    ch.midiChannel = c + 1;

    const int low  = juce::jlimit (0, 115, prof.lowNote + ctx.octaveShift * 12);
    const int high = juce::jlimit (12, 127, prof.highNote + ctx.octaveShift * 12);

    //--- 1. rhythm of the first bar -------------------------------------------
    std::array<bool, 16> on {};
    std::array<float, 16> p {};
    for (int s = 0; s < 16; ++s)
    {
        float prob = prof.onsetProb[(size_t) s] * prof.density * ctx.density;
        if (prof.offbeatBias)
            prob *= (s % 2 == 1) ? 1.25f : 0.75f;
        if (prof.kickSpace && s % 4 == 0)
            prob = 0.0f;
        prob *= matrix.onsetWeight (c, style, s);
        p[(size_t) s] = juce::jlimit (0.0f, 0.97f, prob);
        on[(size_t) s] = rng.nextFloat() < p[(size_t) s];
    }

    if (style == GeneratorStyle::RhythmStab)
    {
        // at least two tension points (steps 4, 8, 12, 16) must hit
        int hits = 0;
        for (int s : { 3, 7, 11, 15 })
            hits += on[(size_t) s] ? 1 : 0;
        for (int s : { 15, 7, 11, 3 })
            if (hits < 2 && ! on[(size_t) s] && matrix.onsetsAt (s, c) < 3)
            {
                on[(size_t) s] = true;
                ++hits;
            }
    }

    // never fewer than 3 notes per bar: add the most probable missing steps
    for (;;)
    {
        int count = 0;
        for (auto b : on) count += b ? 1 : 0;
        if (count >= 3) break;

        int best = -1;
        for (int s = 0; s < 16; ++s)
            if (! on[(size_t) s] && ! (prof.kickSpace && s % 4 == 0)
                && (best < 0 || p[(size_t) s] > p[(size_t) best]))
                best = s;
        if (best < 0) break;
        on[(size_t) best] = true;
    }

    //--- 2. pitches, velocities, gates for bar 1 --------------------------------
    int prev = rootInRegister (ctx.rootNote, low);
    if (InterlockMatrix::isMelodic (style))
        prev = juce::jmin (high, prev + 12 * ((high - low) / 24)); // start mid-register

    for (int s = 0; s < 16; ++s)
    {
        auto& st = ch.steps[(size_t) s];
        st.active = on[(size_t) s];
        if (! st.active)
            continue;

        const bool first = (s == 0) || [&] { for (int k = 0; k < s; ++k) if (on[(size_t) k]) return false; return true; }();
        int note;
        if (style == GeneratorStyle::RollingBass)
        {
            // rolling bass: mostly the root, occasional fifth/octave on the last 16th of a beat
            note = rootInRegister (ctx.rootNote, low);
            if (s % 4 == 3 && rng.nextFloat() < 0.25f)
                note = pickNote (note, low, high, ctx, prof, rng, false);
        }
        else if (first)
            note = rootInRegister (ctx.rootNote, InterlockMatrix::isMelodic (style) ? prev : low);
        else
            note = pickNote (prev, low, high, ctx, prof, rng, true);

        if (ctx.lockToScale)
            note = ScaleLibrary::quantize (note, ctx.rootNote, scale);

        st.note = (int8_t) juce::jlimit (0, 127, note);
        prev = note;

        const bool accent = prof.forcedAccents[(size_t) s]
                         || rng.nextFloat() < juce::jmax (prof.accentProb[(size_t) s], prof.minAccentProb);
        st.accent = accent;
        st.velocity = clampVel (accent ? prof.accentVelocity
                                       : prof.baseVelocity + (rng.nextFloat() * 2.0f - 1.0f) * prof.velocitySpread);
        st.gate = prof.gateMin + rng.nextFloat() * (prof.gateMax - prof.gateMin);
        st.chord = (uint8_t) (prof.chordSize > 1 ? (rng.nextFloat() < 0.3f ? 2 : prof.chordSize) : 1);
    }

    // slides and legato need to know the next onset
    for (int s = 0; s < 16; ++s)
    {
        auto& st = ch.steps[(size_t) s];
        if (! st.active)
            continue;

        int next = s + 1;
        while (next < 16 && ! on[(size_t) next])
            ++next;
        const int distance = next - s; // 16 - s when no more notes in the bar

        if (prof.sustainToNext)
            st.gate = juce::jlimit (0.5f, 4.0f, (float) distance * 0.95f);

        st.slide = next < 16 && distance == 1 && rng.nextFloat() < prof.slideProb;
    }

    if (InterlockMatrix::isMelodic (style) || style == GeneratorStyle::Acid)
        finalisePhrase (ch, 0, 16, ctx, prof, ctx.isResponse);

    //--- 3. derive further bars (form: A A' B A'' / call & response) ------------
    for (int bar = 1; bar * 16 < ch.length; ++bar)
    {
        for (int s = 0; s < 16; ++s)
            ch.steps[(size_t) (bar * 16 + s)] = ch.steps[(size_t) s];

        ChannelPattern tmp = ch;
        tmp.length = 16;
        for (int s = 0; s < 16; ++s)
            tmp.steps[(size_t) s] = ch.steps[(size_t) (bar * 16 + s)];

        const auto var = (bar % 2 == 1) ? VariationStyle::Answer : VariationStyle::Neighbors;
        GeneratorContext vctx = ctx;
        vctx.isResponse = (bar % 2 == 1);
        if (style != GeneratorStyle::RollingBass)
            applyVariation (tmp, var, bar % 2 == 1 ? 0.5f : 0.25f, vctx, rng);

        for (int s = 0; s < 16; ++s)
            ch.steps[(size_t) (bar * 16 + s)] = tmp.steps[(size_t) s];
    }

    return ch;
}

//==============================================================================
void PatternGenerator::applyVariation (ChannelPattern& ch, VariationStyle style, float amount,
                                       const GeneratorContext& ctx, juce::Random& rng)
{
    const auto& scale = ScaleLibrary::get()[ctx.scaleIndex];
    const auto& prof = StyleProfile::get (ch.style);
    const int len = juce::jlimit (1, kMaxSteps, ch.length);
    const int base = 12 * ((prof.lowNote + ctx.octaveShift * 12) / 12);
    amount = juce::jlimit (0.0f, 1.0f, amount);

    auto toDeg = [&] (int note) { return ScaleLibrary::noteToDegree (note, ctx.rootNote, scale, base); };
    auto toNote = [&] (int deg) { return ScaleLibrary::degreeToNote (deg, ctx.rootNote, scale, base); };

    switch (style)
    {
        case VariationStyle::Neighbors:
            for (int s = 0; s < len; ++s)
            {
                auto& st = ch.steps[(size_t) s];
                if (st.active && rng.nextFloat() < amount)
                    st.note = (int8_t) toNote (toDeg (st.note) + (rng.nextBool() ? 1 : -1));
            }
            break;

        case VariationStyle::Contour:
        {
            int prevOrig = -1, prevNew = -1;
            for (int s = 0; s < len; ++s)
            {
                auto& st = ch.steps[(size_t) s];
                if (! st.active)
                    continue;

                const int orig = st.note;
                if (prevOrig >= 0 && rng.nextFloat() < amount)
                {
                    const int dir = orig > prevOrig ? 1 : (orig < prevOrig ? -1 : 0);
                    const int magnitude = 1 + rng.nextInt (3);
                    st.note = (int8_t) toNote (toDeg (prevNew) + dir * magnitude);
                }
                prevOrig = orig;
                prevNew = st.note;
            }
            break;
        }

        case VariationStyle::Answer:
        {
            // second half answers the first: same rhythm, inverted around the tonic, resolves home
            const int half = juce::jmax (1, len / 2);
            const int pivot = toDeg (toNote (0));
            for (int s = 0; s + half < len; ++s)
            {
                const auto& q = ch.steps[(size_t) s];
                auto& a = ch.steps[(size_t) (s + half)];
                a.active = q.active;
                a.velocity = q.velocity;
                a.gate = q.gate;
                a.slide = q.slide;
                a.accent = q.accent;
                a.chord = q.chord;
                if (! q.active)
                    continue;

                int deg = toDeg (q.note);
                if (rng.nextFloat() < amount)
                    deg = pivot - (deg - pivot);
                a.note = (int8_t) toNote (deg);
            }
            if (len >= 2)
                finalisePhrase (ch, half, len, ctx, prof, true);
            break;
        }

        case VariationStyle::Octaves:
            for (int s = 0; s < len; ++s)
            {
                auto& st = ch.steps[(size_t) s];
                if (st.active && rng.nextFloat() < amount * 0.6f)
                {
                    const int shifted = st.note + (rng.nextBool() ? 12 : -12);
                    if (shifted >= 24 && shifted <= 108)
                        st.note = (int8_t) shifted;
                }
            }
            break;

        case VariationStyle::numStyles:
            break;
    }

    // keep everything inside a playable window around the style's register
    const int lowLimit  = juce::jmax (12, prof.lowNote + ctx.octaveShift * 12 - 12);
    const int highLimit = juce::jmin (120, prof.highNote + ctx.octaveShift * 12 + 12);
    for (auto& st : ch.steps)
        st.note = (int8_t) foldIntoRange (st.note, lowLimit, highLimit);

    if (ctx.lockToScale)
        lockToScale (ch, ctx.rootNote, scale);
}

void PatternGenerator::randomizeVelocity (ChannelPattern& ch, juce::Random& rng)
{
    const auto& prof = StyleProfile::get (ch.style);
    for (int s = 0; s < ch.length; ++s)
    {
        auto& st = ch.steps[(size_t) s];
        st.accent = rng.nextFloat() < juce::jmax (0.12f, prof.accentProb[(size_t) (s % 16)]);
        st.velocity = clampVel (st.accent ? prof.accentVelocity : 70.0f + rng.nextFloat() * 40.0f);
    }
}

void PatternGenerator::randomizeGate (ChannelPattern& ch, juce::Random& rng)
{
    const auto& prof = StyleProfile::get (ch.style);
    for (int s = 0; s < ch.length; ++s)
        ch.steps[(size_t) s].gate = prof.gateMin * 0.5f + rng.nextFloat() * (prof.gateMax * 1.3f - prof.gateMin * 0.5f);
}

void PatternGenerator::lockToScale (ChannelPattern& ch, int root, const Scale& scale) noexcept
{
    for (auto& st : ch.steps)
        st.note = (int8_t) ScaleLibrary::quantize (st.note, root, scale);
}

void PatternGenerator::retune (ChannelPattern& ch, int oldRoot, const Scale& oldScale,
                               int newRoot, const Scale& newScale) noexcept
{
    for (auto& st : ch.steps)
    {
        // anchor octave: keep the note within ~6 semitones of its original register
        const int base = 12 * (st.note / 12);
        const int deg = ScaleLibrary::noteToDegree (st.note, oldRoot, oldScale, base);
        int n = ScaleLibrary::degreeToNote (deg * newScale.size() / juce::jmax (1, oldScale.size()),
                                            newRoot, newScale, base);
        if (oldScale.size() == newScale.size())
            n = ScaleLibrary::degreeToNote (deg, newRoot, newScale, base);
        while (n - st.note > 6)  n -= 12;
        while (st.note - n > 6)  n += 12;
        st.note = (int8_t) juce::jlimit (0, 127, n);
    }
}
} // namespace pam
