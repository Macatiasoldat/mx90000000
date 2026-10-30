#pragma once
#include "PluginProcessor.h"

class JovianLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JovianLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
};

class JovianEditor : public juce::AudioProcessorEditor
{
public:
    explicit JovianEditor (JovianProcessor&);
    ~JovianEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        int id = 0;
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        // les attachements sont déclarés après les composants : détruits en premier
        std::unique_ptr<juce::SliderParameterAttachment> sa;
        std::unique_ptr<juce::ComboBoxParameterAttachment> ca;
        std::unique_ptr<juce::ButtonParameterAttachment> ba;
        juce::Component* comp() { return slider ? (juce::Component*) slider.get() : combo ? (juce::Component*) combo.get() : (juce::Component*) toggle.get(); }
    };

    struct Section
    {
        juce::String title;
        int row;
        std::vector<int> ids;
        juce::Rectangle<int> bounds;
    };

    JovianProcessor& proc;
    JovianLookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls; // indexé par PId
    std::vector<Section> sections;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JovianEditor)
};
