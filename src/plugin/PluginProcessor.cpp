// =============================================================================
//  PluginProcessor.cpp
// =============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace octa;

namespace
{
    // Logarithmic (centre-skewed) range for frequency controls.
    juce::NormalisableRange<float> logRange (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    const juce::StringArray kBandNames { "Low", "Mid", "High" };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
OctaphonicAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const ParameterID v1Input  { pid::inputGain,  1 };
    const ParameterID v1Output { pid::outputGain, 1 };

    auto dbAttr = [] (const char* unit) { return AudioParameterFloatAttributes().withLabel (unit); };

    // ---- Global ------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterFloat> (
        v1Input, "Input Gain",
        NormalisableRange<float> (range::gainMinDb, range::gainMaxDb, 0.01f),
        range::gainDefDb, dbAttr ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        v1Output, "Output Gain",
        NormalisableRange<float> (range::gainMinDb, range::gainMaxDb, 0.01f),
        range::gainDefDb, dbAttr ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::mix, 1 }, "Vitalize",
        NormalisableRange<float> (range::mixMin, range::mixMax, 0.001f),
        range::mixDef,
        AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)); })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::air, 1 }, "Air",
        NormalisableRange<float> (range::airMinDb, range::airMaxDb, 0.01f),
        range::airDefDb, dbAttr ("dB")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::oversampling, 1 }, "Oversampling",
        StringArray { "Off (1x)", "2x", "4x", "8x" }, kOversamplingDefaultIndex));

    auto bypass = std::make_unique<AudioParameterBool> (
        ParameterID { pid::bypass, 1 }, "Bypass", false);
    layout.add (std::move (bypass));

    // ---- Crossover ---------------------------------------------------------
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::xLowMid, 1 }, "Low/Mid Freq",
        logRange (range::lowMidMinHz, range::lowMidMaxHz, range::lowMidDefHz),
        range::lowMidDefHz, dbAttr ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::xMidHigh, 1 }, "Mid/High Freq",
        logRange (range::midHighMinHz, range::midHighMaxHz, range::midHighDefHz),
        range::midHighDefHz, dbAttr ("Hz")));

    // ---- Per band ----------------------------------------------------------
    for (int b = 0; b < kNumBands; ++b)
    {
        const juce::String n = kBandNames[b];

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { pid::drive[b], 1 }, n + " Drive",
            NormalisableRange<float> (range::driveMinDb, range::driveMaxDb, 0.01f),
            range::driveDefDb[b], dbAttr ("dB")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { pid::character[b], 1 }, n + " Character",
            NormalisableRange<float> (range::charMin, range::charMax, 0.001f),
            range::charDef[b],
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return v < 0.5f ? juce::String ("Even ") + juce::String (juce::roundToInt ((0.5f - v) * 200.0f)) + "%"
                                                     : juce::String ("Odd ")  + juce::String (juce::roundToInt ((v - 0.5f) * 200.0f)) + "%"; })));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { pid::bandGain[b], 1 }, n + " Gain",
            NormalisableRange<float> (range::gainMinDb, range::gainMaxDb, 0.01f),
            range::gainDefDb, dbAttr ("dB")));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { pid::mute[b], 1 }, n + " Mute", false));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { pid::solo[b], 1 }, n + " Solo", false));
    }

    return layout;
}

//==============================================================================
OctaphonicAudioProcessor::OctaphonicAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pInputGain  = apvts.getRawParameterValue (pid::inputGain);
    pOutputGain = apvts.getRawParameterValue (pid::outputGain);
    pMix        = apvts.getRawParameterValue (pid::mix);
    pAir        = apvts.getRawParameterValue (pid::air);
    pOversample = apvts.getRawParameterValue (pid::oversampling);
    pXLowMid    = apvts.getRawParameterValue (pid::xLowMid);
    pXMidHigh   = apvts.getRawParameterValue (pid::xMidHigh);

    for (int b = 0; b < kNumBands; ++b)
    {
        pDrive[b]    = apvts.getRawParameterValue (pid::drive[b]);
        pChar[b]     = apvts.getRawParameterValue (pid::character[b]);
        pBandGain[b] = apvts.getRawParameterValue (pid::bandGain[b]);
        pMute[b]     = apvts.getRawParameterValue (pid::mute[b]);
        pSolo[b]     = apvts.getRawParameterValue (pid::solo[b]);
    }

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::bypass));
}

//==============================================================================
void OctaphonicAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels(), 1);

    currentOsIndex = (int) *pOversample;
    engine.setOversamplingFactor (oversampleFactorForIndex (currentOsIndex));
    engine.prepare (sampleRate, samplesPerBlock, numCh);
    engine.setSettings (snapshotSettings());

    setLatencySamples (engine.latencySamples());
}

bool OctaphonicAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;
    return main == layouts.getMainInputChannelSet();
}

octa::Settings OctaphonicAudioProcessor::snapshotSettings() const
{
    Settings s;
    s.inputGainDb  = pInputGain->load();
    s.outputGainDb = pOutputGain->load();
    s.mix          = pMix->load();
    s.airDb        = pAir->load();
    s.bypass       = bypassParam != nullptr && bypassParam->get();
    s.lowMidHz     = pXLowMid->load();
    s.midHighHz    = pXMidHigh->load();

    for (int b = 0; b < kNumBands; ++b)
    {
        s.band[b].driveDb   = pDrive[b]->load();
        s.band[b].character = pChar[b]->load();
        s.band[b].gainDb    = pBandGain[b]->load();
        s.band[b].mute      = pMute[b]->load() > 0.5f;
        s.band[b].solo      = pSolo[b]->load() > 0.5f;
    }
    return s;
}

void OctaphonicAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // React to an oversampling change (re-allocates; tolerated as an occasional,
    // user-initiated event rather than per-block).
    const int osIndex = (int) *pOversample;
    if (osIndex != currentOsIndex)
    {
        currentOsIndex = osIndex;
        engine.setOversamplingFactor (oversampleFactorForIndex (osIndex));
        setLatencySamples (engine.latencySamples());
    }

    engine.setSettings (snapshotSettings());

    const int numCh = juce::jmin (totalOut, buffer.getNumChannels());
    engine.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());
}

//==============================================================================
juce::AudioProcessorEditor* OctaphonicAudioProcessor::createEditor()
{
    return new OctaphonicAudioProcessorEditor (*this);
}

void OctaphonicAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void OctaphonicAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OctaphonicAudioProcessor();
}
