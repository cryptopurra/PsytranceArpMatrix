#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace pam
{
namespace
{
    juce::StringArray rootNames()
    {
        juce::StringArray r;
        for (int i = 0; i < 12; ++i)
            r.add (ScaleLibrary::noteName (i));
        return r;
    }

    int choiceIndex (const juce::AudioProcessorValueTreeState& s, const char* id)
    {
        return juce::roundToInt (s.getRawParameterValue (id)->load());
    }

    float value (const juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        return s.getRawParameterValue (id)->load();
    }

    void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plainValue)
    {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    }

    /** RAII guard for the "ignore my own parameter writes" flag. */
    struct Suppress
    {
        explicit Suppress (std::atomic<bool>& f) : flag (f) { flag.store (true); }
        ~Suppress() { flag.store (false); }
        std::atomic<bool>& flag;
    };

    std::atomic<bool> gSuppress { false }; // one per process is enough: only the message thread writes
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withLabel ("%");

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::root, 1 }, "Root", rootNames(), 6));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::scale, 1 }, "Scale", ScaleLibrary::get().names(), 5));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::lock, 1 }, "Lock To Scale", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::autoMatch, 1 }, "Auto Match Key", false));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { ParamID::length, 1 }, "Length", 1, kMaxSteps, 16));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::density, 1 }, "Density", NormalisableRange<float> (25.0f, 150.0f, 1.0f), 100.0f, pct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::division, 1 }, "Time / Sync", PatternSerializer::timeDivisionNames(), 3));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::swing, 1 }, "Swing", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::gate, 1 }, "Gate Length", NormalisableRange<float> (5.0f, 200.0f, 1.0f), 100.0f, pct));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { ParamID::octave, 1 }, "Octave", -3, 3, 0));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { ParamID::octRange, 1 }, "Octave Range", 1, 4, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::arpMode, 1 }, "Mode", PatternSerializer::arpModeNames(), (int) ArpMode::Step));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::velMode, 1 }, "Velocity Mode", PatternSerializer::velocityModeNames(), (int) VelocityMode::Step));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { ParamID::velocity, 1 }, "Velocity", 1, 127, 100));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::holdMode, 1 }, "Hold", PatternSerializer::holdModeNames(), (int) HoldMode::Off));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::keyFollow, 1 }, "Key Follow", false));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::freeRun, 1 }, "Play Without Keys", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::mergeOut, 1 }, "MIDI Output: Master Ch 1", false));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::muteAll, 1 }, "Mute All (no MIDI out)", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::varStyle, 1 }, "Variation Style", PatternSerializer::variationStyleNames(), 0));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::varAmount, 1 }, "Variation", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 65.0f, pct));

    for (int c = 0; c < kNumChannels; ++c)
    {
        l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::channelOn (c), 1 },
                                                     "Ch" + String (c + 1) + " On", c == 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::channelStyle (c), 1 },
                                                       "Ch" + String (c + 1) + " Style",
                                                       PatternSerializer::styleNames(), (int) defaultStyleForChannel (c)));
        l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::channelMidi (c), 1 },
                                                    "Ch" + String (c + 1) + " MIDI Out Channel", 1, 16, c + 1));
        l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::channelOct (c), 1 },
                                                    "Ch" + String (c + 1) + " Octave", -3, 3, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::channelVoice (c), 1 },
                                                       "Ch" + String (c + 1) + " Voice", StringArray { "Poly", "Mono", "Legato" },
                                                       c == 0 ? 1 : 0));
        l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::channelGlide (c), 1 },
                                                    "Ch" + String (c + 1) + " Glide (CC5)", 0, 127, 0));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::channelMute (c), 1 },
                                                     "Ch" + String (c + 1) + " Mute", false));
    }
    return l;
}

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                      ),
      apvts (*this, nullptr, "PsytranceArpMatrix", createLayout())
{
    for (auto* id : { ParamID::root, ParamID::scale, ParamID::lock, ParamID::length, ParamID::density })
        apvts.addParameterListener (id, this);
    for (int c = 0; c < kNumChannels; ++c)
    {
        apvts.addParameterListener (ParamID::channelOn (c), this);
        apvts.addParameterListener (ParamID::channelStyle (c), this);
        apvts.addParameterListener (ParamID::channelMidi (c), this);
    }

    for (int c = 0; c < kNumChannels; ++c)
    {
        octParams[(size_t) c] = apvts.getRawParameterValue (ParamID::channelOct (c));
        voiceParams[(size_t) c] = apvts.getRawParameterValue (ParamID::channelVoice (c));
        glideParams[(size_t) c] = apvts.getRawParameterValue (ParamID::channelGlide (c));
        muteParams[(size_t) c] = apvts.getRawParameterValue (ParamID::channelMute (c));
    }
    muteAllParam = apvts.getRawParameterValue (ParamID::muteAll);

    presets.ensureFolderStructure();
    presets.installFactoryPresets (getFactoryPresets(), false);
    presets.cleanDragCache();

    // Single Arp Mode on startup
    matrix.setSeed (juce::Time::currentTimeMillis());
    matrix.setKey (choiceIndex (apvts, ParamID::root), choiceIndex (apvts, ParamID::scale), false);
    matrix.setLockToScale (value (apvts, ParamID::lock) > 0.5f);
    matrix.setLength ((int) value (apvts, ParamID::length));
    for (int c = 0; c < kNumChannels; ++c)
    {
        matrix.setChannelStyle (c, (GeneratorStyle) choiceIndex (apvts, ParamID::channelStyle (c).toRawUTF8()), false);
        matrix.setChannelMidiChannel (c, juce::roundToInt (value (apvts, ParamID::channelMidi (c))));
    }
    matrix.initialiseDefault();
    publishPattern();

    startTimerHz (4);
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
    cancelPendingUpdate();
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

ArpParams PluginProcessor::readArpParams() const
{
    ArpParams p;
    p.mode            = (ArpMode) choiceIndex (apvts, ParamID::arpMode);
    p.velocityMode    = (VelocityMode) choiceIndex (apvts, ParamID::velMode);
    p.holdMode        = (HoldMode) choiceIndex (apvts, ParamID::holdMode);
    p.division        = (TimeDivision) choiceIndex (apvts, ParamID::division);
    p.swing           = value (apvts, ParamID::swing) / 100.0f;
    p.gateScale       = value (apvts, ParamID::gate) / 100.0f;
    p.octave          = juce::roundToInt (value (apvts, ParamID::octave));
    p.octaveRange     = juce::roundToInt (value (apvts, ParamID::octRange));
    p.holdVelocity    = juce::roundToInt (value (apvts, ParamID::velocity));
    p.lockToScale     = value (apvts, ParamID::lock) > 0.5f;
    p.keyFollow       = value (apvts, ParamID::keyFollow) > 0.5f;
    p.playWithoutKeys = value (apvts, ParamID::freeRun) > 0.5f;
    p.mergeToChannel1 = value (apvts, ParamID::mergeOut) > 0.5f;
    p.internalBpm     = (float) audioPattern.bpm;
    for (int c = 0; c < kNumChannels; ++c) // cached parameter pointers: no allocation on the audio thread
    {
        p.channelTranspose[(size_t) c] = 12 * juce::roundToInt (octParams[(size_t) c]->load());
        p.voiceMode[(size_t) c] = (VoiceMode) juce::roundToInt (voiceParams[(size_t) c]->load());
        const int g = juce::roundToInt (glideParams[(size_t) c]->load());
        p.glide[(size_t) c] = g > 0 ? g : -1;
        p.channelMuted[(size_t) c] = muteParams[(size_t) c]->load() > 0.5f || muteAllParam->load() > 0.5f;
    }
    return p;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (patternDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (patternLock);
        if (lock.isLocked())
        {
            audioPattern = pendingPattern;
            patternDirty.store (false);
        }
    }

    if (panicRequested.exchange (false))
    {
        midi.clear();
        engine.stopAll (midi, readArpParams());
        engine.reset();
        for (int ch = 1; ch <= 16; ++ch)
        {
            midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
            midi.addEvent (juce::MidiMessage::allSoundOff (ch), 0);
        }
        return;
    }
    wasBypassed = false;

    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn())
            keyTracker.noteOn (meta.getMessage().getNoteNumber(), meta.getMessage().getVelocity());

    ArpEngine::Position pos;
    pos.allowInternalClock = wrapperType == wrapperType_Standalone;
    if (auto* ph = getPlayHead())
    {
        if (auto info = ph->getPosition())
        {
            pos.hostPlaying = info->getIsPlaying();
            if (auto ppq = info->getPpqPosition())
            {
                pos.ppq = *ppq;
                pos.hasHostPosition = true;
            }
            else
            {
                pos.allowInternalClock = true; // host without a musical position: free run on held keys
            }
            if (info->getIsLooping())
                if (auto loop = info->getLoopPoints())
                {
                    pos.isLooping = true;
                    pos.loopStart = loop->ppqStart;
                    pos.loopEnd = loop->ppqEnd;
                }
            if (auto bpm = info->getBpm())
            {
                pos.bpm = *bpm;
                lastHostBpm = *bpm;
            }
        }
    }

    engine.process (midi, buffer.getNumSamples(), pos, audioPattern, readArpParams());

    // note previews requested by the editor
    {
        const auto scope = previewFifo.read (previewFifo.getNumReady());
        auto play = [&] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                const auto& ev = previewEvents[(size_t) i];
                const int n = juce::jlimit (0, 127, ev.note);
                if (ev.velocity > 0)
                {
                    midi.addEvent (juce::MidiMessage::noteOn (ev.channel, n, (juce::uint8) juce::jlimit (1, 127, ev.velocity)), 0);
                    previewSounding[(size_t) n] = ev.channel + 1;
                }
                else if (previewSounding[(size_t) n] != 0)
                {
                    midi.addEvent (juce::MidiMessage::noteOff (previewSounding[(size_t) n] - 1, n), 0);
                    previewSounding[(size_t) n] = 0;
                }
            }
        };
        play (scope.startIndex1, scope.blockSize1);
        play (scope.startIndex2, scope.blockSize2);
    }
}

void PluginProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Bypass must silence the synths we drive: close every note we started, once.
    buffer.clear();
    midi.clear();
    if (! wasBypassed)
    {
        engine.stopAll (midi, readArpParams());
        engine.reset();
        wasBypassed = true;
    }
}

//==============================================================================
void PluginProcessor::parameterChanged (const juce::String& id, float)
{
    if (gSuppress.load())
        return;

    if (id == ParamID::root || id == ParamID::scale || id == ParamID::lock)
        keyDirty.store (true);
    else if (id == ParamID::length || id == ParamID::density)
        lengthDirty.store (true);
    else
        channelsDirty.store (true);

    triggerAsyncUpdate();
}

void PluginProcessor::handleAsyncUpdate()
{
    if (keyDirty.load() || lengthDirty.load() || channelsDirty.load())
        pushUndo (true);

    if (keyDirty.exchange (false))
    {
        matrix.setKey (choiceIndex (apvts, ParamID::root), choiceIndex (apvts, ParamID::scale), true);
        matrix.setLockToScale (value (apvts, ParamID::lock) > 0.5f);
    }

    if (lengthDirty.exchange (false))
    {
        matrix.setLength ((int) value (apvts, ParamID::length));
        matrix.setDensity (value (apvts, ParamID::density) / 100.0f);
    }

    if (channelsDirty.exchange (false))
    {
        for (int c = 0; c < kNumChannels; ++c)
        {
            const auto style = (GeneratorStyle) choiceIndex (apvts, ParamID::channelStyle (c).toRawUTF8());
            if (style != matrix.getPattern().channels[(size_t) c].style)
                matrix.setChannelStyle (c, style, true);

            // dynamic activation: enabling generates ONLY this channel, complementary to the others
            const int midiCh = juce::roundToInt (value (apvts, ParamID::channelMidi (c)));
            if (midiCh != matrix.getPattern().channels[(size_t) c].midiChannel)
                matrix.setChannelMidiChannel (c, midiCh);

            const bool on = value (apvts, ParamID::channelOn (c)) > 0.5f;
            if (on != matrix.isChannelEnabled (c))
                matrix.setChannelEnabled (c, on);
        }
    }

    publishPattern();
    if (onMatrixChanged)
        onMatrixChanged();
}

void PluginProcessor::timerCallback()
{
    keyTracker.decay (0.97f);

    if (value (apvts, ParamID::autoMatch) < 0.5f || keyTracker.notesSeen() < 6)
        return;

    const auto k = keyTracker.getDetection();
    if (k.valid && k.confidence >= 0.9f
        && (k.rootNote != choiceIndex (apvts, ParamID::root) || k.scaleIndex != choiceIndex (apvts, ParamID::scale)))
    {
        setParam (apvts, ParamID::root, (float) k.rootNote);  // -> keyDirty -> retune
        setParam (apvts, ParamID::scale, (float) k.scaleIndex);
    }
}

//==============================================================================
void PluginProcessor::publishPattern()
{
    const juce::SpinLock::ScopedLockType lock (patternLock);
    pendingPattern = matrix.getPattern();
    patternDirty.store (true);
}

void PluginProcessor::syncChannelParamsFromMatrix()
{
    const Suppress s (gSuppress);
    const auto& p = matrix.getPattern();
    for (int c = 0; c < kNumChannels; ++c)
    {
        setParam (apvts, ParamID::channelOn (c), p.channels[(size_t) c].enabled ? 1.0f : 0.0f);
        setParam (apvts, ParamID::channelStyle (c), (float) (int) p.channels[(size_t) c].style);
        setParam (apvts, ParamID::channelMidi (c), (float) p.channels[(size_t) c].midiChannel);
    }
    setParam (apvts, ParamID::root, (float) p.rootNote);
    setParam (apvts, ParamID::scale, (float) p.scaleIndex);
    setParam (apvts, ParamID::length, (float) p.channels[0].length);
}

//==============================================================================
void PluginProcessor::pushUndo (bool coalesce)
{
    const auto now = juce::Time::getMillisecondCounter();
    if (gestureOpen && gestureCaptured)
        return;
    if (coalesce && ! undoStack.empty() && now - lastUndoPush < 600)
        return; // a slider being dragged: one undo step for the whole movement

    undoStack.push_back (matrix);
    if (undoStack.size() > 100)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
    lastUndoPush = now;
    if (gestureOpen)
        gestureCaptured = true;
}

void PluginProcessor::restoreSnapshot (const ChannelMatrix& m)
{
    matrix = m;
    publishPattern();
    syncChannelParamsFromMatrix();
    if (onMatrixChanged)
        onMatrixChanged();
}

void PluginProcessor::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back (matrix);
    auto m = undoStack.back();
    undoStack.pop_back();
    restoreSnapshot (m);
}

void PluginProcessor::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back (matrix);
    auto m = redoStack.back();
    redoStack.pop_back();
    restoreSnapshot (m);
}

void PluginProcessor::editMatrix (const std::function<void (ChannelMatrix&)>& fn)
{
    pushUndo (false);
    fn (matrix);
    publishPattern();
    syncChannelParamsFromMatrix();
    if (onMatrixChanged)
        onMatrixChanged();
}

void PluginProcessor::generate()
{
    editMatrix ([this] (ChannelMatrix& m)
    {
        m.setDensity (value (apvts, ParamID::density) / 100.0f);
        m.generateAll();
    });
}

void PluginProcessor::resetToSingleArp()
{
    editMatrix ([] (ChannelMatrix& m) { m.initialiseDefault(); });
    currentPresetName = "Init";
}

void PluginProcessor::newVariation()
{
    const auto style = (VariationStyle) choiceIndex (apvts, ParamID::varStyle);
    const float amount = value (apvts, ParamID::varAmount) / 100.0f;
    const int target = ui.varTarget - 1; // -1 = every active channel
    editMatrix ([=] (ChannelMatrix& m) { m.applyVariation (target, style, amount); });
}

void PluginProcessor::restoreOriginal()
{
    const int target = ui.varTarget - 1;
    editMatrix ([target] (ChannelMatrix& m) { m.restoreOriginal (target); });
}

bool PluginProcessor::importMidi (const juce::File& f, int targetChannel, juce::String& message)
{
    auto res = MidiPatternImporter::importFile (f, matrix.getPattern(), targetChannel, true);
    if (! res.ok())
    {
        message = res.error.isNotEmpty() ? res.error : juce::String ("Nothing to import");
        return false;
    }

    importKey = res.key;
    editMatrix ([&] (ChannelMatrix& m) { m.setPattern (res.pattern); });
    currentPresetName = f.getFileNameWithoutExtension();
    message = "Imported " + juce::String (res.notesImported) + " notes into "
            + juce::String (res.channelsFilled) + " channel(s) - " + res.key.describe();
    return true;
}

//==============================================================================
PresetMeta PluginProcessor::currentMeta() const
{
    PresetMeta m;
    m.name = currentPresetName;
    m.timeDivision = PatternSerializer::timeDivisionNames()[choiceIndex (apvts, ParamID::division)];
    m.swing = value (apvts, ParamID::swing) / 100.0f;
    m.gateScale = value (apvts, ParamID::gate) / 100.0f;
    m.lockToScale = value (apvts, ParamID::lock) > 0.5f;
    m.arpMode = PatternSerializer::arpModeNames()[choiceIndex (apvts, ParamID::arpMode)];
    m.velocityMode = PatternSerializer::velocityModeNames()[choiceIndex (apvts, ParamID::velMode)];
    return m;
}

void PluginProcessor::applyMeta (const PresetMeta& meta)
{
    setParam (apvts, ParamID::division, (float) (int) PatternSerializer::timeDivisionFromName (meta.timeDivision));
    setParam (apvts, ParamID::swing, meta.swing * 100.0f);
    setParam (apvts, ParamID::gate, meta.gateScale * 100.0f);

    const int am = PatternSerializer::arpModeNames().indexOf (meta.arpMode);
    if (am >= 0) setParam (apvts, ParamID::arpMode, (float) am);
    const int vm = PatternSerializer::velocityModeNames().indexOf (meta.velocityMode);
    if (vm >= 0) setParam (apvts, ParamID::velMode, (float) vm);

    const Suppress s (gSuppress);
    setParam (apvts, ParamID::lock, meta.lockToScale ? 1.0f : 0.0f);
}

bool PluginProcessor::loadPreset (const juce::File& f)
{
    Pattern p;
    PresetMeta meta;
    if (! presets.loadPreset (f, p, &meta))
        return false;

    matrix.setLockToScale (meta.lockToScale);
    editMatrix ([&] (ChannelMatrix& m) { m.setPattern (p); });
    applyMeta (meta);
    currentPresetName = meta.name.isNotEmpty() ? meta.name : f.getFileNameWithoutExtension();
    return true;
}

juce::File PluginProcessor::quickSave()
{
    return presets.quickSaveSequence (matrix.getPattern(), currentMeta());
}

juce::File PluginProcessor::savePresetAs (const juce::String& name, const juce::String& category)
{
    currentPresetName = name;
    return presets.savePreset (matrix.getPattern(), currentMeta(), category);
}

MultiMidiExporter::Options PluginProcessor::exportOptions() const
{
    MultiMidiExporter::Options o;
    o.division = (TimeDivision) choiceIndex (apvts, ParamID::division);
    o.swing = value (apvts, ParamID::swing) / 100.0f;
    o.gateScale = value (apvts, ParamID::gate) / 100.0f;
    o.octave = juce::roundToInt (value (apvts, ParamID::octave));
    o.lockToScale = value (apvts, ParamID::lock) > 0.5f;
    o.baseName = PresetManager::sanitiseName (currentPresetName);
    for (int c = 0; c < kNumChannels; ++c)
    {
        o.channelTranspose[(size_t) c] = 12 * juce::roundToInt (octParams[(size_t) c]->load());
        o.voiceMode[(size_t) c] = (VoiceMode) juce::roundToInt (voiceParams[(size_t) c]->load());
        const int g = juce::roundToInt (glideParams[(size_t) c]->load());
        o.glide[(size_t) c] = g > 0 ? g : -1;
    }
    return o;
}

void PluginProcessor::previewNote (int c, int note, int velocity)
{
    c = juce::jlimit (0, kNumChannels - 1, c);
    const bool master = value (apvts, ParamID::mergeOut) > 0.5f;
    const int ch = master ? 1 : matrix.getPattern().channels[(size_t) c].midiChannel;
    const int shifted = note + 12 * juce::roundToInt (value (apvts, ParamID::octave))
                             + 12 * juce::roundToInt (octParams[(size_t) c]->load());
    const auto scope = previewFifo.write (1);
    if (scope.blockSize1 > 0)
        previewEvents[(size_t) scope.startIndex1] = { ch, shifted, velocity };
    else if (scope.blockSize2 > 0)
        previewEvents[(size_t) scope.startIndex2] = { ch, shifted, velocity };
}

juce::StringArray PluginProcessor::createDragFiles (bool multitrack)
{
    // unique folder per drag: hosts that reference the file (instead of copying) keep it intact
    auto dir = presets.getDragCacheDir().getChildFile (juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S_")
                                                       + juce::String (juce::Random::getSystemRandom().nextInt (10000)));
    auto p = matrix.getPattern();
    p.bpm = lastHostBpm;
    for (int c = 0; c < kNumChannels; ++c)
        if (muteParams[(size_t) c]->load() > 0.5f)
            p.channels[(size_t) c].enabled = false; // muted channels are not exported

    juce::StringArray paths;
    if (multitrack)
    {
        auto f = MultiMidiExporter::exportMultiTrack (p, exportOptions(), dir);
        if (f.existsAsFile())
            paths.add (f.getFullPathName());
    }
    else
    {
        for (auto& f : MultiMidiExporter::exportActiveChannels (p, exportOptions(), dir))
            paths.add (f.getFullPathName());
    }
    return paths;
}

juce::Array<juce::File> PluginProcessor::saveMidiToUserFolder (bool multitrack)
{
    auto dir = presets.getUserExportDir().getChildFile ("MIDI");
    auto p = matrix.getPattern();
    p.bpm = lastHostBpm;
    for (int c = 0; c < kNumChannels; ++c)
        if (muteParams[(size_t) c]->load() > 0.5f)
            p.channels[(size_t) c].enabled = false;
    if (multitrack)
        return { MultiMidiExporter::exportMultiTrack (p, exportOptions(), dir) };
    return MultiMidiExporter::exportActiveChannels (p, exportOptions(), dir);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", PatternSerializer::toJson (matrix.getPattern(), currentMeta()), nullptr);
    state.setProperty ("customScale", ScaleLibrary::getCustomMask(), nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    juce::String views, lanes;
    for (int c = 0; c < kNumChannels; ++c)
    {
        views << ui.view[(size_t) c];
        lanes << ui.lane[(size_t) c];
    }
    state.setProperty ("uiViews", views, nullptr);
    state.setProperty ("uiLanes", lanes, nullptr);
    state.setProperty ("uiSizeV2", ui.sizeIndex, nullptr);
    juce::String locks;
    for (int c = 0; c < kNumChannels; ++c)
        for (int b = 0; b < kMaxSteps / 16; ++b)
            locks << (matrix.isBarLocked (c, b) ? "1" : "0");
    state.setProperty ("barLocks", locks, nullptr);
    juce::String roles;
    for (int c = 0; c < kNumChannels; ++c)
        roles << matrix.getRole (c);
    state.setProperty ("roles", roles, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PluginProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    {
        const Suppress s (gSuppress);
        apvts.replaceState (tree);
    }

    ScaleLibrary::setCustomMask ((int) tree.getProperty ("customScale", ScaleLibrary::getCustomMask()));
    currentPresetName = tree.getProperty ("presetName", "Init").toString();
    {
        const auto views = tree.getProperty ("uiViews", "0000").toString();
        const auto lanes = tree.getProperty ("uiLanes", "1111").toString();
        for (int c = 0; c < kNumChannels; ++c)
        {
            ui.view[(size_t) c] = juce::jlimit (0, 1, views.substring (c, c + 1).getIntValue());
            ui.lane[(size_t) c] = juce::jlimit (0, 2, lanes.substring (c, c + 1).getIntValue());
        }
        ui.sizeIndex = juce::jlimit (0, 5, (int) tree.getProperty ("uiSizeV2", 2));
    }

    Pattern p;
    if (PatternSerializer::fromJson (tree.getProperty ("pattern").toString(), p))
    {
        matrix.setLockToScale (value (apvts, ParamID::lock) > 0.5f);
        matrix.setDensity (value (apvts, ParamID::density) / 100.0f);
        matrix.setPattern (p);
    }
    {
        const auto locks = tree.getProperty ("barLocks", "").toString();
        for (int c = 0; c < kNumChannels; ++c)
            for (int b = 0; b < kMaxSteps / 16; ++b)
                matrix.setBarLocked (c, b, locks.substring (c * 4 + b, c * 4 + b + 1) == "1");
        const auto roles = tree.getProperty ("roles", "0000").toString();
        for (int c = 0; c < kNumChannels; ++c)
            matrix.setRole (c, roles.substring (c, c + 1).getIntValue());
    }

    publishPattern();
    triggerAsyncUpdate(); // refreshes the editor on the message thread
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}
} // namespace pam

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pam::PluginProcessor();
}
