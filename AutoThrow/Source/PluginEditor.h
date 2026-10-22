#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg       { 0xff0c0c10 };
    const juce::Colour panel    { 0xff15151b };
    const juce::Colour panelHi  { 0xff1d1d25 };
    const juce::Colour track    { 0xff2a2a34 };
    const juce::Colour text     { 0xffe8e8ee };
    const juce::Colour textDim  { 0xff7c7c8a };
    const juce::Colour accent   { 0xffff2a4a };   // rouge "throw"
    const juce::Colour accentLo { 0xff8a1022 };
    const juce::Colour ice      { 0xff6fd3ff };   // bleu "freeze"
}

class ThrowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ThrowLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class AutoThrowEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AutoThrowEditor (AutoThrowProcessor&);
    ~AutoThrowEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, const juce::String& tip);

    AutoThrowProcessor& proc;
    ThrowLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    Knob decay, shimmer, tone, mix, sens, gap, duck, bloom;
    juce::ToggleButton freezeBtn { "FREEZE" }, wetOnlyBtn { "WET ONLY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach, wetOnlyAttach;

    juce::Rectangle<int> reverbArea, detectArea, sideArea, ledArea, throwMeterArea, inputMeterArea;
    float throwSm = 0.0f, inputSm = 0.0f;
    bool voiceOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoThrowEditor)
};
