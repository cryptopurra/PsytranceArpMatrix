#include "MonitorProcessor.h"

juce::File MonitorProcessor::logFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("PsytranceArpMatrix").getChildFile ("monitor_log.csv");
}

MonitorProcessor::MonitorProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter (record = new juce::AudioParameterBool ({ "record", 1 }, "Record", false));
    startTimerHz (4);
}

MonitorProcessor::~MonitorProcessor()
{
    stopTimer();
    drain();
}

void MonitorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    if (! record->get() || ! streamOpen.load())
        return;

    juce::AudioPlayHead::PositionInfo info;
    if (auto* ph = getPlayHead())
        if (auto p = ph->getPosition())
            info = *p;

    Row base;
    base.block = blockCounter++;
    base.numSamples = buffer.getNumSamples();
    base.ppq = info.getPpqPosition().orFallback (-1.0);
    base.bpm = info.getBpm().orFallback (0.0);
    base.timeSamples = info.getTimeInSamples().orFallback (-1);
    base.hostNs = (juce::int64) info.getHostTimeNs().orFallback (0);
    base.playing = info.getIsPlaying();
    base.looping = info.getIsLooping();
    if (auto loop = info.getLoopPoints())
    {
        base.loopStart = loop->ppqStart;
        base.loopEnd = loop->ppqEnd;
    }

    // one row for the block itself (kind 0) and one per note event
    auto push = [&] (Row r)
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)      rows[(size_t) scope.startIndex1] = r;
        else if (scope.blockSize2 > 0) rows[(size_t) scope.startIndex2] = r;
    };
    push (base);
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOnOrOff())
            continue;
        auto r = base;
        r.offset = meta.samplePosition;
        r.kind = m.isNoteOn() ? 1 : 2;
        r.channel = m.getChannel();
        r.note = m.getNoteNumber();
        r.velocity = m.getVelocity();
        push (r);
    }
}

void MonitorProcessor::timerCallback()
{
    const bool rec = record->get();
    if (rec && ! wasRecording)
    {
        auto f = logFile();
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        stream = f.createOutputStream();
        if (stream != nullptr)
        {
            fifo.reset();
            blockCounter = 0;
            stream->writeText ("# PAM MIDI Monitor sampleRate " + juce::String (getSampleRate(), 1)
                               + " blockSize " + juce::String (getBlockSize()) + "\n"
                               "block,numSamples,kind,playing,looping,ppq,bpm,loopStart,loopEnd,timeSamples,hostNs,offset,channel,note,velocity\n",
                               false, false, nullptr);
            streamOpen.store (true);
        }
    }
    else if (! rec && wasRecording)
    {
        streamOpen.store (false);
        drain();
        stream.reset();
    }
    wasRecording = rec;
    drain();
}

void MonitorProcessor::drain()
{
    if (stream == nullptr)
        return;
    const auto scope = fifo.read (fifo.getNumReady());
    juce::String text;
    auto add = [&] (int start, int size)
    {
        for (int i = start; i < start + size; ++i)
        {
            const auto& r = rows[(size_t) i];
            text << r.block << ',' << r.numSamples << ',' << r.kind << ',' << (r.playing ? 1 : 0) << ',' << (r.looping ? 1 : 0) << ','
                 << juce::String (r.ppq, 6) << ',' << juce::String (r.bpm, 3) << ',' << juce::String (r.loopStart, 6) << ','
                 << juce::String (r.loopEnd, 6) << ',' << r.timeSamples << ',' << r.hostNs << ',' << r.offset << ','
                 << r.channel << ',' << r.note << ',' << r.velocity << '\n';
        }
    };
    add (scope.startIndex1, scope.blockSize1);
    add (scope.startIndex2, scope.blockSize2);
    if (text.isNotEmpty())
        stream->writeText (text, false, false, nullptr);
    stream->flush();
}

juce::AudioProcessorEditor* MonitorProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonitorProcessor();
}
