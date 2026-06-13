// =============================================================================
//  PluginEditor.h  --  Custom UI for the OCTAPHONIC Vitalizer.
//
//  A compact control surface: a global strip (input / vitalize / air / output /
//  oversampling / bypass), the two crossover frequencies, and three identical
//  band strips (Low / Mid / High), each with Drive, Character, Gain and
//  Mute/Solo. Everything is wired to the APVTS via attachments.
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

#include "PluginProcessor.h"

// ----------------------------------------------------------------------------
// Bespoke look: dark panel with cyan / amber accents and a custom rotary.
class OctaphonicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OctaphonicLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    static const juce::Colour bg, panel, accentA, accentB, text, dim;
};

// ----------------------------------------------------------------------------
class OctaphonicAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit OctaphonicAudioProcessorEditor (OctaphonicAudioProcessor&);
    ~OctaphonicAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    // Build a labelled rotary knob bound to a parameter.
    juce::Slider& addKnob (const juce::String& paramID, const juce::String& label);
    juce::TextButton& addToggle (const juce::String& paramID, const juce::String& label);

    OctaphonicAudioProcessor& proc;
    OctaphonicLookAndFeel lnf;

    // Global
    juce::Slider* inputKnob = nullptr;
    juce::Slider* mixKnob   = nullptr;
    juce::Slider* airKnob   = nullptr;
    juce::Slider* outputKnob = nullptr;
    juce::ComboBox osBox;
    juce::TextButton* bypassBtn = nullptr;

    // Crossover
    juce::Slider* lowMidKnob  = nullptr;
    juce::Slider* midHighKnob = nullptr;

    // Bands
    struct BandStrip
    {
        juce::Slider*     drive = nullptr;
        juce::Slider*     character = nullptr;
        juce::Slider*     gain = nullptr;
        juce::TextButton* mute = nullptr;
        juce::TextButton* solo = nullptr;
        juce::Label       title;
    };
    BandStrip bands[octa::kNumBands];

    // Owned widgets + labels + attachments.
    std::vector<std::unique_ptr<juce::Slider>>     ownedSliders;
    std::vector<std::unique_ptr<juce::Label>>      ownedLabels;
    std::vector<std::unique_ptr<juce::TextButton>> ownedButtons;
    std::vector<std::pair<juce::Slider*, juce::Label*>> knobLabels;   // knob -> caption
    std::vector<std::unique_ptr<APVTS::SliderAttachment>>   sliderAtts;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>>   buttonAtts;
    std::unique_ptr<APVTS::ComboBoxAttachment>              osAtt;

    juce::Rectangle<int> globalArea, xoverArea, bandsArea, headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctaphonicAudioProcessorEditor)
};
