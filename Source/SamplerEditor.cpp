#include "SamplerEditor.h"

namespace
{
const juce::Colour kBg (0xff101418), kPanel (0xff1a2027), kAccent (0xff2ec4b6), kAccent2 (0xffffb703),
                   kText (0xffd8dce4), kDim (0xff8b93a1);

bool isAudioFile (const juce::String& f)
{
    return f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff")
        || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".mp3") || f.endsWithIgnoreCase (".ogg")
        || f.endsWithIgnoreCase (".m4a");
}
} // namespace

// ============================================================== look & feel

SamplerLookAndFeel::SamplerLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff27303a));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a4552));
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff222a33));
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::Slider::textBoxTextColourId, kDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::ToggleButton::tickColourId, kAccent);
    setColour (juce::ToggleButton::tickDisabledColourId, kDim);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27303a));
    setColour (juce::TextButton::textColourOffId, kText);
}

void SamplerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff27303a));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);

    juce::Path track, value;
    const float ar = r - 2.f;
    track.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, angle, true);
    g.setColour (juce::Colour (0xff3a4552));
    g.strokePath (track, juce::PathStrokeType (2.5f));
    g.setColour (kAccent);
    g.strokePath (value, juce::PathStrokeType (2.5f));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -r + 5.f, 3.f, r * 0.45f, 1.f);
    g.setColour (kText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

// ============================================================== PadButton

void PadButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.f);
    juce::Colour base = hasSample ? juce::Colour (0xff25303a) : juce::Colour (0xff1b2229);
    if (selected) base = base.brighter (0.15f);
    g.setColour (base.interpolatedWith (kAccent, flash * 0.8f));
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (selected ? kAccent2 : (dragOver ? kAccent : juce::Colour (0xff3a4552)));
    g.drawRoundedRectangle (r, 8.f, selected || dragOver ? 2.5f : 1.f);

    g.setColour (kDim);
    g.setFont (11.f);
    g.drawText (juce::String (index + 1), r.reduced (6.f, 4.f).toNearestInt(), juce::Justification::topLeft);
    g.setColour (hasSample ? kText : kDim.withAlpha (0.6f));
    g.setFont (12.f);
    g.drawFittedText (hasSample ? name : "vide", r.reduced (6.f, 6.f).toNearestInt(), juce::Justification::centred, 2);
}

bool PadButton::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void PadButton::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    for (auto& f : files)
        if (isAudioFile (f)) { if (onFile) onFile (index, juce::File (f)); break; }
}

// ============================================================== WaveformView

void WaveformView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0d1115));
    g.fillRoundedRectangle (r, 6.f);

    if (sample == nullptr || sample->peaks.empty())
    {
        g.setColour (kDim);
        g.setFont (13.f);
        g.drawText ("Glissez un fichier audio ici (WAV, AIFF, MP3, FLAC)", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const float midY = r.getCentreY(), halfH = r.getHeight() * 0.46f;
    const int bins = (int) sample->peaks.size();
    juce::Path p;
    for (int x = 0; x < getWidth(); ++x)
    {
        const float v = sample->peaks[(size_t) juce::jlimit (0, bins - 1, x * bins / std::max (1, getWidth()))];
        p.addRectangle ((float) x, midY - v * halfH, 1.f, std::max (1.f, 2.f * v * halfH));
    }
    g.setColour (kAccent);
    g.fillPath (p);

    const float s = startP ? startP->getValue() : 0.f, e = endP ? endP->getValue() : 1.f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (0.f, 0.f, xOf (s), r.getHeight());
    g.fillRect (xOf (e), 0.f, r.getWidth() - xOf (e), r.getHeight());
    g.setColour (kAccent2);
    g.fillRect (xOf (s) - 1.f, 0.f, 2.f, r.getHeight());
    g.fillRect (xOf (e) - 1.f, 0.f, 2.f, r.getHeight());
    g.setFont (11.f);
    g.drawText ("D", (int) xOf (s) + 3, 2, 14, 14, juce::Justification::left);
    g.drawText ("F", (int) xOf (e) - 17, 2, 14, 14, juce::Justification::right);
}

void WaveformView::mouseDown (const juce::MouseEvent& ev)
{
    if (! startP || ! endP || sample == nullptr) return;
    const float x = (float) ev.x;
    const float ds = std::abs (x - xOf (startP->getValue())), de = std::abs (x - xOf (endP->getValue()));
    dragging = ds <= de ? 1 : 2;
    (dragging == 1 ? startP : endP)->beginChangeGesture();
    mouseDrag (ev);
}

void WaveformView::mouseDrag (const juce::MouseEvent& ev)
{
    if (! dragging) return;
    const float v = juce::jlimit (0.f, 1.f, (float) ev.x / (float) std::max (1, getWidth()));
    if (dragging == 1) startP->setValueNotifyingHost (std::min (v, endP->getValue() - 0.002f));
    else endP->setValueNotifyingHost (std::max (v, startP->getValue() + 0.002f));
    repaint();
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (dragging == 1) startP->endChangeGesture();
    else if (dragging == 2) endP->endChangeGesture();
    dragging = 0;
}

bool WaveformView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void WaveformView::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (isAudioFile (f)) { if (onFile) onFile (juce::File (f)); break; }
}

// ============================================================== StepGrid

void StepGrid::paint (juce::Graphics& g)
{
    if (proc == nullptr) return;
    const float w = (float) getWidth() / 16.f;
    for (int i = 0; i < 16; ++i)
    {
        auto cell = juce::Rectangle<float> (w * (float) i, 0.f, w, (float) getHeight()).reduced (3.f);
        const int v = proc->seq.get (pattern, pad, i);
        const bool active = i < length;
        juce::Colour c = ((i / 4) % 2 == 0) ? juce::Colour (0xff222b34) : juce::Colour (0xff1b232b);
        if (v == 1) c = kAccent;
        if (v == 2) c = kAccent2;
        if (! active) c = c.withAlpha (0.35f);
        g.setColour (c);
        g.fillRoundedRectangle (cell, 5.f);
        if (i == playing)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (cell, 5.f, 2.f);
        }
        g.setColour (v ? juce::Colours::black : kDim);
        g.setFont (11.f);
        g.drawText (juce::String (i + 1), cell.toNearestInt(), juce::Justification::centred);
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& ev)
{
    if (proc == nullptr) return;
    const int i = juce::jlimit (0, 15, (int) ((float) ev.x / ((float) getWidth() / 16.f)));
    const int v = proc->seq.get (pattern, pad, i);
    proc->seq.set (pattern, pad, i, (uint8_t) ((v + 1) % 3));
    repaint();
}

// ============================================================== SamplerEditor

void SamplerEditor::Ctl::attach (juce::RangedAudioParameter& p)
{
    sa.reset(); ca.reset(); ba.reset();
    if (slider)
    {
        sa = std::make_unique<juce::SliderParameterAttachment> (p, *slider);
        slider->setDoubleClickReturnValue (true, def->def);
    }
    else if (combo) ca = std::make_unique<juce::ComboBoxParameterAttachment> (p, *combo);
    else if (toggle) ba = std::make_unique<juce::ButtonParameterAttachment> (p, *toggle);
}

std::unique_ptr<SamplerEditor::Ctl> SamplerEditor::makeCtl (const sp::ParamDef& d, const juce::String& labelText)
{
    auto c = std::make_unique<Ctl>();
    c->def = &d;
    c->label.setText (labelText.isEmpty() ? juce::String (d.name) : labelText, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (juce::Font (juce::FontOptions (11.0f)));
    c->label.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (c->label);

    switch (d.kind)
    {
        case sp::K_FLOAT:
        case sp::K_INT:
            c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 14);
            addAndMakeVisible (*c->slider);
            break;
        case sp::K_CHOICE:
            c->combo = std::make_unique<juce::ComboBox>();
            c->combo->addItemList (juce::StringArray::fromTokens (d.choices, "|", ""), 1);
            addAndMakeVisible (*c->combo);
            break;
        case sp::K_BOOL:
            c->toggle = std::make_unique<juce::ToggleButton>();
            addAndMakeVisible (*c->toggle);
            break;
    }
    return c;
}

SamplerEditor::SamplerEditor (SamplerProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    selected = proc.selectedPad.load();

    // pads (disposition 4x4, pad 1 en bas à gauche comme sur une boîte à rythmes)
    for (int i = 0; i < sp::kPads; ++i)
    {
        auto b = std::make_unique<PadButton>();
        b->index = i;
        b->onDown = [this] (int idx)
        {
            selectPad (idx);
            proc.triggerPad (idx, 0.9f);
        };
        b->onUp = [this] (int idx) { proc.releasePad (idx); };
        b->onFile = [this] (int idx, const juce::File& f)
        {
            selectPad (idx);
            proc.loadSample (idx, f);
            refreshSample();
        };
        addAndMakeVisible (*b);
        pads[(size_t) i] = std::move (b);
    }

    wave.onFile = [this] (const juce::File& f) { proc.loadSample (selected, f); refreshSample(); };
    addAndMakeVisible (wave);
    addAndMakeVisible (steps);
    steps.proc = &proc;

    sampleName.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    sampleName.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (sampleName);

    loadBtn.onClick = [this] { chooseFile(); };
    clearBtn.onClick = [this] { proc.clearSample (selected); refreshSample(); };
    clearPatBtn.onClick = [this]
    {
        proc.seq.clearPattern ((int) proc.apvts.getRawParameterValue ("pattern")->load());
        steps.repaint();
    };
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (clearBtn);
    addAndMakeVisible (clearPatBtn);

    for (int k = 0; k < sp::PK_COUNT; ++k)
        if (k != sp::PK_START && k != sp::PK_END) padCtl[(size_t) k] = makeCtl (sp::kPadKeys[k]);

    for (int i = 0; i < sp::G_COUNT; ++i)
    {
        globalCtl[(size_t) i] = makeCtl (sp::kGlobals[i]);
        globalCtl[(size_t) i]->attach (*proc.apvts.getParameter (sp::kGlobals[i].id));
    }

    selectPad (selected);
    setSize (1100, 740);
    startTimerHz (30);
}

SamplerEditor::~SamplerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SamplerEditor::selectPad (int pad)
{
    selected = juce::jlimit (0, sp::kPads - 1, pad);
    proc.selectedPad.store (selected);
    for (int k = 0; k < sp::PK_COUNT; ++k)
        if (padCtl[(size_t) k])
            padCtl[(size_t) k]->attach (*proc.apvts.getParameter (juce::String (sp::padId (selected, k))));
    wave.startP = proc.apvts.getParameter (juce::String (sp::padId (selected, sp::PK_START)));
    wave.endP = proc.apvts.getParameter (juce::String (sp::padId (selected, sp::PK_END)));
    steps.pad = selected;
    refreshSample();
}

void SamplerEditor::refreshSample()
{
    for (int i = 0; i < sp::kPads; ++i)
    {
        auto s = proc.getSample (i);
        pads[(size_t) i]->hasSample = s != nullptr;
        pads[(size_t) i]->name = s ? juce::String (s->name) : juce::String();
        pads[(size_t) i]->selected = (i == selected);
        pads[(size_t) i]->repaint();
    }
    wave.sample = proc.getSample (selected);
    wave.repaint();
    sampleName.setText ("Pad " + juce::String (selected + 1) + " : "
                            + (wave.sample ? juce::String (wave.sample->name) : juce::String ("(vide)")),
                        juce::dontSendNotification);
}

void SamplerEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choisir un son pour le pad " + juce::String (selected + 1),
                                                   juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) { proc.loadSample (selected, f); refreshSample(); }
                          });
}

void SamplerEditor::timerCallback()
{
    const uint32_t hits = proc.hitMask.exchange (0);
    for (int i = 0; i < sp::kPads; ++i)
    {
        auto& b = *pads[(size_t) i];
        const float old = b.flash;
        if (hits & (1u << i)) b.flash = 1.f; else b.flash = std::max (0.f, b.flash - 0.12f);
        if (b.flash != old) b.repaint();
    }

    const int pat = (int) proc.apvts.getRawParameterValue ("pattern")->load();
    const int len = (int) proc.apvts.getRawParameterValue ("seqLen")->load();
    const int st = proc.seq.displayStep.load();
    if (pat != steps.pattern || st != steps.playing || len != steps.length)
    {
        steps.pattern = pat; steps.playing = st; steps.length = len;
        steps.repaint();
    }
    wave.repaint();
    proc.clearTrash();
}

void SamplerEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kAccent);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("SAMPLER 16", 20, 10, 260, 36, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (11.0f);
    g.drawText ("16 pads - vos propres sons - sequenceur de pas", 22, 40, 360, 16, juce::Justification::centredLeft);

    auto panel = [&] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 8.f);
        g.setColour (kAccent.withAlpha (0.8f));
        g.fillRect (r.getX() + 10, r.getY() + 21, 24, 2);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (title, r.getX() + 10, r.getY() + 4, r.getWidth() - 20, 16, juce::Justification::centredLeft);
    };
    panel (panelPad, "PADS");
    panel (panelSample, "SON DU PAD SELECTIONNE");
    panel (panelSeq, "SEQUENCEUR");
    panel (panelFx, "EFFETS ET SORTIE");
}

void SamplerEditor::layoutRow (std::vector<Ctl*> items, juce::Rectangle<int> area)
{
    if (items.empty()) return;
    const int cw = area.getWidth() / (int) items.size();
    for (auto* c : items)
    {
        auto cell = area.removeFromLeft (cw);
        c->label.setBounds (cell.removeFromTop (16));
        if (c->slider) c->slider->setBounds (cell.reduced (2));
        else if (c->combo) c->combo->setBounds (cell.withSizeKeepingCentre (std::min (cell.getWidth() - 8, 100), 26).translated (0, -4));
        else if (c->toggle) c->toggle->setBounds (cell.withSizeKeepingCentre (30, 30).translated (0, -2));
    }
}

void SamplerEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (62);
    area.reduce (14, 4);

    // --- haut : pads (gauche) + son du pad (droite)
    auto top = area.removeFromTop (352);
    panelPad = top.removeFromLeft (404);
    top.removeFromLeft (10);
    panelSample = top;

    auto pa = panelPad.reduced (10, 0);
    pa.removeFromTop (28);
    pa.removeFromBottom (8);
    const int cw = pa.getWidth() / 4, ch = pa.getHeight() / 4;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
        {
            const int idx = (3 - row) * 4 + col;
            pads[(size_t) idx]->setBounds (pa.getX() + col * cw, pa.getY() + row * ch, cw, ch);
        }

    auto sa = panelSample.reduced (10, 0);
    sa.removeFromTop (26);
    auto head = sa.removeFromTop (28);
    clearBtn.setBounds (head.removeFromRight (80));
    head.removeFromRight (6);
    loadBtn.setBounds (head.removeFromRight (150));
    sampleName.setBounds (head);
    sa.removeFromTop (4);
    wave.setBounds (sa.removeFromTop (118));
    sa.removeFromTop (2);

    using namespace sp;
    layoutRow ({ padCtl[PK_VOL].get(), padCtl[PK_PAN].get(), padCtl[PK_TUNE].get(), padCtl[PK_ATK].get(),
                 padCtl[PK_REL].get(), padCtl[PK_CUT].get(), padCtl[PK_RES].get() }, sa.removeFromTop (92));
    layoutRow ({ padCtl[PK_MODE].get(), padCtl[PK_REV].get(), padCtl[PK_CHOKE].get(),
                 globalCtl[G_KBMODE].get(), globalCtl[G_ROOT].get() }, sa);

    area.removeFromTop (8);

    // --- séquenceur
    panelSeq = area.removeFromTop (190);
    auto qa = panelSeq.reduced (10, 0);
    qa.removeFromTop (26);
    auto stepArea = qa.removeFromTop (50);
    clearPatBtn.setBounds (stepArea.removeFromRight (120).reduced (0, 10));
    steps.setBounds (stepArea);
    qa.removeFromTop (6);
    layoutRow ({ globalCtl[G_PATTERN].get(), globalCtl[G_SEQON].get(), globalCtl[G_SEQFREE].get(), globalCtl[G_SEQRATE].get(),
                 globalCtl[G_SEQLEN].get(), globalCtl[G_SEQGATE].get(), globalCtl[G_SWING].get() }, qa);

    area.removeFromTop (8);

    // --- effets
    panelFx = area;
    auto fa = panelFx.reduced (10, 0);
    fa.removeFromTop (26);
    layoutRow ({ globalCtl[G_DRIVE].get(), globalCtl[G_DLYTIME].get(), globalCtl[G_DLYFB].get(), globalCtl[G_DLYMIX].get(),
                 globalCtl[G_RVBSIZE].get(), globalCtl[G_RVBMIX].get(), globalCtl[G_MASTER].get() }, fa);
}
