#include "ChannelStripComponent.h"

namespace pam
{
ChannelStripComponent::ChannelStripComponent (PluginProcessor& p, int c, juce::Colour col)
    : proc (p), channel (c), colour (col), grid (p, c)
{
    onButton.setButtonText ("CH " + juce::String (c + 1));
    onButton.setTooltip ("Activate this channel. Enabling generates a part complementary to the active channels.");
    addAndMakeVisible (onButton);

    styleBox.addItemList (PatternSerializer::styleNames(), 1);
    addAndMakeVisible (styleBox);

    auto& state = proc.getState();
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ParamID::channelOn (c), onButton);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ParamID::channelStyle (c), styleBox);

    regenButton.setTooltip ("Regenerate this channel only (others stay untouched)");
    regenButton.onClick = [this] { proc.editMatrix ([ch = channel] (ChannelMatrix& m) { m.regenerateChannel (ch); }); };

    varButton.setTooltip ("Apply the selected melody variation to this channel");
    varButton.onClick = [this]
    {
        const auto style = (VariationStyle) juce::roundToInt (proc.getState().getRawParameterValue (ParamID::varStyle)->load());
        const float amount = proc.getState().getRawParameterValue (ParamID::varAmount)->load() / 100.0f;
        proc.editMatrix ([=, ch = channel] (ChannelMatrix& m) { m.applyVariation (ch, style, amount); });
    };

    velButton.setTooltip ("Randomise velocities / accents");
    velButton.onClick = [this] { proc.editMatrix ([ch = channel] (ChannelMatrix& m) { m.randomizeVelocity (ch); }); };

    gateButton.setTooltip ("Randomise gate lengths");
    gateButton.onClick = [this] { proc.editMatrix ([ch = channel] (ChannelMatrix& m) { m.randomizeGate (ch); }); };

    for (auto* b : { &regenButton, &varButton, &velButton, &gateButton })
        addAndMakeVisible (b);

    stats.setFont (juce::FontOptions (11.0f));
    stats.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (stats);

    grid.setAccentColour (colour);
    addAndMakeVisible (grid);
    refresh();
}

void ChannelStripComponent::refresh()
{
    const auto& ch = proc.getMatrix().getPattern().channels[(size_t) channel];
    stats.setText ("MIDI ch " + juce::String (ch.midiChannel) + "  |  " + juce::String (ch.countActive()) + " notes"
                   + (ch.userEdited ? "  |  edited" : ""), juce::dontSendNotification);
    const bool on = ch.enabled;
    for (auto* b : { &regenButton, &varButton, &velButton, &gateButton })
        b->setEnabled (on);
    grid.refresh();
}

void ChannelStripComponent::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff111c2a));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (colour.withAlpha (proc.getMatrix().isChannelEnabled (channel) ? 0.9f : 0.25f));
    g.fillRoundedRectangle (getLocalBounds().removeFromLeft (4).toFloat(), 2.0f);
}

void ChannelStripComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto left = r.removeFromLeft (190);

    auto row1 = left.removeFromTop (24);
    onButton.setBounds (row1.removeFromLeft (70));
    styleBox.setBounds (row1);

    left.removeFromTop (4);
    auto row2 = left.removeFromTop (22);
    const int w = row2.getWidth() / 4;
    regenButton.setBounds (row2.removeFromLeft (w).reduced (1, 0));
    varButton.setBounds (row2.removeFromLeft (w).reduced (1, 0));
    velButton.setBounds (row2.removeFromLeft (w).reduced (1, 0));
    gateButton.setBounds (row2.reduced (1, 0));

    stats.setBounds (left.removeFromTop (18));

    r.removeFromLeft (8);
    grid.setBounds (r);
}
} // namespace pam
