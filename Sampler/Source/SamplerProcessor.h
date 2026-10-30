#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Sequencer.h"
#include "SamplerEngine.h"

class SamplerProcessor : public juce::AudioProcessor
{
public:
    SamplerProcessor();
    ~SamplerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sampler16"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- gestion des sons (thread interface)
    bool loadSample (int pad, const juce::File& file);
    void clearSample (int pad);
    sp::SamplePtr getSample (int pad);
    juce::String getSamplePath (int pad);
    void clearTrash();

    // --- déclenchement depuis l'interface
    void triggerPad (int pad, float vel) { previewOn.fetch_or (1u << pad); (void) vel; }
    void releasePad (int pad) { previewOff.fetch_or (1u << pad); }

    juce::AudioProcessorValueTreeState apvts;
    sp::Sequencer seq;
    std::atomic<int> selectedPad { 0 };
    std::atomic<uint32_t> hitMask { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void installPendingSamples();

    sp::Engine engine;
    juce::dsp::Reverb reverb;
    juce::AudioFormatManager formats;

    std::array<std::atomic<float>*, sp::G_COUNT> rawG {};
    std::array<std::array<std::atomic<float>*, sp::PK_COUNT>, sp::kPads> rawP {};

    juce::SpinLock lock;
    sp::SamplePtr current[sp::kPads];
    bool dirty[sp::kPads] {};
    juce::String paths[sp::kPads];
    std::vector<sp::SamplePtr> trash;

    std::atomic<uint32_t> previewOn { 0 }, previewOff { 0 };
    std::vector<sp::Ev> inEvs, seqEvs, merged;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerProcessor)
};
