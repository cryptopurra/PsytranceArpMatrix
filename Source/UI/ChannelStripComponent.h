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

    /** Bar button clicked: the piano roll scrolls there (the step grid view is shared, set by the editor). */
    void showStep (int step)  { roll.showStep (step); grid.repaint(); }
    /** Visible steps of whichever view this row shows (step grid or piano roll). */
    juce::Range<double> visibleSteps() const;
    std::function<void()> onViewChanged;       // zoom / scroll in the grid or piano roll
    std::function<void()> onPopOut;            // open the piano roll in its own window
    void repaintViews()      { grid.repaint(); roll.repaint(); }
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
    juce::TextButton viewButton, laneButton, popButton;
    void updateViewButtons();
    bool compact = false;     // collapsed row (too small for the grid)

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment, muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment, midiAttachment, octAttachment, voiceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttachment;
};
} // namespace pam
