#pragma once

#include "StepGridComponent.h"

namespace pam
{
/** One matrix row: activation switch, style, per-channel actions and the step grid. */
class ChannelStripComponent final : public juce::Component
{
public:
    ChannelStripComponent (PluginProcessor& p, int channelIndex, juce::Colour colour);

    void setPage (int page)  { grid.setPage (page); }
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    PluginProcessor& proc;
    int channel;
    juce::Colour colour;

    juce::ToggleButton onButton;
    juce::ComboBox styleBox, midiBox;
    juce::TextButton regenButton { "Roll" }, varButton { "Var" }, velButton { "Vel" }, gateButton { "Gate" };
    juce::Label stats;
    StepGridComponent grid;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment, midiAttachment;
};
} // namespace pam
