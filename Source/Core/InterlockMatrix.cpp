#include "InterlockMatrix.h"

#include <algorithm>

namespace pam
{
void InterlockMatrix::clear() noexcept
{
    for (auto& row : grid)
        row.fill (false);
    present.fill (false);
}

void InterlockMatrix::add (int c, const ChannelPattern& ch) noexcept
{
    if (c < 0 || c >= kNumChannels)
        return;

    present[(size_t) c] = true;
    styles[(size_t) c] = ch.style;
    lengths[(size_t) c] = std::max (1, ch.length);

    for (int s = 0; s < kMaxSteps; ++s)
        grid[(size_t) c][(size_t) s] = ch.steps[(size_t) (s % lengths[(size_t) c])].active;
}

bool InterlockMatrix::occupied (int c, int step) const noexcept
{
    return present[(size_t) c] && grid[(size_t) c][(size_t) (step % kMaxSteps)];
}

int InterlockMatrix::onsetsAt (int step, int excluding) const noexcept
{
    int n = 0;
    for (int c = 0; c < kNumChannels; ++c)
        if (c != excluding && occupied (c, step))
            ++n;
    return n;
}

bool InterlockMatrix::anyMelodicAt (int step, int excluding) const noexcept
{
    for (int c = 0; c < kNumChannels; ++c)
        if (c != excluding && present[(size_t) c] && isMelodic (styles[(size_t) c]) && occupied (c, step))
            return true;
    return false;
}

bool InterlockMatrix::anyChannel() const noexcept
{
    return std::any_of (present.begin(), present.end(), [] (bool b) { return b; });
}

int InterlockMatrix::priority (GeneratorStyle s) noexcept
{
    switch (s)
    {
        case GeneratorStyle::RollingBass: return 5;
        case GeneratorStyle::Acid:        return 4;
        case GeneratorStyle::RhythmStab:  return 3;
        case GeneratorStyle::FMLead:      return 2;
        case GeneratorStyle::LeadSynth:   return 1;
        case GeneratorStyle::numStyles:   break;
    }
    return 0;
}

float InterlockMatrix::onsetWeight (int c, GeneratorStyle style, int step) const noexcept
{
    if (! anyChannel())
        return 1.0f;

    float w = 1.0f;
    const int busy = onsetsAt (step, c);
    const int beatPos = step % 4;

    switch (style)
    {
        case GeneratorStyle::FMLead:
        case GeneratorStyle::LeadSynth:
        {
            bool otherMelodic = false;
            for (int o = 0; o < kNumChannels; ++o)
                otherMelodic |= (o != c && present[(size_t) o] && isMelodic (styles[(size_t) o]));

            if (otherMelodic)
                w *= anyMelodicAt (step, c) ? 0.35f : 1.45f; // answer in the gaps (call & response)

            if (busy >= 2)
                w *= 0.4f;
            break;
        }

        case GeneratorStyle::RhythmStab:
            // stabs punctuate: favour steps where the groove channels leave air
            w *= busy == 0 ? 1.4f : (busy == 1 ? 0.8f : 0.35f);
            if (beatPos == 3)
                w *= 1.3f;
            break;

        case GeneratorStyle::Acid:
        case GeneratorStyle::RollingBass:
        {
            // low-end: avoid doubling another bass-register channel
            for (int o = 0; o < kNumChannels; ++o)
                if (o != c && present[(size_t) o]
                    && (styles[(size_t) o] == GeneratorStyle::RollingBass || styles[(size_t) o] == GeneratorStyle::Acid)
                    && occupied (o, step))
                    w *= 0.3f;
            break;
        }

        case GeneratorStyle::numStyles:
            break;
    }
    return w;
}

int InterlockMatrix::resolveCollisions (Pattern& p, int maxVoices) noexcept
{
    int muted = 0;

    for (int s = 0; s < kMaxSteps; ++s)
    {
        int candidates[kNumChannels];
        int n = 0;

        for (int c = 0; c < kNumChannels; ++c)
        {
            auto& ch = p.channels[(size_t) c];
            if (ch.enabled && s < ch.length && ch.steps[(size_t) s].active)
                candidates[n++] = c;
        }

        while (n > maxVoices)
        {
            // pick the lowest-priority, non-accented, non-user-edited candidate
            int worst = -1;
            int worstScore = 1 << 20;
            for (int i = 0; i < n; ++i)
            {
                auto& ch = p.channels[(size_t) candidates[i]];
                const auto& st = ch.steps[(size_t) s];
                const int score = priority (ch.style) * 10 + (st.accent ? 100 : 0) + (ch.userEdited ? 1000 : 0);
                if (score < worstScore)
                {
                    worstScore = score;
                    worst = i;
                }
            }

            if (worst < 0 || worstScore >= 1000)
                break;

            p.channels[(size_t) candidates[worst]].steps[(size_t) s].active = false;
            candidates[worst] = candidates[--n];
            ++muted;
        }
    }
    return muted;
}
} // namespace pam
