#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "PluginProcessor.h"
#include "UI/ChannelStripComponent.h"
#include "UI/MidiDragSource.h"

namespace pam
{
/** Functional editor (deliberately plain: layout is not modelled on any other product). */
class PluginEditor final : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void refreshAll();
    void setStatus (const juce::String& s);
    void showPresetMenu();
    void showSaveAsDialog();
    void chooseMidiToImport();
    void setPage (int p);

    void addCombo (juce::ComboBox& box, const juce::StringArray& items, const char* paramId, const juce::String& tooltip);
    void addKnob (juce::Slider& s, const char* paramId, const juce::String& name);
    void addToggle (juce::ToggleButton& b, const char* paramId, const juce::String& text, const juce::String& tooltip);

    PluginProcessor& proc;
    juce::LookAndFeel_V4 lnf;   // declared first: outlives every child component
    juce::TooltipWindow tooltips { this, 600 };

    // header
    juce::Label title, presetName, status, detected;
    juce::TextButton presetsButton { "PRESETS" }, saveButton { "SAVE" }, saveAsButton { "SAVE AS" },
                     importButton { "IMPORT MIDI" }, generateButton { "GENERATE" }, resetButton { "RESET" },
                     panicButton { "PANIC" };

    // key / global
    juce::ComboBox rootBox, scaleBox;
    juce::ToggleButton lockToggle, autoMatchToggle;
    juce::Slider lengthSlider, densitySlider;
    std::array<juce::TextButton, 4> pageButtons;
    int page = 0;

    // matrix
    std::array<std::unique_ptr<ChannelStripComponent>, kNumChannels> strips;

    // performance
    juce::ComboBox divisionBox, modeBox, velModeBox, holdBox, varStyleBox, octaveBox, octRangeBox;
    juce::Slider swingKnob, gateKnob, velocityKnob, varAmountKnob;
    juce::ToggleButton keyFollowToggle, freeRunToggle, multitrackToggle;
    juce::ComboBox outputBox, sizeBox;
    static juce::StringArray sizeNames();
    static juce::Point<int> sizeFor (int index);
    juce::TextButton newVarButton { "NEW VARIATION" }, originalButton { "ORIGINAL" }, saveMidiButton { "SAVE MIDI" };
    MidiDragSource dragSource;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::map<juce::Component*, std::unique_ptr<juce::Label>> captions;
    void addCaption (juce::Component& c, const juce::String& text);
    void placeWithCaption (juce::Component& c, juce::Rectangle<int> area);

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
} // namespace pam
