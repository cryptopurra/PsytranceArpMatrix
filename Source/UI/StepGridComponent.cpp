#include "StepGridComponent.h"

namespace pam
{
StepGridComponent::StepGridComponent (PluginProcessor& p, int c) : proc (p), channel (c)
{
    setRepaintsOnMouseActivity (false);
}

juce::Rectangle<float> StepGridComponent::cellBounds (int cell) const noexcept
{
    const float w = (float) getWidth() / 16.0f;
    return { cell * w + 1.5f, 1.0f, w - 3.0f, (float) getHeight() - 2.0f };
}

int StepGridComponent::stepAt (juce::Point<float> pos) const noexcept
{
    const int cell = juce::jlimit (0, 15, (int) (pos.x / ((float) getWidth() / 16.0f)));
    return page * 16 + cell;
}

void StepGridComponent::paint (juce::Graphics& g)
{
    const auto& ch = proc.getMatrix().getPattern().channels[(size_t) channel];
    const int playing = proc.currentStep (channel);
    const bool enabled = ch.enabled;

    for (int cell = 0; cell < 16; ++cell)
    {
        const int s = page * 16 + cell;
        auto r = cellBounds (cell);
        const bool inLength = s < ch.length;
        const auto& st = ch.steps[(size_t) s];

        // background: beats are slightly brighter
        auto bg = juce::Colour (0xff0d1622).brighter (cell % 4 == 0 ? 0.12f : 0.0f);
        if (! inLength)
            bg = bg.darker (0.6f);
        g.setColour (bg);
        g.fillRoundedRectangle (r, 3.0f);

        if (inLength && st.active)
        {
            const float level = (float) st.velocity / 127.0f;
            auto bar = r.withTrimmedTop (r.getHeight() * (1.0f - level));
            auto c = enabled ? colour : colour.withSaturation (0.1f);
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.95f), bar.getCentreX(), bar.getY(),
                                                     c.darker (0.8f).withAlpha (0.85f), bar.getCentreX(), r.getBottom(), false));
            g.fillRoundedRectangle (bar, 3.0f);

            g.setColour (juce::Colours::white.withAlpha (enabled ? 0.95f : 0.5f));
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            const auto name = ScaleLibrary::noteName (st.note) + juce::String (st.note / 12 - 1);
            g.drawFittedText (name, r.toNearestInt().withHeight (16).translated (0, 2), juce::Justification::centred, 1);

            juce::String flagText;
            if (st.accent) flagText << "A";
            if (st.slide)  flagText << "~";
            if (st.chord > 1) flagText << "x" << (int) st.chord;
            if (flagText.isNotEmpty())
            {
                g.setFont (juce::FontOptions (10.0f));
                g.drawFittedText (flagText, r.toNearestInt().removeFromBottom (14), juce::Justification::centred, 1);
            }
        }

        if (s == playing && enabled)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 3.0f, 1.6f);
        }
    }
}

void StepGridComponent::modifyStep (int step, const std::function<void (Step&)>& fn)
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

void StepGridComponent::mouseDown (const juce::MouseEvent& e)
{
    const int s = stepAt (e.position);
    if (s >= proc.getMatrix().getPattern().channels[(size_t) channel].length)
        return;

    if (e.mods.isPopupMenu())
    {
        showMenu (s);
        return;
    }

    if (e.mods.isAltDown())
    {
        modifyStep (s, [] (Step& st) { st.accent = ! st.accent; st.active = true; });
        return;
    }

    const auto& st = proc.getMatrix().getPattern().channels[(size_t) channel].steps[(size_t) s];
    proc.beginGesture();
    dragStep = s;
    dragStartNote = st.note;
    dragStartVel = st.velocity;
    dragged = false;
}

void StepGridComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0)
        return;

    const int hover = juce::jlimit (0, proc.getMatrix().getPattern().channels[(size_t) channel].length - 1,
                                    stepAt (e.position));
    if (! dragged && std::abs (e.getDistanceFromDragStartY()) < 2 && hover == dragStep)
        return;

    dragged = true;
    const int dy = -e.getDistanceFromDragStartY();
    const int from = juce::jmin (dragStep, hover), to = juce::jmax (dragStep, hover);
    const int c = channel, origin = dragStep;

    if (e.mods.isShiftDown())
    {
        // velocity: the same value is painted on every step the mouse passes over
        const int vel = juce::jlimit (1, 127, dragStartVel + dy);
        proc.editMatrix ([=] (ChannelMatrix& m)
        {
            for (int s = from; s <= to; ++s)
            {
                auto st = m.getPattern().channels[(size_t) c].steps[(size_t) s];
                if (! st.active && s != origin) continue;
                st.velocity = (uint8_t) vel;
                st.active = true;
                m.setStep (c, s, st);
            }
        });
    }
    else
    {
        // note: drag up/down sets the pitch, moving left/right applies it to the neighbouring steps
        const auto& p = proc.getMatrix().getPattern();
        const auto& scale = ScaleLibrary::get()[p.scaleIndex];
        const int base = 12 * (dragStartNote / 12);
        const int deg = ScaleLibrary::noteToDegree (dragStartNote, p.rootNote, scale, base) + dy / 8;
        const int note = juce::jlimit (12, 120, ScaleLibrary::degreeToNote (deg, p.rootNote, scale, base));
        proc.editMatrix ([=] (ChannelMatrix& m)
        {
            for (int s = from; s <= to; ++s)
            {
                auto st = m.getPattern().channels[(size_t) c].steps[(size_t) s];
                if (! st.active && s != origin) continue;
                st.note = (int8_t) note;
                st.active = true;
                m.setStep (c, s, st);
            }
        });
    }
    repaint();
}

void StepGridComponent::mouseUp (const juce::MouseEvent&)
{
    if (dragStep >= 0 && ! dragged)
        modifyStep (dragStep, [] (Step& st) { st.active = ! st.active; });
    dragStep = -1;
    proc.endGesture();
}

void StepGridComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int s = stepAt (e.position);
    const auto& ch = proc.getMatrix().getPattern().channels[(size_t) channel];
    if (s >= ch.length || ! ch.steps[(size_t) s].active || std::abs (w.deltaY) < 1.0e-4f)
        return;

    const int dir = w.deltaY > 0 ? 1 : -1;
    const auto& st = ch.steps[(size_t) s];

    if (e.mods.isShiftDown())
    {
        const int vel = juce::jlimit (1, 127, (int) st.velocity + dir * 5);
        modifyStep (s, [vel] (Step& x) { x.velocity = (uint8_t) vel; });
        return;
    }

    const auto& p = proc.getMatrix().getPattern();
    const auto& scale = ScaleLibrary::get()[p.scaleIndex];
    const int base = 12 * (st.note / 12);
    const int note = juce::jlimit (12, 120, ScaleLibrary::degreeToNote (
                         ScaleLibrary::noteToDegree (st.note, p.rootNote, scale, base) + dir, p.rootNote, scale, base));
    modifyStep (s, [note] (Step& x) { x.note = (int8_t) note; });
}

void StepGridComponent::showMenu (int s)
{
    const auto st = proc.getMatrix().getPattern().channels[(size_t) channel].steps[(size_t) s];

    juce::PopupMenu m, chord, gate;
    m.addItem (1, "Step on", true, st.active);
    m.addItem (2, "Accent", true, st.accent);
    m.addItem (3, "Slide / Glide", true, st.slide);
    for (int i = 1; i <= 4; ++i)
        chord.addItem (10 + i, i == 1 ? juce::String ("Single note") : juce::String (i) + " notes (stacked thirds)", true, st.chord == i);
    m.addSubMenu ("Chord / Stab", chord);
    const float gates[] = { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f };
    for (int i = 0; i < 8; ++i)
        gate.addItem (20 + i, juce::String (juce::roundToInt (gates[i] * 100.0f)) + "% of step", true,
                      std::abs (st.gate - gates[i]) < 0.01f);
    m.addSubMenu ("Gate", gate);
    m.addItem (4, "Octave up");
    m.addItem (5, "Octave down");
    m.addSeparator();
    m.addItem (6, "Clear channel");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [this, s, gates] (int r) mutable
    {
        switch (r)
        {
            case 0: return;
            case 1: modifyStep (s, [] (Step& x) { x.active = ! x.active; }); break;
            case 2: modifyStep (s, [] (Step& x) { x.accent = ! x.accent; if (x.accent) x.velocity = (uint8_t) juce::jmax<int> (x.velocity, 120); }); break;
            case 3: modifyStep (s, [] (Step& x) { x.slide = ! x.slide; }); break;
            case 4: modifyStep (s, [] (Step& x) { x.note = (int8_t) juce::jmin (120, x.note + 12); }); break;
            case 5: modifyStep (s, [] (Step& x) { x.note = (int8_t) juce::jmax (12, x.note - 12); }); break;
            case 6: proc.editMatrix ([c = channel] (ChannelMatrix& mm) { mm.clearChannel (c); }); break;
            default:
                if (r > 10 && r <= 14)
                    modifyStep (s, [r] (Step& x) { x.chord = (uint8_t) (r - 10); x.active = true; });
                else if (r >= 20 && r < 28)
                {
                    const float g = gates[r - 20];
                    modifyStep (s, [g] (Step& x) { x.gate = g; });
                }
                break;
        }
        repaint();
    });
}
} // namespace pam
