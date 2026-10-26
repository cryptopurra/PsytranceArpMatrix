#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <bitset>
#include "Plugin/PluginProcessor.h"

namespace pam
{
/** Classic piano-roll view of one matrix channel (alternative to the step grid).

    The pattern stays step based (one note per step, chords via the chord size), so
    moving a note in time moves it to another step and its length is the step gate.

    Mouse:  click empty cell          add a note (drag right to set its length)
            drag note body            move in time (steps) and pitch (all selected notes)
            drag right edge           change length (gate) of all selected notes
            Ctrl + click note         add / remove the note from the selection
            Ctrl + drag on empty area rectangle selection
            press and hold on empty   rectangle selection (hold ~0.4 s still, then drag)
            double-click / alt-click  delete note
            right-click note          menu (accent, slide, chord, octave, select all, delete)
            drag the lane border      resize the lane (double-click: default size)
            wheel                     scroll pitch      (horizontal wheel: scroll time)
            hold the wheel + move     pan the view (time and pitch)
            Ctrl + wheel              horizontal zoom
            Shift + wheel             vertical zoom
    Keys (selected notes):            arrows left/right = move one step (Shift: one bar),
                                      up/down = one semitone, or one scale degree with LOCK on
                                      (Shift: one octave), Delete = delete. The moved notes are played.
    Moving notes onto other notes swaps them (the other notes take the freed steps).
    Click on the keyboard strip or on a note: the note is played on the channel's synth.
    Lane (bottom, can be hidden):     drag to draw velocity or gate per step */
class PianoRollComponent final : public juce::Component,
                                 private juce::Timer
{
public:
    enum class Lane { None = 0, Velocity, Gate };

    PianoRollComponent (PluginProcessor& p, int channelIndex);
    ~PianoRollComponent() override;

    /** Visible time range in steps (for the bar buttons). */
    juce::Range<double> getVisibleSteps() const;
    /** Scrolls so that the given step is at the left edge. */
    void showStep (int step);
    std::function<void()> onViewChanged;   // zoom / scroll changed

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
    bool keyPressed (const juce::KeyPress&) override;

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
    void applySteps (const std::array<Step, kMaxSteps>& target);
    void moveSelection (int dStep, int dNote, bool byScaleDegree = false);
    void ensureVisible (int step, int note);
    void viewChanged();
    void timerCallback() override;          // press-and-hold: switch to rectangle selection
    void playNotes (const std::vector<int>& notes);  // short preview after an arrow move
    void stopPlayedNotes();
    std::vector<int> playedNotes;
    int playToken = 0;
    bool createMoved = false;
    Step preCreateStep {};
    void resizeSelection (float dGate);
    void deleteSelection();
    bool isOnLaneBorder (juce::Point<int> p) const;
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
    enum class Drag { None, Move, Resize, Create, Lane, LaneSplit, Select, Pan };
    double panStartStep = 0.0;
    float panStartTop = 0.0f;
    Drag drag = Drag::None;
    int dragStep = -1, dragStartNote = 60, currentStep = -1;
    float dragStartGate = 0.5f;
    int lastDragNote = 60, lastDStep = 0, lastDNote = 0;
    std::array<Step, kMaxSteps> dragOrigin {};
    std::bitset<kMaxSteps> selected, dragSelection;
    int previewingNote = -1;
    void startPreview (int note);
    void stopPreview();
    juce::Point<float> selectStart;
    juce::Rectangle<float> selectRect;
};

//==============================================================================
/** The piano roll of one channel in its own resizable window (like "open in a separate window"
    in a DAW editor). It edits the same pattern as the row in the plugin. */
class PianoRollWindow final : public juce::DocumentWindow
{
public:
    PianoRollWindow (PluginProcessor& p, int channelIndex, juce::Colour colour, std::function<void()> closeCallback)
        : juce::DocumentWindow ("Psytrance Arp Matrix  -  CH " + juce::String (channelIndex + 1) + "  piano roll",
                                juce::Colour (0xff08111b), juce::DocumentWindow::allButtons),
          onClose (std::move (closeCallback))
    {
        auto* r = new PianoRollComponent (p, channelIndex);
        r->setAccentColour (colour);
        r->setSize (980, 460);
        roll = r;
        setContentOwned (r, true);
        setUsingNativeTitleBar (true);
        setResizable (true, true);   // bottom-right corner resizer as well (+ maximise button)
        setResizeLimits (420, 220, 4000, 2500);
        setAlwaysOnTop (true);  // stays above the host window while the plugin is in use
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    PianoRollComponent& getRoll() noexcept { return *roll; }
    void closeButtonPressed() override     { if (onClose) onClose(); }

private:
    PianoRollComponent* roll = nullptr;   // owned by the window (content component)
    std::function<void()> onClose;
};
} // namespace pam
