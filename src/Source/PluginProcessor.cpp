#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Constants.h" // Include constants defining presets, IDs, etc.
#include "LoudnessMeter.h"  // For our wrapper
#include <cmath> // For std::log10, std::sqrt
#include <algorithm> // For std::sort, std::max
#include <limits>   // For std::numeric_limits

//==============================================================================
/** Minimum duration required for reliable LRA measurement in seconds */
static const float LRA_MEASURING_DURATION_SECONDS = 12.0f;

//==============================================================================
DynamicsDoctorProcessor::DynamicsDoctorProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      ),
       parameters (*this, nullptr, juce::Identifier ("DynamicsDoctorParams"), createParameterLayout())
{
    DBG("DynamicsDoctorProcessor Constructor - START");

    // Initialize parameter pointers for real-time access
    presetParam = parameters.getRawParameterValue(ParameterIDs::preset.getParamID());
    peakParam   = parameters.getRawParameterValue(ParameterIDs::peak.getParamID());
    lraParam    = parameters.getRawParameterValue(ParameterIDs::lra.getParamID());
    resetLraParamObject = dynamic_cast<juce::AudioParameterBool*>(parameters.getParameter(ParameterIDs::resetLra.getParamID()));

    // Validate parameter initialization
    jassert(presetParam != nullptr && "Preset parameter not found in parameter layout");
    jassert(peakParam != nullptr && "Peak parameter not found in parameter layout");
    jassert(lraParam != nullptr && "LRA parameter not found in parameter layout");
    jassert(resetLraParamObject != nullptr && "Reset LRA parameter not found or wrong type");

    // Register parameter listeners for state changes
    DBG("Constructor: Adding parameter listeners...");
    parameters.addParameterListener(ParameterIDs::resetLra.getParamID(), this);
    parameters.addParameterListener(ParameterIDs::preset.getParamID(), this);

    // Verify preset default validity
    jassert(ParameterDefaults::preset >= 0 && static_cast<size_t>(ParameterDefaults::preset) < presets.size() 
            && "Default preset index is out of bounds");

    DBG("DynamicsDoctorProcessor Constructor - END");
}

DynamicsDoctorProcessor::~DynamicsDoctorProcessor()
{
    DBG("DynamicsDoctorProcessor Destructor - START");
    // Clean up parameter listeners
    parameters.removeParameterListener(ParameterIDs::resetLra.getParamID(), this);
    parameters.removeParameterListener(ParameterIDs::preset.getParamID(), this);
    DBG("DynamicsDoctorProcessor Destructor - END");
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DynamicsDoctorProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Create preset parameter with labels from presets array
    juce::StringArray presetLabels;
    for (const auto& p : presets)
        presetLabels.add (p.label);
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>(ParameterIDs::preset,
        "preset",
        presetLabels,
        ParameterDefaults::preset));

    // Create peak level parameter with dBFS display
    auto peakAttributes = juce::AudioParameterFloatAttributes()
                              .withStringFromValueFunction ([](float v, int) { return juce::String (v, 1) + " dBFS"; })
                              .withAutomatable (false);
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
            ParameterIDs::peak,
            "Peak Level",
            juce::NormalisableRange<float>(-100.0f, 6.0f),
            ParameterDefaults::peak,
            peakAttributes));

    // Create LRA parameter with LU display
    auto lraAttributes = juce::AudioParameterFloatAttributes()
                         .withStringFromValueFunction ([](float v, int) { return juce::String (v, 1) + " LU"; })
                         .withAutomatable (false);
   
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
            ParameterIDs::lra,
            "Loudness Range",
            juce::NormalisableRange<float>(0.0f, 30.0f),
            ParameterDefaults::lra,
            lraAttributes));
    
    // Create reset button parameter (non-automatable trigger)
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
    DBG("prepareToPlay - internalSampleRate: " << internalSampleRate
           << ", samplesPerBlock: " << samplesPerBlock);

    // Initialize loudness meter
    int numChannelsForMeter = getTotalNumOutputChannels();
    if (numChannelsForMeter == 0) numChannelsForMeter = 2;
    
    loudnessMeter.prepare(internalSampleRate, numChannelsForMeter, samplesPerBlock);
    DBG("LoudnessMeter prepared in prepareToPlay.");
    
    // Reset measurement state
    currentPeak.store(ParameterDefaults::peak);
    currentGlobalLRA.store(0.0f);
    if (lraParam) lraParam->store(currentGlobalLRA.load());

    samplesProcessedSinceReset.store(0);
    samplesUntilLraUpdate = 0; // Trigger first LRA calculation immediately
    currentStatus.store(DynamicsStatus::Measuring);
    DBG("prepareToPlay: currentStatus set to Measuring. samplesProcessedSinceReset = 0. LRA set to 0.");
}

void DynamicsDoctorProcessor::releaseResources()
{
}

bool DynamicsDoctorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Disable if input or output is disabled
    if (layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled() ||
        layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;

    // For non-MIDI effects, ensure input and output layouts match
    #if ! JucePlugin_IsMidiEffect
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // Only support mono and stereo
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
    
    // Get channel counts and clear excess channels
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Check for audio activity in the block
    bool isAudioPresentInBlock = false;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        if (buffer.getMagnitude(ch, 0, buffer.getNumSamples()) > 0.00001f)
        {
            isAudioPresentInBlock = true;
            break;
        }
    }
    
    // Handle bypass state
    const bool bypassed = (bypassParam != nullptr) ? (bypassParam->load() > 0.5f) : false;
    if (bypassed)
    {
        if (currentStatus.load() != DynamicsStatus::Bypassed)
        {
            currentStatus.store(DynamicsStatus::Bypassed);
            DBG("PROCESSOR::processBlock - Entered Bypassed state.");
        }
        return;
    }
    
    // Handle transition from bypassed state
    if (currentStatus.load() == DynamicsStatus::Bypassed)
    {
        DBG("PROCESSOR::processBlock - Transitioning FROM Bypassed state. Entering Measuring state.");
        double rateForPrepare = (internalSampleRate > 0.0) ? internalSampleRate : 44100.0;
        int chansForPrepare = (getTotalNumOutputChannels() > 0) ? getTotalNumOutputChannels() : 2;
        int blockForPrepare = (getBlockSize() > 0) ? getBlockSize() : 512;
        
        loudnessMeter.prepare(rateForPrepare, chansForPrepare, blockForPrepare);
        
        currentGlobalLRA.store(0.0f);
        if (lraParam) lraParam->store(0.0f);
        samplesProcessedSinceReset.store(0);
        samplesUntilLraUpdate = 0;
        currentStatus.store(DynamicsStatus::Measuring);
    }
    
    // Track peak level
    float blockMax = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch) {
        blockMax = std::max(blockMax, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    }
    currentPeak.store(juce::Decibels::gainToDecibels(blockMax, -std::numeric_limits<float>::infinity()));
    if (peakParam != nullptr) peakParam->store(currentPeak);
    
    // Process audio through loudness meter
    loudnessMeter.processBlock(buffer);
    
    // Update measurement duration counter
    if (currentStatus.load() == DynamicsStatus::Measuring && isAudioPresentInBlock)
    {
        samplesProcessedSinceReset.fetch_add(buffer.getNumSamples());
    }
    
    // Handle periodic LRA updates
    samplesUntilLraUpdate -= buffer.getNumSamples();
    if (samplesUntilLraUpdate <= 0)
    {
        double rate = (internalSampleRate > 0.0) ? internalSampleRate : 44100.0;
        samplesUntilLraUpdate += static_cast<int>(rate);
        
        DBG("PROCESSOR::processBlock - LRA Update Triggered.");
        
        // Get current LRA from meter
        float newLRA = loudnessMeter.getLoudnessRange();
        if (std::isinf(newLRA) || std::isnan(newLRA) || newLRA < 0)
        {
            DBG("PROCESSOR::processBlock - Invalid LRA value. Clamping to 0.0f.");
            newLRA = 0.0f;
        }
        
        // Update LRA values
        currentGlobalLRA.store(newLRA);
        if (lraParam != nullptr)
        {
            lraParam->store(newLRA);
        }
        DBG("PROCESSOR::processBlock - Updated LRA: " << newLRA);
        
        // Check if measurement period is complete
        const int minSamplesForReliableLRA = static_cast<int>(rate * LRA_MEASURING_DURATION_SECONDS);
        
        if (currentStatus.load() == DynamicsStatus::Measuring)
        {
            DBG("PROCESSOR::processBlock - Measuring State. Samples: " << samplesProcessedSinceReset.load() 
                << " / " << minSamplesForReliableLRA);
            
            if (samplesProcessedSinceReset.load() >= minSamplesForReliableLRA)
            {
                DBG("PROCESSOR::processBlock - Measurement complete. Updating status based on LRA: " << newLRA);
                updateStatusBasedOnLRA(newLRA);
            }
            else
            {
                DBG("PROCESSOR::processBlock - Still measuring. Current LRA: " << newLRA);
            }
        }
        else
        {
            DBG("PROCESSOR::processBlock - Updating status with new LRA: " << newLRA);
            updateStatusBasedOnLRA(newLRA);
        }
    }
}

//==============================================================================
void DynamicsDoctorProcessor::updateStatusBasedOnLRA(float measuredLRA)
{
    if (presetParam == nullptr) {
        currentStatus.store(DynamicsStatus::Ok);
        return;
    }
    
    // Get current preset and validate index
    const int presetIndex = static_cast<int>(presetParam->load());
    const int validPresetIndex = (presetIndex >= 0 && static_cast<size_t>(presetIndex) < presets.size())
                                 ? presetIndex
                                 : ParameterDefaults::preset;
    const auto& selectedPreset = presets[validPresetIndex];

    // Determine status based on LRA thresholds
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

//==============================================================================
void DynamicsDoctorProcessor::handleResetLRA()
{
    DBG("    **********************************************************");
    DBG("    PROCESSOR: handleResetLRA() - Entered.");

    // Validate sample rate
    if (internalSampleRate <= 0.0) {
        DBG("    PROCESSOR: handleResetLRA() - ABORTING: Invalid sample rate: " << internalSampleRate);
        DBG("    **********************************************************");
        return;
    }

    // Prepare loudness meter with current settings
    int numChannelsForMeter = getTotalNumOutputChannels();
    if (numChannelsForMeter == 0) {
        numChannelsForMeter = 2;
        DBG("    PROCESSOR: handleResetLRA() - Using default channel count: 2");
    }

    int blockSizeToUse = getBlockSize();
    if (blockSizeToUse == 0) {
        blockSizeToUse = 512;
        DBG("    PROCESSOR: handleResetLRA() - Using default block size: 512");
    }

    // Reset meter and measurement state
    loudnessMeter.prepare(internalSampleRate, numChannelsForMeter, blockSizeToUse);
    currentGlobalLRA.store(0.0f);
    if (lraParam) {
        lraParam->store(currentGlobalLRA.load());
    }

    samplesProcessedSinceReset.store(0);
    samplesUntilLraUpdate = 0;
    currentStatus.store(DynamicsStatus::Measuring);
    
    DBG("    PROCESSOR: handleResetLRA() - Reset complete.");
    DBG("    **********************************************************");
}

//==============================================================================
void DynamicsDoctorProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    DBG("--------------------------------------------------------------");
    DBG("PROCESSOR: parameterChanged - Parameter: '" << parameterID
        << "', Value: " << newValue << (newValue > 0.5f ? " (TRUE)" : " (FALSE)"));

    // Handle reset button
    if (parameterID == ParameterIDs::resetLra.getParamID())
    {
        if (newValue > 0.5f)
        {
            DBG("PROCESSOR: Reset triggered. Calling handleResetLRA()");
            handleResetLRA();
            
            // Reset button state
            if (resetLraParamObject != nullptr)
            {
                resetLraParamObject->beginChangeGesture();
                resetLraParamObject->setValueNotifyingHost(0.0f);
                resetLraParamObject->endChangeGesture();
            }
            else
            {
                auto* paramToReset = static_cast<juce::AudioParameterBool*>(
                    parameters.getParameter(ParameterIDs::resetLra.getParamID()));
                if (paramToReset != nullptr) {
                    paramToReset->beginChangeGesture();
                    paramToReset->setValueNotifyingHost(0.0f);
                    paramToReset->endChangeGesture();
                } else {
                    DBG("PROCESSOR: ERROR - Reset parameter not found!");
                    jassertfalse;
                }
            }
        }
    }
    // Handle preset changes
    else if (parameterID == ParameterIDs::preset.getParamID())
    {
        DBG("PROCESSOR: Preset changed. Resetting measurement.");
        handleResetLRA();
    }

    DBG("--------------------------------------------------------------");
}

//==============================================================================
// Standard AudioProcessor implementations
const juce::String DynamicsDoctorProcessor::getName() const { return JucePlugin_Name; }
bool DynamicsDoctorProcessor::acceptsMidi() const { return false; }
bool DynamicsDoctorProcessor::producesMidi() const { return false; }
bool DynamicsDoctorProcessor::isMidiEffect() const { return false; }
double DynamicsDoctorProcessor::getTailLengthSeconds() const { return 0.0; }
int DynamicsDoctorProcessor::getNumPrograms() { return 1; }
int DynamicsDoctorProcessor::getCurrentProgram() { return 0; }
void DynamicsDoctorProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String DynamicsDoctorProcessor::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void DynamicsDoctorProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }
bool DynamicsDoctorProcessor::hasEditor() const { return true; }

//==============================================================================
juce::AudioProcessorEditor* DynamicsDoctorProcessor::createEditor()
{
    return new DynamicsDoctorEditor (*this, parameters);
}

//==============================================================================
void DynamicsDoctorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DynamicsDoctorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
    if (xmlState != nullptr && xmlState->hasTagName (parameters.state.getType()))
    {
        DBG("setStateInformation: Loading state...");
        parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
        handleResetLRA();
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState& DynamicsDoctorProcessor::getValueTreeState() { return parameters; }
DynamicsStatus DynamicsDoctorProcessor::getCurrentStatus() const { return currentStatus.load(); }
float DynamicsDoctorProcessor::getReportedLRA() const { return currentGlobalLRA.load(); }

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DynamicsDoctorProcessor();
}
