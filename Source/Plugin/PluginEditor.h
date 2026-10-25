#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <bitset>
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

    /** Holds the whole UI; scaled down uniformly for the small window sizes. */
    struct Content final : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };
    Content content;

    /** Draggable border between two channel rows (double-click: automatic heights). */
    struct RowSplitter final : public juce::Component
    {
        RowSplitter() { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); setRepaintsOnMouseActivity (true); }
        std::function<void()> onStart, onReset;
        std::function<void (int)> onDrag;
        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colour (0xff35e0ff).withAlpha (isMouseOverOrDragging() ? 0.6f : 0.12f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth() * 0.3f, 2.0f), 1.0f);
        }
        void mouseDown (const juce::MouseEvent&) override         { if (onStart) onStart(); }
        void mouseDrag (const juce::MouseEvent& e) override       { if (onDrag) onDrag (e.getDistanceFromDragStartY()); }
        void mouseDoubleClick (const juce::MouseEvent&) override  { if (onReset) onReset(); }
    };
    std::array<RowSplitter, kNumChannels - 1> splitters;
    std::array<int, kNumChannels> rowHeights {}, dragStartHeights {};
    bool manualRows = false;
    int lastMask = -1;
    juce::TextButton undoButton, redoButton;
    void paintContent (juce::Graphics&);
    void layoutContent();

    // header
    juce::Label title, presetName, status, detected;
    juce::TextButton presetsButton { "PRESETS" }, saveButton { "SAVE" }, saveAsButton { "SAVE AS" },
                     importButton { "IMPORT MIDI" }, generateButton { "GENERATE" }, resetButton { "RESET" },
                     panicButton { "PANIC" };

    // key / global
    juce::ComboBox rootBox, scaleBox;
    juce::ToggleButton lockToggle, autoMatchToggle;
    juce::Slider lengthSlider, densitySlider;
    /** Bar button: left click shows the bar, right click locks it for the selected channel. */
    struct PageButton final : public juce::TextButton
    {
        std::function<void()> onRightClick;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
            juce::TextButton::mouseDown (e);
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu())
                juce::TextButton::mouseUp (e);
        }
    };
    std::array<PageButton, 4> pageButtons;
    juce::Label pagesCaption;
    void updatePageButtons();
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
