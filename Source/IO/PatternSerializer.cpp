#include "PatternSerializer.h"
#include <cmath>
#include "../Core/Scales.h"

namespace pam
{
juce::StringArray PatternSerializer::timeDivisionNames()   { return { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }
juce::StringArray PatternSerializer::arpModeNames()        { return { "Up", "Down", "Up / Down", "As Played", "Random", "Chord", "Melody", "Epic Motif", "Step" }; }
juce::StringArray PatternSerializer::velocityModeNames()   { return { "Key", "Hold", "Step", "Step + Key", "Step + Hold" }; }
juce::StringArray PatternSerializer::variationStyleNames() { return { "Neighbors", "Contour", "Answer", "Octaves" }; }
juce::StringArray PatternSerializer::holdModeNames()       { return { "Hold Off", "Latch", "Clean Hold" }; }

juce::StringArray PatternSerializer::styleNames()
{
    juce::StringArray r;
    for (int i = 0; i < (int) GeneratorStyle::numStyles; ++i)
        r.add (styleName ((GeneratorStyle) i));
    return r;
}

juce::String PatternSerializer::timeDivisionName (TimeDivision d)
{
    return timeDivisionNames()[juce::jlimit (0, (int) TimeDivision::numDivisions - 1, (int) d)];
}

TimeDivision PatternSerializer::timeDivisionFromName (const juce::String& s)
{
    const int i = timeDivisionNames().indexOf (s.trim());
    return i >= 0 ? (TimeDivision) i : TimeDivision::Sixteenth;
}

//==============================================================================
juce::var PatternSerializer::toVar (const Pattern& p, const PresetMeta& meta)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "PsytranceArpMatrix");
    root->setProperty ("version", 1);
    root->setProperty ("name", meta.name);
    root->setProperty ("category", meta.category);
    root->setProperty ("author", meta.author);
    if (meta.description.isNotEmpty())
        root->setProperty ("description", meta.description);
    root->setProperty ("bpm", p.bpm);
    root->setProperty ("key", ScaleLibrary::noteName (p.rootNote));
    root->setProperty ("rootNote", p.rootNote);
    root->setProperty ("scale", ScaleLibrary::get()[p.scaleIndex].name);
    root->setProperty ("length", p.channels[0].length);
    root->setProperty ("timeDivision", meta.timeDivision);
    root->setProperty ("swing", meta.swing);
    root->setProperty ("gate", meta.gateScale);
    root->setProperty ("lock", meta.lockToScale);
    root->setProperty ("arpMode", meta.arpMode);
    root->setProperty ("velocityMode", meta.velocityMode);

    juce::Array<juce::var> channels;
    for (int c = 0; c < kNumChannels; ++c)
    {
        const auto& ch = p.channels[(size_t) c];
        auto* co = new juce::DynamicObject();
        co->setProperty ("index", c + 1);
        co->setProperty ("enabled", ch.enabled);
        co->setProperty ("style", juce::String (styleName (ch.style)));
        co->setProperty ("midiChannel", ch.midiChannel);
        co->setProperty ("length", ch.length);
        juce::Array<juce::var> muted;
        for (auto b : ch.barMuted)
            muted.add (b);
        co->setProperty ("mutedBars", muted);

        juce::Array<juce::var> steps;
        for (int s = 0; s < ch.length; ++s)
        {
            const auto& st = ch.steps[(size_t) s];
            auto* so = new juce::DynamicObject();
            so->setProperty ("note", (int) st.note);
            so->setProperty ("velocity", (int) st.velocity);
            so->setProperty ("gate", std::round (st.gate * 1000.0f) / 1000.0f);
            so->setProperty ("slide", st.slide);
            so->setProperty ("accent", st.accent);
            so->setProperty ("active", st.active);
            so->setProperty ("chord", (int) st.chord);
            steps.add (juce::var (so));
        }
        co->setProperty ("steps", steps);
        channels.add (juce::var (co));
    }
    root->setProperty ("channels", channels);
    return juce::var (root);
}

juce::String PatternSerializer::toJson (const Pattern& p, const PresetMeta& meta)
{
    // LF line endings on every platform (stable diffs, git-friendly presets)
    return juce::JSON::toString (toVar (p, meta), false, 4).replace ("\r\n", "\n");
}

bool PatternSerializer::fromVar (const juce::var& v, Pattern& out, PresetMeta* meta)
{
    if (! v.isObject() || ! v.hasProperty ("channels") || ! v["channels"].isArray())
        return false;

    Pattern p;
    p.bpm = juce::jlimit (60.0, 220.0, (double) v.getProperty ("bpm", 142.0));

    int root = (int) v.getProperty ("rootNote", -1);
    if (root < 0)
        root = ScaleLibrary::pitchClassFromName (v.getProperty ("key", "F#").toString());
    p.rootNote = juce::jlimit (0, 11, root < 0 ? 6 : root);

    const int si = ScaleLibrary::get().indexOf (v.getProperty ("scale", "Phrygian").toString());
    p.scaleIndex = si >= 0 ? si : 5;

    const int globalLength = juce::jlimit (1, kMaxSteps, (int) v.getProperty ("length", 16));
    const auto* chans = v["channels"].getArray();

    for (int c = 0; c < kNumChannels; ++c)
    {
        auto& ch = p.channels[(size_t) c];
        ch.style = defaultStyleForChannel (c);
        ch.midiChannel = c + 1;
        ch.length = globalLength;
        ch.enabled = false;

        if (c >= chans->size())
            continue;

        const auto& cv = chans->getReference (c);
        ch.enabled = (bool) cv.getProperty ("enabled", false);
        ch.style = styleFromName (cv.getProperty ("style", styleName (ch.style)).toString().toRawUTF8());
        ch.midiChannel = juce::jlimit (1, 16, (int) cv.getProperty ("midiChannel", c + 1));
        ch.length = juce::jlimit (1, kMaxSteps, (int) cv.getProperty ("length", globalLength));
        if (const auto* muted = cv["mutedBars"].getArray())
            for (int b = 0; b < juce::jmin (muted->size(), kMaxSteps / 16); ++b)
                ch.barMuted[(size_t) b] = (bool) muted->getReference (b);

        if (const auto* steps = cv["steps"].getArray())
        {
            const int n = steps->size();
            for (int s = 0; s < kMaxSteps && n > 0; ++s)
            {
                // fewer stored steps than length: loop them (16-step file in a 32-step pattern)
                const auto& sv = steps->getReference (s % n);
                auto& st = ch.steps[(size_t) s];
                st.note     = (int8_t) juce::jlimit (0, 127, (int) sv.getProperty ("note", 60));
                st.velocity = (uint8_t) juce::jlimit (1, 127, (int) sv.getProperty ("velocity", 100));
                st.gate     = juce::jlimit (0.05f, 4.0f, (float) (double) sv.getProperty ("gate", 0.5));
                st.slide    = (bool) sv.getProperty ("slide", false);
                st.accent   = (bool) sv.getProperty ("accent", false);
                st.active   = (bool) sv.getProperty ("active", true);
                st.chord    = (uint8_t) juce::jlimit (1, 4, (int) sv.getProperty ("chord", 1));
            }
        }
    }

    if (meta != nullptr)
    {
        meta->name         = v.getProperty ("name", "").toString();
        meta->category     = v.getProperty ("category", "").toString();
        meta->author       = v.getProperty ("author", "").toString();
        meta->description  = v.getProperty ("description", "").toString();
        meta->timeDivision = v.getProperty ("timeDivision", "1/16").toString();
        meta->swing        = (float) (double) v.getProperty ("swing", 0.0);
        meta->gateScale    = (float) (double) v.getProperty ("gate", 1.0);
        meta->lockToScale  = (bool) v.getProperty ("lock", true);
        meta->arpMode      = v.getProperty ("arpMode", "Step").toString();
        meta->velocityMode = v.getProperty ("velocityMode", "Step").toString();
    }

    out = p;
    return true;
}

bool PatternSerializer::fromJson (const juce::String& json, Pattern& p, PresetMeta* meta)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed())
        return false;
    return fromVar (v, p, meta);
}
} // namespace pam
