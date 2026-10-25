#include "PianoRollComponent.h"

namespace pam
{
namespace
{
    bool isBlackKey (int note) noexcept
    {
        switch (((note % 12) + 12) % 12)
        {
            case 1: case 3: case 6: case 8: case 10: return true;
            default: return false;
        }
    }
}

PianoRollComponent::PianoRollComponent (PluginProcessor& p, int c) : proc (p), channel (c)
{
    setWantsKeyboardFocus (false);
    centreOnNotes();
}

const ChannelPattern& PianoRollComponent::channelData() const
{
    return proc.getMatrix().getPattern().channels[(size_t) channel];
}

//==============================================================================
juce::Rectangle<int> PianoRollComponent::laneArea() const
{
    if (lane == Lane::None)
        return {};
    auto r = getLocalBounds().withTrimmedLeft (keyboardWidth);
    const float frac = juce::jlimit (0.1f, 0.75f, proc.ui.laneSize[(size_t) channel]);
    return r.removeFromBottom (juce::jmax (20, juce::roundToInt ((float) getHeight() * frac)));
}

juce::Rectangle<int> PianoRollComponent::noteArea() const
{
    auto r = getLocalBounds().withTrimmedLeft (keyboardWidth);
    if (lane != Lane::None)
        r.removeFromBottom (laneArea().getHeight() + 2);
    return r;
}

float PianoRollComponent::stepToX (double step) const noexcept
{
    return (float) noteArea().getX() + (float) ((step - scrollStep) * pxPerStep);
}

double PianoRollComponent::xToStep (float x) const noexcept
{
    return scrollStep + (double) (x - (float) noteArea().getX()) / pxPerStep;
}

float PianoRollComponent::noteToY (int note) const noexcept
{
    return (float) noteArea().getY() + (topNote - (float) note) * rowHeight;
}

int PianoRollComponent::yToNote (float y) const noexcept
{
    const float u = (y - (float) noteArea().getY()) / rowHeight;
    return juce::jlimit (0, 127, (int) std::ceil (topNote - u - 1.0e-4f));
}

juce::Rectangle<float> PianoRollComponent::noteRect (int step) const
{
    const auto& st = channelData().steps[(size_t) step];
    const float x = stepToX (step);
    const float w = juce::jmax (4.0f, st.gate * pxPerStep - 1.0f);
    return { x + 0.5f, noteToY (st.note) + 0.5f, w, rowHeight - 1.0f };
}

int PianoRollComponent::noteAt (juce::Point<float> p, bool& onRightEdge) const
{
    const auto& ch = channelData();
    onRightEdge = false;
    for (int s = 0; s < ch.length; ++s)
    {
        if (! ch.steps[(size_t) s].active)
            continue;
        auto r = noteRect (s);
        if (r.expanded (0.0f, 1.0f).contains (p))
        {
            onRightEdge = p.x > r.getRight() - juce::jmin (7.0f, r.getWidth() * 0.35f);
            return s;
        }
    }
    return -1;
}

void PianoRollComponent::clampView()
{
    pxPerStep = juce::jlimit (4.0f, 140.0f, pxPerStep);
    rowHeight = juce::jlimit (4.0f, 40.0f, rowHeight);

    const auto area = noteArea();
    const double visibleSteps = area.getWidth() / (double) pxPerStep;
    const double maxScroll = juce::jmax (0.0, (double) channelData().length - visibleSteps);
    scrollStep = juce::jlimit (0.0, maxScroll, scrollStep);

    const float visibleRows = (float) area.getHeight() / rowHeight;
    topNote = juce::jlimit (juce::jmin (127.0f, visibleRows), 127.0f, topNote);
}

void PianoRollComponent::centreOnNotes()
{
    const auto& ch = channelData();
    int lo = 127, hi = 0;
    for (int s = 0; s < ch.length; ++s)
        if (ch.steps[(size_t) s].active)
        {
            lo = juce::jmin (lo, (int) ch.steps[(size_t) s].note);
            hi = juce::jmax (hi, (int) ch.steps[(size_t) s].note);
        }
    if (lo > hi) { lo = 48; hi = 72; }

    const float visibleRows = juce::jmax (8.0f, (float) noteArea().getHeight() / rowHeight);
    topNote = (float) (lo + hi) * 0.5f + visibleRows * 0.5f;
    clampView();
}

void PianoRollComponent::resized()
{
    // fit the whole pattern horizontally by default
    const auto area = noteArea();
    if (area.getWidth() > 0)
        pxPerStep = juce::jmax (4.0f, (float) area.getWidth() / (float) juce::jmax (16, channelData().length));

    // and fit the pitch range of the pattern vertically (a little margin above and below)
    const auto& ch = channelData();
    int lo = 127, hi = 0;
    for (int s = 0; s < ch.length; ++s)
        if (ch.steps[(size_t) s].active)
        {
            lo = juce::jmin (lo, (int) ch.steps[(size_t) s].note);
            hi = juce::jmax (hi, (int) ch.steps[(size_t) s].note);
        }
    if (lo > hi) { lo = 48; hi = 72; }
    if (area.getHeight() > 0)
        rowHeight = juce::jlimit (4.0f, 14.0f, (float) area.getHeight() / (float) (hi - lo + 5));
    centreOnNotes();
}

//==============================================================================
void PianoRollComponent::paint (juce::Graphics& g)
{
    const auto& p = proc.getMatrix().getPattern();
    const auto& ch = channelData();
    const auto& scale = ScaleLibrary::get()[p.scaleIndex];
    const auto area = noteArea();
    const bool enabled = ch.enabled;

    g.setColour (juce::Colour (0xff0b131d));
    g.fillRect (getLocalBounds());

    // rows + keyboard
    const int firstNote = yToNote ((float) area.getBottom() - 1.0f);
    const int lastNote  = yToNote ((float) area.getY());
    for (int n = firstNote; n <= lastNote; ++n)
    {
        const float y = noteToY (n);
        auto row = juce::Rectangle<float> ((float) area.getX(), y, (float) area.getWidth(), rowHeight);
        juce::Colour c = isBlackKey (n) ? juce::Colour (0xff0e1824) : juce::Colour (0xff142131);
        if (scale.contains (n - p.rootNote))
            c = c.brighter (0.12f);
        if (((n - p.rootNote) % 12 + 12) % 12 == 0)
            c = c.interpolatedWith (colour, 0.10f);
        g.setColour (c);
        g.fillRect (row);
        g.setColour (juce::Colour (0xff0a1018));
        g.drawHorizontalLine ((int) (y + rowHeight), (float) area.getX(), (float) area.getRight());

        auto key = juce::Rectangle<float> (0.0f, y, (float) keyboardWidth - 2.0f, rowHeight - 0.5f);
        g.setColour (isBlackKey (n) ? juce::Colour (0xff1b2430) : juce::Colour (0xffd8dee6));
        g.fillRect (key);
        if (n % 12 == 0 && rowHeight >= 7.0f)
        {
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (juce::jmin (11.0f, rowHeight)));
            g.drawText ("C" + juce::String (n / 12 - 1), key.reduced (2.0f, 0.0f), juce::Justification::centredRight);
        }
    }

    // vertical grid: steps, beats, bars; area after the pattern end is shaded
    const int s0 = juce::jmax (0, (int) std::floor (scrollStep));
    const int s1 = (int) std::ceil (xToStep ((float) area.getRight()));
    for (int s = s0; s <= s1; ++s)
    {
        const float x = stepToX (s);
        const float alpha = s % 16 == 0 ? 0.35f : (s % 4 == 0 ? 0.18f : 0.06f);
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
    }
    const float endX = stepToX (ch.length);
    if (endX < (float) area.getRight())
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (endX, (float) area.getY(), (float) area.getRight() - endX, (float) area.getHeight()));
    }

    // playhead
    const int playing = proc.currentStep (channel);
    if (enabled && playing >= 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (stepToX (playing), (float) area.getY(), pxPerStep, (float) area.getHeight()));
    }

    // notes (chord tones drawn as ghosts)
    g.saveState();
    g.reduceClipRegion (area);
    for (int s = 0; s < ch.length; ++s)
    {
        const auto& st = ch.steps[(size_t) s];
        if (! st.active)
            continue;

        auto r = noteRect (s);
        const float vel = (float) st.velocity / 127.0f;
        auto c = (enabled ? colour : colour.withSaturation (0.15f)).withMultipliedBrightness (0.55f + 0.45f * vel);

        if (st.chord > 1)
        {
            const int deg = ScaleLibrary::noteToDegree (st.note, p.rootNote, scale, 48);
            for (int v = 1; v < juce::jmin<int> (st.chord, 4); ++v)
            {
                const int cn = ScaleLibrary::degreeToNote (deg + 2 * v, p.rootNote, scale, 48) + (st.note - ScaleLibrary::degreeToNote (deg, p.rootNote, scale, 48));
                auto gr = r.withY (noteToY (cn) + 0.5f);
                g.setColour (c.withAlpha (0.35f));
                g.fillRoundedRectangle (gr, 2.0f);
            }
        }

        g.setColour (c);
        g.fillRoundedRectangle (r, 2.0f);
        const bool sel = selected[(size_t) s];
        g.setColour (sel ? juce::Colours::yellow : (st.accent ? juce::Colours::white : c.brighter (0.6f)));
        g.drawRoundedRectangle (r, 2.0f, sel ? 2.0f : (st.accent ? 1.6f : 0.8f));

        if (st.slide)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawLine (r.getRight() - 6.0f, r.getBottom() - 1.0f, r.getRight(), r.getY() + 1.0f, 1.2f);
        }

        if (r.getWidth() > 26.0f && rowHeight >= 9.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.setFont (juce::FontOptions (juce::jmin (10.0f, rowHeight - 1.0f)));
            g.drawText (ScaleLibrary::noteName (st.note) + juce::String (st.note / 12 - 1), r.reduced (3.0f, 0.0f),
                        juce::Justification::centredLeft, false);
        }
    }
    g.restoreState();

    if (drag == Drag::Select)
    {
        g.setColour (juce::Colours::yellow.withAlpha (0.15f));
        g.fillRect (selectRect);
        g.setColour (juce::Colours::yellow.withAlpha (0.7f));
        g.drawRect (selectRect, 1.0f);
    }

    // lane
    if (lane != Lane::None)
    {
        auto la = laneArea();
        g.setColour (juce::Colour (0xff0e1824));
        g.fillRect (la);
        g.setColour (colour.withAlpha (0.5f)); // draggable border between notes and lane
        g.fillRect (la.getX(), la.getY() - 2, la.getWidth(), 2);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (lane == Lane::Velocity ? "VEL" : "GATE",
                    juce::Rectangle<int> (0, la.getY(), keyboardWidth - 2, 14), juce::Justification::centred);

        g.saveState();
        g.reduceClipRegion (la);
        for (int s = 0; s < ch.length; ++s)
        {
            const auto& st = ch.steps[(size_t) s];
            if (! st.active)
                continue;
            const float v = lane == Lane::Velocity ? (float) st.velocity / 127.0f
                                                   : juce::jlimit (0.0f, 1.0f, st.gate / 2.0f);
            const float x = stepToX (s);
            const float h = v * ((float) la.getHeight() - 4.0f);
            auto bar = juce::Rectangle<float> (x + 1.0f, (float) la.getBottom() - h - 2.0f,
                                               juce::jmax (2.0f, juce::jmin (pxPerStep - 2.0f, 10.0f)), h);
            g.setColour (st.accent ? juce::Colours::white : colour);
            g.fillRect (bar);
        }
        g.restoreState();
    }
}

//==============================================================================
void PianoRollComponent::editStep (int step, const std::function<void (Step&)>& fn)
{
    const int c = channel;
    proc.editMatrix ([=] (ChannelMatrix& m)
    {
        auto st = m.getPattern().channels[(size_t) c].steps[(size_t) step];
        fn (st);
        m.setStep (c, step, st);
    });
    repaint();
}

void PianoRollComponent::applySteps (const std::array<Step, kMaxSteps>& target)
{
    const int c = channel;
    proc.editMatrix ([c, target] (ChannelMatrix& m)
    {
        const auto& cur = m.getPattern().channels[(size_t) c].steps;
        for (int s = 0; s < kMaxSteps; ++s)
            if (! (cur[(size_t) s] == target[(size_t) s]))
                m.setStep (c, s, target[(size_t) s]);
    });
    repaint();
}

void PianoRollComponent::moveSelection (int dStep, int dNote)
{
    // rebuilt from the snapshot taken at mouse-down: the selected notes land dStep steps later
    // and dNote semitones higher, overwriting whatever is there
    const int len = channelData().length;
    int minS = kMaxSteps, maxS = -1;
    for (int s = 0; s < len; ++s)
        if (dragSelection[(size_t) s]) { minS = juce::jmin (minS, s); maxS = juce::jmax (maxS, s); }
    if (maxS < 0)
        return;
    dStep = juce::jlimit (-minS, len - 1 - maxS, dStep);

    auto target = dragOrigin;
    for (int s = 0; s < len; ++s)
        if (dragSelection[(size_t) s])
            target[(size_t) s].active = false;

    std::bitset<kMaxSteps> newSel;
    for (int s = 0; s < len; ++s)
        if (dragSelection[(size_t) s])
        {
            auto st = dragOrigin[(size_t) s];
            st.note = (int8_t) juce::jlimit (0, 127, st.note + dNote);
            st.active = true;
            target[(size_t) (s + dStep)] = st;
            newSel.set ((size_t) (s + dStep));
        }

    selected = newSel;
    applySteps (target);
}

void PianoRollComponent::resizeSelection (float dGate)
{
    auto target = dragOrigin;
    for (int s = 0; s < kMaxSteps; ++s)
        if (dragSelection[(size_t) s])
            target[(size_t) s].gate = juce::jlimit (0.1f, 4.0f, dragOrigin[(size_t) s].gate + dGate);
    applySteps (target);
}

void PianoRollComponent::deleteSelection()
{
    auto target = channelData().steps;
    for (int s = 0; s < kMaxSteps; ++s)
        if (selected[(size_t) s])
            target[(size_t) s].active = false;
    selected.reset();
    applySteps (target);
}

void PianoRollComponent::setLaneValue (juce::Point<float> p)
{
    const auto la = laneArea();
    const int s = (int) std::floor (xToStep (p.x));
    if (s < 0 || s >= channelData().length || ! channelData().steps[(size_t) s].active)
        return;

    const float v = juce::jlimit (0.0f, 1.0f, ((float) la.getBottom() - 2.0f - p.y) / ((float) la.getHeight() - 4.0f));
    if (lane == Lane::Velocity)
        editStep (s, [v] (Step& st) { st.velocity = (uint8_t) juce::jlimit (1, 127, juce::roundToInt (v * 127.0f)); });
    else
        editStep (s, [v] (Step& st) { st.gate = juce::jlimit (0.05f, 4.0f, v * 2.0f); });
}

bool PianoRollComponent::isOnLaneBorder (juce::Point<int> p) const
{
    return lane != Lane::None && p.x > keyboardWidth && std::abs (p.y - (laneArea().getY() - 1)) <= 4;
}

void PianoRollComponent::mouseDown (const juce::MouseEvent& e)
{
    drag = Drag::None;
    proc.beginGesture();

    if (isOnLaneBorder (e.getPosition()))
    {
        drag = Drag::LaneSplit;
        return;
    }

    if (lane != Lane::None && laneArea().contains (e.getPosition()))
    {
        drag = Drag::Lane;
        setLaneValue (e.position);
        return;
    }
    if (! noteArea().contains (e.getPosition()))
        return;

    bool edge = false;
    const int n = noteAt (e.position, edge);
    const bool multi = e.mods.isCommandDown() || e.mods.isCtrlDown();

    if (e.mods.isPopupMenu())
    {
        if (n >= 0)
        {
            if (! selected[(size_t) n])
            {
                selected.reset();
                selected.set ((size_t) n);
            }
            showNoteMenu (n);
        }
        return;
    }

    if (n >= 0 && e.mods.isAltDown())
    {
        editStep (n, [] (Step& st) { st.active = false; });
        selected.reset ((size_t) n);
        return;
    }

    if (n >= 0 && multi)
    {
        selected.flip ((size_t) n); // Ctrl + click: add / remove from the selection
        repaint();
        return;
    }

    if (n >= 0)
    {
        if (! selected[(size_t) n])
        {
            selected.reset();
            selected.set ((size_t) n);
        }
        dragOrigin = channelData().steps;
        dragSelection = selected;
        dragStep = n;
        dragStartGate = channelData().steps[(size_t) n].gate;
        lastDStep = lastDNote = 0;
        drag = edge ? Drag::Resize : Drag::Move;
        repaint();
        return;
    }

    if (multi)
    {
        // Ctrl + drag on empty space: rectangle selection
        drag = Drag::Select;
        selectStart = e.position;
        selectRect = { e.position, e.position };
        return;
    }

    selected.reset();
    const int s = (int) std::floor (xToStep (e.position.x));
    if (s < 0 || s >= channelData().length)
    {
        repaint();
        return;
    }

    const int note = yToNote (e.position.y);
    const bool wasActive = channelData().steps[(size_t) s].active;
    editStep (s, [note, wasActive] (Step& st)
    {
        st.note = (int8_t) note;
        if (! wasActive)
        {
            st.velocity = 100;
            st.gate = 0.9f;
            st.chord = 1;
            st.accent = st.slide = false;
        }
        st.active = true;
    });
    selected.set ((size_t) s);
    dragOrigin = channelData().steps;
    dragSelection = selected;
    dragStep = s;
    dragStartGate = channelData().steps[(size_t) s].gate;
    drag = Drag::Create;
}

void PianoRollComponent::mouseDrag (const juce::MouseEvent& e)
{
    switch (drag)
    {
        case Drag::LaneSplit:
        {
            const float frac = ((float) getHeight() - e.position.y) / (float) juce::jmax (1, getHeight());
            proc.ui.laneSize[(size_t) channel] = juce::jlimit (0.1f, 0.75f, frac);
            clampView();
            repaint();
            break;
        }

        case Drag::Lane:
            setLaneValue (e.position);
            break;

        case Drag::Select:
        {
            selectRect = juce::Rectangle<float> (selectStart, e.position);
            selected.reset();
            const auto& ch = channelData();
            for (int s = 0; s < ch.length; ++s)
                if (ch.steps[(size_t) s].active && noteRect (s).intersects (selectRect))
                    selected.set ((size_t) s);
            repaint();
            break;
        }

        case Drag::Resize:
        case Drag::Create:
        {
            if (drag == Drag::Create && e.getDistanceFromDragStartX() < 3)
                break;
            const float newGate = juce::jlimit (0.1f, 4.0f, (float) (xToStep (e.position.x) - dragStep));
            resizeSelection (newGate - dragStartGate);
            break;
        }

        case Drag::Move:
        {
            const int dStep = juce::roundToInt (e.getDistanceFromDragStartX() / pxPerStep);
            const int dNote = juce::roundToInt (-e.getDistanceFromDragStartY() / rowHeight);
            if (dStep != lastDStep || dNote != lastDNote)
            {
                lastDStep = dStep;
                lastDNote = dNote;
                moveSelection (dStep, dNote);
            }
            break;
        }

        case Drag::None:
            break;
    }
}

void PianoRollComponent::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::None;
    dragStep = -1;
    proc.endGesture();
    repaint();
}

void PianoRollComponent::mouseMove (const juce::MouseEvent& e)
{
    if (isOnLaneBorder (e.getPosition()))
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        return;
    }
    bool edge = false;
    const int n = noteArea().contains (e.getPosition()) ? noteAt (e.position, edge) : -1;
    setMouseCursor (n < 0 ? juce::MouseCursor::NormalCursor
                          : (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor));
}

void PianoRollComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (isOnLaneBorder (e.getPosition()))
    {
        proc.ui.laneSize[(size_t) channel] = 0.28f; // default lane height
        clampView();
        repaint();
        return;
    }
    bool edge = false;
    const int n = noteAt (e.position, edge);
    if (n >= 0)
    {
        editStep (n, [] (Step& st) { st.active = false; });
        selected.reset ((size_t) n);
    }
}

void PianoRollComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // some systems turn Shift + wheel into a horizontal wheel: accept either axis
    const float d = std::abs (w.deltaY) >= std::abs (w.deltaX) ? w.deltaY : w.deltaX;
    if (std::abs (d) < 1.0e-5f)
        return;
    const float factor = d > 0 ? 1.15f : 1.0f / 1.15f;

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double anchor = xToStep (e.position.x);
        pxPerStep *= factor;
        clampView();
        scrollStep = anchor - (e.position.x - (float) noteArea().getX()) / pxPerStep;
    }
    else if (e.mods.isShiftDown())
    {
        const float anchorRow = (e.position.y - (float) noteArea().getY()) / rowHeight;
        const float anchorNote = topNote - anchorRow;
        rowHeight *= factor;
        topNote = anchorNote + (e.position.y - (float) noteArea().getY()) / rowHeight;
    }
    else if (std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        scrollStep -= w.deltaX * 16.0;
    }
    else
    {
        topNote += w.deltaY * 24.0f;
    }

    clampView();
    repaint();
}

void PianoRollComponent::showNoteMenu (int step)
{
    const auto st = channelData().steps[(size_t) step];
    const int count = (int) selected.count();
    const juce::String suffix = count > 1 ? " (" + juce::String (count) + " notes)" : juce::String();

    juce::PopupMenu m, chord;
    m.addItem (1, "Accent" + suffix, true, st.accent);
    m.addItem (2, "Slide / Glide" + suffix, true, st.slide);
    for (int i = 1; i <= 4; ++i)
        chord.addItem (10 + i, i == 1 ? juce::String ("Single note") : juce::String (i) + " notes (stacked thirds)", true, st.chord == i);
    m.addSubMenu ("Chord / Stab" + suffix, chord);
    m.addItem (3, "Octave up" + suffix);
    m.addItem (4, "Octave down" + suffix);
    m.addSeparator();
    m.addItem (6, "Select all");
    m.addItem (5, "Delete" + suffix);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, st] (int r)
    {
        if (r == 0)
            return;
        if (r == 6)
        {
            selected.reset();
            for (int s = 0; s < channelData().length; ++s)
                if (channelData().steps[(size_t) s].active)
                    selected.set ((size_t) s);
            repaint();
            return;
        }
        if (r == 5)
        {
            deleteSelection();
            return;
        }

        auto target = channelData().steps;
        for (int s = 0; s < kMaxSteps; ++s)
        {
            if (! selected[(size_t) s])
                continue;
            auto& x = target[(size_t) s];
            switch (r)
            {
                case 1: x.accent = ! st.accent; if (x.accent) x.velocity = (uint8_t) juce::jmax<int> (x.velocity, 120); break;
                case 2: x.slide = ! st.slide; break;
                case 3: x.note = (int8_t) juce::jmin (120, x.note + 12); break;
                case 4: x.note = (int8_t) juce::jmax (12, x.note - 12); break;
                default:
                    if (r > 10 && r <= 14)
                        x.chord = (uint8_t) (r - 10);
                    break;
            }
        }
        proc.beginGesture();
        applySteps (target);
        proc.endGesture();
    });
}
} // namespace pam
