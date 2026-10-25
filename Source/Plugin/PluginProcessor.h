#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Core/ArpEngine.h"
#include "Core/ChannelMatrix.h"
#include "Core/MidiAnalyzer.h"
#include "IO/MidiPatternImporter.h"
#include "IO/MultiMidiExporter.h"
#include "IO/PresetManager.h"

namespace pam
{
namespace ParamID
{
    inline constexpr const char* root        = "root";
    inline constexpr const char* scale       = "scale";
    inline constexpr const char* lock        = "lock";
    inline constexpr const char* autoMatch   = "autoMatch";
    inline constexpr const char* length      = "length";
    inline constexpr const char* density     = "density";
    inline constexpr const char* division    = "division";
    inline constexpr const char* swing       = "swing";
    inline constexpr const char* gate        = "gate";
    inline constexpr const char* octave      = "octave";
    inline constexpr const char* octRange    = "octRange";
    inline constexpr const char* arpMode     = "arpMode";
    inline constexpr const char* velMode     = "velMode";
    inline constexpr const char* velocity    = "velocity";
    inline constexpr const char* holdMode    = "holdMode";
    inline constexpr const char* keyFollow   = "keyFollow";
    inline constexpr const char* freeRun     = "freeRun";
    inline constexpr const char* mergeOut    = "mergeOut";
    inline constexpr const char* varStyle    = "varStyle";
    inline constexpr const char* varAmount   = "varAmount";
    inline juce::String channelOn (int c)    { return "ch" + juce::String (c + 1) + "On"; }
    inline juce::String channelStyle (int c) { return "ch" + juce::String (c + 1) + "Style"; }
}

/** MIDI generator plugin. Owns:
     - ChannelMatrix (message thread): generation, activation, editing
     - ArpEngine (audio thread): realtime playback of a pattern snapshot
     - PresetManager / MultiMidiExporter / MidiPatternImporter: files */
class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater,
                              private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    //=== AudioProcessor ===========================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }
    const juce::String getName() const override         { return "Psytrance Arp Matrix"; }
    bool acceptsMidi() const override                   { return true; }
    bool producesMidi() const override                  { return true; }
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override        { return 0.0; }
    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //=== Matrix access (message thread) ===========================================
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    const ChannelMatrix& getMatrix() const noexcept        { return matrix; }

    /** Applies an edit to the matrix and publishes the result to the audio thread. */
    void editMatrix (const std::function<void (ChannelMatrix&)>& fn);

    void generate();                                // Generate / Roll dice
    void resetToSingleArp();                        // Reset: Single Arp Mode
    void newVariation();                            // Melody Var
    void restoreOriginal();
    bool importMidi (const juce::File& f, int targetChannel, juce::String& message);

    //=== Presets / files =========================================================
    PresetManager& getPresetManager() noexcept      { return presets; }
    bool loadPreset (const juce::File& f);
    juce::File quickSave();
    juce::File savePresetAs (const juce::String& name, const juce::String& category);
    juce::String getCurrentPresetName() const       { return currentPresetName; }

    MultiMidiExporter::Options exportOptions() const;
    /** Files for the multi-MIDI drag: one per active channel (or one multitrack file). */
    juce::StringArray createDragFiles (bool multitrack);
    juce::Array<juce::File> saveMidiToUserFolder (bool multitrack);

    //=== Monitoring ===============================================================
    int currentStep (int channel) const noexcept    { return engine.currentStep (channel); }
    KeyDetection detectedKey() const                { return keyTracker.getDetection(); }
    int keyFollowTranspose() const noexcept         { return engine.currentTranspose(); }
    int heldKeys() const noexcept                   { return engine.heldKeyCount(); }
    KeyDetection lastImportKey() const noexcept     { return importKey; }
    void panic() noexcept                           { panicRequested.store (true); }

    std::function<void()> onMatrixChanged;          // editor hook (message thread)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void publishPattern();
    void syncChannelParamsFromMatrix();
    ArpParams readArpParams() const;
    PresetMeta currentMeta() const;
    void applyMeta (const PresetMeta& meta);

    juce::AudioProcessorValueTreeState apvts;
    ChannelMatrix matrix;
    ArpEngine engine;
    RealtimeKeyTracker keyTracker;
    PresetManager presets;

    // pattern hand-off message thread -> audio thread
    juce::SpinLock patternLock;
    Pattern pendingPattern, audioPattern;
    std::atomic<bool> patternDirty { true };

    std::atomic<bool> paramsDirty { false }, keyDirty { false }, channelsDirty { false }, lengthDirty { false };
    std::atomic<bool> panicRequested { false };
    bool wasBypassed = false;
    bool suppressListener = false;
    juce::String currentPresetName = "Init";
    KeyDetection importKey;
    double lastHostBpm = 142.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
} // namespace pam
