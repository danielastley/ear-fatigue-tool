#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Constants.h" // Include constants defining presets, IDs, etc.
#include "LoudnessMeter.h"  // For our wrapper
#include <cmath> // For std::log10, std::sqrt
#include <algorithm> // For std::sort, std::max
#include <limits>   // For std::numeric_limits

//==============================================================================
DynamicsDoctorProcessor::DynamicsDoctorProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      ),
       parameters (*this, nullptr, juce::Identifier ("DynamicsDoctorParams"), createParameterLayout())
{
   // Retrieve bypass parameter. If not found, throw and break in debug builds.
    bypassParam = parameters.getRawParameterValue (ParameterIDs::bypass.getParamID());
        
        if (bypassParam == nullptr)
        {
            DBG("Error: bypassParam is null. Check ParameterIDs::bypass or createParameterLayout().");
            jassertfalse;
            throw std::runtime_error("bypassParam could not be initialized.");
        }

        // Initialize other parameters
    presetParam = parameters.getRawParameterValue (ParameterIDs::preset.getParamID());
    peakParam   = parameters.getRawParameterValue (ParameterIDs::peak.getParamID());
    lraParam    = parameters.getRawParameterValue (ParameterIDs::lra.getParamID());

    // Get the actual AudioParameterBool object for resetLra for direct manipulation if needed,
    // but we will primarily use the parameterChanged callback
    resetLraParamObject = dynamic_cast<juce::AudioParameterBool*>(parameters.getParameter(ParameterIDs::resetLra.getParamID()));
    
    // Ensure pointers are valid (good practice)
    jassert (presetParam != nullptr);
    jassert (peakParam   != nullptr);
    jassert (lraParam    != nullptr);
    // jassert (resetLraPramObject != nullptr);     // Optional: assert it's found
    
    

    // Initialize LRA values
    currentPeak.store(ParameterDefaults::peak);
    currentGlobalLRA.store(ParameterDefaults::lra);
    if (peakParam) peakParam->store(ParameterDefaults::peak);
    if (lraParam) lraParam->store(ParameterDefaults::lra);
    
    // Add listner for the reset parameter
    parameters.addParameterListener(ParameterIDs::resetLra.getParamID(), this);

    updateStatusBasedOnLRA(currentGlobalLRA.load()); // Initial status
}

DynamicsDoctorProcessor::~DynamicsDoctorProcessor()
{
    parameters.removeParameterListener(ParameterIDs::resetLra.getParamID(), this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DynamicsDoctorProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
   
    // Bypass Parameter
    auto bypassAttributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction([](float v, int) { return v > 0.5f ? "Bypassed" : "Active"; });
    
   
    params.push_back (std::make_unique<juce::AudioParameterBool>(ParameterIDs::bypass, "bypass",
        false,
        bypassAttributes
                                                        
                                                                 ));
    
    // Preset Parameter
    juce::StringArray presetLabels;
    for (const auto& p : presets)
        presetLabels.add (p.label);
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>(ParameterIDs::preset,
        "preset",
        presetLabels,
        ParameterDefaults::preset));

    // Reporting Parameters
    auto peakAttributes = juce::AudioParameterFloatAttributes()
                              .withStringFromValueFunction ([](float v, int) { return juce::String (v, 1) + " dBFS"; })
                              .withAutomatable (false);
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
            ParameterIDs::peak, // <--- CHANGE HERE
                "peak",
                juce::NormalisableRange<float>(-100.0f, 6.0f),
                ParameterDefaults::peak,
                peakAttributes
                                                                               ));
   
    
    auto lraAttributes = juce::AudioParameterFloatAttributes()
                             .withStringFromValueFunction ([](float v, int) { return juce::String (v, 1) + " LU"; })
                             .withAutomatable (false);
    // LRA range might be 0 to 25 LU, for example. Adjust as needed.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(ParameterIDs::lra, "Loudness Range", juce::NormalisableRange<float>(0.0f, 30.0f), ParameterDefaults::lra, lraAttributes));
    
    // Reset Button Parameter (non-automatable, acts as a trigger)
    params.push_back(std::make_unique<juce::AudioParameterBool>(ParameterIDs::resetLra, "resetLra", false, juce::AudioParameterBoolAttributes().withAutomatable(false)));

    return { params.begin(), params.end() };
}

//==============================================================================
void DynamicsDoctorProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    DBG("--- PREPARE TO PLAY ---");
    internalSampleRate = newSampleRate;
    samplesUntilLraUpdate = static_cast<int>(internalSampleRate); // Trigger update after 1st second

    // Prepare our LoudnessMeter wrapper with number of channels from the main output bus
    // It's important to use the actual number of channels the processor will output
    // as libebur128 needs to be initialized for that specific channel count.
    int numChannelsForMeter = getTotalNumOutputChannels();
    if (numChannelsForMeter == 0) numChannelsForMeter = 2; // Fallback if buses not ready
    
    loudnessMeter.prepare(internalSampleRate, numChannelsForMeter, samplesPerBlock);
    DBG("LoudnessMeter prepared in prepareToPlay.");
    
    // Reset internal state
    currentPeak.store(ParameterDefaults::peak);
    currentGlobalLRA.store(ParameterDefaults::lra); // Reset LRA
    if (peakParam) peakParam->store(ParameterDefaults::peak);
    if (lraParam) lraParam->store(ParameterDefaults::lra); // Update reporting param

    currentStatus.store(DynamicsStatus::Bypassed); // Start as bypassed
}

void DynamicsDoctorProcessor::releaseResources()
{
}

bool DynamicsDoctorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled() ||
        layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;
   #if ! JucePlugin_IsMidiEffect
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    const auto& mainInputSet = layouts.getMainInputChannelSet();

    if (mainInputSet != juce::AudioChannelSet::mono() && mainInputSet != juce::AudioChannelSet::stereo())
        return false;
   #endif
   return true;
}

//==============================================================================
void DynamicsDoctorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // --- COMMENT OUT OR MODIFY BYPASS CHECK ---
    const bool bypassed = bypassParam->load() > 0.5f;
    // const bool bypassed = false; // For testing, assume not bypassed OR handle bypassParam being null
    // Or, more safely for this test, just remove the bypass logic entirely for a moment

    if (bypassed)
    {
         if (currentStatus.load() != DynamicsStatus::Bypassed)
         {
             currentStatus.store(DynamicsStatus::Bypassed);
             currentGlobalLRA.store(ParameterDefaults::lra);
             // if (lraParam) lraParam->store(currentGlobalLRA.load());
         }
        return; // Keep the early return for bypassed state if you hardcode `bypassed`
    }
     if (currentStatus.load() == DynamicsStatus::Bypassed)
     {
        updateStatusBasedOnLRA(currentGlobalLRA.load()); // Relies on parameters
     }
    
    // --- Peak Tracking --- (This part uses `currentPeak` (atomic) so it's mostly fine)
    float blockMax = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch) {
        blockMax = std::max(blockMax, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    }
    currentPeak.store(juce::Decibels::gainToDecibels(blockMax, -std::numeric_limits<float>::infinity()));
    
     if (peakParam != nullptr) peakParam->store(currentPeak);

    // --- Feed audio to LoudnessMeter --- (Assumed fine for now)
    loudnessMeter.processBlock(buffer);
    
    samplesUntilLraUpdate -= buffer.getNumSamples();
    if (samplesUntilLraUpdate <=0)
    {
        samplesUntilLraUpdate += static_cast<int>(internalSampleRate);
        
        float newLRA = loudnessMeter.getLoudnessRange();
        if (std::isinf(newLRA) || std::isnan(newLRA)) {
            newLRA = 0.0f;
        }
        currentGlobalLRA.store(newLRA);

        
         if (lraParam != nullptr) lraParam->store(currentGlobalLRA.load());

          updateStatusBasedOnLRA(currentGlobalLRA.load());
    }
}

//==============================================================================
void DynamicsDoctorProcessor::updateStatusBasedOnLRA(float measuredLRA)
{
    if (bypassParam->load() > 0.5f) {
        currentStatus.store(DynamicsStatus::Bypassed);
        return;
    }

    const int presetIndex = static_cast<int>(presetParam->load());
    // Ensure presetIndex is valid for the `presets` vector
    const int validPresetIndex = (presetIndex >= 0 && static_cast<size_t>(presetIndex) < presets.size())
                                 ? presetIndex
                                 : ParameterDefaults::preset;
    const auto& selectedPreset = presets[validPresetIndex];

    DynamicsStatus newStatus;
    if (measuredLRA < selectedPreset.lraThresholdRed) {
        newStatus = DynamicsStatus::Loss; // Red
    } else if (measuredLRA < selectedPreset.lraThresholdAmber) {
        newStatus = DynamicsStatus::Reduced; // Amber
    } else {
        newStatus = DynamicsStatus::Ok; // Green
    }
    currentStatus.store(newStatus);
}

// --- Reset LRA Handling ---
void DynamicsDoctorProcessor::parameterChanged(const juce::String& parameterID, float newValue) 
{
    DBG("Parameter Changed ID = " << parameterID << ", New Value = " << newValue);
    if (parameterID == ParameterIDs::resetLra.getParamID())
    {
        DBG("ResetLRA Parameter Changed. New Value: " << newValue);
        if (newValue > 0.5f) // If reset button is "pressed" (true)
        {
            DBG("ResetLRA parameter is TRUE, calling handleResetLRA()");
            handleResetLRA();
            
            juce::Value vtsParam = parameters.getParameterAsValue(ParameterIDs::resetLra.getParamID());
                        vtsParam.setValue(0.0f); // Set the parameter back to false
           DBG("ResetLRA parameter set back to false.");
                // Or parameters.getParameterAsValue(ParameterIDs::resetLra.getParamID()).setValue(0.0f);
        }
    
    }
        else if (parameterID == ParameterIDs::preset.getParamID() || parameterID == ParameterIDs::bypass.getParamID())
    {
        DBG("Preset or Bypass Parameter Changed. Updating status.");
         // If preset or bypass changes, immediately update status
        updateStatusBasedOnLRA(currentGlobalLRA.load());
    }
}

void DynamicsDoctorProcessor::handleResetLRA()
{
    DBG("--- HANDLE RESET LRA CALLED ---");
    // Re-initialize libebur128 via our wrapper
    // This effectively resets its entire history for LRA calculation
    int numChannelsForMeter = getTotalNumOutputChannels();
    if (numChannelsForMeter == 0) numChannelsForMeter = 2;
    int currentBlockSize = getBlockSize(); // Get current block size
    if (currentBlockSize == 0) currentBlockSize = 512;
    
      loudnessMeter.prepare(internalSampleRate, numChannelsForMeter, currentBlockSize); // Using a typical block size
     DBG("LoudnessMeter prepared in handleResetLRA.");

    // Reset our reported LRA values
    currentGlobalLRA.store(ParameterDefaults::lra); // Or 0.0f
    if (lraParam) lraParam->store(currentGlobalLRA.load());
    DBG("currentGlobalLRA and lraParam reset to default: " << ParameterDefaults::lra);
    
    samplesUntilLraUpdate = static_cast<int>(internalSampleRate); // Reset timer for next update
    DBG("samplesUntilLraUpdate reset.");
    // Update status immediately based on the reset LRA (likely Green if default LRA is high)
    updateStatusBasedOnLRA(currentGlobalLRA.load());
    DBG("Status updated after reset. New status: " << (int)currentStatus.load());
    // DBG("LRA Rese_t Called"); // For debugging
}


// --- Standard AudioProcessor Methods (getName, acceptsMidi, etc.) ---
 const juce::String DynamicsDoctorProcessor::getName() const { return JucePlugin_Name; }
bool DynamicsDoctorProcessor::acceptsMidi() const { return false; }
bool DynamicsDoctorProcessor::producesMidi() const { return false; }
bool DynamicsDoctorProcessor::isMidiEffect() const { return false; }
double DynamicsDoctorProcessor::getTailLengthSeconds() const { return 0.0; }
int DynamicsDoctorProcessor::getNumPrograms() { return 1; }
int DynamicsDoctorProcessor::getCurrentProgram() { return 0; }
void DynamicsDoctorProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); updateStatusBasedOnLRA(currentGlobalLRA.load()); }
 const juce::String DynamicsDoctorProcessor::getProgramName (int index) { juce::ignoreUnused (index);
    return {}; }
     
void DynamicsDoctorProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

     bool DynamicsDoctorProcessor::hasEditor() const
{
    return true;
    
 }
//=== Create Editor ===
 juce::AudioProcessorEditor* DynamicsDoctorProcessor::createEditor()
     {
         return new DynamicsDoctorEditor (*this, parameters);
         }
         
//======================================================================
void DynamicsDoctorProcessor::getStateInformation (juce::MemoryBlock& destData) {
    
     auto state = parameters.copyState(); // Get the current state from APVTS
     std::unique_ptr<juce::XmlElement> xml (state.createXml());
     copyXmlToBinary (*xml, destData);

}

void DynamicsDoctorProcessor::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName (parameters.state.getType())) // Check if it's our ValueTree
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
            
            // Your existing logic for after state is loaded:
            // Ensure LRA is reset and status reflects new preset/bypass.
            // It's generally better to do this in response to parameter changes
            // triggered by replaceState, or in a separate update call if needed.
            // However, let's keep your logic for now and see if the primary issue is resolved.
            // Consider if handleResetLRA() should be called if only a preset changed
            // without the whole project state being reloaded.
            handleResetLRA(); // Reset LRA as history isn't saved
            updateStatusBasedOnLRA(currentGlobalLRA.load());
            updateStatusBasedOnLRA(currentGlobalLRA.load());
        }
    }
}

 juce::AudioProcessorValueTreeState& DynamicsDoctorProcessor::getValueTreeState() { return parameters; }
 DynamicsStatus DynamicsDoctorProcessor::getCurrentStatus() const { return currentStatus.load(); }
 float DynamicsDoctorProcessor::getReportedLRA() const { return currentGlobalLRA.load(); }
     
//=== Creates a new instance of the plugin =========================
     juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
     {
     return new DynamicsDoctorProcessor();
        }
