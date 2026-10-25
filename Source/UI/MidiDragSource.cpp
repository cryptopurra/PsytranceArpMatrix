#include "MidiDragSource.h"

namespace pam
{
MidiDragSource::MidiDragSource (PluginProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto your DAW timeline: one MIDI track per active channel");
}

void MidiDragSource::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const int n = proc.getMatrix().getPattern().numEnabled();

    g.setColour (juce::Colour (0xff1b3a52));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colour (0xff35e0ff));
    g.drawRoundedRectangle (r, 6.0f, 1.5f);

    // small "stack of tracks" icon: one bar per file that will be dropped
    auto icon = r.removeFromLeft (r.getHeight()).reduced (8.0f);
    const int bars = multitrack ? 1 : juce::jmax (1, n);
    const float bh = icon.getHeight() / 4.0f;
    for (int i = 0; i < bars; ++i)
        g.fillRoundedRectangle (icon.getX(), icon.getY() + i * bh, icon.getWidth(), bh - 2.0f, 2.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    const auto text = multitrack ? juce::String ("DRAG MIDI  (1 file, ") + juce::String (n) + " tracks)"
                                 : juce::String ("DRAG MIDI  (") + juce::String (n) + (n == 1 ? " track)" : " tracks)");
    g.drawFittedText (text, r.toNearestInt(), juce::Justification::centredLeft, 1);
}

void MidiDragSource::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;

    dragging = true;
    const auto files = proc.createDragFiles (multitrack);
    if (! files.isEmpty())
        juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this, [this] { dragging = false; });
    else
        dragging = false;
}
} // namespace pam
