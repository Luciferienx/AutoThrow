#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AutoThrowEngine.h"

class AutoThrowProcessor : public juce::AudioProcessor
{
public:
    AutoThrowProcessor();
    ~AutoThrowProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Lus par l'interface (thread graphique)
    std::atomic<float> meterThrow { 0.0f };
    std::atomic<float> meterInput { 0.0f };
    std::atomic<bool>  meterVoice { false };

private:
    autothrow::Engine engine;
    std::vector<float> monoScratch;

    std::atomic<float>* pType = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pShimmer = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pSens = nullptr;
    std::atomic<float>* pGap = nullptr;
    std::atomic<float>* pDuck = nullptr;
    std::atomic<float>* pBloom = nullptr;
    std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pWetOnly = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoThrowProcessor)
};
