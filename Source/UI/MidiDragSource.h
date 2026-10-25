#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Plugin/PluginProcessor.h"

namespace pam
{
/** "MIDI Export" handle. Dragging it out of the plugin performs a native external
    drag of one .mid file per ACTIVE channel (1 in Single Arp Mode, up to 4), or of a
    single multitrack file when `multitrack` is set. Most DAWs (Ableton Live, Cubase,
    Studio One, Reaper, Bitwig) create one track per dropped file. */
class MidiDragSource final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MidiDragSource (PluginProcessor& p);

    void setMultitrack (bool shouldUseMultitrack) { multitrack = shouldUseMultitrack; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    PluginProcessor& proc;
    bool multitrack = false;
    bool dragging = false;
};
} // namespace pam
