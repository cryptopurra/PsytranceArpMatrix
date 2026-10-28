#include "ArpEngine.h"

#include <algorithm>
#include <cmath>

namespace pam
{
void ArpEngine::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    outBuffer.ensureSize (4096);
    reset();
}

void ArpEngine::reset()
{
    for (auto& k : keySets)
        k.num = 0;
    physicallyDown = 0;
    numSounding = 0;
    internalPpq = 0.0;
    lastPpqEnd = -1.0;
    lastTriggeredStep.fill (-1);
    running = false;
    arpCounter.fill (0);
    lastGlideSent.fill (-2);
    for (auto& s : playingStep)
        s.store (-1);
}

//==============================================================================
void ArpEngine::handleInput (const juce::MidiBuffer& in, const ArpParams& p)
{
    auto addKey = [this] (KeySet& ks, int note, int velocity)
    {
        for (int i = 0; i < ks.num; ++i)
            if (ks.held[(size_t) i].note == note)
            {
                ks.held[(size_t) i].velocity = velocity;
                return;
            }
        if (ks.num < (int) ks.held.size())
            ks.held[(size_t) ks.num++] = { note, velocity, orderCounter };
    };

    auto& live  = keySets[(size_t) HoldMode::Off];
    auto& latch = keySets[(size_t) HoldMode::Latch];
    auto& clean = keySets[(size_t) HoldMode::CleanHold];

    for (const auto meta : in)
    {
        const auto m = meta.getMessage();

        if (m.isNoteOn())
        {
            if (physicallyDown == 0)
                clean.num = 0; // Clean Hold: a new chord replaces the latched one

            ++physicallyDown;
            lastKeyNote.store (m.getNoteNumber());
            for (auto* ks : { &live, &latch, &clean })
                addKey (*ks, m.getNoteNumber(), m.getVelocity());
            ++orderCounter;
        }
        else if (m.isNoteOff())
        {
            physicallyDown = std::max (0, physicallyDown - 1);
            for (int i = 0; i < live.num; ++i)
                if (live.held[(size_t) i].note == m.getNoteNumber())
                {
                    live.held[(size_t) i] = live.held[(size_t) --live.num];
                    break;
                }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            for (auto& k : keySets)
                k.num = 0;
            physicallyDown = 0;
        }
    }

    if (physicallyDown == 0)
        live.num = 0;
    const int n = maxHeld (p);
    heldCount.store (n);
    if (n == 0 || ! p.keyFollow)
        transposeNow.store (0);
}

int ArpEngine::maxHeld (const ArpParams& p) const noexcept
{
    // keys that matter right now: the largest key set used by any channel's HOLD mode
    int n = 0;
    for (int c = 0; c < kNumChannels; ++c)
        n = std::max (n, keySets[(size_t) juce::jlimit (0, 2, (int) p.holdMode[(size_t) c])].num);
    return n;
}

//==============================================================================
void ArpEngine::noteOn (juce::MidiBuffer& out, int channel, int note, int velocity, int offset, int lengthSamples,
                        int source, bool slide)
{
    note = juce::jlimit (0, 127, note);
    channel = juce::jlimit (1, 16, channel);

    offset = std::max (offset, minNoteOnOffset);

    // retrigger: close an already sounding identical note first, one sample earlier, so the
    // note-off can never be ordered after the new note-on by the host or the synth
    for (int i = 0; i < numSounding; ++i)
        if (sounding[(size_t) i].channel == channel && sounding[(size_t) i].note == note)
        {
            if (offset == 0)
                offset = 1; // blocks always have more than one sample in practice
            out.addEvent (juce::MidiMessage::noteOff (channel, note), offset - 1);
            sounding[(size_t) i] = sounding[(size_t) --numSounding];
            break;
        }

    if (numSounding >= (int) sounding.size())
        return;

    out.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)), offset);
    sounding[(size_t) numSounding++] = { channel, note, offset + std::max (1, lengthSamples), source, slide };
}

void ArpEngine::flushNoteOffs (juce::MidiBuffer& out, int numSamples)
{
    for (int i = 0; i < numSounding;)
    {
        auto& s = sounding[(size_t) i];
        if (s.samplesLeft < numSamples)
        {
            out.addEvent (juce::MidiMessage::noteOff (s.channel, s.note), std::max (0, s.samplesLeft));
            s = sounding[(size_t) --numSounding];
        }
        else
        {
            s.samplesLeft -= numSamples;
            ++i;
        }
    }
}

void ArpEngine::allSoundOff (juce::MidiBuffer& out, const ArpParams& p, int offset)
{
    // "All Notes Off" on every channel we can output on: clears notes a synth may still hold
    if (p.mergeToChannel1)
        out.addEvent (juce::MidiMessage::allNotesOff (1), offset);
    else
        for (int ch = 1; ch <= kNumChannels; ++ch)
            out.addEvent (juce::MidiMessage::allNotesOff (ch), offset);
}

void ArpEngine::stopAll (juce::MidiBuffer& out, const ArpParams& p)
{
    lastGlideSent.fill (-2); // resend the glide controllers on the next start
    allNotesOff (out, 0);
    allSoundOff (out, p, 0);
    running = false;
    lastTriggeredStep.fill (-1);
    lastPpqEnd = -1.0;
    arpCounter.fill (0);
    for (auto& s : playingStep)
        s.store (-1);
}

void ArpEngine::sendGlideControllers (juce::MidiBuffer& out, const Pattern& pattern, const ArpParams& p)
{
    for (int c = 0; c < kNumChannels; ++c)
    {
        const int g = p.glide[(size_t) c];
        if (g == lastGlideSent[(size_t) c] || ! pattern.channels[(size_t) c].enabled)
            continue;
        const int previous = lastGlideSent[(size_t) c];
        lastGlideSent[(size_t) c] = g;
        const int ch = p.mergeToChannel1 ? 1 : pattern.channels[(size_t) c].midiChannel;
        if (g < 0)
        {
            if (previous > 0) // glide switched off: turn the synth's portamento off once
                out.addEvent (juce::MidiMessage::controllerEvent (ch, 65, 0), 0);
            continue;
        }
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 5, juce::jlimit (0, 127, g)), 0);   // portamento time
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 65, g > 0 ? 127 : 0), 0);          // portamento on/off
    }
}

void ArpEngine::allNotesOff (juce::MidiBuffer& out, int offset)
{
    for (int i = 0; i < numSounding; ++i)
        out.addEvent (juce::MidiMessage::noteOff (sounding[(size_t) i].channel, sounding[(size_t) i].note), offset);
    numSounding = 0;
}

//==============================================================================
int ArpEngine::arpNote (int c, const KeySet& keys, const Step& st, const Pattern& pattern, const ArpParams& p, int& keyVelocity)
{
    const int numHeld = keys.num;
    const auto mode = p.mode[(size_t) c];
    std::array<Held, 32> sorted {};
    std::copy (keys.held.begin(), keys.held.begin() + numHeld, sorted.begin());
    const auto end = sorted.begin() + numHeld;

    if (mode == ArpMode::AsPlayed)
        std::sort (sorted.begin(), end, [] (const Held& a, const Held& b) { return a.order < b.order; });
    else
        std::sort (sorted.begin(), end, [] (const Held& a, const Held& b) { return a.note < b.note; });

    const int range = juce::jlimit (1, 4, p.octaveRange);
    const int n = numHeld * range;
    auto seqAt = [&] (int idx) -> Held
    {
        Held h = sorted[(size_t) (idx % numHeld)];
        h.note += 12 * (idx / numHeld);
        return h;
    };

    const int i = arpCounter[(size_t) c]++;
    int idx = 0, extraOct = 0;

    switch (mode)
    {
        case ArpMode::Up:
        case ArpMode::AsPlayed:  idx = i % n; break;
        case ArpMode::Down:      idx = n - 1 - (i % n); break;
        case ArpMode::UpDown:
        {
            const int period = std::max (1, 2 * n - 2);
            const int j = i % period;
            idx = n > 1 ? (j < n ? j : period - j) : 0;
            break;
        }
        case ArpMode::Random:    idx = rng.nextInt (n); break;
        case ArpMode::Melody:
        {
            // the generated pattern's contour drives which held note plays
            const auto& scale = ScaleLibrary::get()[pattern.scaleIndex];
            const int deg = ScaleLibrary::noteToDegree (st.note, pattern.rootNote, scale, 48);
            const int m = ((deg % n) + n) % n;
            extraOct = (deg - m) / n;
            idx = m;
            break;
        }
        case ArpMode::EpicMotif:
        {
            static constexpr int motif[] = { 0, 2, 1, 3, 0, 3, 2, 4, 0, 2, 1, 3, 2, 4, 3, 5 };
            const int v = motif[i % 16];
            idx = v % n;
            extraOct = v / n;
            break;
        }
        case ArpMode::Chord:
        case ArpMode::Step:
        case ArpMode::numModes:  idx = i % n; break;
    }

    const Held h = seqAt (idx);
    keyVelocity = h.velocity;
    return h.note + 12 * juce::jlimit (-2, 2, extraOct);
}

void ArpEngine::triggerChannel (int c, juce::int64 k, int offset, double samplesPerStep,
                                const Pattern& pattern, const ArpParams& p, juce::MidiBuffer& out)
{
    const auto& scale = ScaleLibrary::get()[pattern.scaleIndex];
    const auto& keySet = keySets[(size_t) juce::jlimit (0, 2, (int) p.holdMode[(size_t) c])];
    const int numHeld = keySet.num;
    const bool keys = numHeld > 0;
    const auto mode = p.mode[(size_t) c];

    int lowestHeld = 128, lastVel = 100, newest = -1;
    for (int i = 0; i < numHeld; ++i)
    {
        lowestHeld = std::min (lowestHeld, keySet.held[(size_t) i].note);
        if (keySet.held[(size_t) i].order > newest)
        {
            newest = keySet.held[(size_t) i].order;
            lastVel = keySet.held[(size_t) i].velocity;
        }
    }

    {
        const auto& ch = pattern.channels[(size_t) c];
        if (! ch.enabled)
        {
            playingStep[(size_t) c].store (-1);
            return;
        }

        const int len = juce::jlimit (1, kMaxSteps, ch.length);
        const int s = (int) (k % len);
        playingStep[(size_t) c].store (s);

        const auto& st = ch.steps[(size_t) s];
        if (! st.active || ch.barMuted[(size_t) (s / 16)] || p.channelMuted[(size_t) c] || (! keys && ! p.playWithoutKeys))
            return;

        int keyVel = keys ? lastVel : st.velocity;
        std::array<int, 8> notes {};
        int numNotes = 0;
        int transpose = 0;

        if (keys && mode == ArpMode::Chord)
        {
            for (int i = 0; i < numHeld && numNotes < 8; ++i)
                notes[(size_t) numNotes++] = keySet.held[(size_t) i].note;
        }
        else if (keys && mode != ArpMode::Step)
        {
            notes[(size_t) numNotes++] = arpNote (c, keySet, st, pattern, p, keyVel);
        }
        else
        {
            if (keys && p.keyFollow)
            {
                transpose = ((lowestHeld - pattern.rootNote) % 12 + 12) % 12;
                if (transpose > 6)
                    transpose -= 12;
            }

            if (st.chord > 1)
            {
                // stacked scale thirds above the step note
                const int deg = ScaleLibrary::noteToDegree (st.note, pattern.rootNote, scale, 48);
                for (int v = 0; v < std::min<int> (st.chord, 4); ++v)
                    notes[(size_t) numNotes++] = ScaleLibrary::degreeToNote (deg + 2 * v, pattern.rootNote, scale, 48) + transpose;
            }
            else
            {
                notes[(size_t) numNotes++] = st.note + transpose;
            }
        }

        if (c == 0 || transpose != 0)
            transposeNow.store (transpose);
        const int root = (pattern.rootNote + transpose + 12) % 12;

        int velocity = st.velocity;
        switch (p.velocityMode[(size_t) c])
        {
            case VelocityMode::Key:      velocity = keyVel + (st.accent ? 15 : 0); break;
            case VelocityMode::Hold:     velocity = p.holdVelocity + (st.accent ? 15 : 0); break;
            case VelocityMode::Step:     velocity = st.velocity; break;
            case VelocityMode::StepKey:  velocity = st.velocity * keyVel / 127; break;
            case VelocityMode::StepHold: velocity = st.velocity * p.holdVelocity / 127; break;
            case VelocityMode::numModes: break;
        }

        const auto voice = p.voiceMode[(size_t) c];
        if (voice != VoiceMode::Poly)
            numNotes = std::min (numNotes, 1); // mono / legato: one note at a time

        float gate = st.gate * p.gateScale;
        if (st.slide)
            gate = std::max (gate, 1.08f); // overlap the next note -> legato / glide on mono synths
        if (voice == VoiceMode::Legato)
        {
            // tie every note to the next active step (continuous legato / glide)
            int dist = 1;
            while (dist < len && ! ch.steps[(size_t) ((s + dist) % len)].active)
                ++dist;
            gate = std::max (gate, (float) dist + 0.06f);
        }
        const int lengthSamples = std::max (16, (int) (gate * samplesPerStep));

        const int outChannel = p.mergeToChannel1 ? 1 : ch.midiChannel;

        if (voice == VoiceMode::Mono)
        {
            // classic mono: the new note cuts the previous one, unless that one was a slide
            int cutAt = std::max (0, std::max (offset, minNoteOnOffset) - 1);
            for (int i = 0; i < numSounding;)
            {
                auto& sn = sounding[(size_t) i];
                if (sn.source == c && ! sn.slide)
                {
                    if (offset == 0 && minNoteOnOffset == 0)
                        offset = 1, cutAt = 0;
                    out.addEvent (juce::MidiMessage::noteOff (sn.channel, sn.note), cutAt);
                    sn = sounding[(size_t) --numSounding];
                }
                else
                    ++i;
            }
        }

        for (int i = 0; i < numNotes; ++i)
        {
            int note = notes[(size_t) i] + 12 * p.octave + p.channelTranspose[(size_t) c];
            if (p.lockToScale)
                note = ScaleLibrary::quantize (note, root, scale);
            noteOn (out, outChannel, note, velocity, offset, lengthSamples, c, st.slide || voice == VoiceMode::Legato);
        }
    }
}

//==============================================================================
void ArpEngine::process (juce::MidiBuffer& midi, int numSamples, const Position& pos,
                         const Pattern& pattern, const ArpParams& p)
{
    handleInput (midi, p);

    auto& out = outBuffer;
    out.clear();
    // pass through everything that is not a note (CC, pitch bend, ...)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOnOrOff())
            out.addEvent (m, meta.samplePosition);
    }

    const bool host = pos.hostPlaying && pos.hasHostPosition;
    // Inside a DAW the transport is the only clock: when it stops we stop, even with latched keys.
    // The free-running internal clock is only for the Standalone app / hosts without a playhead.
    const bool internal = ! host && maxHeld (p) > 0 && pos.allowInternalClock;

    if (! host && ! internal)
    {
        if (running)
        {
            allNotesOff (out, 0);
            allSoundOff (out, p, 0);
            lastGlideSent.fill (-2);
            running = false;
            lastTriggeredStep.fill (-1);
            lastPpqEnd = -1.0;
            arpCounter.fill (0);
            for (auto& s : playingStep)
                s.store (-1);
        }
        flushNoteOffs (out, numSamples);
        midi.swapWith (out);
        return;
    }

    if ((internal && ! running) || (running && host != lastRunWasHost))
    {
        // (re)start or clock source changed: start a fresh step count
        if (internal)
            internalPpq = 0.0;
        lastTriggeredStep.fill (-1);
        lastPpqEnd = -1.0;
        arpCounter.fill (0);
    }
    lastRunWasHost = host;

    running = true;
    const double bpm = (pos.hasHostPosition && pos.bpm > 0.0) ? pos.bpm : (double) p.internalBpm;
    const double ppqPerSample = bpm / 60.0 / sampleRate;
    const double rawStart = host ? pos.ppq : internalPpq;
    // every channel has its own step length; jump / cycle detection uses the shortest one
    std::array<double, kNumChannels> stepLens {};
    double stepLen = 4.0;
    for (int c = 0; c < kNumChannels; ++c)
    {
        stepLens[(size_t) c] = stepLengthInQuarters (p.division[(size_t) c]);
        stepLen = std::min (stepLen, stepLens[(size_t) c]);
    }

    // loop / locate detection. Hosts (Cubase with latency compensation, for example) report
    // block positions with small jitter, so only a real jump counts: going back by more than half
    // a step (cycle / locate) or forward by more than two steps.
    const bool jumped = lastPpqEnd >= 0.0
        && (rawStart < lastPpqEnd - stepLen * 0.5 || rawStart > lastPpqEnd + stepLen * 2.0);

    // Continuous clock: during normal playback the block starts where the previous one ended and
    // only follows the host's reported position slowly. Placing notes from the jittery reported
    // position made a rolling bass drift a few ms back and forth against the kick.
    // The correction is a running mean (fast right after start / locate, then very slow), so the
    // clock settles on the host's average position and its block-to-block jitter is filtered out.
    double ppqStart = rawStart;
    if (host && ! jumped && lastPpqEnd >= 0.0)
    {
        const double drift = rawStart - lastPpqEnd;
        if (std::abs (drift) < stepLen * 0.25)
        {
            const double gain = std::max (0.004, 1.0 / (double) (smoothBlocks + 2));
            ppqStart = lastPpqEnd + drift * gain;
            ++smoothBlocks;
        }
        else
        {
            smoothBlocks = 0;
        }
    }
    else
    {
        smoothBlocks = 0;
    }
    const double ppqEnd = ppqStart + numSamples * ppqPerSample;

    sendGlideControllers (out, pattern, p);

    const bool cycle = host && pos.isLooping && pos.loopEnd > pos.loopStart + stepLen;
    minNoteOnOffset = 0;

    // A cycle wrap (end marker -> start marker) is musically continuous: the notes that are
    // sounding finish with their own gate exactly as they would inside the loop. Cutting them
    // here (and re-striking at the same sample) is what made synths like Serum misbehave
    // after every cycle restart.
    const bool cycleWrap = jumped && cycle
        && std::abs (ppqStart - pos.loopStart) < stepLen
        && std::abs (lastPpqEnd - pos.loopEnd) < stepLen;

    if (jumped)
    {
        if (! cycleWrap)
        {
            allNotesOff (out, 0);
            minNoteOnOffset = numSamples > 1 ? 1 : 0; // never note-off and note-on on the same sample
        }
        lastTriggeredStep.fill (-1);
        arpCounter.fill (0);
    }

    // continuous playback: scan from where the previous block ended, so a small forward jitter
    // cannot skip a step (a small backward jitter is covered by lastTriggeredStep)
    const double scanStart = (! jumped && lastPpqEnd >= 0.0) ? juce::jmin (ppqStart, lastPpqEnd) : ppqStart;

    auto scan = [&] (double from, double to, double ppqAtSampleZero, int maxOffset = 1 << 30)
    {
        for (int c = 0; c < kNumChannels; ++c)
        {
            if (! pattern.channels[(size_t) c].enabled)
            {
                playingStep[(size_t) c].store (-1);
                continue;
            }
            const double len = stepLens[(size_t) c];
            const double swingOffset = juce::jlimit (0.0f, 1.0f, p.swing) * len / 3.0;
            const double samplesPerStep = len / ppqPerSample;
            const auto firstStep = (juce::int64) std::floor (from / len) - 1;
            const auto lastStep  = (juce::int64) std::floor (to / len);

            for (auto k = std::max<juce::int64> (0, firstStep); k <= lastStep; ++k)
            {
                const double t = (double) k * len + ((k % 2) == 1 ? swingOffset : 0.0);
                if (t < from || t >= to || k <= lastTriggeredStep[(size_t) c])
                    continue;

                const int offset = juce::jlimit (0, std::min (numSamples - 1, maxOffset), (int) ((t - ppqAtSampleZero) / ppqPerSample));
                triggerChannel (c, k, offset, samplesPerStep, pattern, p, out);
                lastTriggeredStep[(size_t) c] = k;
            }
        }
    };

    double endOfBlock = ppqEnd;
    if (cycle && ppqStart < pos.loopEnd && ppqEnd > pos.loopEnd)
    {
        // the block crosses the end marker (hosts that do not split blocks at the cycle):
        // play up to the end marker, then continue from the start marker in the same block
        scan (scanStart, pos.loopEnd, ppqStart);
        lastTriggeredStep.fill (-1);
        const double over = ppqEnd - pos.loopEnd;
        // LOOP FIX: events right after the wrap are placed on the last sample before it, so no
        // event of this block carries a song position beyond the end marker (1 sample early at most)
        const int wrapOffset = (int) ((pos.loopEnd - ppqStart) / ppqPerSample);
        const int maxOffset = p.loopDefer ? std::max (0, wrapOffset - 1) : (1 << 30);
        scan (pos.loopStart, pos.loopStart + over, pos.loopStart - (pos.loopEnd - ppqStart), maxOffset);
        endOfBlock = pos.loopStart + over;
    }
    else
    {
        scan (scanStart, ppqEnd, ppqStart);
    }

    flushNoteOffs (out, numSamples);
    lastPpqEnd = endOfBlock;
    if (internal)
        internalPpq = ppqEnd;

    midi.swapWith (out);
}
} // namespace pam
