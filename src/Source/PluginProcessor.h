/*
  ==============================================================================

    PluginProcessor.h - Declaration file for the audio processing logic.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic> // For thread-safe value storage

//==============================================================================
/**
    The main audio processing class.
*/
class EarfatiguetoolAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    EarfatiguetoolAudioProcessor();
    ~EarfatiguetoolAudioProcessor() override;

    //==============================================================================
    // Called before playback starts or when sample rate/buffer size changes.
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    // Called when playback stops.
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    // Checks if the plugin supports the host's channel layout.
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    // The main audio processing callback.
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    // Creates the plugin's editor window.
    juce::AudioProcessorEditor* createEditor() override;
    // Returns true if the plugin has a custom editor.
    bool hasEditor() const override;

    //==============================================================================
    // Standard plugin information methods.
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    // Program handling methods (basic implementation).
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    // Saves the plugin's state (parameters).
    void getStateInformation (juce::MemoryBlock& destData) override;
    // Restores the plugin's state.
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter Management using AudioProcessorValueTreeState (APVTS)
    juce::AudioProcessorValueTreeState apvts;

    // Define Parameter IDs for easy access
    static const juce::StringRef PARAM_BYPASS_ID;
    static const juce::StringRef PARAM_STANDARD_ID;

    // Enum to represent the dynamic range status
    enum class DynamicsStatus
    {
        Ok,
        Reduced,
        Loss
    };

    // Getter for the editor to poll the current status (thread-safe)
    DynamicsStatus getCurrentStatus() const { return currentStatus.load(); }
    // Getter for the editor to poll the current crest factor (thread-safe)
    float getCurrentCrestFactorDb() const { return currentCrestFactorDb.load(); }


private:
    //==============================================================================
    // Helper function to define all parameters for APVTS.
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Atomic members to store analysis results thread-safely.
    // Initialized to represent silence or an invalid state.
    std::atomic<float> currentPeakDb { -100.0f };
    std::atomic<float> currentRmsDb  { -100.0f };
    std::atomic<float> currentCrestFactorDb { 0.0f }; // Crest Factor = Peak dB - RMS dB
    std::atomic<DynamicsStatus> currentStatus { DynamicsStatus::Ok };

    // Thresholds based on our discussion (can be tuned later)
    const float highDrLimitOk = 14.0f; // Below this is Reduced for High DR
    const float highDrLimitReduced = 11.0f; // Below this is Loss for High DR

    const float medDrLimitOk = 11.0f; // Below this is Reduced for Medium DR
    const float medDrLimitReduced = 8.0f; // Below this is Loss for Medium DR

    const float lowDrLimitOk = 8.0f;  // Below this is Reduced for Low DR
    const float lowDrLimitReduced = 5.0f;  // Below this is Loss for Low DR


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarfatiguetoolAudioProcessor)
};
