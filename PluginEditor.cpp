#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff15171c), kPanel (0xff1f2229), kAccent (0xffff8a1f), kText (0xffd8dce4), kDim (0xff8b93a1);
}

JovianLookAndFeel::JovianLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2b2f38));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3f4b));
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff23262e));
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::Slider::textBoxTextColourId, kDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::ToggleButton::tickColourId, kAccent);
    setColour (juce::ToggleButton::tickDisabledColourId, kDim);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2f38));
    setColour (juce::TextButton::textColourOffId, kText);
}

void JovianLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff2b2f38));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);

    juce::Path track, value;
    const float ar = r - 2.f;
    track.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, angle, true);
    g.setColour (juce::Colour (0xff3d4350));
    g.strokePath (track, juce::PathStrokeType (2.5f));
    g.setColour (kAccent);
    g.strokePath (value, juce::PathStrokeType (2.5f));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -r + 5.f, 3.f, r * 0.45f, 1.f);
    g.setColour (kText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

JovianEditor::JovianEditor (JovianProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    controls.resize (jv::P_COUNT);
    for (int i = 0; i < jv::P_COUNT; ++i)
    {
        const auto& d = jv::kDefs[i];
        auto c = std::make_unique<Control>();
        c->id = i;
        c->label.setText (d.name, juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setFont (juce::Font (juce::FontOptions (11.0f)));
        c->label.setColour (juce::Label::textColourId, kDim);
        addAndMakeVisible (c->label);

        auto* param = p.apvts.getParameter (d.id);
        switch (d.kind)
        {
            case jv::K_FLOAT:
            case jv::K_INT:
                c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 14);
                c->sa = std::make_unique<juce::SliderParameterAttachment> (*param, *c->slider);
                c->slider->setDoubleClickReturnValue (true, d.def);
                addAndMakeVisible (*c->slider);
                break;
            case jv::K_CHOICE:
                c->combo = std::make_unique<juce::ComboBox>();
                c->combo->addItemList (juce::StringArray::fromTokens (d.choices, "|", ""), 1);
                c->ca = std::make_unique<juce::ComboBoxParameterAttachment> (*param, *c->combo);
                addAndMakeVisible (*c->combo);
                break;
            case jv::K_BOOL:
                c->toggle = std::make_unique<juce::ToggleButton>();
                c->ba = std::make_unique<juce::ButtonParameterAttachment> (*param, *c->toggle);
                addAndMakeVisible (*c->toggle);
                break;
        }
        controls[(size_t) i] = std::move(c);
    }

    using namespace jv;
    sections = {
        { "OSCILLATEUR 1", 0, { P_O1WAVE, P_O1RANGE }, {} },
        { "OSCILLATEUR 2", 0, { P_O2WAVE, P_O2RANGE, P_O2SEMI, P_O2FINE, P_SYNC }, {} },
        { "MIXEUR / MOD", 0, { P_MIX, P_NOISE, P_PW, P_PWM, P_XMOD }, {} },
        { "FILTRE (HPF / VCF)", 1, { P_HPF, P_CUTOFF, P_RES, P_SLOPE, P_ENVAMT, P_KEYFOL, P_LFOFILT }, {} },
        { "ENV-1 FILTRE", 1, { P_FA, P_FD, P_FS, P_FR }, {} },
        { "ENV-2 AMPLI", 1, { P_AA, P_AD, P_AS, P_AR }, {} },
        { "LFO", 2, { P_LFOWAVE, P_LFORATE, P_LFODELAY, P_LFOPITCH }, {} },
        { "ARPEGIATEUR", 2, { P_ARPON, P_ARPHOLD, P_ARPMODE, P_ARPRATE, P_ARPOCT, P_ARPGATE }, {} },
        { "GLOBAL", 3, { P_POLY, P_GLIDE, P_UNIDET, P_SPREAD, P_CHORUS, P_BENDRANGE, P_VELSENS, P_MASTER }, {} },
    };

    // barre de presets
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    prevBtn.onClick = [this]
    {
        const int n = proc.getNumPrograms();
        presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + n - 1) % n, juce::sendNotification);
    };
    nextBtn.onClick = [this]
    {
        const int n = proc.getNumPrograms();
        presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + 1) % n, juce::sendNotification);
    };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    setSize (1100, 640);
}

JovianEditor::~JovianEditor() { setLookAndFeel (nullptr); }

void JovianEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kAccent);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("JOVIAN 8", 20, 10, 200, 36, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (11.0f);
    g.drawText ("synthe polyphonique analogique virtuel - 8 voix", 22, 40, 320, 16, juce::Justification::centredLeft);

    for (const auto& s : sections)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (s.bounds.toFloat(), 8.f);
        g.setColour (kAccent.withAlpha (0.8f));
        g.fillRect (s.bounds.getX() + 10, s.bounds.getY() + 21, 24, 2);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (s.title, s.bounds.getX() + 10, s.bounds.getY() + 4, s.bounds.getWidth() - 20, 16, juce::Justification::centredLeft);
    }
}

void JovianEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (62).reduced (16, 12);
    nextBtn.setBounds (top.removeFromRight (34));
    top.removeFromRight (4);
    prevBtn.setBounds (top.removeFromRight (34));
    top.removeFromRight (6);
    presetBox.setBounds (top.removeFromRight (300));

    area.reduce (12, 6);
    const int rows = 4;
    const int rowH = area.getHeight() / rows;

    for (int r = 0; r < rows; ++r)
    {
        int total = 0;
        for (auto& s : sections) if (s.row == r) total += (int) s.ids.size();
        auto rowArea = area.removeFromTop (rowH).reduced (0, 4);
        for (auto& s : sections)
        {
            if (s.row != r) continue;
            const int w = rowArea.getWidth() * (int) s.ids.size() / std::max (1, total);
            s.bounds = rowArea.removeFromLeft (w).reduced (4, 0);
            total -= (int) s.ids.size();
        }
    }

    for (auto& s : sections)
    {
        auto inner = s.bounds.reduced (6, 0);
        inner.removeFromTop (28);
        const int cw = inner.getWidth() / std::max (1, (int) s.ids.size());
        for (int id : s.ids)
        {
            auto cell = inner.removeFromLeft (cw);
            auto& c = *controls[(size_t) id];
            c.label.setBounds (cell.removeFromTop (16));
            if (c.slider)
                c.slider->setBounds (cell.reduced (2));
            else if (c.combo)
                c.combo->setBounds (cell.withSizeKeepingCentre (std::min (cell.getWidth() - 8, 96), 26).translated (0, -8));
            else if (c.toggle)
                c.toggle->setBounds (cell.withSizeKeepingCentre (30, 30).translated (0, -6));
        }
    }
}
