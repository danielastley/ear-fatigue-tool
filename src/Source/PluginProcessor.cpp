/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
                       )
#endif
{
    // --- Our own bypass Parameter ---
    addParameter(bypassParameter = new juce::AudioParameterBool(
        "bypass",       // Parameter ID (Unique internal identifier)
        "Bypass",       // Parameter Name (shown in DAW)
        false));        // Default value (false = not bypassed)
    // --- End of own bypass Parameter ---
    
}

EarfatiguetoolAudioProcessor::~EarfatiguetoolAudioProcessor()
{
}

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
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EarfatiguetoolAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EarfatiguetoolAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EarfatiguetoolAudioProcessor::getProgramName (int index)
{
    return {};
}

void EarfatiguetoolAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EarfatiguetoolAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void EarfatiguetoolAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EarfatiguetoolAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void EarfatiguetoolAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // --- Get Bypass State ---
    const bool isBypassed = bypassParameter->get();
    // --- End of added line ---
    
        // ---[ Initial Buffer Clearing ]---
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // -- Add Bypass Check ---
    if (isBypassed)
    {
        // If bypassed, we simply do nothing further, ensuring pass-through.
        // The buffer already contains the input audio.
        return; // Exit processBlock early
    }
        // --- End of Bypass Check
    
    // ---[ Main Processing Loop Placeholder ]---
        // This code will only run if isBypassed is false.
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer (channel); // Gets a pointer to the audio samples for this channel
        // ..do something to the data...
    }
    // --- End of Placeholder ---
}

//==============================================================================
bool EarfatiguetoolAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EarfatiguetoolAudioProcessor::createEditor()
{
    return new EarfatiguetoolAudioProcessorEditor (*this);
}

//==============================================================================
void EarfatiguetoolAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EarfatiguetoolAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EarfatiguetoolAudioProcessor();
}
