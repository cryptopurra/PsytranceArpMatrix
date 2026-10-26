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

    for (int i = -3; i <= 3; ++i)
        octBox.addItem ("OCT " + juce::String (i > 0 ? "+" : "") + juce::String (i), i + 4);
    octBox.setScrollWheelEnabled (true);
    octBox.setTooltip ("Octave of this channel only (mouse wheel to change)");
    addAndMakeVisible (octBox);

    voiceBox.addItemList ({ "POLY", "MONO", "LEGATO" }, 1);
    voiceBox.setScrollWheelEnabled (true);
    voiceBox.setTooltip ("POLY: chords, free overlaps.  MONO: one note at a time, each note cuts the previous one, "
                         "slide steps overlap (classic 303 glide with a legato synth).  LEGATO: every note is tied "
                         "to the next (continuous legato / glide). Set the synth to mono + legato/portamento.");
    addAndMakeVisible (voiceBox);

    glideSlider.setSliderStyle (juce::Slider::LinearBar);
    glideSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 0, 0);
    glideSlider.setColour (juce::Slider::trackColourId, col.withAlpha (0.45f));
    glideSlider.setTooltip ("Portamento time sent to the synth as MIDI CC5 (+ CC65 portamento on). "
                            "Off = nothing is sent. MIDI-learn CC5 in the synth if it does not respond by default.");
    addAndMakeVisible (glideSlider);

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd64545));
    muteButton.onClick = [this] { repaint(); };
    muteButton.setTooltip ("Mute this channel (no MIDI out, the pattern is kept; turning CH off/on instead makes a new one)");
    addAndMakeVisible (muteButton);

    roleBox.addItemList ({ "AUTO", "CALL Q", "ANSW A" }, 1);
    roleBox.setScrollWheelEnabled (true);
    roleBox.setTooltip ("Call & response (Q = question/domanda, A = answer/risposta): CALL asks the musical question, "
                        "ANSW replies in the gaps and resolves on the tonic. AUTO: the first active channel asks, the "
                        "others answer. Changing it regenerates the channel.");
    roleBox.onChange = [this]
    {
        const int role = roleBox.getSelectedItemIndex();
        if (role < 0 || role == proc.getMatrix().getRole (channel))
            return;
        proc.editMatrix ([c = channel, role] (ChannelMatrix& m)
        {
            m.setRole (c, role);
            if (m.isChannelEnabled (c))
                m.regenerateChannel (c); // hear the new role straight away
        });
    };
    addAndMakeVisible (roleBox);

    auto& state = proc.getState();
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ParamID::channelOn (c), onButton);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ParamID::channelMute (c), muteButton);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ParamID::channelStyle (c), styleBox);
    midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ParamID::channelMidi (c), midiBox);
    octAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ParamID::channelOct (c), octBox);
    voiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ParamID::channelVoice (c), voiceBox);
    glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamID::channelGlide (c), glideSlider);
    // after the attachment (it installs its own text function)
    glideSlider.textFromValueFunction = [] (double v) { return v < 0.5 ? juce::String ("GLIDE off") : "GLIDE " + juce::String (juce::roundToInt (v)); };
    glideSlider.updateText();

    regenButton.setTooltip ("Regenerate this channel only (others stay untouched)");
    regenButton.onClick = [this] { proc.editMatrix ([ch = channel] (ChannelMatrix& m) { m.regenerateChannel (ch); }); };

    varButton.setTooltip ("Apply the selected melody variation to this channel");
    varButton.onClick = [this]
    {
        const auto [style, amount] = proc.variationFor (channel); // this channel's own variation settings
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
    popButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\x97"));   // north-east arrow
    popButton.setTooltip ("Open this channel's piano roll in a separate window");
    popButton.onClick = [this] { if (onPopOut) onPopOut(); };
    grid.onViewChanged = [this] { if (onViewChanged) onViewChanged(); };
    roll.onViewChanged = [this] { if (onViewChanged) onViewChanged(); };
    addAndMakeVisible (viewButton);
    addAndMakeVisible (laneButton);
    addAndMakeVisible (popButton);
    updateViewButtons();
    addMouseListener (this, true); // clicks anywhere in the row select the channel
    refresh();
}

void ChannelStripComponent::mouseDown (const juce::MouseEvent&)
{
    if (proc.ui.selectedChannel != channel)
    {
        proc.selectChannel (channel);
        if (onSelected)
            onSelected();
    }
}

juce::Range<double> ChannelStripComponent::visibleSteps() const
{
    if (isPianoRoll() && ! compact)
        return roll.getVisibleSteps();
    return { (double) proc.ui.gridFirst, (double) (proc.ui.gridFirst + proc.ui.gridCount) };
}

void ChannelStripComponent::updateViewButtons()
{
    const bool pr = isPianoRoll();
    viewButton.setButtonText (pr ? "PIANO" : "STEPS");
    const int l = proc.ui.lane[(size_t) channel];
    laneButton.setButtonText (l == 0 ? "NO LANE" : (l == 1 ? "VEL" : "GATE"));
    laneButton.setEnabled (pr);
    roll.setLane ((PianoRollComponent::Lane) l);
    grid.setVisible (! pr && ! compact);
    roll.setVisible (pr && ! compact);
    for (auto* b : std::initializer_list<juce::Component*> { &regenButton, &varButton, &velButton, &gateButton, &laneButton, &popButton, &glideSlider, &roleBox })
        b->setVisible (! compact);
}

void ChannelStripComponent::refresh()
{
    const auto& ch = proc.getMatrix().getPattern().channels[(size_t) channel];
    const bool master = proc.getState().getRawParameterValue (ParamID::mergeOut)->load() > 0.5f;
    juce::ignoreUnused (master);
    stats.setText (juce::String (ch.countActive()) + (ch.userEdited ? " nt*" : " nt"), juce::dontSendNotification);
    stats.setTooltip (juce::String (ch.countActive()) + " notes" + (ch.userEdited ? " (edited)" : "")
                      + (master ? ", MIDI OUTPUT = Master: sent on channel 1" : ", sent on MIDI channel " + juce::String (ch.midiChannel)));
    // role: show what AUTO currently means for this channel
    const int role = proc.getMatrix().getRole (channel);
    const bool answer = proc.getMatrix().isAnswer (channel);
    roleBox.changeItemText (1, answer ? "AUTO A" : "AUTO Q");
    roleBox.setSelectedItemIndex (role, juce::dontSendNotification);

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
    if (proc.ui.selectedChannel == channel)
    {
        g.setColour (colour.withAlpha (0.7f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.2f);
    }

}

void ChannelStripComponent::paintOverChildren (juce::Graphics& g)
{
    if (muteButton.getToggleState())
    {
        // muted: dim the grid area (controls stay readable)
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (grid.isVisible() ? grid.getBounds() : roll.getBounds());
    }
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
        onButton.setBounds (row.removeFromLeft (66));
        muteButton.setBounds (row.removeFromLeft (28).reduced (2, 0));
        styleBox.setBounds (row.removeFromLeft (140).reduced (2, 0));
        midiBox.setBounds (row.removeFromLeft (76).reduced (2, 0));
        octBox.setBounds (row.removeFromLeft (80).reduced (2, 0));
        voiceBox.setBounds (row.removeFromLeft (90).reduced (2, 0));
        viewButton.setBounds (row.removeFromLeft (96).reduced (2, 0));
        row.removeFromLeft (8);
        stats.setBounds (row);
        return;
    }

    auto left = r.removeFromLeft (270);

    auto row1 = left.removeFromTop (24);
    onButton.setBounds (row1.removeFromLeft (66));
    muteButton.setBounds (row1.removeFromLeft (28).reduced (2, 1));
    row1.removeFromLeft (2);
    popButton.setBounds (row1.removeFromRight (26).reduced (1, 1));
    row1.removeFromRight (2);
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
    midiBox.setBounds (row3.removeFromLeft (72));
    row3.removeFromLeft (3);
    octBox.setBounds (row3.removeFromLeft (78));
    row3.removeFromLeft (3);
    voiceBox.setBounds (row3);
    stats.setBounds ({});

    left.removeFromTop (4);
    auto row4 = left.removeFromTop (22);
    viewButton.setBounds (row4.removeFromLeft (60).reduced (1, 0));
    laneButton.setBounds (row4.removeFromLeft (52).reduced (1, 0));
    roleBox.setBounds (row4.removeFromLeft (94).reduced (1, 0));
    glideSlider.setBounds (row4.reduced (1, 1));

    r.removeFromLeft (8);
    grid.setBounds (r);
    roll.setBounds (r);
}
} // namespace pam
