#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Plugin/PluginProcessor.h"

namespace pam
{
/** 16-step view of one matrix channel (one page of up to 4).

    Mouse:  click            toggle step on/off
            drag up/down     move note by scale degrees
            shift + drag     velocity
            alt/option-click accent
            right-click      menu (accent, slide, chord size, gate, octave, clear) */
class StepGridComponent final : public juce::Component
{
public:
    StepGridComponent (PluginProcessor& p, int channelIndex);

    void setPage (int newPage)               { page = juce::jlimit (0, kMaxSteps / 16 - 1, newPage); repaint(); }
    void setAccentColour (juce::Colour c)    { colour = c; repaint(); }
    void refresh()                           { repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int stepAt (juce::Point<float> pos) const noexcept;
    juce::Rectangle<float> cellBounds (int cell) const noexcept;
    void showMenu (int step);
    void modifyStep (int step, const std::function<void (Step&)>& fn);

    PluginProcessor& proc;
    int channel = 0;
    int page = 0;
    juce::Colour colour { 0xff35e0ff };

    int dragStep = -1, dragStartNote = 60, dragStartVel = 100;
    bool dragged = false;
};
} // namespace pam
