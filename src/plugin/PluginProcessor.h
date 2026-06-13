// =============================================================================
//  PluginProcessor.h  --  JUCE AudioProcessor wrapper around OctaphonicEngine.
//
//  The processor owns the parameter tree (APVTS) and, every block, translates
//  the current parameter values into an octa::Settings snapshot that the
//  framework-independent DSP engine consumes.
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

#include "../dsp/OctaphonicEngine.h"
#include "../dsp/Parameters.h"

class OctaphonicAudioProcessor : public juce::AudioProcessor
{
public:
    OctaphonicAudioProcessor();
    ~OctaphonicAudioProcessor() override = default;

    // ---- AudioProcessor ----------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;   // keep the double overload visible
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OCTAPHONIC Vitalizer"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioParameterBool* getBypassParameter() const override { return bypassParam; }

    // ---- Public accessors used by the editor -------------------------------
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    octa::Settings snapshotSettings() const;

    octa::OctaphonicEngine engine;
    int currentOsIndex = octa::kOversamplingDefaultIndex;

    // Cached raw parameter pointers (avoids string lookups on the audio thread).
    std::atomic<float>* pInputGain   = nullptr;
    std::atomic<float>* pOutputGain  = nullptr;
    std::atomic<float>* pMix         = nullptr;
    std::atomic<float>* pAir         = nullptr;
    std::atomic<float>* pOversample  = nullptr;
    std::atomic<float>* pXLowMid     = nullptr;
    std::atomic<float>* pXMidHigh    = nullptr;

    std::array<std::atomic<float>*, octa::kNumBands> pDrive {};
    std::array<std::atomic<float>*, octa::kNumBands> pChar  {};
    std::array<std::atomic<float>*, octa::kNumBands> pBandGain {};
    std::array<std::atomic<float>*, octa::kNumBands> pMute  {};
    std::array<std::atomic<float>*, octa::kNumBands> pSolo  {};

    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctaphonicAudioProcessor)
};
