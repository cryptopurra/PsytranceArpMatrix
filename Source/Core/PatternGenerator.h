#pragma once

#include <juce_core/juce_core.h>
#include "PatternTypes.h"
#include "InterlockMatrix.h"
#include "Scales.h"
#include "StyleProfiles.h"

namespace pam
{
struct GeneratorContext
{
    int   rootNote     = 6;     // pitch class
    int   scaleIndex   = 5;
    bool  lockToScale  = true;  // Lock Mode: no out-of-scale notes at all
    float density      = 1.0f;  // global density multiplier (0.25 .. 1.5)
    int   octaveShift  = 0;     // per-channel octave offset
    bool  isResponse   = false; // call & response: this channel answers another one
};

/** Procedural generator for one channel of the matrix.

    Rhythm: Bernoulli draw per 16th using the learned onset probabilities of the
            style, shaped by role rules (kick space, off-beat bias) and by the
            InterlockMatrix weights of the channels that already exist.
    Pitch:  constraint-satisfaction over the in-scale notes of the style register.
            Each candidate is scored  degreeWeight(scale degree) x intervalProb(leap)
            with extra emphasis on tonic, (minor) third and perfect fifth, then
            drawn by weighted random choice.
    Form:   bar 1 is the "question"; further bars are derived from it (repeat,
            neighbour variation, answer) so 32/48/64-step patterns stay coherent. */
class PatternGenerator
{
public:
    static ChannelPattern generate (int channelIndex, GeneratorStyle style, int length,
                                    const GeneratorContext& ctx, const InterlockMatrix& matrix,
                                    juce::Random& rng);

    /** Melody variation (Neighbors / Contour / Answer / Octaves). `amount` 0..1. */
    static void applyVariation (ChannelPattern& ch, VariationStyle style, float amount,
                                const GeneratorContext& ctx, juce::Random& rng);

    static void randomizeVelocity (ChannelPattern& ch, juce::Random& rng);
    static void randomizeGate (ChannelPattern& ch, juce::Random& rng);

    /** Weight of a note `semitonesFromRoot` in the scale for a given style. */
    static float degreeWeight (int semitonesFromRoot, const Scale& scale, const StyleProfile& prof) noexcept;

    /** Quantizes every note into the scale (used when Lock Mode is switched on). */
    static void lockToScale (ChannelPattern& ch, int root, const Scale& scale) noexcept;

    /** Moves every note to the same scale degree of a new key/scale (keeps the contour). */
    static void retune (ChannelPattern& ch, int oldRoot, const Scale& oldScale,
                        int newRoot, const Scale& newScale) noexcept;

private:
    static ChannelPattern generateArpeggio (int channelIndex, int length, const GeneratorContext& ctx,
                                            const InterlockMatrix& matrix, juce::Random& rng);
    static int pickNote (int prev, int low, int high, const GeneratorContext& ctx,
                         const StyleProfile& prof, juce::Random& rng, bool allowOctaveJump);
    static void finalisePhrase (ChannelPattern& ch, int from, int to, const GeneratorContext& ctx,
                                const StyleProfile& prof, bool resolveToTonic);
};
} // namespace pam
