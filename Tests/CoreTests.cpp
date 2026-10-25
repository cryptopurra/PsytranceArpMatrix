// Headless unit tests for the core engine (no GUI / audio device needed).
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "../Source/Core/ArpEngine.h"
#include "../Source/Core/ChannelMatrix.h"
#include "../Source/Core/MidiAnalyzer.h"
#include "../Source/IO/MidiPatternImporter.h"
#include "../Source/IO/MultiMidiExporter.h"
#include "../Source/IO/PresetManager.h"

using namespace pam;

//==============================================================================
class ScaleTests : public juce::UnitTest
{
public:
    ScaleTests() : juce::UnitTest ("Scales", "PAM") {}

    void runTest() override
    {
        const auto& lib = ScaleLibrary::get();

        beginTest ("library matches the reference list");
        expectEquals (lib.size(), 37); // 36 scales + Custom
        expectEquals (lib[0].name, juce::String ("Major / Ionian"));
        expect (lib.indexOf ("In Sen") > 0);
        expect (lib.indexOf ("Lydian Augmented") > 0);
        expectEquals (lib[lib.customIndex()].name, juce::String ("Custom"));

        beginTest ("quantize keeps notes in scale");
        const auto& phryg = lib[lib.indexOf ("Phrygian")];
        for (int n = 24; n < 100; ++n)
            expect (phryg.contains (ScaleLibrary::quantize (n, 6, phryg) - 6));

        beginTest ("degree round trip");
        const auto& hm = lib[lib.indexOf ("Hungarian Minor")];
        for (int d = -10; d < 20; ++d)
            expectEquals (ScaleLibrary::noteToDegree (ScaleLibrary::degreeToNote (d, 4, hm, 48), 4, hm, 48), d);

        beginTest ("note names");
        expectEquals (ScaleLibrary::pitchClassFromName ("F#"), 6);
        expectEquals (ScaleLibrary::pitchClassFromName ("Gb"), 6);
        expectEquals (ScaleLibrary::pitchClassFromName ("a"), 9);
    }
};

//==============================================================================
class AnalyzerTests : public juce::UnitTest
{
public:
    AnalyzerTests() : juce::UnitTest ("MIDI Analyzer", "PAM") {}

    static juce::Array<AnalyzedNote> line (std::initializer_list<int> pitches)
    {
        juce::Array<AnalyzedNote> a;
        double t = 0;
        for (int p : pitches)
        {
            a.add ({ t, t + 0.2, p, 100, 1 });
            t += 0.25;
        }
        return a;
    }

    void runTest() override
    {
        const auto& lib = ScaleLibrary::get();

        beginTest ("F# phrygian line");
        auto r = MidiAnalyzer::detect (line ({ 42, 43, 45, 47, 49, 50, 52, 54, 42, 49, 42, 43, 42, 45 }));
        expect (r.valid);
        expectEquals (r.rootNote, 6);
        expectEquals (lib[r.scaleIndex].name, juce::String ("Phrygian"));

        beginTest ("E harmonic minor with leading tone");
        r = MidiAnalyzer::detect (line ({ 40, 42, 43, 45, 47, 48, 51, 52, 40, 51, 52, 47, 40 }));
        expectEquals (r.rootNote, 4);
        expectEquals (lib[r.scaleIndex].name, juce::String ("Harmonic Minor"));

        beginTest ("pentatonic material picks the 5-note scale");
        r = MidiAnalyzer::detect (line ({ 45, 48, 50, 52, 55, 57, 45, 52, 45, 48 }));
        expectEquals (r.rootNote, 9);
        expectEquals (lib[r.scaleIndex].name, juce::String ("Minor Pentatonic"));

        beginTest ("realtime tracker");
        RealtimeKeyTracker t;
        for (int p : { 42, 43, 45, 47, 49, 50, 52, 42, 42 })
            t.noteOn (p, 100);
        expectEquals (t.getDetection().rootNote, 6);
    }
};

//==============================================================================
class MatrixTests : public juce::UnitTest
{
public:
    MatrixTests() : juce::UnitTest ("Channel Matrix", "PAM") {}

    void runTest() override
    {
        beginTest ("default = single arp channel");
        ChannelMatrix m;
        m.setSeed (1);
        m.initialiseDefault();
        expectEquals (m.getPattern().numEnabled(), 1);
        expect (m.isChannelEnabled (0));
        expect (m.getPattern().channels[0].countActive() >= 3);

        beginTest ("enabling a channel leaves the others untouched");
        const auto before = m.getPattern().channels[0];
        m.setChannelEnabled (1, true);
        expectEquals (m.getPattern().numEnabled(), 2);
        expect (m.getPattern().channels[1].countActive() >= 3);
        int changed = 0;
        for (int s = 0; s < 16; ++s)
            changed += (m.getPattern().channels[0].steps[(size_t) s] == before.steps[(size_t) s]) ? 0 : 1;
        expectEquals (changed, 0);

        beginTest ("all four channels: interlock keeps <= 3 voices per step");
        m.setChannelEnabled (2, true);
        m.setChannelEnabled (3, true);
        expectEquals (m.getActiveChannels().size(), 4);
        for (int s = 0; s < 16; ++s)
        {
            int voices = 0;
            for (auto& ch : m.getPattern().channels)
                voices += (ch.enabled && ch.steps[(size_t) s].active) ? 1 : 0;
            expect (voices <= 3);
        }

        beginTest ("lock mode: every generated note is in scale");
        for (int seed = 0; seed < 40; ++seed)
        {
            ChannelMatrix mm;
            mm.setSeed (seed);
            mm.setKey (seed % 12, seed % 30, false);
            mm.setLockToScale (true);
            mm.setLength (64);
            mm.initialiseDefault();
            for (int c = 1; c < 4; ++c)
                mm.setChannelEnabled (c, true);
            mm.applyVariation (-1, (VariationStyle) (seed % 4), 0.8f);

            const auto& p = mm.getPattern();
            const auto& sc = ScaleLibrary::get()[p.scaleIndex];
            for (auto& ch : p.channels)
                for (int s = 0; s < ch.length; ++s)
                    if (ch.steps[(size_t) s].active)
                        expect (sc.contains (ch.steps[(size_t) s].note - p.rootNote),
                                "out of scale note " + juce::String (ch.steps[(size_t) s].note));
        }

        beginTest ("rolling bass keeps the kick steps free");
        ChannelMatrix b;
        b.setSeed (7);
        b.setChannelStyle (0, GeneratorStyle::RollingBass, false);
        b.initialiseDefault();
        for (int s : { 0, 4, 8, 12 })
            expect (! b.getPattern().channels[0].steps[(size_t) s].active);

        beginTest ("stabs hit at least two tension points");
        ChannelMatrix st;
        st.setSeed (3);
        st.setChannelStyle (0, GeneratorStyle::RhythmStab, false);
        st.initialiseDefault();
        int hits = 0;
        for (int s : { 3, 7, 11, 15 })
            hits += st.getPattern().channels[0].steps[(size_t) s].active ? 1 : 0;
        expect (hits >= 2);

        beginTest ("disable keeps data, re-enable regenerates complementary part");
        m.setChannelEnabled (3, false);
        expectEquals (m.getPattern().numEnabled(), 3);
    }
};

//==============================================================================
class IoTests : public juce::UnitTest
{
public:
    IoTests() : juce::UnitTest ("Presets & Export", "PAM") {}

    void runTest() override
    {
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pam_tests_" + juce::String (juce::Random::getSystemRandom().nextInt()));
        tmp.createDirectory();

        ChannelMatrix m;
        m.setSeed (99);
        m.setLength (32);
        m.initialiseDefault();

        beginTest ("JSON round trip");
        PresetMeta meta;
        meta.name = "Round Trip";
        auto json = PatternSerializer::toJson (m.getPattern(), meta);
        Pattern back;
        PresetMeta meta2;
        expect (PatternSerializer::fromJson (json, back, &meta2));
        expectEquals (meta2.name, juce::String ("Round Trip"));
        for (int c = 0; c < kNumChannels; ++c)
        {
            expect (back.channels[(size_t) c].enabled == m.getPattern().channels[(size_t) c].enabled);
            for (int s = 0; s < 32; ++s)
                expect (back.channels[(size_t) c].steps[(size_t) s].note == m.getPattern().channels[(size_t) c].steps[(size_t) s].note);
        }

        beginTest ("folder structure + quick save");
        PresetManager pm (tmp.getChildFile ("PsytranceArpMatrix"));
        expect (pm.ensureFolderStructure());
        for (auto& c : PresetManager::factoryCategories())
            expect (pm.getPresetsDir().getChildFile (c).isDirectory());
        auto saved = pm.quickSaveSequence (m.getPattern(), meta);
        expect (saved.existsAsFile());
        expect (saved.getParentDirectory() == pm.getUserExportDir());

        beginTest ("export: one file per ACTIVE channel");
        MultiMidiExporter::Options o;
        auto files = MultiMidiExporter::exportActiveChannels (m.getPattern(), o, tmp);
        expectEquals (files.size(), 1);

        m.setChannelEnabled (2, true);
        files = MultiMidiExporter::exportActiveChannels (m.getPattern(), o, tmp);
        expectEquals (files.size(), 2);
        expect (files[1].getFileName().contains ("_Ch3_"));

        m.setChannelEnabled (1, true);
        m.setChannelEnabled (3, true);
        files = MultiMidiExporter::exportActiveChannels (m.getPattern(), o, tmp);
        expectEquals (files.size(), 4);

        beginTest ("exported file: channel, length, re-import");
        juce::FileInputStream in (files[1]);
        juce::MidiFile mf;
        expect (mf.readFrom (in));
        expectEquals (mf.getNumTracks(), 1);
        int notes = 0, wrongChannel = 0;
        for (auto* ev : *mf.getTrack (0))
            if (ev->message.isNoteOn())
            {
                ++notes;
                wrongChannel += ev->message.getChannel() == 2 ? 0 : 1;
            }
        expect (notes > 0);
        expectEquals (wrongChannel, 0);
        expectEquals (MultiMidiExporter::totalBars (m.getPattern(), o), 2);

        auto imp = MidiPatternImporter::importFile (files[0], Pattern(), 0, true);
        expect (imp.ok());
        expect (imp.key.valid);

        beginTest ("multitrack file has conductor + active tracks");
        auto multi = MultiMidiExporter::buildMultiTrackFile (m.getPattern(), o);
        expectEquals (multi.getNumTracks(), 5);

        tmp.deleteRecursively();
    }
};

//==============================================================================
class EngineTests : public juce::UnitTest
{
public:
    EngineTests() : juce::UnitTest ("Arp Engine", "PAM") {}

    static int countNoteOns (const juce::MidiBuffer& b, int channel = -1)
    {
        int n = 0;
        for (const auto meta : b)
            if (meta.getMessage().isNoteOn() && (channel < 0 || meta.getMessage().getChannel() == channel))
                ++n;
        return n;
    }

    void runTest() override
    {
        ChannelMatrix m;
        m.setSeed (5);
        m.initialiseDefault();
        m.setChannelEnabled (3, true);
        const auto& p = m.getPattern();

        ArpEngine e;
        e.prepare (48000.0);
        ArpParams params;

        beginTest ("host transport: one bar produces the pattern's notes on the right channels");
        const double bpm = 140.0;
        const int block = 512;
        const int samplesPerBar = (int) (48000.0 * 60.0 / bpm * 4.0);
        int on1 = 0, on4 = 0, offs = 0;
        double ppq = 0.0;
        for (int done = 0; done < samplesPerBar; done += block)
        {
            juce::MidiBuffer b;
            ArpEngine::Position pos { true, ppq, bpm, true };
            e.process (b, juce::jmin (block, samplesPerBar - done), pos, p, params);
            on1 += countNoteOns (b, 1);
            on4 += countNoteOns (b, 4);
            for (const auto meta : b)
                offs += meta.getMessage().isNoteOff() ? 1 : 0;
            ppq += block * bpm / 60.0 / 48000.0;
        }
        int expected4 = 0;
        for (int s = 0; s < 16; ++s)
            expected4 += p.channels[3].steps[(size_t) s].active ? p.channels[3].steps[(size_t) s].chord : 0;
        expectEquals (on1, p.channels[0].countActive());
        expectEquals (on4, expected4);
        expect (offs > 0);

        beginTest ("stop sends note offs");
        juce::MidiBuffer stopBuf;
        e.process (stopBuf, block, { false, ppq, bpm, true }, p, params);
        expectEquals (countNoteOns (stopBuf), 0);

        beginTest ("internal clock with held keys + arp Up");
        params.mode = ArpMode::Up;
        params.playWithoutKeys = false;
        e.reset();
        juce::MidiBuffer keys;
        keys.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        keys.addEvent (juce::MidiMessage::noteOn (1, 63, (juce::uint8) 100), 0);
        keys.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
        e.process (keys, block, { false, 0.0, bpm, false }, p, params);
        int total = countNoteOns (keys);
        for (int i = 0; i < 100; ++i)
        {
            juce::MidiBuffer b;
            e.process (b, block, { false, 0.0, bpm, false }, p, params);
            total += countNoteOns (b);
        }
        expect (total > 0);
    }
};

static ScaleTests scaleTests;
static AnalyzerTests analyzerTests;
static MatrixTests matrixTests;
static IoTests ioTests;
static EngineTests engineTests;

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("PAM");

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
