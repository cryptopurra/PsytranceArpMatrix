#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

// Tiny diagnostic plugin: passes MIDI through and logs every note event it receives together with the
// host position of the block. Put it on the track that receives the MIDI of another plugin to see
// what the host really delivers.
class MonitorProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    MonitorProcessor();
    ~MonitorProcessor() override;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PAM MIDI Monitor"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::AudioParameterBool* record = nullptr;
    static juce::File logFile();

private:
    struct Row
    {
        juce::int64 block = 0, timeSamples = 0, hostNs = 0;
        double ppq = 0, loopStart = 0, loopEnd = 0, bpm = 0;
        int numSamples = 0, offset = 0, kind = 0, channel = 0, note = 0, velocity = 0;
        bool playing = false, looping = false;
    };

    void timerCallback() override;
    void drain();

    juce::AbstractFifo fifo { 32768 };
    std::vector<Row> rows = std::vector<Row> (32768);
    std::unique_ptr<juce::FileOutputStream> stream;
    bool wasRecording = false;
    juce::int64 blockCounter = 0;
    std::atomic<bool> streamOpen { false };
};
