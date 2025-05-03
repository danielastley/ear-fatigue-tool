/*
  ==============================================================================

    PluginProcessor.cpp - Implementation file for the audio processing logic.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath> // For std::sqrt, std::abs, std::log10 (used via Decibels)

// Define static Parameter IDs from the header
const juce::StringRef EarfatiguetoolAudioProcessor::PARAM_BYPASS_ID = "bypass";
const juce::StringRef EarfatiguetoolAudioProcessor::PARAM_STANDARD_ID = "standard";

//==============================================================================
// Helper function implementation to create the parameter layout.
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EarfatiguetoolAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Add Bypass Parameter
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { PARAM_BYPASS_ID, 1 }, // Use defined ID
        "Bypass",                                 // Name
        false                                     // Default value
    ));

    // Add Mastering Standard Parameter (using Choice for text labels)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { PARAM_STANDARD_ID, 1 }, // Use defined ID
        "Standard",                                 // Name
        juce::StringArray { "High DR", "Medium DR", "Low DR" }, // Choices
        0                                           // Default index (High DR)
    ));

    return layout;
}

//==============================================================================
// Constructor: Initializes the APVTS.
//==============================================================================
EarfatiguetoolAudioProcessor::EarfatiguetoolAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       // Initialize APVTS using the helper function
       apvts (*this, nullptr, "Parameters", createParameterLayout())
#else
     : apvts (*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    DBG("EarfatiguetoolAudioProcessor CONSTRUCTOR finished.");
}

//==============================================================================
// Destructor
//==============================================================================
EarfatiguetoolAudioProcessor::~EarfatiguetoolAudioProcessor()
{
}

//==============================================================================
// Standard Plugin Info Methods (Implementation)
//==============================================================================
const juce::String EarfatiguetoolAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EarfatiguetoolAudioProcessor::acceptsMidi() const
{
    #if JucePlugin_WantsMidiInput
        return true;
    #else
        return false;
    #endif
        }

bool EarfatiguetoolAudioProcessor::producesMidi() const
{
    #if JucePlugin_ProducesMidiOutput
    return true;
    #else
    return false;
    #endif
}

bool EarfatiguetoolAudioProcessor::isMidiEffect() const
{
    #if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

        
double EarfatiguetoolAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EarfatiguetoolAudioProcessor::getNumPrograms()
{
    return 1;
}

int EarfatiguetoolAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EarfatiguetoolAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}
const juce::String EarfatiguetoolAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}
void EarfatiguetoolAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName);
}

//==============================================================================
// prepareToPlay: Called before playback starts.
//==============================================================================
void EarfatiguetoolAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
    // Reset levels perhaps?
    currentPeakDb.store(-100.0f);
    currentRmsDb.store(-100.0f);
    currentCrestFactorDb.store(0.0f);
    currentStatus.store(DynamicsStatus::Ok);
    DBG("EarfatiguetoolAudioProcessor::prepareToPlay called.");
}

//==============================================================================
// releaseResources: Called when playback stops.
//==============================================================================
void EarfatiguetoolAudioProcessor::releaseResources()
{
    // Nothing specific needed for now.
}

//==============================================================================
// isBusesLayoutSupported: Checks channel layouts.
//==============================================================================
#ifndef JucePlugin_PreferredChannelConfigurations
bool EarfatiguetoolAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts); return true;
  #else
    // Allow stereo input and output
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
        layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo())
        return true;
    else
        return false;
  #endif
}
#endif

//==============================================================================
// processBlock: Main audio processing callback.
//==============================================================================
void EarfatiguetoolAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals; // Optimization helper

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // --- 1. Get Parameter Values (Thread Safe) ---
    // Use getRawParameterValue for efficiency in audio thread. Convert bool > 0.5f.
    const bool isBypassed = *apvts.getRawParameterValue(PARAM_BYPASS_ID) > 0.5f;
    // Get the integer index of the selected choice parameter.
    const int standardIndex = (int) *apvts.getRawParameterValue(PARAM_STANDARD_ID); // 0=High, 1=Medium, 2=Low

    // --- 2. Clear Extra Output Channels ---
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // --- 3. Handle Bypass ---
    if (isBypassed)
    {
        // Reset levels if needed when bypassed
        // currentPeakDb.store(-100.0f);
        // currentRmsDb.store(-100.0f);
        // currentCrestFactorDb.store(0.0f);
        // currentStatus.store(DynamicsStatus::Ok);
        return; // Exit early, leaving input audio in buffer (pass-through)
    }

    // --- 4. Audio Analysis (if not bypassed) ---

    float blockPeakLinear = 0.0f;
    float blockSumSquare = 0.0f;
    auto numSamples = buffer.getNumSamples();
    int totalSamplesProcessed = numSamples * totalNumInputChannels; // Only stereo supported

    if (totalSamplesProcessed == 0) // Safety check if buffer is empty
    {
         currentPeakDb.store(-100.0f);
         currentRmsDb.store(-100.0f);
         currentCrestFactorDb.store(0.0f);
         currentStatus.store(DynamicsStatus::Ok);
         return; // Nothing to process
    }

    // Calculate Peak and SumSquare across all input channels (assuming stereo focus)
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getReadPointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentSample = channelData[sample];
            blockPeakLinear = std::max(blockPeakLinear, std::abs(currentSample));
            blockSumSquare += currentSample * currentSample;
        }
    }

    // Calculate RMS (linear)
    float blockRmsLinear = std::sqrt(blockSumSquare / totalSamplesProcessed);

    // Calculate Levels in dB
    float peakdB = juce::Decibels::gainToDecibels(blockPeakLinear, -100.0f);
    float rmsdB = juce::Decibels::gainToDecibels(blockRmsLinear, -100.0f);

    // Calculate Crest Factor (handle silence where RMS is very low)
    float crestFactor = 0.0f;
    if (rmsdB > -90.0f) // Only calculate if RMS is reasonably above noise floor
    {
        crestFactor = peakdB - rmsdB;
    }

    // Store results atomically for GUI thread
    currentPeakDb.store(peakdB);
    currentRmsDb.store(rmsdB);
    currentCrestFactorDb.store(crestFactor);

    // Determine Status based on selected standard and Crest Factor
    DynamicsStatus newStatus = DynamicsStatus::Ok; // Default to Ok
    float limitOk = 100.0f; // Initialize high
    float limitReduced = 100.0f;

    if (standardIndex == 0) // High DR
    {
        limitOk = highDrLimitOk;
        limitReduced = highDrLimitReduced;
    }
    else if (standardIndex == 1) // Medium DR
    {
        limitOk = medDrLimitOk;
        limitReduced = medDrLimitReduced;
    }
    else // Low DR (index 2)
    {
        limitOk = lowDrLimitOk;
        limitReduced = lowDrLimitReduced;
    }

    // Compare Crest Factor to thresholds
    if (crestFactor < limitReduced)
    {
        newStatus = DynamicsStatus::Loss;
    }
    else if (crestFactor < limitOk)
    {
        newStatus = DynamicsStatus::Reduced;
    }
    else
    {
        newStatus = DynamicsStatus::Ok;
    }

    currentStatus.store(newStatus); // Store the determined status


    // --- 5. Explicit Pass-Through ---
    // Since we only read the buffer, copy input to output to ensure sound passes through.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        if (channel < totalNumOutputChannels)
        {
            if (buffer.getReadPointer(channel) != buffer.getWritePointer(channel))
            {
                 buffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);
            }
        }
    }

    // --- Optional DBG for verification ---
     // DBG("Peak: " << peakdB << ", RMS: " << rmsdB << ", CF: " << crestFactor << ", Status: " << (int)newStatus);

}

//==============================================================================
// hasEditor: Returns true because we provide an editor.
//==============================================================================
bool EarfatiguetoolAudioProcessor::hasEditor() const
{
    return true; // We have an editor now
}

//==============================================================================
// createEditor: Creates the editor instance.
//==============================================================================
juce::AudioProcessorEditor* EarfatiguetoolAudioProcessor::createEditor()
{
    // Create the editor, passing 'this' processor instance to it
    return new EarfatiguetoolAudioProcessorEditor (*this);
}

//==============================================================================
// getStateInformation: Saves parameters using APVTS.
//==============================================================================
void EarfatiguetoolAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Get the current state from APVTS
    auto state = apvts.copyState();
    // Convert state to XML
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    // Save XML to binary memory block
    copyXmlToBinary (*xml, destData);
     DBG ("EarfatiguetoolAudioProcessor::getStateInformation called.");
}

//==============================================================================
// setStateInformation: Restores parameters using APVTS.
//==============================================================================
void EarfatiguetoolAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
     DBG ("EarfatiguetoolAudioProcessor::setStateInformation called.");
    // Get XML from binary memory block
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    // Check if XML is valid and has the correct tag name
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            // Restore the state into APVTS
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// createPluginFilter: Factory function called by host.
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
     DBG("createPluginFilter() called.");
    return new EarfatiguetoolAudioProcessor();
}
