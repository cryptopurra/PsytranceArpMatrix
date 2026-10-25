#include "PluginEditor.h"

namespace pam
{
namespace
{
    const juce::Colour kBg       { 0xff08111b };
    const juce::Colour kPanel    { 0xff0f1b29 };
    const juce::Colour kAccent   { 0xff35e0ff };
    const std::array<juce::Colour, kNumChannels> kChannelColours {
        juce::Colour (0xff5cff8d),  // acid
        juce::Colour (0xff35e0ff),  // fm lead
        juce::Colour (0xffb98cff),  // lead synth
        juce::Colour (0xffffb347)   // stabs
    };
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), proc (p), dragSource (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.painter = [this] (juce::Graphics& g) { paintContent (g); };
    auto& lf = lnf;
    lf.setColour (juce::ResizableWindow::backgroundColourId, kBg);
    lf.setColour (juce::ComboBox::backgroundColourId, kPanel.brighter (0.1f));
    lf.setColour (juce::ComboBox::outlineColourId, kAccent.withAlpha (0.4f));
    lf.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff15304a));
    lf.setColour (juce::TextButton::buttonOnColourId, kAccent.darker (0.3f));
    lf.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    lf.setColour (juce::Slider::thumbColourId, kAccent);
    lf.setColour (juce::ToggleButton::tickColourId, kAccent);
    lf.setColour (juce::PopupMenu::backgroundColourId, kPanel);
    lf.setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha (0.35f));

    //--- header ------------------------------------------------------------------
    title.setText ("PSYTRANCE ARP MATRIX  v" JucePlugin_VersionString, juce::dontSendNotification);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kAccent);
    content.addAndMakeVisible (title);

    presetName.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    presetName.setJustificationType (juce::Justification::centred);
    content.addAndMakeVisible (presetName);

    presetsButton.onClick  = [this] { showPresetMenu(); };
    saveButton.onClick     = [this]
    {
        auto f = proc.quickSave();
        setStatus (f.existsAsFile() ? "Saved: Sequences/User_Exported/" + f.getFileName() : "Save failed");
    };
    saveAsButton.onClick   = [this] { showSaveAsDialog(); };
    importButton.onClick   = [this] { chooseMidiToImport(); };
    generateButton.onClick = [this] { proc.generate(); setStatus ("Generated " + juce::String (proc.getMatrix().getPattern().numEnabled()) + " channel(s)"); };
    resetButton.onClick    = [this] { proc.resetToSingleArp(); setStatus ("Reset to Single Arp Mode"); };
    panicButton.onClick    = [this] { proc.panic(); setStatus ("All notes off"); };

    generateButton.setTooltip ("Roll: regenerate every active channel (Interlocking Matrix + call & response)");
    resetButton.setTooltip ("Back to Single Arp Mode: only channel 1, freshly generated");
    saveButton.setTooltip ("One-click save into Documents/PsytranceArpMatrix/Sequences/User_Exported");
    importButton.setTooltip ("Load a MIDI file: notes are analysed (key + scale) and loaded into the matrix. You can also drop .mid files on the plugin.");

    for (auto* b : { &presetsButton, &saveButton, &saveAsButton, &importButton, &generateButton, &resetButton, &panicButton })
        content.addAndMakeVisible (b);

    //--- key / global ------------------------------------------------------------
    juce::StringArray roots;
    for (int i = 0; i < 12; ++i)
        roots.add (ScaleLibrary::noteName (i));
    addCombo (rootBox, roots, ParamID::root, "Root note");
    addCombo (scaleBox, ScaleLibrary::get().names(), ParamID::scale, "Scale (36 scales + Custom)");
    addToggle (lockToggle, ParamID::lock, "LOCK", "Lock Mode: every generated and played note is forced into the scale");
    addToggle (autoMatchToggle, ParamID::autoMatch, "AUTO MATCH", "Follow the key detected from the notes you play");

    for (auto* s : { &lengthSlider, &densitySlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
        content.addAndMakeVisible (s);
    }
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (proc.getState(), ParamID::length, lengthSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (proc.getState(), ParamID::density, densitySlider));
    addCaption (rootBox, "ROOT");
    addCaption (scaleBox, "SCALE");
    addCaption (lengthSlider, "LENGTH");
    addCaption (densitySlider, "DENSITY");

    detected.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    detected.setFont (juce::FontOptions (12.0f));
    content.addAndMakeVisible (detected);

    undoButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));   // left arrow
    redoButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));   // right arrow
    undoButton.setTooltip ("Undo: back to the previous state (generate, edits, channel on/off, key...)");
    redoButton.setTooltip ("Redo");
    undoButton.onClick = [this] { proc.undo(); setStatus ("Undo"); };
    redoButton.onClick = [this] { proc.redo(); setStatus ("Redo"); };
    content.addAndMakeVisible (undoButton);
    content.addAndMakeVisible (redoButton);

    const char* pageNames[] = { "1-16", "17-32", "33-48", "49-64" };
    for (int i = 0; i < 4; ++i)
    {
        pageButtons[(size_t) i].setButtonText (pageNames[i]);
        pageButtons[(size_t) i].setClickingTogglesState (false);
        pageButtons[(size_t) i].onClick = [this, i] { setPage (i); };
        pageButtons[(size_t) i].onRightClick = [this, i]
        {
            const int c = proc.ui.selectedChannel;
            const bool lock = ! proc.getMatrix().isBarLocked (c, i);
            proc.editMatrix ([c, i, lock] (ChannelMatrix& m) { m.setBarLocked (c, i, lock); });
            setStatus ((lock ? "Locked bar " : "Unlocked bar ") + juce::String (i + 1) + " of channel " + juce::String (c + 1)
                       + (lock ? " (kept by Roll / Generate)" : ""));
        };
        pageButtons[(size_t) i].setTooltip ("Left click: show these 16 steps. Right click: lock / unlock this bar "
                                             "for the selected channel, so Roll / Generate do not change it.");
        content.addAndMakeVisible (pageButtons[(size_t) i]);
    }

    pagesCaption.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    pagesCaption.setColour (juce::Label::textColourId, kAccent.withAlpha (0.75f));
    content.addAndMakeVisible (pagesCaption);

    //--- matrix ------------------------------------------------------------------
    for (int c = 0; c < kNumChannels; ++c)
    {
        strips[(size_t) c] = std::make_unique<ChannelStripComponent> (proc, c, kChannelColours[(size_t) c]);
        strips[(size_t) c]->onLayoutChanged = [this] { layoutContent(); };
        strips[(size_t) c]->onSelected = [this] { updatePageButtons(); for (auto& s : strips) s->repaint(); };
        content.addAndMakeVisible (*strips[(size_t) c]);
    }

    for (int i = 0; i < kNumChannels - 1; ++i)
    {
        auto& sp = splitters[(size_t) i];
        sp.onStart = [this] { dragStartHeights = rowHeights; };
        sp.onDrag = [this, i] (int dy)
        {
            constexpr int minH = 40;
            auto h = dragStartHeights;
            dy = juce::jlimit (minH - h[(size_t) i], h[(size_t) i + 1] - minH, dy);
            h[(size_t) i] += dy;
            h[(size_t) i + 1] -= dy;
            rowHeights = h;
            manualRows = true;
            layoutContent();
        };
        sp.onReset = [this] { manualRows = false; layoutContent(); };
        content.addAndMakeVisible (sp);
    }

    //--- performance -------------------------------------------------------------
    addCombo (divisionBox, PatternSerializer::timeDivisionNames(), ParamID::division, "Step length");
    addCombo (modeBox, PatternSerializer::arpModeNames(), ParamID::arpMode,
              "Channel 1 with held keys: Up/Down/... arpeggiate the keys with the pattern rhythm, Step plays the pattern transposed");
    addCombo (velModeBox, PatternSerializer::velocityModeNames(), ParamID::velMode, "Where the output velocity comes from");
    addCombo (holdBox, PatternSerializer::holdModeNames(), ParamID::holdMode, "Clean Hold: latch, a new chord replaces the old one");
    addCombo (varStyleBox, PatternSerializer::variationStyleNames(), ParamID::varStyle, "Melody variation algorithm");
    addCombo (octaveBox, { "-3", "-2", "-1", "+0", "+1", "+2", "+3" }, ParamID::octave, "Global octave");
    addCombo (octRangeBox, { "1", "2", "3", "4" }, ParamID::octRange, "Arp octave range");

    addKnob (swingKnob, ParamID::swing, "SWING");
    addKnob (gateKnob, ParamID::gate, "GATE");
    addKnob (velocityKnob, ParamID::velocity, "VELOCITY");
    addKnob (varAmountKnob, ParamID::varAmount, "VARIATION");

    addToggle (keyFollowToggle, ParamID::keyFollow, "KEY FOLLOW", "Held MIDI key transposes the sequence (playing the root note = no change). Leave OFF if you only write a long note to start the pattern: with FREE RUN on no note is needed at all.");
    addToggle (freeRunToggle, ParamID::freeRun, "FREE RUN", "Play with the transport even when no key is held");
    addCombo (outputBox, { "Per channel", "Master (ch 1)" }, ParamID::mergeOut,
              "Per channel: each matrix channel goes out on its own MIDI channel (OUT 1-16 in every row), "
              "so channel 1 can drive Serum, channel 2 Spire, ... Master: everything on MIDI channel 1");
    addCaption (outputBox, "MIDI OUTPUT");

    multitrackToggle.setButtonText ("1 FILE");
    multitrackToggle.setTooltip ("Drag a single multitrack MIDI file instead of one file per channel");
    multitrackToggle.onClick = [this] { dragSource.setMultitrack (multitrackToggle.getToggleState()); };
    content.addAndMakeVisible (multitrackToggle);

    addCaption (divisionBox, "TIME / SYNC");
    addCaption (modeBox, "MODE");
    addCaption (velModeBox, "VELOCITY MODE");
    addCaption (holdBox, "HOLD");
    addCaption (varStyleBox, "VARIATION STYLE");
    addCaption (octaveBox, "OCT");
    addCaption (octRangeBox, "OCT RANGE");

    newVarButton.onClick   = [this] { proc.newVariation(); setStatus ("New variation"); };
    originalButton.onClick = [this] { proc.restoreOriginal(); setStatus ("Original pattern restored"); };
    saveMidiButton.onClick = [this]
    {
        auto files = proc.saveMidiToUserFolder (multitrackToggle.getToggleState());
        setStatus ("Saved " + juce::String (files.size()) + " MIDI file(s) to Sequences/User_Exported/MIDI");
    };
    for (auto* b : { &newVarButton, &originalButton, &saveMidiButton })
        content.addAndMakeVisible (b);
    content.addAndMakeVisible (dragSource);

    status.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    status.setFont (juce::FontOptions (12.0f));
    content.addAndMakeVisible (status);

    proc.onMatrixChanged = [this] { refreshAll(); };

    setPage (0);
    setResizable (true, true);
    sizeBox.addItemList (sizeNames(), 1);
    sizeBox.setScrollWheelEnabled (true);
    sizeBox.setTooltip ("Plugin window size (you can also drag the bottom-right corner)");
    sizeBox.onChange = [this]
    {
        const int i = sizeBox.getSelectedItemIndex();
        if (i < 0) return;
        proc.ui.sizeIndex = i;
        const auto s = sizeFor (i);
        setSize (s.x, s.y);
    };
    content.addAndMakeVisible (sizeBox);
    addCaption (sizeBox, "SIZE");

    setResizeLimits (640, 450, 2200, 1500);
    {
        const auto s = sizeFor (proc.ui.sizeIndex);
        setSize (s.x, s.y);
        sizeBox.setSelectedItemIndex (proc.ui.sizeIndex, juce::dontSendNotification);
    }
    refreshAll();
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    proc.onMatrixChanged = nullptr;
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void PluginEditor::addCombo (juce::ComboBox& box, const juce::StringArray& items, const char* id, const juce::String& tip)
{
    box.addItemList (items, 1);
    box.setTooltip (tip + "  (mouse wheel: change value)");
    box.setScrollWheelEnabled (true);
    content.addAndMakeVisible (box);
    comboAttachments.push_back (std::make_unique<ComboAttachment> (proc.getState(), id, box));
}

void PluginEditor::addKnob (juce::Slider& s, const char* id, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    content.addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (proc.getState(), id, s));
    addCaption (s, name);
}

void PluginEditor::addToggle (juce::ToggleButton& b, const char* id, const juce::String& text, const juce::String& tip)
{
    b.setButtonText (text);
    b.setTooltip (tip);
    content.addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.getState(), id, b));
}

void PluginEditor::addCaption (juce::Component& c, const juce::String& text)
{
    auto l = std::make_unique<juce::Label> (juce::String(), text);
    l->setFont (juce::FontOptions (11.0f, juce::Font::bold));
    l->setColour (juce::Label::textColourId, kAccent.withAlpha (0.75f));
    l->setJustificationType (juce::Justification::centredLeft);
    content.addAndMakeVisible (*l);
    captions[&c] = std::move (l);
}

void PluginEditor::placeWithCaption (juce::Component& c, juce::Rectangle<int> area)
{
    if (auto it = captions.find (&c); it != captions.end())
        it->second->setBounds (area.removeFromTop (16));
    c.setBounds (area);
}

//==============================================================================
juce::StringArray PluginEditor::sizeNames()
{
    return { "XS  760 x 535", "S  880 x 620", "M  1080 x 760", "L  1280 x 900", "XL  1500 x 1050", "XXL  1800 x 1260" };
}

juce::Point<int> PluginEditor::sizeFor (int index)
{
    static const juce::Point<int> sizes[] = { { 760, 535 }, { 880, 620 }, { 1080, 760 }, { 1280, 900 }, { 1500, 1050 }, { 1800, 1260 } };
    return sizes[juce::jlimit (0, 5, index)];
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
}

void PluginEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kPanel);
    g.fillRect (content.getLocalBounds().removeFromTop (52));
    g.fillRoundedRectangle (content.getLocalBounds().removeFromBottom (170).reduced (8, 4).toFloat(), 8.0f);
}

void PluginEditor::resized()
{
    // below the design size everything is scaled down uniformly; above it the layout stretches
    constexpr float designW = 1080.0f, designH = 760.0f;
    const float s = juce::jmin (1.0f, (float) getWidth() / designW, (float) getHeight() / designH);
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, juce::roundToInt ((float) getWidth() / s), juce::roundToInt ((float) getHeight() / s));
    layoutContent();
}

void PluginEditor::layoutContent()
{
    auto r = content.getLocalBounds();

    // header
    auto header = r.removeFromTop (52).reduced (10, 10);
    title.setBounds (header.removeFromLeft (300));
    for (auto* b : { &panicButton, &resetButton, &generateButton, &importButton, &saveAsButton, &saveButton, &presetsButton })
    {
        b->setBounds (header.removeFromRight (b == &importButton ? 112 : 92).reduced (2, 0));
    }
    presetName.setBounds (header);

    // key row
    auto keyRow = r.removeFromTop (58).reduced (10, 4);
    placeWithCaption (rootBox, keyRow.removeFromLeft (70));
    keyRow.removeFromLeft (8);
    placeWithCaption (scaleBox, keyRow.removeFromLeft (210));
    keyRow.removeFromLeft (8);
    lockToggle.setBounds (keyRow.removeFromLeft (70).withTrimmedTop (16));
    autoMatchToggle.setBounds (keyRow.removeFromLeft (110).withTrimmedTop (16));
    auto undoArea = keyRow.removeFromRight (84).withTrimmedTop (16);
    undoButton.setBounds (undoArea.removeFromLeft (42).reduced (2, 0));
    redoButton.setBounds (undoArea.reduced (2, 0));
    keyRow.removeFromRight (8);
    auto pages = keyRow.removeFromRight (240).withTrimmedTop (16);
    keyRow.removeFromRight (8);
    const int sliderW = juce::jmax (100, (keyRow.getWidth() - 8) / 2);
    placeWithCaption (lengthSlider, keyRow.removeFromLeft (sliderW));
    keyRow.removeFromLeft (8);
    placeWithCaption (densitySlider, keyRow.removeFromLeft (sliderW));
    pagesCaption.setBounds (pages.translated (0, -16).withHeight (16));
    for (auto& b : pageButtons)
        b.setBounds (pages.removeFromLeft (60).reduced (2, 0));

    // bottom performance panel
    auto bottom = r.removeFromBottom (170).reduced (18, 10);
    auto statusRow = bottom.removeFromBottom (18);
    detected.setJustificationType (juce::Justification::centredRight);
    detected.setBounds (statusRow.removeFromRight (statusRow.getWidth() / 2));
    status.setBounds (statusRow);

    auto knobs = bottom.removeFromLeft (300);
    const int kw = knobs.getWidth() / 4;
    for (auto* k : { &swingKnob, &gateKnob, &velocityKnob, &varAmountKnob })
        placeWithCaption (*k, knobs.removeFromLeft (kw).reduced (4, 0));

    bottom.removeFromLeft (10);
    auto combos1 = bottom.removeFromTop (44);
    for (auto* c : { &divisionBox, &modeBox, &velModeBox, &holdBox })
        placeWithCaption (*c, combos1.removeFromLeft (130).reduced (3, 0));
    placeWithCaption (octaveBox, combos1.removeFromLeft (60).reduced (3, 0));
    placeWithCaption (octRangeBox, combos1.removeFromLeft (80).reduced (3, 0));

    bottom.removeFromTop (6);
    auto row2 = bottom.removeFromTop (44);
    placeWithCaption (varStyleBox, row2.removeFromLeft (130).reduced (3, 0));
    newVarButton.setBounds (row2.removeFromLeft (124).withTrimmedTop (16).reduced (3, 0));
    originalButton.setBounds (row2.removeFromLeft (90).withTrimmedTop (16).reduced (3, 0));
    keyFollowToggle.setBounds (row2.removeFromLeft (110).withTrimmedTop (16));
    freeRunToggle.setBounds (row2.removeFromLeft (100).withTrimmedTop (16));
    placeWithCaption (outputBox, row2.removeFromLeft (130).reduced (3, 0));

    bottom.removeFromTop (6);
    auto row3 = bottom.removeFromTop (34);
    dragSource.setBounds (row3.removeFromLeft (300));
    row3.removeFromLeft (8);
    multitrackToggle.setBounds (row3.removeFromLeft (80));
    saveMidiButton.setBounds (row3.removeFromLeft (110).reduced (2, 2));
    row3.removeFromLeft (16);
    sizeBox.setBounds (row3.removeFromLeft (150).reduced (2, 3));
    if (auto it = captions.find (&sizeBox); it != captions.end())
        it->second->setBounds (sizeBox.getBounds().translated (0, -16).withHeight (16));

    // matrix: rows share the height automatically (disabled channels collapse), or follow the
    // heights the user dragged with the splitters (double-click a splitter to go back to auto)
    auto matrixArea = r.reduced (8, 2);
    constexpr int gap = 6, compactH = 40;
    const int available = matrixArea.getHeight() - gap * (kNumChannels - 1);

    int mask = 0;
    for (int c = 0; c < kNumChannels; ++c)
        mask |= proc.getMatrix().isChannelEnabled (c) ? (1 << c) : 0;
    if (mask != lastMask)
    {
        manualRows = false;
        lastMask = mask;
    }

    std::array<int, kNumChannels> h {};
    if (manualRows)
    {
        int sum = 0;
        for (auto v : rowHeights) sum += v;
        int used = 0;
        for (int c = 0; c < kNumChannels; ++c)
        {
            h[(size_t) c] = c == kNumChannels - 1 ? available - used
                                                  : juce::roundToInt ((float) available * (float) rowHeights[(size_t) c] / (float) juce::jmax (1, sum));
            used += h[(size_t) c];
        }
    }
    else
    {
        const int n = (int) std::bitset<kNumChannels> ((unsigned long) mask).count();
        const bool equal = n == 0 || n == kNumChannels || available - (kNumChannels - n) * compactH < n * 110;
        int used = 0;
        for (int c = 0; c < kNumChannels; ++c)
        {
            const bool on = (mask >> c) & 1;
            int v = equal ? available / kNumChannels
                          : (on ? (available - (kNumChannels - n) * compactH) / n : compactH);
            if (c == kNumChannels - 1)
                v = available - used;
            h[(size_t) c] = v;
            used += v;
        }
    }
    rowHeights = h;

    for (int c = 0; c < kNumChannels; ++c)
    {
        strips[(size_t) c]->setBounds (matrixArea.removeFromTop (h[(size_t) c]));
        if (c < kNumChannels - 1)
            splitters[(size_t) c].setBounds (matrixArea.removeFromTop (gap));
    }
}

//==============================================================================
void PluginEditor::setPage (int p)
{
    page = p;
    updatePageButtons();
    for (auto& s : strips)
        s->setPage (p);
}

void PluginEditor::updatePageButtons()
{
    const int c = proc.ui.selectedChannel;
    const char* names[] = { "1-16", "17-32", "33-48", "49-64" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = pageButtons[(size_t) i];
        const bool locked = proc.getMatrix().isBarLocked (c, i);
        b.setButtonText (locked ? juce::String::fromUTF8 ("\xe2\x96\xa0 ") + names[i] : juce::String (names[i]));
        auto col = i == page ? kAccent.darker (0.4f) : juce::Colour (0xff15304a);
        if (locked)
            col = col.interpolatedWith (juce::Colour (0xffff9d3b), 0.6f);
        b.setColour (juce::TextButton::buttonColourId, col);
    }
    pagesCaption.setText ("BARS  CH " + juce::String (c + 1) + "  (right-click = lock)", juce::dontSendNotification);
}

void PluginEditor::refreshAll()
{
    presetName.setText (proc.getCurrentPresetName() + "   |   " + juce::String (proc.getMatrix().getPattern().numEnabled())
                        + " active channel(s)", juce::dontSendNotification);
    for (auto& s : strips)
        s->refresh();

    const int len = proc.getMatrix().getLength();
    for (int i = 0; i < 4; ++i)
        pageButtons[(size_t) i].setEnabled (i * 16 < len);
    updatePageButtons();
    if (page * 16 >= len)
        setPage (0);

    int mask = 0;
    for (int c = 0; c < kNumChannels; ++c)
        mask |= proc.getMatrix().isChannelEnabled (c) ? (1 << c) : 0;
    if (mask != lastMask)
        layoutContent();

    dragSource.repaint();
    repaint();
}

void PluginEditor::timerCallback()
{
    undoButton.setEnabled (proc.canUndo());
    redoButton.setEnabled (proc.canRedo());
    for (auto& s : strips)
        s->refresh();

    auto k = proc.detectedKey();
    auto imp = proc.lastImportKey();
    juce::String text = "Input: " + k.describe();
    if (proc.heldKeys() > 0)
    {
        const int t = proc.keyFollowTranspose();
        text << "   |   Keys held: " << proc.heldKeys();
        if (t != 0)
            text << " (Key Follow " << (t > 0 ? "+" : "") << t << " st)";
    }
    if (imp.valid)
        text << "   |   Last import: " << imp.describe();
    detected.setText (text, juce::dontSendNotification);

    if (proc.getMatrix().getVersion() != lastVersion)
    {
        lastVersion = proc.getMatrix().getVersion();
        dragSource.repaint();
    }
}

void PluginEditor::setStatus (const juce::String& s)
{
    status.setText (s, juce::dontSendNotification);
}

//==============================================================================
void PluginEditor::showPresetMenu()
{
    auto& pm = proc.getPresetManager();
    auto files = std::make_shared<std::vector<juce::File>>();

    // folders become sub-menus (recursively), so a large imported library stays browsable:
    // Presets/Library/<Category>/<Pack>/<preset>.json
    std::function<juce::PopupMenu (const juce::File&)> buildMenu = [&] (const juce::File& dir)
    {
        juce::PopupMenu m;
        auto dirs = dir.findChildFiles (juce::File::findDirectories, false);
        std::sort (dirs.begin(), dirs.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        for (auto& d : dirs)
        {
            auto sub = buildMenu (d);
            if (sub.getNumItems() > 0)
                m.addSubMenu (d.getFileName().replace ("_", " "), sub);
        }

        auto jsons = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        std::sort (jsons.begin(), jsons.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        for (auto& f : jsons)
        {
            files->push_back (f);
            m.addItem ((int) files->size(), f.getFileNameWithoutExtension());
        }
        return m;
    };

    juce::PopupMenu menu = buildMenu (pm.getPresetsDir());
    auto user = buildMenu (pm.getUserExportDir());
    if (user.getNumItems() > 0)
        menu.addSubMenu ("User Exported", user);
    menu.addSeparator();
    menu.addItem (100000, "Open preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsButton),
                        [this, files] (int r)
    {
        if (r == 100000)
            proc.getPresetManager().getRoot().startAsProcess();
        else if (r > 0 && r <= (int) files->size())
        {
            const auto& f = (*files)[(size_t) (r - 1)];
            setStatus (proc.loadPreset (f) ? "Loaded " + f.getFileName() : "Could not load " + f.getFileName());
        }
    });
}

void PluginEditor::showSaveAsDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name and category", juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", proc.getCurrentPresetName(), "Name");
    saveDialog->addComboBox ("category", PresetManager::factoryCategories(), "Category");
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveDialog != nullptr)
        {
            const auto name = saveDialog->getTextEditorContents ("name");
            const auto cat = saveDialog->getComboBoxComponent ("category")->getText();
            auto f = proc.savePresetAs (name, cat);
            setStatus (f.existsAsFile() ? "Saved preset " + cat + "/" + f.getFileName() : "Save failed");
            refreshAll();
        }
        saveDialog.reset();
    }), false);
}

void PluginEditor::chooseMidiToImport()
{
    chooser = std::make_unique<juce::FileChooser> ("Import MIDI", proc.getPresetManager().getRoot(), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile())
            return;
        juce::String msg;
        proc.importMidi (f, 0, msg);
        setStatus (msg);
    });
}

bool PluginEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi") || f.endsWithIgnoreCase (".json"))
            return true;
    return false;
}

void PluginEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    // dropped on a channel row -> import into that channel
    int target = 0;
    for (int c = 0; c < kNumChannels; ++c)
        if (strips[(size_t) c]->getBounds().contains (content.getLocalPoint (this, juce::Point<int> (x, y))))
            target = c;

    for (auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension (".json"))
        {
            setStatus (proc.loadPreset (f) ? "Loaded " + f.getFileName() : "Not a pattern file");
            return;
        }

        juce::String msg;
        proc.importMidi (f, target, msg);
        setStatus (msg);
        return;
    }
}
} // namespace pam
