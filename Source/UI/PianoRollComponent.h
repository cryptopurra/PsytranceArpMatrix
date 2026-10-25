#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Plugin/PluginProcessor.h"

namespace pam
{
/** Classic piano-roll view of one matrix channel (alternative to the step grid).

    The pattern stays step based (one note per step, chords via the chord size), so
    moving a note in time moves it to another step and its length is the step gate.

    Mouse:  click empty cell          add a note (drag right to set its length)
            drag note body            move in time (steps) and pitch
            drag right edge           change length (gate)
            double-click / alt-click  delete note
            right-click note          menu (accent, slide, chord, delete)
            wheel                     scroll pitch      (horizontal wheel: scroll time)
            Ctrl + wheel              horizontal zoom
            Shift + wheel             vertical zoom
    Lane (bottom, can be hidden):     drag to draw velocity or gate per step */
class PianoRollComponent final : public juce::Component
{
public:
    enum class Lane { None = 0, Velocity, Gate };

    PianoRollComponent (PluginProcessor& p, int channelIndex);

    void setAccentColour (juce::Colour c)  { colour = c; repaint(); }
    void setLane (Lane l)                  { lane = l; repaint(); }
    Lane getLane() const noexcept          { return lane; }
    void refresh()                         { repaint(); }
    void centreOnNotes();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int keyboardWidth = 34;

    juce::Rectangle<int> noteArea() const;
    juce::Rectangle<int> laneArea() const;
    float stepToX (double step) const noexcept;
    double xToStep (float x) const noexcept;
    float noteToY (int note) const noexcept;
    int yToNote (float y) const noexcept;
    juce::Rectangle<float> noteRect (int step) const;
    int noteAt (juce::Point<float> p, bool& onRightEdge) const;
    void clampView();
    void editStep (int step, const std::function<void (Step&)>& fn);
    void moveNote (int fromStep, int toStep, int newNote);
    void setLaneValue (juce::Point<float> p);
    void showNoteMenu (int step);
    const ChannelPattern& channelData() const;

    PluginProcessor& proc;
    int channel;
    juce::Colour colour { 0xff35e0ff };
    Lane lane = Lane::Velocity;

    // view
    float pxPerStep = 22.0f;
    float rowHeight = 9.0f;
    double scrollStep = 0.0;   // first visible step
    float topNote = 84.0f;     // pitch at the top edge

    // drag state
    enum class Drag { None, Move, Resize, Create, Lane };
    Drag drag = Drag::None;
    int dragStep = -1, dragStartNote = 60, currentStep = -1;
    float dragStartGate = 0.5f;
    int lastDragNote = 60;
    std::array<Step, kMaxSteps> dragOrigin {};
};
} // namespace pam
