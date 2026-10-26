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
    inline juce::String channelMidi (int c)  { return "ch" + juce::String (c + 1) + "Midi"; }
    inline juce::String channelOct (int c)   { return "ch" + juce::String (c + 1) + "Oct"; }
    inline juce::String channelVoice (int c) { return "ch" + juce::String (c + 1) + "Voice"; }
    inline juce::String channelGlide (int c) { return "ch" + juce::String (c + 1) + "Glide"; }
    inline juce::String channelMute (int c)  { return "ch" + juce::String (c + 1) + "Mute"; }
    // per channel TIME / MODE / VELOCITY MODE / HOLD (the global controls edit the selected channel)
    inline juce::String channelDiv (int c)     { return "ch" + juce::String (c + 1) + "Div"; }
    inline juce::String channelMode (int c)    { return "ch" + juce::String (c + 1) + "Mode"; }
    inline juce::String channelVelMode (int c) { return "ch" + juce::String (c + 1) + "VelMode"; }
    inline juce::String channelHold (int c)    { return "ch" + juce::String (c + 1) + "Hold"; }
    inline constexpr const char* muteAll     = "muteAll";
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

    //=== Undo / redo (every matrix change, one step per mouse gesture) ==============
    void undo();
    void redo();
    bool canUndo() const noexcept                   { return ! undoStack.empty(); }
    bool canRedo() const noexcept                   { return ! redoStack.empty(); }
    void beginGesture() noexcept                    { gestureOpen = true; gestureCaptured = false; }
    void endGesture() noexcept                      { gestureOpen = false; }

    /** Plays a note immediately on the channel's synth (velocity 0 = note off). */
    void previewNote (int matrixChannel, int note, int velocity);
    juce::Array<juce::File> exportFilesFor (bool multitrack, const juce::File& dir);

    void generate();                                // Generate / Roll dice
    void resetToSingleArp();                        // Reset: Single Arp Mode
    void newVariation();                            // Melody Var
    void restoreOriginal();
    bool importMidi (const juce::File& f, int targetChannel, juce::String& message);
    /** Dropped on one channel row: every note of the file goes into that channel only. */
    bool importMidiIntoChannel (const juce::File& f, int channel, juce::String& message);
    /** Loads one channel of a preset into `channel` (same channel of the preset if it is active,
        otherwise its first active one), transposed into the current key. */
    bool loadChannelPreset (const juce::File& f, int channel, juce::String& message);

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

    /** Editor-only state, saved with the project. */
    struct UiState
    {
        std::array<int, kNumChannels> view { 0, 0, 0, 0 };  // 0 = step grid, 1 = piano roll
        std::array<int, kNumChannels> lane { 1, 1, 1, 1 };  // piano roll lane: 0 hidden, 1 velocity, 2 gate
        std::array<float, kNumChannels> laneSize { 0.28f, 0.28f, 0.28f, 0.28f }; // lane height (fraction)
        int sizeIndex = 2;                                  // index into the SIZE menu (M)
        int selectedChannel = 0;                            // channel the bar locks apply to
        int varTarget = 0;                                  // NEW VARIATION target: 0 = all active, 1..4 = channel
        int gridFirst = 0, gridCount = 16;                  // step view: first visible step, visible steps (zoom)
        std::array<int, kNumChannels> varStyle { 0, 0, 0, 0 };              // per-channel VARIATION STYLE
        std::array<float, kNumChannels> varAmount { 65.0f, 65.0f, 65.0f, 65.0f }; // per-channel VARIATION amount
    } ui;

    /** Selects a channel: the VARIATION STYLE / amount controls switch to that channel's own settings
        (each channel remembers its own; untouched channels show the defaults), VAR ON follows. */
    void selectChannel (int c);
    /** Variation settings of a channel (the live controls for the selected one). */
    std::pair<VariationStyle, float> variationFor (int c) const;

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
    std::vector<ChannelMatrix> undoStack, redoStack;
    bool gestureOpen = false, gestureCaptured = false;
    juce::uint32 lastUndoPush = 0;
    void pushUndo (bool coalesce);
    void restoreSnapshot (const ChannelMatrix& m);
    bool wasBypassed = false;
    std::array<std::atomic<float>*, kNumChannels> octParams {}, voiceParams {}, glideParams {}, muteParams {},
                                                  divParams {}, modeParams {}, velModeParams {}, holdParams {};
    std::atomic<bool> proxyDirty { false };
    void copyProxiesToChannel (int channel);   // global TIME/MODE/VEL/HOLD -> that channel
    void showChannelInProxies (int channel);   // that channel -> global controls
    void applyChannelMeta (int channel, const PresetMeta& meta, int sourceChannel);
    std::atomic<float>* muteAllParam = nullptr;

    // note preview (piano-roll keyboard / note clicks): message thread -> audio thread
    struct PreviewEvent { int channel; int note; int velocity; };
    juce::AbstractFifo previewFifo { 64 };
    std::array<PreviewEvent, 64> previewEvents {};
    std::array<int, 128> previewSounding {}; // per note: MIDI channel + 1 while a preview note is on
    bool suppressListener = false;
    juce::String currentPresetName = "Init";
    KeyDetection importKey;
    double lastHostBpm = 142.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
} // namespace pam
