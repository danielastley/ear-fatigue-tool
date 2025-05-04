#pragma once

// Include necessary JUCE modules explicitly
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h> // For core GUI elements like Label, Button, Timer
#include <juce_gui_extra/juce_gui_extra.h>   // For ComboBox

// Include project-specific headers
#include "PluginProcessor.h"
#include "TrafficLightComponent.h" // Include the traffic light header
#include "Constants.h"             // For DynamicsStatus enum, ParameterIDs etc.

//==============================================================================
/**
    The editor component for the DynamicsDoctor plugin.
    Handles UI display, user interaction, and communication with the processor.
*/
class DynamicsDoctorEditor : public juce::AudioProcessorEditor,
                             private juce::ComboBox::Listener, // Keep private if callbacks only used internally
                             private juce::Button::Listener,   // Keep private if callbacks only used internally
                             private juce::Timer             // Keep private for internal UI updates
{
public:
    /** Constructor. Takes references to the processor and its Value Tree State. */
    explicit DynamicsDoctorEditor (DynamicsDoctorProcessor&, juce::AudioProcessorValueTreeState&);

    /** Destructor. */
    ~DynamicsDoctorEditor() override;

    //==============================================================================
    /** Paints the editor component. */
    void paint (juce::Graphics&) override;

    /** Called when the editor component is resized. */
    void resized() override;

private:
    //==============================================================================
    // Listener Callbacks (implementations in .cpp file)
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    void buttonClicked (juce::Button* buttonThatWasClicked) override;

    /** Timer callback, used to periodically poll the processor for status updates. */
    void timerCallback() override;

    //==============================================================================
    // Helper method to update UI elements based on processor status
    void updateUIStatus();

    // Reference to the processor object (required).
    DynamicsDoctorProcessor& processorRef;

    // Reference to the processor's parameter state manager (required).
    juce::AudioProcessorValueTreeState& valueTreeState;

    // --- UI Components ---
    TrafficLightComponent trafficLight; // Custom component for visual status
    juce::Label statusLabel       { "statusLabel", "Status:" }; // Use initializer list for default text
    juce::ComboBox presetSelector { "presetSelector" };         // Use initializer list for component name
    juce::Label presetLabel       { "presetLabel", "Preset:" };
    juce::ToggleButton bypassButton { "bypassButton" };          // Use initializer list for component name
    juce::Label bypassLabel       { "bypassLabel", "Bypass" };

    juce::Label peakValueLabel    { "peakValueLabel", "-inf dBFS" }; // Default text
    juce::Label lufsValueLabel    { "lufsValueLabel", "-inf LUFS" }; // Default text
    juce::Label presetInfoLabel   { "presetInfoLabel", ""};       // Initially empty

    // --- Parameter Attachments ---
    // Use RAII to manage the connection between UI elements and parameters.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicsDoctorEditor)
};
