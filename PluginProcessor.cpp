#include "PluginProcessor.h"
#include "PluginEditor.h"

JovianProcessor::JovianProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", makeLayout())
{
    for (int i = 0; i < jv::P_COUNT; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (jv::kDefs[i].id);

    noteEvs.reserve (512);
    ctrlEvs.reserve (512);
    arpOut.reserve (1024);
    merged.reserve (2048);
}

juce::AudioProcessorValueTreeState::ParameterLayout JovianProcessor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    for (int i = 0; i < jv::P_COUNT; ++i)
    {
        const auto& d = jv::kDefs[i];
        const juce::ParameterID pid { d.id, 1 };
        switch (d.kind)
        {
            case jv::K_FLOAT:
            {
                juce::NormalisableRange<float> r (d.mn, d.mx, 0.0f, d.skew);
                ps.push_back (std::make_unique<juce::AudioParameterFloat> (pid, d.name, r, d.def));
                break;
            }
            case jv::K_CHOICE:
                ps.push_back (std::make_unique<juce::AudioParameterChoice> (
                    pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
                break;
            case jv::K_BOOL:
                ps.push_back (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;
            case jv::K_INT:
                ps.push_back (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.mn, (int) d.mx, (int) d.def));
                break;
        }
    }
    return { ps.begin(), ps.end() };
}

void JovianProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, std::max (samplesPerBlock, 1024));
    arp.prepare (sampleRate);
    arpWasOn = false;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) std::max (samplesPerBlock, 1), 2 };
    chorus.prepare (spec);
    chorus.reset();
}

bool JovianProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void JovianProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    jv::Params p;
    for (int i = 0; i < jv::P_COUNT; ++i) p.v[i] = raw[(size_t) i]->load();

    // --- MIDI -> événements internes
    noteEvs.clear();
    ctrlEvs.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        if (m.isNoteOn())
            noteEvs.push_back ({ pos, jv::EV_NOTE_ON, m.getNoteNumber(), m.getFloatVelocity() });
        else if (m.isNoteOff())
            noteEvs.push_back ({ pos, jv::EV_NOTE_OFF, m.getNoteNumber(), 0.f });
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            noteEvs.push_back ({ pos, jv::EV_ALL_OFF, 0, 0.f });
        else if (m.isPitchWheel())
            ctrlEvs.push_back ({ pos, jv::EV_BEND, 0, (m.getPitchWheelValue() - 8192) / 8192.f });
        else if (m.isControllerOfType (1))
            ctrlEvs.push_back ({ pos, jv::EV_MOD, 0, m.getControllerValue() / 127.f });
        else if (m.isControllerOfType (64))
            ctrlEvs.push_back ({ pos, jv::EV_SUSTAIN, 0, m.getControllerValue() >= 64 ? 1.f : 0.f });
    }
    midi.clear();

    // --- Arpégiateur
    const bool arpOn = p[jv::P_ARPON] > 0.5f;
    if (arpOn != arpWasOn)
    {
        engine.allNotesOff();
        arp.reset();
        arpWasOn = arpOn;
    }

    const std::vector<jv::Ev>* notes = &noteEvs;
    if (arpOn)
    {
        double bpm = 120.0, ppq = -1.0;
        bool playing = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                bpm = pos->getBpm().orFallback (120.0);
                playing = pos->getIsPlaying();
                ppq = pos->getPpqPosition().orFallback (-1.0);
            }

        jv::ArpSettings s;
        s.mode = (int) p[jv::P_ARPMODE];
        s.rate = (int) p[jv::P_ARPRATE];
        s.octaves = (int) p[jv::P_ARPOCT];
        s.gate = p[jv::P_ARPGATE];
        s.hold = p[jv::P_ARPHOLD] > 0.5f;

        arpOut.clear();
        arp.process (noteEvs, arpOut, n, s, bpm, playing, ppq);
        notes = &arpOut;
    }

    // --- fusion triée (contrôleurs d'abord à position égale), sans allocation
    merged.clear();
    {
        size_t a = 0, b = 0;
        while (a < ctrlEvs.size() || b < notes->size())
        {
            if (b >= notes->size() || (a < ctrlEvs.size() && ctrlEvs[a].pos <= (*notes)[b].pos))
                merged.push_back (ctrlEvs[a++]);
            else
                merged.push_back ((*notes)[b++]);
        }
    }

    // --- rendu
    engine.setParams (p);
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int cur = 0;
    for (const auto& e : merged)
    {
        if (e.pos > cur)
        {
            engine.render (L + cur, R + cur, e.pos - cur);
            cur = e.pos;
        }
        engine.handle (e);
    }
    if (cur < n) engine.render (L + cur, R + cur, n - cur);

    // --- chorus
    const int ch = (int) p[jv::P_CHORUS];
    if (ch > 0)
    {
        chorus.setRate (ch == 1 ? 0.5f : 0.8f);
        chorus.setDepth (ch == 1 ? 0.25f : 0.35f);
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.0f);
        chorus.setMix (0.5f);
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorus.process (ctx);
    }
}

void JovianProcessor::setParam (int id, float v)
{
    if (auto* prm = apvts.getParameter (jv::kDefs[id].id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void JovianProcessor::setCurrentProgram (int index)
{
    const auto& list = jv::presets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    for (int i = 0; i < jv::P_COUNT; ++i) setParam (i, jv::kDefs[i].def);
    for (const auto& kv : list[(size_t) index].v) setParam (kv.first, kv.second);
}

const juce::String JovianProcessor::getProgramName (int index)
{
    const auto& list = jv::presets();
    return (index >= 0 && index < (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

void JovianProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void JovianProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* JovianProcessor::createEditor() { return new JovianEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JovianProcessor(); }
