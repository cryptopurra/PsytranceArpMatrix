#include "MultiMidiExporter.h"
#include <cmath>
#include "../Core/Scales.h"

namespace pam
{
juce::String MultiMidiExporter::trackName (const Pattern& p, int c)
{
    return "Ch" + juce::String (c + 1) + " " + styleName (p.channels[(size_t) c].style);
}

int MultiMidiExporter::totalBars (const Pattern& p, const Options& o)
{
    if (o.bars > 0)
        return o.bars;

    const double stepLen = stepLengthInQuarters (o.division);
    const double stepsPerBar = 4.0 / stepLen;
    int maxLen = 1;
    for (auto& ch : p.channels)
        if (ch.enabled)
            maxLen = juce::jmax (maxLen, ch.length);
    return juce::jmax (1, (int) std::ceil (maxLen / stepsPerBar - 1.0e-9));
}

void MultiMidiExporter::addHeader (juce::MidiMessageSequence& seq, const Pattern& p, const Options&,
                                   const juce::String& name, bool withTempo)
{
    auto nameMsg = juce::MidiMessage::textMetaEvent (3, name);
    nameMsg.setTimeStamp (0);
    seq.addEvent (nameMsg);

    if (withTempo)
    {
        auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / juce::jmax (1.0, p.bpm)));
        tempo.setTimeStamp (0);
        seq.addEvent (tempo);
        auto ts = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
        ts.setTimeStamp (0);
        seq.addEvent (ts);
    }
}

juce::MidiMessageSequence MultiMidiExporter::buildChannelSequence (const Pattern& p, int c, const Options& o)
{
    juce::MidiMessageSequence seq;
    if (c < 0 || c >= kNumChannels)
        return seq;

    const auto& ch = p.channels[(size_t) c];
    const auto& scale = ScaleLibrary::get()[p.scaleIndex];
    const double stepLen = stepLengthInQuarters (o.division);
    const double ticksPerStep = stepLen * o.ppq;
    const int stepsPerBar = juce::roundToInt (4.0 / stepLen);
    const int totalSteps = totalBars (p, o) * stepsPerBar;
    const int len = juce::jlimit (1, kMaxSteps, ch.length);
    const double swingTicks = juce::jlimit (0.0f, 1.0f, o.swing) * ticksPerStep / 3.0;
    const int midiCh = juce::jlimit (1, 16, ch.midiChannel);

    for (int k = 0; k < totalSteps; ++k)
    {
        const auto& st = ch.steps[(size_t) (k % len)];
        if (! st.active)
            continue;

        const double start = k * ticksPerStep + ((k % 2) == 1 ? swingTicks : 0.0);
        float gate = st.gate * o.gateScale;
        if (st.slide)
            gate = juce::jmax (gate, 1.08f);
        const double end = juce::jmin ((double) totalSteps * ticksPerStep, start + juce::jmax (0.02, (double) gate) * ticksPerStep);

        juce::Array<int> notes;
        if (st.chord > 1)
        {
            const int deg = ScaleLibrary::noteToDegree (st.note, p.rootNote, scale, 48);
            for (int v = 0; v < juce::jmin<int> (st.chord, 4); ++v)
                notes.add (ScaleLibrary::degreeToNote (deg + 2 * v, p.rootNote, scale, 48));
        }
        else
            notes.add (st.note);

        for (int n : notes)
        {
            n += 12 * o.octave + o.channelTranspose[(size_t) c];
            if (o.lockToScale)
                n = ScaleLibrary::quantize (n, p.rootNote, scale);
            n = juce::jlimit (0, 127, n);

            auto on = juce::MidiMessage::noteOn (midiCh, n, (juce::uint8) st.velocity);
            on.setTimeStamp (std::round (start));
            auto off = juce::MidiMessage::noteOff (midiCh, n);
            off.setTimeStamp (std::round (juce::jmax (start + 1.0, end)));
            seq.addEvent (on);
            seq.addEvent (off);
        }
    }

    seq.sort();
    seq.updateMatchedPairs();
    return seq;
}

juce::MidiFile MultiMidiExporter::buildChannelFile (const Pattern& p, int c, const Options& o)
{
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (o.ppq);

    juce::MidiMessageSequence track;
    addHeader (track, p, o, trackName (p, c), true);
    track.addSequence (buildChannelSequence (p, c, o), 0.0);
    track.updateMatchedPairs();

    const double endTick = totalBars (p, o) * 4.0 * o.ppq;
    auto eot = juce::MidiMessage::endOfTrack();
    eot.setTimeStamp (endTick);
    track.addEvent (eot);

    mf.addTrack (track);
    return mf;
}

juce::MidiFile MultiMidiExporter::buildMultiTrackFile (const Pattern& p, const Options& o)
{
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (o.ppq);
    const double endTick = totalBars (p, o) * 4.0 * o.ppq;

    juce::MidiMessageSequence conductor;
    addHeader (conductor, p, o, o.baseName, true);
    mf.addTrack (conductor);

    for (int c = 0; c < kNumChannels; ++c)
    {
        if (! p.channels[(size_t) c].enabled)
            continue;

        juce::MidiMessageSequence track;
        addHeader (track, p, o, trackName (p, c), false);
        track.addSequence (buildChannelSequence (p, c, o), 0.0);
        auto eot = juce::MidiMessage::endOfTrack();
        eot.setTimeStamp (endTick);
        track.addEvent (eot);
        mf.addTrack (track);
    }
    return mf;
}

bool MultiMidiExporter::writeFile (const juce::MidiFile& mf, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream out (f);
    if (! out.openedOk())
        return false;
    const bool ok = mf.writeTo (out, 1);
    out.flush();
    return ok;
}

juce::Array<juce::File> MultiMidiExporter::exportActiveChannels (const Pattern& p, const Options& o, const juce::File& dir)
{
    juce::Array<juce::File> files;
    dir.createDirectory();

    const auto key = ScaleLibrary::noteName (p.rootNote).replace ("#", "s");
    for (int c = 0; c < kNumChannels; ++c)
    {
        if (! p.channels[(size_t) c].enabled)
            continue;

        const auto style = juce::String (styleName (p.channels[(size_t) c].style))
                               .replace (" / ", "_").replace (" ", "_");
        auto f = dir.getChildFile (o.baseName + "_Ch" + juce::String (c + 1) + "_" + style + "_"
                                   + key + "_" + juce::String (juce::roundToInt (p.bpm)) + "bpm.mid");
        if (writeFile (buildChannelFile (p, c, o), f))
            files.add (f);
    }
    return files;
}

juce::File MultiMidiExporter::exportMultiTrack (const Pattern& p, const Options& o, const juce::File& dir)
{
    dir.createDirectory();
    auto f = dir.getChildFile (o.baseName + "_Multitrack_" + juce::String (p.numEnabled()) + "ch.mid");
    return writeFile (buildMultiTrackFile (p, o), f) ? f : juce::File();
}
} // namespace pam
