/*
  ==============================================================================

    PluginEditor.cpp - Implementation file for the plugin's GUI.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Constructor: Initializes UI elements and attachments.
//==============================================================================
EarfatiguetoolAudioProcessorEditor::EarfatiguetoolAudioProcessorEditor (EarfatiguetoolAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) // Initialize base class and processor reference
{
    DBG("EarfatiguetoolAudioProcessorEditor CONSTRUCTOR started.");

    // --- Configure UI Elements ---

    // Bypass Button
    bypassButton.setClickingTogglesState(true); // Make it behave like a toggle
    addAndMakeVisible(bypassButton); // Add to the editor and make visible

    // Standard ComboBox
    standardComboBox.addItem("High DR", 1);    // ID 1 corresponds to index 0
    standardComboBox.addItem("Medium DR", 2); // ID 2 corresponds to index 1
    standardComboBox.addItem("Low DR", 3);    // ID 3 corresponds to index 2
    standardComboBox.setSelectedId(1, juce::dontSendNotification); // Set default without triggering listeners
    addAndMakeVisible(standardComboBox);

    // Status Label
    statusLabel.setFont (juce::FontOptions(18.0f, juce::Font::bold));
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(statusLabel);

    // Reference Track Button (Placeholder)
    referenceTrackButton.setEnabled(false); // Not yet implemented
    addAndMakeVisible(referenceTrackButton);


    // --- Create Parameter Attachments ---
    // Link UI elements to the parameters defined in the processor's APVTS.

    // Correct way to pass the parameter ID string:
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts,
        // Construct a String from the StringRef ID
        juce::String(EarfatiguetoolAudioProcessor::PARAM_BYPASS_ID),
        bypassButton);

    // Correct way to pass the parameter ID string:
    standardAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts,
        // Construct a String from the StringRef ID
        juce::String(EarfatiguetoolAudioProcessor::PARAM_STANDARD_ID),
        standardComboBox);

    // --- End Corrections ---
    // --- Start Timer ---
    // Call timerCallback() approximately 15 times per second (Hz).
    startTimerHz(15);

    // --- Set Editor Size ---
    // Set the initial size of the plugin window.
    setSize (400, 200); // Adjusted size

    DBG("EarfatiguetoolAudioProcessorEditor CONSTRUCTOR finished.");
}

//==============================================================================
// Destructor
//==============================================================================
EarfatiguetoolAudioProcessorEditor::~EarfatiguetoolAudioProcessorEditor()
{
    // Stop the timer when the editor is destroyed.
    stopTimer();
    // Attachments (bypassAttachment, standardAttachment) are automatically cleaned up
    // by std::unique_ptr, detaching listeners.
}

//==============================================================================
// paint: Draws the background.
//==============================================================================
void EarfatiguetoolAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fill the background with a solid colour.
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker(0.5f));
    // UI elements (Label, Button, ComboBox) draw themselves.
}

//==============================================================================
// resized: Sets the position and size of UI elements.
//==============================================================================
void EarfatiguetoolAudioProcessorEditor::resized()
{
    // Define layout areas using rectangles.
    auto bounds = getLocalBounds(); // Get the total area of the editor window

    auto controlsBounds = bounds.removeFromBottom(60); // Area for bottom controls
    auto statusBounds = bounds; // Remaining area for status label

    // Layout bottom controls
    int buttonWidth = 80;
    int comboWidth = 120;
    int refButtonWidth = 140;
    int padding = 10;

    bypassButton.setBounds(controlsBounds.removeFromLeft(buttonWidth).reduced(padding));
    referenceTrackButton.setBounds(controlsBounds.removeFromRight(refButtonWidth).reduced(padding));
    standardComboBox.setBounds(controlsBounds.reduced(padding)); // Remaining space in middle

    // Layout status label
    statusLabel.setBounds(statusBounds.reduced(20)); // Add some padding around label
}


//==============================================================================
// timerCallback: Called periodically by the timer to update the display.
//==============================================================================
void EarfatiguetoolAudioProcessorEditor::timerCallback()
{
    // Get the latest status and crest factor from the processor (thread-safe).
    auto status = audioProcessor.getCurrentStatus();
    // auto crestFactor = audioProcessor.getCurrentCrestFactorDb(); // Could display this too

    juce::String statusText;
    juce::Colour statusColour;

    // Determine the text and colour based on the status.
    switch (status)
    {
        case EarfatiguetoolAudioProcessor::DynamicsStatus::Ok:
            statusText = "Dynamics in Range";
            statusColour = juce::Colours::limegreen;
            break;
        case EarfatiguetoolAudioProcessor::DynamicsStatus::Reduced:
            statusText = "Dynamics Reduced";
            statusColour = juce::Colours::orange;
            break;
        case EarfatiguetoolAudioProcessor::DynamicsStatus::Loss:
            statusText = "Dynamics Loss (Ear Fatigue Likely)";
            statusColour = juce::Colours::red;
            break;
    }

    // --- Update the Label (This runs on the message thread, so it's safe) ---
    statusLabel.setText(statusText, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, statusColour);

    // You could potentially add the Crest Factor value to the text for debugging:
    // statusLabel.setText(statusText + " (CF: " + juce::String(crestFactor, 1) + " dB)", juce::dontSendNotification);
}
