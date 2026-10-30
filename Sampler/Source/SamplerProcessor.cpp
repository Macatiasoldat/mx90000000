#include "SamplerProcessor.h"
#include "SamplerEditor.h"

namespace
{
void addParam (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& ps, const juce::String& id, const sp::ParamDef& d)
{
    const juce::ParameterID pid { id, 1 };
    switch (d.kind)
    {
        case sp::K_FLOAT:
        {
            juce::NormalisableRange<float> r (d.mn, d.mx, 0.0f, d.skew);
            ps.push_back (std::make_unique<juce::AudioParameterFloat> (pid, d.name, r, d.def));
            break;
        }
        case sp::K_CHOICE:
            ps.push_back (std::make_unique<juce::AudioParameterChoice> (
                pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
            break;
        case sp::K_BOOL:
            ps.push_back (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
            break;
        case sp::K_INT:
            ps.push_back (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.mn, (int) d.mx, (int) d.def));
            break;
    }
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SamplerProcessor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    for (int i = 0; i < sp::G_COUNT; ++i)
        addParam (ps, sp::kGlobals[i].id, sp::kGlobals[i]);
    for (int p = 0; p < sp::kPads; ++p)
        for (int k = 0; k < sp::PK_COUNT; ++k)
        {
            const sp::ParamDef& d = sp::kPadKeys[k];
            const juce::String name = "Pad " + juce::String (p + 1) + " " + d.name;
            const juce::ParameterID pid { juce::String (sp::padId (p, k)), 1 };
            switch (sp::kPadKeys[k].kind)
            {
                case sp::K_FLOAT:
                {
                    juce::NormalisableRange<float> r (d.mn, d.mx, 0.0f, d.skew);
                    ps.push_back (std::make_unique<juce::AudioParameterFloat> (pid, name, r, d.def));
                    break;
                }
                case sp::K_CHOICE:
                    ps.push_back (std::make_unique<juce::AudioParameterChoice> (
                        pid, name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
                    break;
                case sp::K_BOOL:
                    ps.push_back (std::make_unique<juce::AudioParameterBool> (pid, name, d.def > 0.5f));
                    break;
                case sp::K_INT:
                    ps.push_back (std::make_unique<juce::AudioParameterInt> (pid, name, (int) d.mn, (int) d.mx, (int) d.def));
                    break;
            }
        }
    return { ps.begin(), ps.end() };
}

SamplerProcessor::SamplerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", makeLayout())
{
    for (int i = 0; i < sp::G_COUNT; ++i)
        rawG[(size_t) i] = apvts.getRawParameterValue (sp::kGlobals[i].id);
    for (int p = 0; p < sp::kPads; ++p)
        for (int k = 0; k < sp::PK_COUNT; ++k)
            rawP[(size_t) p][(size_t) k] = apvts.getRawParameterValue (juce::String (sp::padId (p, k)));

    formats.registerBasicFormats();
    trash.reserve (64);
    inEvs.reserve (1024);
    seqEvs.reserve (1024);
    merged.reserve (2048);
}

void SamplerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    seq.reset();
    reverb.reset();
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) std::max (samplesPerBlock, 1), 2 };
    reverb.prepare (spec);
    {
        const juce::SpinLock::ScopedLockType l (lock);
        for (int p = 0; p < sp::kPads; ++p) dirty[p] = true; // réinstalle les sons dans le moteur
    }
}

bool SamplerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ------------------------------------------------------------------ sons

bool SamplerProcessor::loadSample (int pad, const juce::File& file)
{
    if (pad < 0 || pad >= sp::kPads) return false;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 32) return false;

    const int frames = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 120.0));
    const int chans = (int) std::min<unsigned> (2u, reader->numChannels);
    juce::AudioBuffer<float> buf (chans, frames);
    if (! reader->read (&buf, 0, frames, 0, true, chans > 1)) return false;

    auto s = std::make_shared<sp::Sample>();
    s->sr = reader->sampleRate;
    s->frames = frames;
    s->name = file.getFileNameWithoutExtension().toStdString();
    s->L.assign (buf.getReadPointer (0), buf.getReadPointer (0) + frames);
    if (chans > 1) s->R.assign (buf.getReadPointer (1), buf.getReadPointer (1) + frames);

    const int bins = 800;
    s->peaks.assign ((size_t) bins, 0.f);
    for (int b = 0; b < bins; ++b)
    {
        const int a = (int) ((juce::int64) frames * b / bins), e = std::max (a + 1, (int) ((juce::int64) frames * (b + 1) / bins));
        float m = 0.f;
        for (int i = a; i < e && i < frames; ++i)
        {
            m = std::max (m, std::fabs (s->L[(size_t) i]));
            if (chans > 1) m = std::max (m, std::fabs (s->R[(size_t) i]));
        }
        s->peaks[(size_t) b] = m;
    }

    {
        const juce::SpinLock::ScopedLockType l (lock);
        current[pad] = s;
        dirty[pad] = true;
        paths[pad] = file.getFullPathName();
    }
    return true;
}

void SamplerProcessor::clearSample (int pad)
{
    if (pad < 0 || pad >= sp::kPads) return;
    const juce::SpinLock::ScopedLockType l (lock);
    current[pad] = nullptr;
    dirty[pad] = true;
    paths[pad] = {};
}

sp::SamplePtr SamplerProcessor::getSample (int pad)
{
    const juce::SpinLock::ScopedLockType l (lock);
    return current[pad];
}

juce::String SamplerProcessor::getSamplePath (int pad)
{
    const juce::SpinLock::ScopedLockType l (lock);
    return paths[pad];
}

void SamplerProcessor::clearTrash()
{
    std::vector<sp::SamplePtr> tmp;
    tmp.reserve (64);
    {
        const juce::SpinLock::ScopedLockType l (lock);
        for (auto& x : trash) tmp.push_back (std::move (x));
        trash.clear();
    }
} // tmp détruit ici, hors verrou

void SamplerProcessor::installPendingSamples()
{
    if (! lock.tryEnter()) return; // l'interface est en train d'écrire : on réessaie au bloc suivant
    for (int p = 0; p < sp::kPads; ++p)
        if (dirty[p])
        {
            auto old = engine.setSample (p, current[p]);
            if (old && trash.size() < trash.capacity()) trash.push_back (std::move (old));
            dirty[p] = false;
        }
    lock.exit();
}

// ------------------------------------------------------------------ audio

void SamplerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    installPendingSamples();

    sp::Params P;
    for (int i = 0; i < sp::G_COUNT; ++i) P.g[i] = rawG[(size_t) i]->load();
    for (int p = 0; p < sp::kPads; ++p)
        for (int k = 0; k < sp::PK_COUNT; ++k) P.pad[p][k] = rawP[(size_t) p][(size_t) k]->load();

    const bool kb = P.g[sp::G_KBMODE] > 0.5f;

    // --- MIDI + déclenchements de l'interface
    inEvs.clear();
    if (const uint32_t m = previewOn.exchange (0))
        for (int p = 0; p < sp::kPads; ++p)
            if (m & (1u << p)) inEvs.push_back ({ 0, sp::EV_PAD_ON, p, 0.9f });
    if (const uint32_t m = previewOff.exchange (0))
        for (int p = 0; p < sp::kPads; ++p)
            if (m & (1u << p)) inEvs.push_back ({ 0, sp::EV_PAD_OFF, p, 0.f });

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        const int note = msg.getNoteNumber();
        if (msg.isNoteOn())
        {
            if (kb) inEvs.push_back ({ pos, sp::EV_KEY_ON, note, msg.getFloatVelocity() });
            else if (note >= 36 && note < 36 + sp::kPads) inEvs.push_back ({ pos, sp::EV_PAD_ON, note - 36, msg.getFloatVelocity() });
        }
        else if (msg.isNoteOff())
        {
            if (kb) inEvs.push_back ({ pos, sp::EV_KEY_OFF, note, 0.f });
            else if (note >= 36 && note < 36 + sp::kPads) inEvs.push_back ({ pos, sp::EV_PAD_OFF, note - 36, 0.f });
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            inEvs.push_back ({ pos, sp::EV_ALL_OFF, 0, 0.f });
    }
    midi.clear();

    // --- séquenceur
    double bpm = 120.0, ppq = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            bpm = pos->getBpm().orFallback (120.0);
            playing = pos->getIsPlaying();
            ppq = pos->getPpqPosition().orFallback (-1.0);
        }
    seqEvs.clear();
    seq.process (seqEvs, n, getSampleRate(), P, bpm, playing, ppq);

    // --- fusion triée (sans allocation)
    for (size_t i = 1; i < inEvs.size(); ++i) // tri par insertion stable, sans allocation
    {
        const sp::Ev x = inEvs[i];
        size_t j = i;
        while (j > 0 && inEvs[j - 1].pos > x.pos) { inEvs[j] = inEvs[j - 1]; --j; }
        inEvs[j] = x;
    }
    merged.clear();
    {
        size_t a = 0, b = 0;
        while (a < inEvs.size() || b < seqEvs.size())
        {
            if (b >= seqEvs.size() || (a < inEvs.size() && inEvs[a].pos <= seqEvs[b].pos)) merged.push_back (inEvs[a++]);
            else merged.push_back (seqEvs[b++]);
        }
    }

    // --- rendu
    engine.setParams (P);
    engine.setKeyboardPad (selectedPad.load());
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int cur = 0;
    uint32_t hits = 0;
    for (const auto& e : merged)
    {
        if (e.pos > cur)
        {
            engine.render (L + cur, R + cur, e.pos - cur);
            cur = e.pos;
        }
        engine.handle (e);
        if (e.type == sp::EV_PAD_ON) hits |= 1u << e.note;
        else if (e.type == sp::EV_KEY_ON) hits |= 1u << selectedPad.load();
    }
    if (cur < n) engine.render (L + cur, R + cur, n - cur);
    if (hits) hitMask.fetch_or (hits);

    // --- reverb
    if (P.g[sp::G_RVBMIX] > 0.001f)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = P.g[sp::G_RVBSIZE];
        rp.damping = 0.45f;
        rp.wetLevel = P.g[sp::G_RVBMIX] * 0.6f;
        rp.dryLevel = 1.0f - P.g[sp::G_RVBMIX] * 0.35f;
        rp.width = 1.0f;
        reverb.setParameters (rp);
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

    // --- master + limiteur doux
    const float master = P.g[sp::G_MASTER];
    for (int i = 0; i < n; ++i)
    {
        L[i] = std::tanh (L[i] * master);
        R[i] = std::tanh (R[i] * master);
    }
}

// ------------------------------------------------------------------ état

void SamplerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeAllChildren (nullptr);

    juce::ValueTree extra ("EXTRA");
    extra.setProperty ("selected", selectedPad.load(), nullptr);
    for (int p = 0; p < sp::kPads; ++p)
        extra.setProperty ("path" + juce::String (p), getSamplePath (p), nullptr);
    for (int pat = 0; pat < sp::Sequencer::kPatterns; ++pat)
    {
        juce::String s;
        for (int pad = 0; pad < sp::kPads; ++pad)
            for (int st = 0; st < sp::Sequencer::kSteps; ++st)
                s << (char) ('0' + seq.get (pat, pad, st));
        extra.setProperty ("pat" + juce::String (pat), s, nullptr);
    }
    state.addChild (extra, -1, nullptr);

    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void SamplerProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    auto extra = tree.getChildWithName ("EXTRA");
    tree.removeChild (extra, nullptr);
    apvts.replaceState (tree);

    if (! extra.isValid()) return;
    selectedPad.store (juce::jlimit (0, sp::kPads - 1, (int) extra.getProperty ("selected", 0)));

    for (int pat = 0; pat < sp::Sequencer::kPatterns; ++pat)
    {
        const juce::String s = extra.getProperty ("pat" + juce::String (pat)).toString();
        for (int pad = 0; pad < sp::kPads; ++pad)
            for (int st = 0; st < sp::Sequencer::kSteps; ++st)
            {
                const int i = pad * sp::Sequencer::kSteps + st;
                const int v = i < s.length() ? (int) (s[i] - '0') : 0;
                seq.set (pat, pad, st, (uint8_t) juce::jlimit (0, 2, v));
            }
    }

    for (int p = 0; p < sp::kPads; ++p)
    {
        const juce::String path = extra.getProperty ("path" + juce::String (p)).toString();
        if (path.isNotEmpty())
        {
            const juce::File f (path);
            if (f.existsAsFile()) loadSample (p, f);
        }
    }
}

juce::AudioProcessorEditor* SamplerProcessor::createEditor() { return new SamplerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SamplerProcessor(); }
