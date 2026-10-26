#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Plugin/PluginProcessor.h"

namespace pam
{
/** Step view of one matrix channel. Shows proc.ui.gridCount steps starting at proc.ui.gridFirst
    (shared by all rows, so the channels stay aligned): 16 = one bar, zoom 4 ... 64 steps.

    Mouse:  click            toggle step on/off
            drag up/down     move note by scale degrees (moving left/right paints the same note
                             on the neighbouring active steps)
            shift + drag     velocity
            alt/option-click accent
            mouse wheel      note up/down by scale degree (shift + wheel: velocity)
            Ctrl + wheel     zoom (4 / 8 / 16 / 32 / 64 steps visible)
            horizontal wheel, Alt + wheel or hold the wheel and drag: scroll
            double-click the note name: type a note (e.g. F#3, Gb2, A4 or a MIDI number)
            right-click      menu (accent, slide, chord size, gate, octave, clear) */
class StepGridComponent final : public juce::Component
{
public:
    StepGridComponent (PluginProcessor& p, int channelIndex);

    std::function<void()> onViewChanged;   // zoom / scroll changed (all rows + bar buttons follow)
    /** Zoom levels: visible steps. */
    static int zoomLevel (int current, int direction);
    static void clampView (PluginProcessor::UiState& ui);
    void setAccentColour (juce::Colour c)    { colour = c; repaint(); }
    void refresh()                           { repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    int stepAt (juce::Point<float> pos) const noexcept;
    juce::Rectangle<float> cellBounds (int cell) const noexcept;
    void showMenu (int step);
    void modifyStep (int step, const std::function<void (Step&)>& fn);

    void finishNoteEdit (bool apply);
    std::unique_ptr<juce::TextEditor> noteEditor;
    int editingStep = -1;

    PluginProcessor& proc;
    int channel = 0;
    int first() const noexcept { return proc.ui.gridFirst; }
    int count() const noexcept { return juce::jmax (1, proc.ui.gridCount); }
    void scrollBy (int steps);
    bool panning = false;
    int panStartFirst = 0;
    juce::Colour colour { 0xff35e0ff };

    int dragStep = -1, dragStartNote = 60, dragStartVel = 100;
    bool dragged = false;
};
} // namespace pam
