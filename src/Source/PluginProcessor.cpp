/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout EarfatiguetoolAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 }, // Parameter ID (string and version)
        "Bypass",                          // Parameter Name
        false                              // Default value
    ));

    // We will add more parameters here later (e.g., standard selection)

    return layout;
}

EarfatiguetoolAudioProcessor::EarfatiguetoolAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
        // --- Initializer list starts here ---
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
        // Add the APVTS initialization
        apvts (*this, nullptr, "Parameters", createParameterLayout())
        // --- Initializer list ends here ---
#else
        // Alternative Initializer list starts here (if
        // JucePlugin_PreferredChannelConfigurations is defined)
        : apvts (*this, nullptr, "Parameters", createParameterLayout())
        // Alternative Initializer list ends here --
#endif

{
    // Constructor body is now likely empoty for basic parameter setup,
    // as initialization happens in the initializer list above
           
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

    // --- Get Bypass State
        // Note: Using getRawParameterValue is efficient in the audio thread.
        // It returns an atomic float*, so we dereference and compare.
        const bool isBypassed = *apvts.getRawParameterValue("bypass") > 0.5f;
        // --- End Modify ---
    
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
    // Use the APVTS to easily store the state
      auto state = apvts.copyState();
      std::unique_ptr<juce::XmlElement> xml (state.createXml());
      copyXmlToBinary (*xml, destData);
}

void EarfatiguetoolAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Use the APVTS to easily restore the state
       std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

       if (xmlState.get() != nullptr)
           if (xmlState->hasTagName (apvts.state.getType()))
               apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EarfatiguetoolAudioProcessor();
}
