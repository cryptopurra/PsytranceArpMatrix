#include "MidiAnalyzer.h"

namespace pam
{
juce::String KeyDetection::describe() const
{
    if (! valid)
        return "No key detected";

    return ScaleLibrary::noteName (rootNote) + " " + ScaleLibrary::get()[scaleIndex].name
         + " (" + juce::String (juce::roundToInt (confidence * 100.0f)) + "%)";
}

//==============================================================================
static const char* const kPsyPriority[] = { "Natural Minor / Aeolian", "Phrygian", "Harmonic Minor",
                                            "Phrygian Dominant", "Hungarian Minor", "Dorian",
                                            "Minor Pentatonic", "Double Harmonic Major" };

KeyDetection MidiAnalyzer::detectFromHistogram (const std::array<float, 12>& hist,
                                                int lowestPc, int firstPc)
{
    KeyDetection best;
    float total = 0.0f;
    int used = 0;
    for (auto h : hist)
    {
        total += h;
        used += h > 0.0f ? 1 : 0;
    }

    if (total <= 0.0f)
        return best;

    const auto& lib = ScaleLibrary::get();
    float bestScore = -1.0e9f;

    for (int si = 0; si < lib.customIndex(); ++si)
    {
        const auto& sc = lib[si];
        if (sc.name == "Chromatic")
            continue;

        float priority = 0.0f;
        for (int p = 0; p < (int) std::size (kPsyPriority); ++p)
            if (sc.name == kPsyPriority[p])
                priority = 0.004f * (float) ((int) std::size (kPsyPriority) - p);

        for (int root = 0; root < 12; ++root)
        {
            float inside = 0.0f;
            for (auto iv : sc.intervals)
                inside += hist[(size_t) ((root + iv) % 12)];
            inside /= total;

            float score = inside;
            score -= 0.015f * (float) juce::jmax (0, sc.size() - used);
            score += 0.06f * hist[(size_t) root] / total;
            score += root == lowestPc ? 0.04f : 0.0f;
            score += root == firstPc ? 0.02f : 0.0f;
            if (sc.intervals.contains (7))
                score += 0.03f * hist[(size_t) ((root + 7) % 12)] / total;
            score += priority;

            if (score > bestScore)
            {
                bestScore = score;
                best.rootNote = root;
                best.scaleIndex = si;
                best.confidence = inside;
                best.valid = true;
            }
        }
    }
    return best;
}

KeyDetection MidiAnalyzer::detect (const juce::Array<AnalyzedNote>& notes)
{
    if (notes.isEmpty())
        return {};

    std::array<float, 12> hist {};
    int lowest = 128;
    const AnalyzedNote* firstNote = nullptr;

    for (auto& n : notes)
    {
        const double durTicks = juce::jmax (1.0, (n.end - n.start) * 96.0); // same units as the python tool (ppq 96)
        hist[(size_t) (n.pitch % 12)] += (float) (std::sqrt (durTicks) * (n.velocity / 100.0 + 0.5));
        lowest = juce::jmin (lowest, n.pitch);
        if (firstNote == nullptr || n.start < firstNote->start
            || (juce::approximatelyEqual (n.start, firstNote->start) && n.pitch < firstNote->pitch))
            firstNote = &n;
    }

    return detectFromHistogram (hist, lowest % 12, firstNote->pitch % 12);
}

juce::Array<AnalyzedNote> MidiAnalyzer::extractNotes (const juce::MidiFile& fileIn)
{
    juce::Array<AnalyzedNote> out;
    const double ppq = fileIn.getTimeFormat() > 0 ? (double) fileIn.getTimeFormat() : 960.0;

    for (int t = 0; t < fileIn.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq (*fileIn.getTrack (t));
        seq.updateMatchedPairs();

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* ev = seq.getEventPointer (i);
            if (! ev->message.isNoteOn())
                continue;

            AnalyzedNote n;
            n.start = ev->message.getTimeStamp() / ppq;
            n.end = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() / ppq
                                                 : n.start + 0.25;
            n.end = juce::jmax (n.end, n.start + 1.0 / 96.0);
            n.pitch = ev->message.getNoteNumber();
            n.velocity = ev->message.getVelocity();
            n.channel = ev->message.getChannel();
            out.add (n);
        }
    }

    std::sort (out.begin(), out.end(), [] (const AnalyzedNote& a, const AnalyzedNote& b)
    {
        return a.start < b.start || (juce::approximatelyEqual (a.start, b.start) && a.pitch < b.pitch);
    });
    return out;
}

juce::Array<AnalyzedNote> MidiAnalyzer::extractNotes (const juce::File& f)
{
    juce::FileInputStream in (f);
    juce::MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in))
        return {};
    return extractNotes (mf);
}

//==============================================================================
void RealtimeKeyTracker::noteOn (int midiNote, int velocity) noexcept
{
    auto& h = hist[(size_t) (midiNote % 12)];
    h.store (h.load() + 1.0f + velocity / 254.0f);

    if (seen.fetch_add (1) == 0)
        first.store (midiNote % 12);

    if (midiNote < lowestNote.load())
    {
        lowestNote.store (midiNote);
        lowest.store (midiNote % 12);
    }
}

void RealtimeKeyTracker::decay (float factor) noexcept
{
    for (auto& h : hist)
        h.store (h.load() * factor);
}

void RealtimeKeyTracker::reset() noexcept
{
    for (auto& h : hist)
        h.store (0.0f);
    lowest.store (-1);
    first.store (-1);
    lowestNote.store (128);
    seen.store (0);
}

KeyDetection RealtimeKeyTracker::getDetection() const
{
    std::array<float, 12> h {};
    for (size_t i = 0; i < 12; ++i)
        h[i] = hist[i].load();

    if (seen.load() < 3)
        return {};

    return MidiAnalyzer::detectFromHistogram (h, lowest.load(), first.load());
}
} // namespace pam
