#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h> // Kept for future DSP use
#include <juce_audio_utils/juce_audio_utils.h> // Often needed for APVTS related things
#include <atomic> // Required for std::atomic
#include "Constants.h" // Provides presets, ParameterIDs, DynamicStatus enum, etc.
#include "LoudnessMeter.h" // Out C++ wrapper for libebur128
// Forward declaration for the Parameter Layout function if needed,
// though usually kept entirely within the .cpp or defined inline if simple.
// If createParameterLayout is a private member function, no forward declaration needed here.


//==============================================================================
/**
    The main audio processing class for the DynamicsDoctor plugin.
*/
class DynamicsDoctorProcessor : public juce::AudioProcessor,
                                public juce::AudioProcessorValueTreeState::Listener
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
    
    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override; // Defined in CPP

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
    // Public accessors for the editor
    juce::AudioProcessorValueTreeState& getValueTreeState();
    DynamicsStatus getCurrentStatus() const;
    // Provide LRA value for potential display in editor
    float getReportedLRA() const; // This will be the LRA from libebur128
    
    // Callback for parameter changes, specifically for the reset button
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    //==============================================================================
    // Manages all plugin parameters and their state
    juce::AudioProcessorValueTreeState parameters;
    
    // Parameter Creation Helper
    // Defined in the .cpp file, returns the layout for the APVTS constructor
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Raw pointers to atomic parameter values for efficient real-time access
    // Initialized in the constructor after 'parameters' is created.
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* presetParam = nullptr;
    std::atomic<float>* peakParam   = nullptr; // Reports instantaneous peak
    std::atomic<float>* lraParam   = nullptr; // Reports long-term LRA
    // Pointer for the reset parameter to listen to it
    juce::AudioParameterBool* resetLraParamObject = nullptr;
    
    // Loudness Measurement using our wrapper
    LoudnessMeter loudnessMeter;
    
    // State for periodic updates (e.g., once per second)
    double internalSampleRate = 44100.;
    int samplesUntilLraUpdate = 0;
    
    // Results (atomic for thread-safe reading by editor timer)
    std::atomic<float> currentPeak { ParameterDefaults::peak};
    std::atomic<float> currentGlobalLRA { ParameterDefaults::lra }; // LRA from libebur128
    std::atomic<DynamicsStatus> currentStatus { DynamicsStatus::Bypassed };
    
    // Private Helper Methods
    void handleResetLRA();  // Method to perform the reset
    void updateStatusBasedOnLRA(float measuredLRA); // Takes the current LRA
  

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicsDoctorProcessor)
};
