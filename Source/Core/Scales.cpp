#include "Scales.h"

namespace pam
{
std::atomic<int> ScaleLibrary::customMask { 0b010110101101 }; // natural minor by default

ScaleLibrary::ScaleLibrary()
{
    auto add = [this] (const char* n, std::initializer_list<int> iv)
    {
        Scale s;
        s.name = n;
        for (auto i : iv)
            s.intervals.add (i);
        scales.add (s);
    };

    add ("Major / Ionian",          { 0, 2, 4, 5, 7, 9, 11 });
    add ("Natural Minor / Aeolian", { 0, 2, 3, 5, 7, 8, 10 });
    add ("Harmonic Minor",          { 0, 2, 3, 5, 7, 8, 11 });
    add ("Melodic Minor",           { 0, 2, 3, 5, 7, 9, 11 });
    add ("Dorian",                  { 0, 2, 3, 5, 7, 9, 10 });
    add ("Phrygian",                { 0, 1, 3, 5, 7, 8, 10 });
    add ("Lydian",                  { 0, 2, 4, 6, 7, 9, 11 });
    add ("Mixolydian",              { 0, 2, 4, 5, 7, 9, 10 });
    add ("Locrian",                 { 0, 1, 3, 5, 6, 8, 10 });
    add ("Major Pentatonic",        { 0, 2, 4, 7, 9 });
    add ("Minor Pentatonic",        { 0, 3, 5, 7, 10 });
    add ("Minor Blues",             { 0, 3, 5, 6, 7, 10 });
    add ("Major Blues",             { 0, 2, 3, 4, 7, 9 });
    add ("Whole Tone",              { 0, 2, 4, 6, 8, 10 });
    add ("Diminished Whole-Half",   { 0, 2, 3, 5, 6, 8, 9, 11 });
    add ("Diminished Half-Whole",   { 0, 1, 3, 4, 6, 7, 9, 10 });
    add ("Chromatic",               { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 });
    add ("Phrygian Dominant",       { 0, 1, 4, 5, 7, 8, 10 });
    add ("Lydian Dominant",         { 0, 2, 4, 6, 7, 9, 10 });
    add ("Double Harmonic Major",   { 0, 1, 4, 5, 7, 8, 11 });
    add ("Hungarian Minor",         { 0, 2, 3, 6, 7, 8, 11 });
    add ("Persian",                 { 0, 1, 4, 5, 6, 8, 11 });
    add ("Spanish Eight Tone",      { 0, 1, 3, 4, 5, 6, 8, 10 });
    add ("In Sen",                  { 0, 1, 5, 7, 10 });
    add ("Hirajoshi",               { 0, 2, 3, 7, 8 });
    add ("Iwato",                   { 0, 1, 5, 6, 10 });
    add ("Egyptian Pentatonic",     { 0, 2, 5, 7, 10 });
    add ("Neapolitan Minor",        { 0, 1, 3, 5, 7, 8, 11 });
    add ("Neapolitan Major",        { 0, 1, 3, 5, 7, 9, 11 });
    add ("Enigmatic",               { 0, 1, 4, 6, 8, 10, 11 });
    add ("Altered / Super Locrian", { 0, 1, 3, 4, 6, 8, 10 });
    add ("Prometheus",              { 0, 2, 4, 6, 9, 10 });
    add ("Major Bebop",             { 0, 2, 4, 5, 7, 8, 9, 11 });
    add ("Dominant Bebop",          { 0, 2, 4, 5, 7, 9, 10, 11 });
    add ("Minor Bebop",             { 0, 2, 3, 4, 5, 7, 9, 10 });
    add ("Lydian Augmented",        { 0, 2, 4, 6, 8, 9, 11 });
    add ("Custom",                  { 0, 2, 3, 5, 7, 8, 10 }); // rebuilt from customMask
}

const ScaleLibrary& ScaleLibrary::get()
{
    static ScaleLibrary instance;
    return instance;
}

int ScaleLibrary::indexOf (const juce::String& name) const
{
    for (int i = 0; i < scales.size(); ++i)
        if (scales.getReference (i).name.equalsIgnoreCase (name))
            return i;

    // tolerate short names like "Aeolian" or "Ionian"
    for (int i = 0; i < scales.size(); ++i)
        if (scales.getReference (i).name.containsIgnoreCase (name))
            return i;

    return -1;
}

juce::StringArray ScaleLibrary::names() const
{
    juce::StringArray r;
    for (auto& s : scales)
        r.add (s.name);
    return r;
}

void ScaleLibrary::setCustomMask (int mask12)
{
    mask12 = (mask12 | 1) & 0xFFF;
    customMask.store (mask12);

    auto& lib = const_cast<ScaleLibrary&> (get());
    auto& custom = lib.scales.getReference (lib.customIndex());
    custom.intervals.clearQuick();
    for (int i = 0; i < 12; ++i)
        if ((mask12 >> i) & 1)
            custom.intervals.add (i);
}

int ScaleLibrary::getCustomMask() noexcept { return customMask.load(); }

//==============================================================================
int ScaleLibrary::quantize (int midiNote, int root, const Scale& scale) noexcept
{
    if (scale.size() == 0)
        return midiNote;

    for (int d = 0; d <= 6; ++d)
    {
        if (scale.contains (midiNote - d - root)) return juce::jlimit (0, 127, midiNote - d);
        if (scale.contains (midiNote + d - root)) return juce::jlimit (0, 127, midiNote + d);
    }
    return midiNote;
}

int ScaleLibrary::degreeToNote (int degree, int root, const Scale& scale, int baseOctaveNote) noexcept
{
    const int n = juce::jmax (1, scale.size());
    int octave = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
    int idx = degree - octave * n;
    const int base = baseOctaveNote - (((baseOctaveNote - root) % 12) + 12) % 12; // root note at/below base
    return juce::jlimit (0, 127, base + octave * 12 + scale.intervals[idx]);
}

int ScaleLibrary::noteToDegree (int midiNote, int root, const Scale& scale, int baseOctaveNote) noexcept
{
    const int q = quantize (midiNote, root, scale);
    const int n = juce::jmax (1, scale.size());
    const int base = baseOctaveNote - (((baseOctaveNote - root) % 12) + 12) % 12;
    const int rel = q - base;
    int octave = rel >= 0 ? rel / 12 : -((-rel + 11) / 12);
    const int pc = rel - octave * 12;
    int idx = scale.intervals.indexOf (pc);
    if (idx < 0) idx = 0;
    return octave * n + idx;
}

juce::String ScaleLibrary::noteName (int pitchClass)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((pitchClass % 12) + 12) % 12];
}

int ScaleLibrary::pitchClassFromName (const juce::String& nameIn)
{
    auto name = nameIn.trim().toUpperCase();
    if (name.isEmpty())
        return -1;

    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    const auto c = name[0];
    if (c < 'A' || c > 'G')
        return -1;

    int pc = base[c - 'A'];
    for (int i = 1; i < name.length(); ++i)
    {
        if (name[i] == '#')      ++pc;
        else if (name[i] == 'B') --pc;
        else break;
    }
    return (pc + 12) % 12;
}
} // namespace pam
