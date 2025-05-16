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
    DBG("DynamicsDoctorProcessor Constructor - START");

    // Retrieve raw pointers to parameter values
    bypassParam = parameters.getRawParameterValue(ParameterIDs::bypass.getParamID());
    presetParam = parameters.getRawParameterValue(ParameterIDs::preset.getParamID());
    peakParam   = parameters.getRawParameterValue(ParameterIDs::peak.getParamID());
    lraParam    = parameters.getRawParameterValue(ParameterIDs::lra.getParamID());

    // Retrieve the AudioParameterBool object for resetLra
    resetLraParamObject = dynamic_cast<juce::AudioParameterBool*>(parameters.getParameter(ParameterIDs::resetLra.getParamID()));

    // --- Assertions to ensure parameters were found and correctly typed ---
    if (bypassParam == nullptr) {
        DBG("ERROR IN CONSTRUCTOR: bypassParam is nullptr!");
        DBG("Ensure 'ParameterIDs::bypass.getParamID()' (" << ParameterIDs::bypass.getParamID()
            << ") matches a parameter in createParameterLayout().");
        jassertfalse; // Halt in debug if null
    }
    if (presetParam == nullptr) {
        DBG("ERROR IN CONSTRUCTOR: presetParam is nullptr!");
        DBG("Ensure 'ParameterIDs::preset.getParamID()' (" << ParameterIDs::preset.getParamID()
            << ") matches a parameter in createParameterLayout().");
        jassertfalse;
    }
    if (peakParam == nullptr) {
        DBG("ERROR IN CONSTRUCTOR: peakParam is nullptr!");
        DBG("Ensure 'ParameterIDs::peak.getParamID()' (" << ParameterIDs::peak.getParamID()
            << ") matches a parameter in createParameterLayout().");
        jassertfalse;
    }
    if (lraParam == nullptr) {
        DBG("ERROR IN CONSTRUCTOR: lraParam is nullptr!");
        DBG("Ensure 'ParameterIDs::lra.getParamID()' (" << ParameterIDs::lra.getParamID()
            << ") matches a parameter in createParameterLayout().");
        jassertfalse;
    }
    if (resetLraParamObject == nullptr) {
        DBG("ERROR IN CONSTRUCTOR: resetLraParamObject is nullptr after dynamic_cast!");
        DBG("Ensure 'ParameterIDs::resetLra.getParamID()' (" << ParameterIDs::resetLra.getParamID()
            << ") matches a juce::AudioParameterBool in createParameterLayout().");
        jassertfalse; // Halt in debug if null
    }
    // A general check for the remaining critical ones (can be individual like above too)
    // jassert (presetParam != nullptr && peakParam != nullptr && lraParam != nullptr && "One or more essential parameters are null!");


    // Initialize atomic members (already initialized at declaration in .h, but explicit here is fine too)
    // currentPeak.store(ParameterDefaults::peak);       // Values are already set by member initialization
    // currentGlobalLRA.store(ParameterDefaults::lra);
    // currentStatus.store(DynamicsStatus::Ok);          // Or whatever initial logical status

    // Note: No need to do peakParam->store() or lraParam->store() here.
    // The APVTS parameters will take their default values as defined in createParameterLayout().
    // Your atomic members currentPeak and currentGlobalLRA are for your internal logic.

    // Add listeners for parameters you need to react to in parameterChanged
    DBG("Constructor: Adding parameter listeners...");
    parameters.addParameterListener(ParameterIDs::resetLra.getParamID(), this);
    parameters.addParameterListener(ParameterIDs::bypass.getParamID(), this);
    parameters.addParameterListener(ParameterIDs::preset.getParamID(), this);

    // Set initial status based on default parameter values (especially bypass)
    // This ensures the plugin starts in a consistent state.
    DBG("Constructor: Calling initial updateStatusBasedOnLRA().");
    updateStatusBasedOnLRA(currentGlobalLRA.load());
    
    // Check preset default validity
    jassert(ParameterDefaults::preset >= 0 && static_cast<size_t>(ParameterDefaults::preset) < presets.size() && "Default preset index is out of bounds!");

    DBG("DynamicsDoctorProcessor Constructor - END");
}

DynamicsDoctorProcessor::~DynamicsDoctorProcessor()
{
    DBG("DynamicsDoctorProcessor Destructor - START");
    // Remove listeners to prevent dangling pointers if the APVTS outlives this or if callbacks happen during destruction
    parameters.removeParameterListener(ParameterIDs::resetLra.getParamID(), this);
    parameters.removeParameterListener(ParameterIDs::bypass.getParamID(), this);
    parameters.removeParameterListener(ParameterIDs::preset.getParamID(), this);
    DBG("DynamicsDoctorProcessor Destructor - END");
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DynamicsDoctorProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
   // Bypass Parameter
    auto bypassAttributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction([](float v, int) { return v > 0.5f ? "Bypassed" : "Active"; });
    
   params.push_back (std::make_unique<juce::AudioParameterBool>(
            ParameterIDs::bypass,
            "bypass",
            ParameterDefaults::bypass,
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
            ParameterIDs::peak,
                "Peak Level",
                juce::NormalisableRange<float>(-100.0f, 6.0f),
                ParameterDefaults::peak,
                peakAttributes
                                                                               ));
   
        auto lraAttributes = juce::AudioParameterFloatAttributes()
                             .withStringFromValueFunction ([](float v, int) { return juce::String (v, 1) + " LU"; })
                             .withAutomatable (false);
   
    // LRA range might be 0 to 25 LU, for example. Adjust as needed.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
            ParameterIDs::lra,
            "Loudness Range",
            juce::NormalisableRange<float>(0.0f, 30.0f),
            ParameterDefaults::lra,
            lraAttributes
    ));
    
    // Reset Button Parameter (non-automatable, acts as a trigger)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
            ParameterIDs::resetLra,
            "resetLra",
            false,
            juce::AudioParameterBoolAttributes().withAutomatable(false)));

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
    // if (peakParam) peakParam->store(ParameterDefaults::peak); // redundant
    // if (lraParam) lraParam->store(ParameterDefaults::lra); // redundant

    // currentStatus.store(DynamicsStatus::Bypassed); // Start as bypassed
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
    
    // Check if the sample rate is valid before proceeding
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Ensure bypassParam is not null before dereferencing
    const bool bypassed = (bypassParam != nullptr) ? (bypassParam->load() > 0.5f) : false; // Default to not bypassed if param somehow null
    
    if (bypassed)
    {
        if (currentStatus.load() != DynamicsStatus::Bypassed)
        {
            currentStatus.store(DynamicsStatus::Bypassed);
            currentGlobalLRA.store(ParameterDefaults::lra);
            if (lraParam) lraParam->store(currentGlobalLRA.load());
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
        if (std::isinf(newLRA) || std::isnan(newLRA))
        {
            newLRA = 0.0f;
        }
        currentGlobalLRA.store(newLRA);
            
            
        if (lraParam != nullptr) lraParam->store(currentGlobalLRA.load());
            
        updateStatusBasedOnLRA(currentGlobalLRA.load());
        
        
    }
}


//==============================================================================
// --- Update Status Based on LRA ---
void DynamicsDoctorProcessor::updateStatusBasedOnLRA(float measuredLRA)
    {
        // Ensure parameters are not null before dereferencing
        if (bypassParam != nullptr && bypassParam->load() > 0.5f) { // Good: added nullptr check
            currentStatus.store(DynamicsStatus::Bypassed);
            return;
        }

        if (presetParam == nullptr) { // Good: added nullptr check
            currentStatus.store(DynamicsStatus::Ok);
            return;
        }
        
        const int presetIndex = static_cast<int>(presetParam->load());
        const int validPresetIndex = (presetIndex >= 0 && static_cast<size_t>(presetIndex) < presets.size())
                                     ? presetIndex
                                     : ParameterDefaults::preset;
        const auto& selectedPreset = presets[validPresetIndex];

        DynamicsStatus newStatus;
        if (measuredLRA < selectedPreset.lraThresholdRed) {
            newStatus = DynamicsStatus::Loss;
        } else if (measuredLRA < selectedPreset.lraThresholdAmber) {
            newStatus = DynamicsStatus::Reduced;
        } else {
            newStatus = DynamicsStatus::Ok;
        }
        currentStatus.store(newStatus);
    }
//============================================================================
// --- Reset LRA  ---
      
void DynamicsDoctorProcessor::handleResetLRA()
{
    DBG("    **********************************************************");
    DBG("    PROCESSOR: handleResetLRA() - Entered.");

    if (internalSampleRate <= 0.0) {
        DBG("    PROCESSOR: handleResetLRA() - ABORTING: Invalid internalSampleRate: " << internalSampleRate);
        DBG("    **********************************************************");
        return;
    }

    int numChannelsForMeter = getTotalNumOutputChannels();
    if (numChannelsForMeter == 0) {
        numChannelsForMeter = 2;
        DBG("    PROCESSOR: handleResetLRA() - numChannelsForMeter was 0, defaulted to 2.");
    }

    int blockSizeToUse = getBlockSize();
    if (blockSizeToUse == 0) {
        blockSizeToUse = 512;
        DBG("    PROCESSOR: handleResetLRA() - blockSizeToUse was 0, defaulted to 512.");
    }

    DBG("    PROCESSOR: handleResetLRA() - Calling loudnessMeter.prepare() with Rate: " << internalSampleRate
        << ", Channels: " << numChannelsForMeter << ", BlockSize: " << blockSizeToUse);
    loudnessMeter.prepare(internalSampleRate, numChannelsForMeter, blockSizeToUse);
    DBG("    PROCESSOR: handleResetLRA() - Returned from loudnessMeter.prepare().");

    float oldAtomicLRA = currentGlobalLRA.load();
    currentGlobalLRA.store(ParameterDefaults::lra);
    DBG("    PROCESSOR: handleResetLRA() - currentGlobalLRA (atomic) reset from " << oldAtomicLRA << " to " << ParameterDefaults::lra);

    if (lraParam) { // lraParam is your std::atomic<float>* linked to APVTS
        float oldParamLRA = lraParam->load();
        lraParam->store(ParameterDefaults::lra);
        DBG("    PROCESSOR: handleResetLRA() - lraParam (APVTS value) set from " << oldParamLRA << " to " << ParameterDefaults::lra);
    } else {
        DBG("    PROCESSOR: handleResetLRA() - WARNING: lraParam is null, cannot update APVTS LRA parameter value directly via pointer.");
    }

    samplesUntilLraUpdate = static_cast<int>(internalSampleRate);
    DBG("    PROCESSOR: handleResetLRA() - samplesUntilLraUpdate reset to " << samplesUntilLraUpdate);

    updateStatusBasedOnLRA(currentGlobalLRA.load());
    DBG("    PROCESSOR: handleResetLRA() - Exiting.");
    DBG("    **********************************************************");
}
void DynamicsDoctorProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    DBG("--------------------------------------------------------------");
    DBG("PROCESSOR: parameterChanged - Received ID: '" << parameterID
        << "', New Value: " << newValue << (newValue > 0.5f ? " (TRUE)" : " (FALSE)"));

    if (parameterID == ParameterIDs::resetLra.getParamID()) // Use your .getParamID()
    {
        DBG("PROCESSOR: parameterChanged - Matched ID: '" << ParameterIDs::resetLra.getParamID() << "'.");

        if (newValue > 0.5f) // If reset button is "pressed" (true)
        {
            DBG("PROCESSOR: resetLra new value is TRUE. Calling handleResetLRA().");
            handleResetLRA();

            DBG("PROCESSOR: Returned from handleResetLRA(). Attempting to set resetLra param back to FALSE (0.0f).");
            
            // Try this more direct way to set the AudioParameterBool back to false
            if (resetLraParamObject != nullptr) // Use the member variable if it's valid
            {
                resetLraParamObject->beginChangeGesture(); // Good practice
                resetLraParamObject->setValueNotifyingHost(0.0f);
                resetLraParamObject->endChangeGesture();   // Good practice
                DBG("PROCESSOR: resetLra (via resetLraParamObject) was set back to FALSE. Current bool state: "
                    << (resetLraParamObject->get() ? "TRUE" : "FALSE"));
            }
            else // Fallback if resetLraParamObject was somehow null, though we guard against it in constructor
            {
                auto* paramToReset = static_cast<juce::AudioParameterBool*>(parameters.getParameter(ParameterIDs::resetLra.getParamID()));
                if (paramToReset != nullptr) {
                    paramToReset->beginChangeGesture();
                    paramToReset->setValueNotifyingHost(0.0f);
                    paramToReset->endChangeGesture();
                    DBG("PROCESSOR: resetLra (via getParameter) was set back to FALSE. Current bool state: "
                        << (paramToReset->get() ? "TRUE" : "FALSE"));
                } else {
                    DBG("PROCESSOR: ERROR - resetLra parameter NOT FOUND to set it back to false!");
                    jassertfalse;
                }
            }
        }
        else
        {
            DBG("PROCESSOR: resetLra new value is FALSE. No action taken in this 'else' block.");
        }
    }
    // ... (rest of your parameterChanged for bypass and preset) ...
    else if (parameterID == ParameterIDs::preset.getParamID() || parameterID == ParameterIDs::bypass.getParamID())
    {
        DBG("PROCESSOR: parameterChanged - Matched ID: '" << parameterID << "'. Updating status ONLY.");
        updateStatusBasedOnLRA(currentGlobalLRA.load());
    }
    else
    {
        DBG("PROCESSOR: parameterChanged - ID '" << parameterID << "' was not resetLra, preset, or bypass.");
    }
    DBG("--------------------------------------------------------------");
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
