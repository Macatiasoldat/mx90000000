#pragma once
#include "SamplerProcessor.h"

class SamplerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SamplerLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
};

// Un pad cliquable qui accepte le glisser-déposer d'un fichier audio
class PadButton : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    int index = 0;
    juce::String name;
    bool selected = false, hasSample = false, dragOver = false;
    float flash = 0.f;
    std::function<void (int)> onDown, onUp;
    std::function<void (int, const juce::File&)> onFile;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onDown) onDown (index); }
    void mouseUp (const juce::MouseEvent&) override { if (onUp) onUp (index); }
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;
};

// Forme d'onde avec poignées Début / Fin
class WaveformView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    sp::SamplePtr sample;
    juce::RangedAudioParameter* startP = nullptr;
    juce::RangedAudioParameter* endP = nullptr;
    std::function<void (const juce::File&)> onFile;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    int dragging = 0; // 0 aucun, 1 début, 2 fin
    float xOf (float norm) const { return norm * (float) getWidth(); }
};

// 16 pas du pad sélectionné dans le pattern courant : clic = vide → normal → accent
class StepGrid : public juce::Component
{
public:
    SamplerProcessor* proc = nullptr;
    int pad = 0, pattern = 0, playing = -1, length = 16;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
};

class SamplerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SamplerEditor (SamplerProcessor&);
    ~SamplerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Ctl
    {
        const sp::ParamDef* def = nullptr;
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        // attachements après les composants : détruits en premier
        std::unique_ptr<juce::SliderParameterAttachment> sa;
        std::unique_ptr<juce::ComboBoxParameterAttachment> ca;
        std::unique_ptr<juce::ButtonParameterAttachment> ba;

        void attach (juce::RangedAudioParameter& p);
        juce::Component* comp() { return slider ? (juce::Component*) slider.get() : combo ? (juce::Component*) combo.get() : (juce::Component*) toggle.get(); }
    };

    std::unique_ptr<Ctl> makeCtl (const sp::ParamDef& d, const juce::String& labelText = {});
    void layoutRow (std::vector<Ctl*> items, juce::Rectangle<int> area);
    void selectPad (int pad);
    void refreshSample();
    void timerCallback() override;
    void chooseFile();

    SamplerProcessor& proc;
    SamplerLookAndFeel lnf;
    std::array<std::unique_ptr<PadButton>, sp::kPads> pads;
    WaveformView wave;
    StepGrid steps;
    std::array<std::unique_ptr<Ctl>, sp::PK_COUNT> padCtl;
    std::array<std::unique_ptr<Ctl>, sp::G_COUNT> globalCtl;
    juce::Label sampleName;
    juce::TextButton loadBtn { "Charger un son..." }, clearBtn { "Effacer" }, clearPatBtn { "Vider le pattern" };
    std::unique_ptr<juce::FileChooser> chooser;
    int selected = 0;
    juce::Rectangle<int> panelPad, panelSample, panelSeq, panelFx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEditor)
};
