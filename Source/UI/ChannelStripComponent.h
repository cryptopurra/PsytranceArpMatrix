#pragma once

#include "StepGridComponent.h"
#include "PianoRollComponent.h"

namespace pam
{
/** One matrix row: activation switch, style, per-channel actions and the step grid. */
class ChannelStripComponent final : public juce::Component
{
public:
    ChannelStripComponent (PluginProcessor& p, int channelIndex, juce::Colour colour);

    void setPage (int page)  { grid.setPage (page); }
    bool isPianoRoll() const noexcept { return proc.ui.view[(size_t) channel] == 1; }
    std::function<void()> onLayoutChanged;     // view switched: the editor re-distributes heights
    std::function<void()> onSelected;          // clicked: becomes the selected channel
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

private:
    PluginProcessor& proc;
    int channel;
    juce::Colour colour;

    juce::ToggleButton onButton;
    juce::ComboBox styleBox, midiBox, octBox, voiceBox, roleBox;
    juce::TextButton muteButton { "M" };
    juce::Slider glideSlider;
    juce::TextButton regenButton { "Roll" }, varButton { "Var" }, velButton { "Vel" }, gateButton { "Gate" };
    juce::Label stats;
    StepGridComponent grid;
    PianoRollComponent roll;
    juce::TextButton viewButton, laneButton;
    void updateViewButtons();
    bool compact = false;     // collapsed row (too small for the grid)

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment, muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment, midiAttachment, octAttachment, voiceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttachment;
};
} // namespace pam
