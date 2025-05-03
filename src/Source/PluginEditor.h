/*
  ==============================================================================

    PluginEditor.h - Declaration file for the plugin's GUI.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h" // Include processor header

// Alias for parameter attachments to shorten names
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

//==============================================================================
/**
    The GUI class for the plugin.
*/
class EarfatiguetoolAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                            private juce::Timer // Inherit from Timer for periodic updates
{
public:
    // Constructor: Takes a reference to the processor.
    EarfatiguetoolAudioProcessorEditor (EarfatiguetoolAudioProcessor&);
    // Destructor.
    ~EarfatiguetoolAudioProcessorEditor() override;

    //==============================================================================
    // Called to draw the GUI.
    void paint (juce::Graphics&) override;
    // Called when the GUI component is resized.
    void resized() override;

private:
    //==============================================================================
    // Timer callback function for updating the status display periodically.
    void timerCallback() override;

    // Reference to the processor object.
    EarfatiguetoolAudioProcessor& audioProcessor;

    // --- UI Elements ---
    juce::TextButton bypassButton { "Bypass" }; // TextButton for Bypass
    juce::ComboBox standardComboBox { "Standard" }; // ComboBox for Standard selection
    juce::Label statusLabel { "Status", "Waiting for audio..." }; // Label to display status message
    juce::TextButton referenceTrackButton { "Load Ref Track (NYI)" }; // Placeholder button

    // --- Parameter Attachments ---
    // These automatically link UI elements to APVTS parameters.
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ComboBoxAttachment> standardAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarfatiguetoolAudioProcessorEditor)
};
