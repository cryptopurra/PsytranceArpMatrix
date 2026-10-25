#include "ChannelStripComponent.h"

namespace pam
{
ChannelStripComponent::ChannelStripComponent (PluginProcessor& p, int c, juce::Colour col)
    : proc (p), channel (c), colour (col), grid (p, c), roll (p, c)
{
    onButton.setButtonText ("CH " + juce::String (c + 1));
    onButton.setTooltip ("Activate this channel. Enabling generates a part complementary to the active channels.");
    addAndMakeVisible (onButton);

    styleBox.addItemList (PatternSerializer::styleNames(), 1);
    styleBox.setScrollWheelEnabled (true);
    addAndMakeVisible (styleBox);

    for (int i = 1; i <= 16; ++i)
        midiBox.addItem ("OUT " + juce::String (i), i);
    midiBox.setScrollWheelEnabled (true);
    midiBox.setTooltip ("MIDI channel this matrix channel is sent on (route each one to a different synth). "
                        "Ignored when OUTPUT is set to Master.");
    addAndMakeVisible (midiBox);

    auto& state = proc.getState();
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ParamID::channelOn (c), onButton);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ParamID::channelStyle (c), styleBox);
    midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ParamID::channelMidi (c), midiBox);

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
    roll.setAccentColour (colour);
    addChildComponent (roll);

    viewButton.setTooltip ("Switch between the step grid and a classic piano roll for this channel");
    viewButton.onClick = [this]
    {
        auto& v = proc.ui.view[(size_t) channel];
        v = v == 0 ? 1 : 0;
        updateViewButtons();
        if (onLayoutChanged)
            onLayoutChanged();
    };
    laneButton.setTooltip ("Piano roll lane: velocity / gate / hidden");
    laneButton.onClick = [this]
    {
        auto& l = proc.ui.lane[(size_t) channel];
        l = (l + 1) % 3;
        updateViewButtons();
        roll.resized();
    };
    addAndMakeVisible (viewButton);
    addAndMakeVisible (laneButton);
    updateViewButtons();
    refresh();
}

void ChannelStripComponent::updateViewButtons()
{
    const bool pr = isPianoRoll();
    viewButton.setButtonText (pr ? "PIANO ROLL" : "STEPS");
    const int l = proc.ui.lane[(size_t) channel];
    laneButton.setButtonText (l == 0 ? "LANE OFF" : (l == 1 ? "LANE VEL" : "LANE GATE"));
    laneButton.setEnabled (pr);
    roll.setLane ((PianoRollComponent::Lane) l);
    grid.setVisible (! pr && ! compact);
    roll.setVisible (pr && ! compact);
    for (auto* b : { &regenButton, &varButton, &velButton, &gateButton, &laneButton })
        b->setVisible (! compact);
}

void ChannelStripComponent::refresh()
{
    const auto& ch = proc.getMatrix().getPattern().channels[(size_t) channel];
    const bool master = proc.getState().getRawParameterValue (ParamID::mergeOut)->load() > 0.5f;
    stats.setText ((master ? juce::String ("-> master ch 1") : "-> ch " + juce::String (ch.midiChannel))
                   + "  |  " + juce::String (ch.countActive()) + " notes"
                   + (ch.userEdited ? "  |  edited" : ""), juce::dontSendNotification);
    const bool on = ch.enabled;
    for (auto* b : { &regenButton, &varButton, &velButton, &gateButton })
        b->setEnabled (on);
    grid.refresh();
    roll.refresh();
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
    const bool wasCompact = compact;
    compact = getHeight() < 96;
    if (compact != wasCompact)
        updateViewButtons();

    auto r = getLocalBounds().reduced (8, 4);

    if (compact)
    {
        // collapsed row: only the essentials on one line, no grid
        auto row = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (24, r.getHeight()));
        onButton.setBounds (row.removeFromLeft (70));
        styleBox.setBounds (row.removeFromLeft (150).reduced (2, 0));
        midiBox.setBounds (row.removeFromLeft (80).reduced (2, 0));
        viewButton.setBounds (row.removeFromLeft (100).reduced (2, 0));
        row.removeFromLeft (8);
        stats.setBounds (row);
        return;
    }

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

    left.removeFromTop (4);
    auto row3 = left.removeFromTop (22);
    midiBox.setBounds (row3.removeFromLeft (76));
    row3.removeFromLeft (6);
    stats.setBounds (row3);

    left.removeFromTop (4);
    auto row4 = left.removeFromTop (22);
    viewButton.setBounds (row4.removeFromLeft (row4.getWidth() / 2).reduced (1, 0));
    laneButton.setBounds (row4.reduced (1, 0));

    r.removeFromLeft (8);
    grid.setBounds (r);
    roll.setBounds (r);
}
} // namespace pam
