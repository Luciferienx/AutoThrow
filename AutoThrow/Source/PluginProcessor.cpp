#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

AutoThrowProcessor::AutoThrowProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AutoThrow", createLayout())
{
    pDecay   = apvts.getRawParameterValue ("decay");
    pShimmer = apvts.getRawParameterValue ("shimmer");
    pTone    = apvts.getRawParameterValue ("tone");
    pMix     = apvts.getRawParameterValue ("mix");
    pSens    = apvts.getRawParameterValue ("sens");
    pGap     = apvts.getRawParameterValue ("gap");
    pDuck    = apvts.getRawParameterValue ("duck");
    pBloom   = apvts.getRawParameterValue ("bloom");
    pFreeze  = apvts.getRawParameterValue ("freeze");
    pWetOnly = apvts.getRawParameterValue ("wetonly");
}

APVTS::ParameterLayout AutoThrowProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };

    NormalisableRange<float> decayRange (1.0f, 30.0f, 0.01f);
    decayRange.setSkewForCentre (6.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 }, "Decay", decayRange, 8.0f,
        AudioParameterFloatAttributes().withLabel ("s")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " s"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "shimmer", 1 }, "Shimmer",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sens", 1 }, "Sens",
        NormalisableRange<float> (-60.0f, -10.0f, 0.1f), -35.0f,
        AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    NormalisableRange<float> gapRange (30.0f, 800.0f, 1.0f);
    gapRange.setSkewForCentre (200.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gap", 1 }, "Gap", gapRange, 180.0f,
        AudioParameterFloatAttributes().withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " ms"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "duck", 1 }, "Duck",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.9f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    NormalisableRange<float> bloomRange (20.0f, 2000.0f, 1.0f);
    bloomRange.setSkewForCentre (300.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bloom", 1 }, "Bloom", bloomRange, 250.0f,
        AudioParameterFloatAttributes().withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " ms"; })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "wetonly", 1 }, "Wet Only", false));

    return { p.begin(), p.end() };
}

bool AutoThrowProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void AutoThrowProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    monoScratch.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
}

void AutoThrowProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    // Entrée mono -> sortie stéréo : on duplique la voix sur les deux côtés
    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    autothrow::Params p;
    p.decaySec = pDecay->load();
    p.shimmer  = pShimmer->load();
    p.tone     = pTone->load();
    p.mix      = pMix->load();
    p.sensDb   = pSens->load();
    p.gapMs    = pGap->load();
    p.duck     = pDuck->load();
    p.bloomMs  = pBloom->load();
    p.freeze   = pFreeze->load() > 0.5f;
    p.wetOnly  = pWetOnly->load() > 0.5f;
    engine.setParams (p);

    float* L = buffer.getWritePointer (0);
    if (numOut >= 2)
    {
        engine.process (L, buffer.getWritePointer (1), numSamples);
    }
    else
    {
        // Piste mono : on traite en stéréo dans un buffer temporaire puis on somme
        if ((int) monoScratch.size() < numSamples) monoScratch.resize ((size_t) numSamples);
        std::copy (L, L + numSamples, monoScratch.begin());
        engine.process (L, monoScratch.data(), numSamples);
        for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + monoScratch[(size_t) i]);
    }

    meterThrow.store (engine.getThrowLevel());
    meterInput.store (engine.getInputEnv());
    meterVoice.store (engine.isVoiceActive());
}

void AutoThrowProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AutoThrowProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AutoThrowProcessor::createEditor()
{
    return new AutoThrowEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoThrowProcessor();
}
