#include "MidiPatternImporter.h"

#include <map>

namespace pam
{
MidiPatternImporter::Result MidiPatternImporter::importNotes (const juce::Array<AnalyzedNote>& all, const Pattern& base,
                                                              int targetChannel, bool applyKey, bool mergeIntoTarget)
{
    Result r;
    r.pattern = base;

    if (all.isEmpty())
    {
        r.error = "The MIDI file contains no notes";
        return r;
    }

    r.key = MidiAnalyzer::detect (all);
    if (applyKey && r.key.valid)
    {
        r.pattern.rootNote = r.key.rootNote;
        r.pattern.scaleIndex = r.key.scaleIndex;
    }

    // group by MIDI channel (multi-channel files map to several matrix channels)
    std::map<int, juce::Array<AnalyzedNote>> groups;
    for (auto& n : all)
        groups[mergeIntoTarget ? 0 : n.channel].add (n);

    const bool multi = groups.size() > 1;
    int matrixChannel = multi ? 0 : juce::jlimit (0, kNumChannels - 1, targetChannel);

    // shift so the first note's bar starts at step 0
    double firstStart = all.getFirst().start;
    for (auto& n : all)
        firstStart = juce::jmin (firstStart, n.start);
    const double barOffset = std::floor (firstStart / 4.0) * 4.0;

    for (auto& [midiCh, notes] : groups)
    {
        if (matrixChannel >= kNumChannels)
            break;

        auto& ch = r.pattern.channels[(size_t) matrixChannel];
        for (auto& st : ch.steps)
            st = Step();

        int lastStep = 0;
        std::array<int, kMaxSteps> voices {};
        juce::Array<int> vels;
        for (auto& n : notes)
            vels.add (n.velocity);
        vels.sort();
        const int median = vels[vels.size() / 2];
        const bool dynamic = vels.getLast() - vels.getFirst() >= 12;

        for (auto& n : notes)
        {
            const int s = juce::roundToInt ((n.start - barOffset) / 0.25);
            if (s < 0 || s >= kMaxSteps)
                continue;

            auto& st = ch.steps[(size_t) s];
            ++voices[(size_t) s];
            if (st.active && n.pitch < st.note)
                continue; // keep the top voice

            st.active = true;
            st.note = (int8_t) n.pitch;
            st.velocity = (uint8_t) juce::jlimit (1, 127, n.velocity);
            st.gate = (float) juce::jlimit (0.05, 4.0, (n.end - n.start) / 0.25);
            st.accent = dynamic && n.velocity >= median + 10;
            lastStep = juce::jmax (lastStep, s);
            ++r.notesImported;
        }

        for (int s = 0; s < kMaxSteps; ++s)
        {
            auto& st = ch.steps[(size_t) s];
            st.chord = (uint8_t) juce::jlimit (1, 4, voices[(size_t) s]);
            if (st.active && st.gate > 1.0f && s + 1 < kMaxSteps && ch.steps[(size_t) s + 1].active)
                st.slide = true;
        }

        ch.length = juce::jlimit (16, kMaxSteps, ((lastStep / 16) + 1) * 16);
        ch.enabled = true;
        ch.midiChannel = matrixChannel + 1;
        ch.userEdited = true;
        ++matrixChannel;
        ++r.channelsFilled;
    }

    // all channels share the global length
    int len = 16;
    for (auto& ch : r.pattern.channels)
        if (ch.enabled)
            len = juce::jmax (len, ch.length);
    for (auto& ch : r.pattern.channels)
        ch.length = len;

    return r;
}

MidiPatternImporter::Result MidiPatternImporter::importFile (const juce::File& file, const Pattern& base,
                                                             int targetChannel, bool applyKey, bool mergeIntoTarget)
{
    juce::FileInputStream in (file);
    juce::MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in))
    {
        Result r;
        r.pattern = base;
        r.error = "Cannot read MIDI file: " + file.getFileName();
        return r;
    }

    auto notes = MidiAnalyzer::extractNotes (mf);
    return importNotes (notes, base, targetChannel, applyKey, mergeIntoTarget);
}
} // namespace pam
