#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace swarm;

//==============================================================================
static juce::String formatValue (const ParamDef& d, float v)
{
    const juce::String unit (d.unit);
    if (unit == "Hz")   return v < 1000.f ? juce::String (v, 0) + " Hz" : juce::String (v / 1000.f, 2) + " kHz";
    if (unit == "s")    return v < 1.f ? juce::String (v * 1000.f, 0) + " ms" : juce::String (v, 2) + " s";
    if (unit == "st")   return (v > 0 && d.min < 0 ? "+" : "") + juce::String (v, 1) + " st";
    if (unit == "ct")   return (v > 0 ? "+" : "") + juce::String (v, 0) + " ct";
    if (unit == "dB")   return juce::String (v, 1) + " dB";
    if (unit == "oct")  return juce::String (v, 2) + " oct";
    if (d.min < 0)      return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.f)) + "%";
    return juce::String (juce::roundToInt (v / d.max * 100.f)) + "%";
}

static juce::String hostName (const ParamDef& d)
{
    const juce::String g (d.group);
    if (g == "ENV A" || g == "ENV B" || g == "AMP" || g == "LFO") return g + " " + d.name;
    return d.name;
}

juce::AudioProcessorValueTreeState::ParameterLayout Swarm8Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < NumParams; ++i)
    {
        const ParamDef& d = kParams[i];
        const juce::ParameterID pid { d.id, 1 };
        switch (d.type)
        {
            case PType::Float:
            {
                juce::NormalisableRange<float> r (d.min, d.max);
                if (d.centre > 0.f) r.setSkewForCentre (d.centre);
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, hostName (d), r, d.def,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (d.unit)
                        .withStringFromValueFunction ([i] (float v, int) { return formatValue (kParams[i], v); })));
                break;
            }
            case PType::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, hostName (d), d.def > 0.5f));
                break;
            case PType::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    pid, hostName (d), juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
                break;
            case PType::Int:
            {
                auto attrs = juce::AudioParameterIntAttributes();
                if (juce::String (d.unit) == "note")
                    attrs = attrs.withStringFromValueFunction ([] (int v, int) { return juce::MidiMessage::getMidiNoteName (v, true, true, 3); })
                                 .withValueFromStringFunction ([] (const juce::String& s) { return s.getIntValue(); });
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, hostName (d), (int) d.min, (int) d.max, (int) d.def, attrs));
                break;
            }
        }
    }
    return layout;
}

//==============================================================================
Swarm8Processor::Swarm8Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SWARM8", createLayout())
{
    for (int i = 0; i < NumParams; ++i)
    {
        params[(size_t) i] = apvts.getParameter (kParams[i].id);
        raw[(size_t) i]    = apvts.getRawParameterValue (kParams[i].id);
        ccValue[(size_t) i].store (0.f);
        ccPending[(size_t) i].store (false);
    }
    resetCCMapToDefaults();

    osc.addListener (this);
    setOscPort (9000);
    startTimerHz (30);
}

Swarm8Processor::~Swarm8Processor()
{
    stopTimer();
    osc.removeListener (this);
    osc.disconnect();
}

//==============================================================================
void Swarm8Processor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool Swarm8Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Swarm8Processor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                         engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())                   engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel())                engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())           engine.pressure ((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())                engine.pressure ((float) m.getAfterTouchValue() / 127.0f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc < 0 || cc > 127) return;

        const int learning = learnParam.load();
        if (learning >= 0)
        {
            for (auto& c : ccMap) if (c.load() == learning) c.store (-1);
            ccMap[(size_t) cc].store (learning);
            learnParam.store (-1);
            ++ccVersion;
        }

        const int idx = ccMap[(size_t) cc].load();
        if (idx < 0) return;

        float norm = (float) val / 127.0f;
        if (kParams[idx].type == PType::Bool) norm = val >= 64 ? 1.0f : 0.0f;
        ccValue[(size_t) idx].store (norm);
        ccPending[(size_t) idx].store (true);

        float live[NumParams];
        std::memcpy (live, engine.params(), sizeof (live));
        live[idx] = params[(size_t) idx]->convertFrom0to1 (norm);
        engine.setParams (live);
    }
}

void Swarm8Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    buffer.clear();

    float live[NumParams];
    for (int i = 0; i < NumParams; ++i)
        live[i] = ccPending[(size_t) i].load() ? params[(size_t) i]->convertFrom0to1 (ccValue[(size_t) i].load())
                                               : raw[(size_t) i]->load();
    engine.setParams (live);

    auto render = [&] (int start, int len)
    {
        constexpr int chunk = 256;
        float l[chunk], r[chunk];
        while (len > 0)
        {
            const int n = juce::jmin (len, chunk);
            engine.process (l, r, n);
            if (numCh >= 2)
            {
                buffer.copyFrom (0, start, l, n);
                buffer.copyFrom (1, start, r, n);
            }
            else if (numCh == 1)
            {
                for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + r[i]);
                buffer.copyFrom (0, start, l, n);
            }
            start += n; len -= n;
        }
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        if (t > pos) { render (pos, t - pos); pos = t; }
        handleMidi (meta.getMessage());
    }
    if (pos < numSamples) render (pos, numSamples - pos);

    uiSwarm.store (engine.currentSwarm());
    uiPitch.store (engine.currentPitch());
}

//==============================================================================
void Swarm8Processor::timerCallback()
{
    // push CC-driven values to the host so automation / UI follow
    for (int i = 0; i < NumParams; ++i)
        if (ccPending[(size_t) i].exchange (false))
            params[(size_t) i]->setValueNotifyingHost (ccValue[(size_t) i].load());
}

void Swarm8Processor::setParamNorm (int index, float norm)
{
    params[(size_t) index]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
}

void Swarm8Processor::setParamReal (int index, float realValue)
{
    setParamNorm (index, params[(size_t) index]->convertTo0to1 (realValue));
}

//==============================================================================
int Swarm8Processor::ccForParam (int paramIndex) const
{
    for (int c = 0; c < 128; ++c) if (ccMap[(size_t) c].load() == paramIndex) return c;
    return -1;
}

void Swarm8Processor::clearCC (int paramIndex)
{
    for (auto& c : ccMap) if (c.load() == paramIndex) c.store (-1);
    ++ccVersion;
}

void Swarm8Processor::resetCCMapToDefaults()
{
    for (auto& c : ccMap) c.store (-1);
    for (int i = 0; i < NumParams; ++i)
        if (kParams[i].defaultCC >= 0) ccMap[(size_t) kParams[i].defaultCC].store (i);
    ++ccVersion;
}

//==============================================================================
bool Swarm8Processor::setOscPort (int port)
{
    osc.disconnect();
    oscPort = port;
    oscConnected = port > 0 && port < 65536 && osc.connect (port);
    return oscConnected;
}

void Swarm8Processor::oscBundleReceived (const juce::OSCBundle& b)
{
    for (auto& el : b)
    {
        if (el.isMessage())     oscMessageReceived (el.getMessage());
        else if (el.isBundle()) oscBundleReceived (el.getBundle());
    }
}

void Swarm8Processor::oscMessageReceived (const juce::OSCMessage& m)
{
    const juce::String addr = m.getAddressPattern().toString();
    auto arg = [&] (int i) -> float
    {
        if (i >= m.size()) return 0.0f;
        const auto& a = m[i];
        if (a.isFloat32()) return a.getFloat32();
        if (a.isInt32())   return (float) a.getInt32();
        return 0.0f;
    };

    if (addr == "/swarm/xy")             // XY pad: x = pitch ribbon, y = swarm
    {
        setParamNorm (pRibbon, arg (0));
        if (m.size() > 1) setParamNorm (pSwarm, arg (1));
    }
    else if (addr == "/swarm/touch" || addr == "/swarm/gate")
        setParamNorm (pRibGate, arg (0) > 0.5f ? 1.0f : 0.0f);
    else if (addr == "/swarm/preset")
        setCurrentProgram (juce::jlimit (0, getNumPrograms() - 1, (int) arg (0)));
    else if (addr == "/swarm/next")
    { if (m.size() == 0 || arg (0) > 0.5f) setCurrentProgram ((currentProgram + 1) % getNumPrograms()); }
    else if (addr == "/swarm/prev")
    { if (m.size() == 0 || arg (0) > 0.5f) setCurrentProgram ((currentProgram + getNumPrograms() - 1) % getNumPrograms()); }
    else if (addr.startsWith ("/swarm/"))  // /swarm/<paramId>  value 0..1
    {
        const int idx = findParam (addr.fromLastOccurrenceOf ("/", false, false).toRawUTF8());
        if (idx >= 0) setParamNorm (idx, arg (0));
    }
}

//==============================================================================
int Swarm8Processor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String Swarm8Processor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void Swarm8Processor::setCurrentProgram (int index)
{
    const auto& p = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) p.size())) return;
    float v[NumParams];
    presetValues (p[(size_t) index], v);
    v[pRibGate] = 0.0f;
    for (int i = 0; i < NumParams; ++i) setParamReal (i, v[i]);
    currentProgram = index;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

//==============================================================================
void Swarm8Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* extra = xml->createNewChildElement ("SWARM8_EXTRA");
    extra->setAttribute ("oscPort", oscPort);
    extra->setAttribute ("program", currentProgram);
    for (int c = 0; c < 128; ++c)
    {
        const int idx = ccMap[(size_t) c].load();
        if (idx >= 0) extra->setAttribute ("cc" + juce::String (c), kParams[idx].id);
    }
    copyXmlToBinary (*xml, destData);
}

void Swarm8Processor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    if (auto* extra = xml->getChildByName ("SWARM8_EXTRA"))
    {
        for (auto& c : ccMap) c.store (-1);
        for (int c = 0; c < 128; ++c)
        {
            const auto key = "cc" + juce::String (c);
            if (extra->hasAttribute (key))
            {
                const int idx = findParam (extra->getStringAttribute (key).toRawUTF8());
                if (idx >= 0) ccMap[(size_t) c].store (idx);
            }
        }
        ++ccVersion;
        currentProgram = extra->getIntAttribute ("program", 0);
        const int port = extra->getIntAttribute ("oscPort", 9000);
        if (port != oscPort) setOscPort (port);
        xml->removeChildElement (extra, true);
    }

    // never restore a "finger on the ribbon" state
    for (auto* child : xml->getChildIterator())
        if (child->getStringAttribute ("id") == "ribGate") child->setAttribute ("value", 0.0);

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* Swarm8Processor::createEditor() { return new Swarm8Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Swarm8Processor(); }
