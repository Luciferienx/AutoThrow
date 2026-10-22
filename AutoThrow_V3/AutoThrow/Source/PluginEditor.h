#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Palette "hi-fi années 80" : façade alu sombre, chrome, écran ambré, accents orange / rouge
namespace ui
{
    const juce::Colour face      { 0xff1b1815 };
    const juce::Colour faceLo    { 0xff110f0d };
    const juce::Colour chromeHi  { 0xffeeebe5 };
    const juce::Colour chromeMid { 0xffb5b0a8 };
    const juce::Colour chromeLo  { 0xff6f6a63 };
    const juce::Colour glass     { 0xff0b0805 };
    const juce::Colour amber     { 0xffffab3d };
    const juce::Colour amberDim  { 0xff3d2610 };
    const juce::Colour orange    { 0xffff6a13 };
    const juce::Colour red       { 0xffe5261b };
    const juce::Colour text      { 0xffe9dfd2 };
    const juce::Colour textDim   { 0xff8c8174 };
    const juce::Colour wood      { 0xff5b3119 };
    const juce::Colour woodDark  { 0xff311708 };
}

class ThrowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ThrowLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // Touche mécanique façon chaîne hi-fi, avec sa LED
    static void drawKey (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text,
                         bool on, bool highlighted, juce::Colour ledColour);
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
    void syncTypeButtons();
    void renderStaticBackground();
    void drawDisplay (juce::Graphics&);
    void drawSpeaker (juce::Graphics&);

    AutoThrowProcessor& proc;
    ThrowLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    Knob decay, shimmer, tone, mix, sens, gap, duck, bloom;
    juce::ToggleButton freezeBtn { "FREEZE" }, wetOnlyBtn { "WET ONLY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach, wetOnlyAttach;
    juce::TextButton typeBtns[4];
    juce::RangedAudioParameter* typeParam = nullptr;

    juce::Image background;
    juce::Rectangle<int> displayArea, speakerArea, keysArea, knobsArea, reverbGroup, detectGroup;
    float throwSm = 0.0f, inputSm = 0.0f, needlePos = 0.0f;
    bool voiceOn = false;
    int typeIndex = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoThrowEditor)
};
