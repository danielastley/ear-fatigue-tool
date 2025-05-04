#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DynamicsDoctorProcessor::DynamicsDoctorProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      ),
       parameters (*this, nullptr, juce::Identifier ("DynamicsDoctorParams"), createParameterLayout()) // Use helper function
{
    // Get pointers to the atomic parameters for real-time access
    // It's good practice to assert that they are not null after creation
    bypassParam = parameters.getRawParameterValue (ParameterIDs::bypass);
    presetParam = parameters.getRawParameterValue (ParameterIDs::preset);
    peakParam   = parameters.getRawParameterValue (ParameterIDs::peak);
    lufsParam   = parameters.getRawParameterValue (ParameterIDs::lufs);

    jassert (bypassParam != nullptr);
    jassert (presetParam != nullptr);
    jassert (peakParam   != nullptr);
    jassert (lufsParam   != nullptr);

    // No Parameter Listeners needed here anymore, updates are handled in processBlock

    // Initial status update based on defaults (safe to call here before audio runs)
    updateStatus();
}

DynamicsDoctorProcessor::~DynamicsDoctorProcessor()
{
    // No listeners to remove
}

// Helper function to create the parameter layout (cleaner constructor)
juce::AudioProcessorValueTreeState::ParameterLayout DynamicsDoctorProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Bypass Parameter
    params.push_back (std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ParameterIDs::bypass, 1 },
                                                                 "Bypass",
                                                                 ParameterDefaults::bypass));
    // Preset Parameter
    params.push_back (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ParameterIDs::preset, 1 },
                                                                   "Preset",
                                                                   juce::StringArray { presets[0].label, presets[1].label, presets[2].label },
                                                                   ParameterDefaults::preset));

    // Reporting Parameters (Not automated, just for state display)
    // Using AudioParameterFloat is standard, even for reporting.
    // Mark them as non-automatable if they shouldn't appear in host automation lanes.
    auto peakAttributes = juce::AudioParameterFloatAttributes()
                              .withStringFromValueFunction ([](float v, int) { return juce::String (v, 1) + " dBFS"; })
                              .withAutomatable (false); // Indicate this is for reporting, not automation

    params.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParameterIDs::peak, 1 },
                                                                  "Peak Level",
                                                                  juce::NormalisableRange<float>(-100.0f, 6.0f), // Allow slightly above 0 for true peak
                                                                  -100.0f,
                                                                  peakAttributes));


    auto lufsAttributes = juce::AudioParameterFloatAttributes()
                              .withStringFromValueFunction ([](float v, int) { return juce::String (v, 1) + " LUFS"; })
                              .withAutomatable (false); // Indicate this is for reporting, not automation

    params.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParameterIDs::lufs, 1 },
                                                                  "LUFS Integrated",
                                                                   juce::NormalisableRange<float>(-100.0f, 0.0f), // LUFS usually <= 0
                                                                   -100.0f,
                                                                   lufsAttributes));

    return { params.begin(), params.end() };
}


//==============================================================================
const juce::String DynamicsDoctorProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DynamicsDoctorProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DynamicsDoctorProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DynamicsDoctorProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DynamicsDoctorProcessor::getTailLengthSeconds() const
{
    return 0.0; // Assuming no tail for this analysis plugin
}

int DynamicsDoctorProcessor::getNumPrograms()
{
    // APVTS generally handles preset management better than old program methods.
    // Return 1 if you must, but rely on APVTS state saving/loading.
    return 1;
}

int DynamicsDoctorProcessor::getCurrentProgram()
{
    return 0;
}

void DynamicsDoctorProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
    // Program changes are typically handled via parameter state restoration
}

const juce::String DynamicsDoctorProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {}; // Use APVTS for preset naming
}

void DynamicsDoctorProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
    // Use APVTS for preset management
}

//==============================================================================
void DynamicsDoctorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback initialisation
    juce::ignoreUnused (sampleRate, samplesPerBlock);

    // Reset analysis variables here
    currentPeak = -100.0f;
    currentLufs = -100.0f;
    // Reset any internal buffers or states for your peak/LUFS calculation

    // Consider preparing smoothing filters/classes for reporting parameters here if needed.
}

void DynamicsDoctorProcessor::releaseResources()
{
    // When playback stops, free up memory allocated in prepareToPlay.
}

bool DynamicsDoctorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Prevent UAD console from crashing (per JUCE forum recommendations)
    // Ensure input and output counts match unless it's specifically designed otherwise
    if (layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled() ||
        layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;

    // This plugin works with mono or stereo inputs/outputs, keeping the same layout.
   #if ! JucePlugin_IsMidiEffect
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // Allow mono or stereo
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #endif

    return true;
}

void DynamicsDoctorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; // Ensure no denormal numbers slow down processing
    juce::ignoreUnused(midiMessages);    // We don't process MIDI in this plugin

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any excess output channels not fed by input channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // --- Get Current Parameter Values (Thread Safe Reads) ---
    // Read atomic values directly. These are guaranteed to be up-to-date from host/UI changes.
    const bool bypassed = bypassParam->load() > 0.5f; // load() is explicit for atomics

    // --- Bypass Logic ---
    if (bypassed)
    {
        // If bypassed, reset internal state and potentially update status immediately
        if (currentStatus != DynamicsStatus::Bypassed)
        {
            currentPeak = -100.0f;
            currentLufs = -100.0f;
            currentStatus = DynamicsStatus::Bypassed;

            // Update reporting parameters to reflect bypass state
            // Using store() is explicit for atomics. Non-notifying updates are fine
            // as the host isn't automating these.
            peakParam->store (currentPeak);
            lufsParam->store (currentLufs);
        }
        // No further processing needed if bypassed
        return;
    }

    // --- Audio Analysis ---
    // ** PLACEHOLDER: Perform actual audio analysis here **
    // This is where you would integrate your DSP code to calculate:
    // 1. True Peak (dBFS) - Consider oversampling.
    // 2. Integrated LUFS - Implement an EBU R128 compliant meter.

    // --- Simulated Analysis for Prototype (Replace with real analysis) ---
    float maxPeakThisBlock = -100.0f;
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        // Use getMagnitude() for potentially faster block-level peak calculation
        maxPeakThisBlock = juce::jmax (maxPeakThisBlock, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    }
    // Convert linear gain (0-1) to dBFS, using a floor value
    float peakDbThisBlock = juce::Decibels::gainToDecibels (maxPeakThisBlock, -100.0f);

    // Update overall peak (simple max hold with slow decay placeholder)
    if (peakDbThisBlock > currentPeak)
        currentPeak = peakDbThisBlock;
    else
        // Simple decay for the placeholder peak
        // Use sampleRate from prepareToPlay if needed for accurate decay timing
        currentPeak = juce::jmax (-100.0f, currentPeak - 0.05f * (float)buffer.getNumSamples() / 44100.0f); // Placeholder decay

    // Simulate LUFS (extremely basic placeholder - replace with real R128 meter)
    currentLufs += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.1f; // Random walk
    currentLufs = juce::jlimit (-40.0f, -5.0f, currentLufs);                   // Clamp
    // --- End Simulated Analysis ---


    // --- Update Reporting Parameters ---
    // Update the atomic parameters directly from the audio thread.
    // The UI / host can read these atomically when needed.
    // No need to notify host as these are marked non-automatable.
    peakParam->store (currentPeak);
    lufsParam->store (currentLufs);

    // --- Update Dynamics Status ---
    // This is called every block, ensuring status reflects current params and analysis
    updateStatus();

    // --- Pass Audio Through ---
    // This plugin currently only analyses, it doesn't modify the buffer's content.
    // If modifying, do it here after analysis (or before, depending on goal).
}

//==============================================================================


juce::AudioProcessorEditor* DynamicsDoctorProcessor::createEditor()
{
    // Pass the processor instance and the APVTS reference to the editor
    return new DynamicsDoctorEditor (*this, parameters);
}

//==============================================================================
void DynamicsDoctorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Use APVTS to handle state saving automatically
    juce::MemoryOutputStream stream(destData, false);
    parameters.state.writeToStream (stream);
}

void DynamicsDoctorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Use APVTS to handle state loading automatically
    juce::ValueTree tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
    {
        parameters.replaceState (tree);

        // Re-calculate the status based on the newly loaded parameters AFTER state is restored.
        // This ensures the initial state after loading is correct, without needing a listener.
        // It's safe to call updateStatus here as it reads atomics now.
        updateStatus();
    }
}

//==============================================================================
// Parameter listener callback is NO LONGER NEEDED for bypass/preset updates.
// void DynamicsDoctorProcessor::parameterChanged (const juce::String& parameterID, float newValue) { ... }

//==============================================================================
juce::AudioProcessorValueTreeState& DynamicsDoctorProcessor::getValueTreeState()
{
    return parameters;
}

DynamicsStatus DynamicsDoctorProcessor::getCurrentStatus() const
{
    // Ensure thread safety if accessed from multiple threads. std::atomic<DynamicsStatus> could be used.
    // Or, ensure it's only read from the message thread (e.g., via a timer in the editor polling this).
    // For simplicity here, assume it's polled carefully.
    return currentStatus; // Consider making currentStatus atomic if read by editor directly/frequently
}

// Private function to update the status
void DynamicsDoctorProcessor::updateStatus()
{
    // This function is now primarily called from processBlock (audio thread)
    // or occasionally from constructor/setStateInformation (message thread).
    // Reads parameters atomically, reads analysis results (written only by audio thread).

    // Get current parameter values safely
    const bool bypassed = bypassParam->load() > 0.5f;
    const int presetIndex = static_cast<int>(presetParam->load());

    // If bypassed, status is already handled in processBlock, but double-check
    if (bypassed)
    {
        currentStatus = DynamicsStatus::Bypassed;
        // No need to reset reporting params here, processBlock handles it.
        return;
    }

    // Validate preset index
    const int validatedPresetIndex = (presetIndex < 0 || presetIndex >= presets.size())
                                     ? ParameterDefaults::preset
                                     : presetIndex;
    const auto& selectedPreset = presets[validatedPresetIndex];

    // Get the latest analysis values (members updated in processBlock)
    // Reading these non-atomic members is safe *if* updateStatus is only called
    // from the audio thread or when audio is stopped (like constructor/setStateInformation).
    float peak = currentPeak;
    float lufs = currentLufs;

    // Calculate status based on the difference
    float drDifference = peak - lufs;
    DynamicsStatus newStatus;

    if (lufs < -60.0f) // Treat very low LUFS as OK (near silence)
    {
        newStatus = DynamicsStatus::Ok;
    }
    else if (drDifference >= selectedPreset.minDiffOk)
    {
        newStatus = DynamicsStatus::Ok;
    }
    else if (drDifference >= selectedPreset.minDiffReduced)
    {
        newStatus = DynamicsStatus::Reduced;
    }
    else
    {
        newStatus = DynamicsStatus::Loss;
    }

    // Only update if the status actually changed
    if (currentStatus != newStatus)
    {
        currentStatus = newStatus;
        // The editor should poll getCurrentStatus() or use an AsyncUpdater
        // triggered from processBlock if immediate UI updates are needed.
        // Avoid direct UI calls from audio thread.
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DynamicsDoctorProcessor();
}
