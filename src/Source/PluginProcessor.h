#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h> // Often needed for APVTS related things
#include <atomic> // Required for std::atomic
#include "Constants.h" // Include our constants

// Forward declaration for the Parameter Layout function if needed,
// though usually kept entirely within the .cpp or defined inline if simple.
// If createParameterLayout is a private member function, no forward declaration needed here.


//==============================================================================
/**
    The main audio processing class for the DynamicsDoctor plugin.
*/
class DynamicsDoctorProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    DynamicsDoctorProcessor();
    ~DynamicsDoctorProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // Note: `using AudioProcessor::processBlock;` is usually not needed here

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    // Program management (less critical with APVTS, but required overrides)
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Public access to state for the editor or other components
    juce::AudioProcessorValueTreeState& getValueTreeState();
    DynamicsStatus getCurrentStatus() const; // Provides the latest calculated status

private:
    //==============================================================================
    // Parameter Creation Helper
    // Defined in the .cpp file, returns the layout for the APVTS constructor
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Manages all plugin parameters and their state
    juce::AudioProcessorValueTreeState parameters;

    // Raw pointers to atomic parameter values for efficient real-time access
    // Initialized in the constructor after 'parameters' is created.
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* presetParam = nullptr;
    std::atomic<float>* peakParam   = nullptr; // Reporting parameter
    std::atomic<float>* lufsParam   = nullptr; // Reporting parameter

    // Internal state variables for analysis and status
    std::atomic<DynamicsStatus> currentStatus; // Made atomic for safe access from UI/polling
    float currentPeak = -100.0f; // Holds latest peak dBFS (written by audio thread)
    float currentLufs = -100.0f; // Holds latest integrated LUFS (written by audio thread)
    // Add other internal variables needed for analysis algorithms here...

    // Private helper function to update the status based on current parameters and analysis
    void updateStatus();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicsDoctorProcessor)
};
