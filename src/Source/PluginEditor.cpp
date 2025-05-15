#include "PluginProcessor.h" // Provides DynamicsDoctorProcessor
#include "PluginEditor.h"    // Provides DynamicsDoctorEditor class declaration
#include "Constants.h"       // Provides Palette, DynamicsStatus, presets, ParameterIDs, helpers

//==============================================================================
DynamicsDoctorEditor::DynamicsDoctorEditor (DynamicsDoctorProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), processorRef (p), valueTreeState (vts)
{
    // --- Traffic Light ---
    addAndMakeVisible (trafficLight);
    
    // --- Status Label ---
    // Use FontOptions for specifying style (bold) with height
    statusLabel.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setText ("Initializing...", juce::dontSendNotification); // Initial text
    addAndMakeVisible (statusLabel);
    
    // --- Preset Selector ---
    presetLabel.setFont (juce::FontOptions(14.0f)); // Use direct height overload
    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetLabel.attachToComponent(&presetSelector, true); // Label to the left
    addAndMakeVisible (presetLabel);
    
    presetSelector.setTooltip ("Select the dynamic range reference standard");
    // Populate ComboBox items using the global `presets` vector from Constants.h
    for (int i = 0; i < presets.size(); ++i)
        presetSelector.clear(); // Good practice to clear before repopulating, though usually only done once
    for (int i = 0; i < presets.size(); ++i) // 'presets' here refers to the global const vector from Constants.h
    {
        presetSelector.addItem (presets[i].label, i + 1); // use the new labels from Constants.h
    // ComboBox IDs are 1-based
    }
    addAndMakeVisible (presetSelector);
    presetSelector.addListener (this); // Listen for immediate user changes

    // Attachment: Connects the ComboBox value to the APVTS parameter
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, ParameterIDs::preset.getParamID(), presetSelector); // Use getParamID()

     // --- Bypass Toggle Button ---
    bypassLabel.setFont (juce::FontOptions(14.0f)); // Use direct height overload
    bypassLabel.setJustificationType (juce::Justification::centredRight);
    bypassLabel.attachToComponent(&bypassButton, true); // Label to the left
    addAndMakeVisible (bypassLabel);

    // Use LookAndFeel for toggle button style instead of text
    bypassButton.setButtonText("");
    bypassButton.setTooltip ("Bypass dynamics monitoring");
    addAndMakeVisible (bypassButton);
    bypassButton.addListener (this); // Listen for immediate user clicks

    // Attachment: Connects the Button state (on/off) to the APVTS parameter
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        valueTreeState, ParameterIDs::bypass.getParamID(), bypassButton); // Use getParamID()


    // --- Value Labels ---
    peakValueLabel.setFont (juce::FontOptions(12.0f)); // Use direct height overload
    peakValueLabel.setJustificationType (juce::Justification::centred);
    peakValueLabel.setColour(juce::Label::textColourId, Palette::Foreground.withAlpha(0.7f)); // Use Palette namespace
    addAndMakeVisible (peakValueLabel);

    // This label now displays LRA
    lraValueLabel.setFont (juce::FontOptions(12.0f)); // Use direct height overload
    lraValueLabel.setJustificationType (juce::Justification::centred);
    lraValueLabel.setColour(juce::Label::textColourId, Palette::Foreground.withAlpha(0.7f)); // Use Palette namespace
    addAndMakeVisible (lraValueLabel);

    presetInfoLabel.setFont (juce::FontOptions(12.0f)); // Use direct height overload
    presetInfoLabel.setJustificationType (juce::Justification::centred);
    presetInfoLabel.setColour(juce::Label::textColourId, Palette::Foreground.withAlpha(0.7f)); // Use Palette namespace
    addAndMakeVisible (presetInfoLabel);

    // --- Reset LRA Button Setup --- //
    resetLraButton.setTooltip("Reset the Loudness Range (LRA) measurement history");
      addAndMakeVisible(resetLraButton);
      resetLraButton.addListener(this); // Optional: for direct click feedback if needed beyond parameter
      resetLraAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          valueTreeState, ParameterIDs::resetLra.getParamID(), resetLraButton);
    
    
    // --- Version Label Setup --- // <<< ADD THIS SECTION
      versionLabel.setText("v" + juce::String(JucePlugin_VersionString), juce::dontSendNotification);
      versionLabel.setFont(juce::FontOptions (10.0f)); // Use a small font size
      versionLabel.setJustificationType(juce::Justification::bottomRight); // Align bottom-right
      versionLabel.setColour(juce::Label::textColourId, Palette::Foreground.withAlpha(0.6f)); // Dimmed text colour
      addAndMakeVisible(versionLabel); // Make it visible
    

    // Set editor size - Required call
    setSize (250, 430); // Increase height slightly for reset button

    // Start timer for periodic UI updates (e.g., 10-15 times per second for responsiveness)
    startTimerHz (15);

    // Perform an initial UI update based on the current processor state
    updateUIStatus();
    
    // In PluginEditor constructor
    versionLabel.setText ("Build: " + juce::String(__DATE__) + " " + juce::String(__TIME__), juce::dontSendNotification);
    addAndMakeVisible (versionLabel);
    // In PluginEditor::resized()
    // myVersionLabel.setBounds (10, 10, 200, 20);
}

DynamicsDoctorEditor::~DynamicsDoctorEditor()
{
    // Stop the timer safely
    stopTimer();

    // Remove listeners explicitly added
    presetSelector.removeListener(this);
    bypassButton.removeListener(this);
    resetLraButton.removeListener(this);
    
    // Attachments (`presetAttachment`, `bypassAttachment`) are automatically cleaned up
    // by their std::unique_ptr destructors.
}

//==============================================================================
void DynamicsDoctorEditor::paint (juce::Graphics& g)
{
    
    g.fillAll (Palette::Background);
    auto bounds = getLocalBounds();
    auto topAreaHeight = bounds.getHeight() * 0.55f; // adjusted
    auto lightArea = bounds.removeFromTop(static_cast<int>(topAreaHeight)).reduced(20);
    g.setColour(Palette::Foreground.withAlpha(0.1f)); // Use palette colour
    g.drawRoundedRectangle(lightArea.toFloat(), 5.0f, 2.0f);
    // Draw separator line below traffic light area
    g.setColour(Palette::Foreground.withAlpha(0.2f)); // Use palette colour
    g.drawLine(static_cast<float>(bounds.getX()), topAreaHeight, static_cast<float>(bounds.getRight()), topAreaHeight, 1.0f);
}

void DynamicsDoctorEditor::resized()
{
    // Layout the positions and sizes of child components within the editor bounds.

    // Define some constants for margins/padding
    constexpr int padding = 15;
    constexpr int controlHeight = 25;
    constexpr int labelWidth = 80; // Estimated width for labels next to controls
    constexpr int valueLabelHeight = 20;
    constexpr int topSectionHeight = 40; // Height for the main status label
    constexpr int controlGap = 5; // Gap between controls
    constexpr int buttonRowHeight = 30; // Height for the reset button row

    auto bounds = getLocalBounds().reduced(padding); // Overall content area

    // Top section: Status Label and Traffic Light
    auto topArea = bounds.removeFromTop(bounds.getHeight() * 0.55f); // adjusted
    statusLabel.setBounds(topArea.removeFromTop(topSectionHeight));
    // Add padding around the traffic light within its allocated space
    trafficLight.setBounds(topArea.reduced(10));


    // Bottom section: Controls and Info Labels
    auto bottomArea = bounds; // Remaining space after topArea was removed
    auto versionAreaHeight = 15; // Height for version label
    versionLabel.setBounds(bottomArea.removeFromBottom(versionAreaHeight).reduced(0, padding / 2)); // Position version label first from bottom
    
    auto resetButtonArea = bottomArea.removeFromBottom(buttonRowHeight + controlGap); // Added for reset button
    
    auto infoArea = bottomArea.removeFromBottom(valueLabelHeight * 3 + 10); // Space for 3 value labels + padding
    auto controlArea = bottomArea; // Space left for controls

    
    // --- Layout Reset Button ---
    resetLraButton.setBounds(resetButtonArea.reduced(controlArea.getWidth() * 0.2f, 0)); // Centered, bit wider
    
    // Layout Controls (Preset and Bypass) vertically
    controlArea.reduce(10, 5); // Adjust horizontal/vertical padding
    auto presetRow = controlArea.removeFromTop(controlHeight);
    controlArea.removeFromTop(controlGap); // Add gap
    auto bypassRow = controlArea.removeFromTop(controlHeight);

    // Preset Row: Label (implicit via attachToComponent) + ComboBox
    presetSelector.setBounds(presetRow.withLeft(presetRow.getX() + labelWidth).reduced(5, 0));

    // Bypass Row: Label (implicit via attachToComponent) + ToggleButton
    bypassButton.setBounds(bypassRow.withLeft(bypassRow.getX() + labelWidth).reduced(5, 0));


    // Layout Info Labels (Peak, LRA, Preset Info)
    infoArea.reduce(0, 5); // Reduce top/bottom padding within info area
    auto peakArea = infoArea.removeFromTop(valueLabelHeight);
    auto lraDisplayArea = infoArea.removeFromTop(valueLabelHeight); // This area now shows LRA
    auto presetInfoDisplayArea = infoArea;

    peakValueLabel.setBounds(peakArea);
    lraValueLabel.setBounds(lraDisplayArea); // Label displaying LRA
    presetInfoLabel.setBounds(presetInfoDisplayArea);
    
    // --- Position Version Label --- // <<< ADD THIS LINE
        // Position it in the bottom-right corner, using padding from the edge
    versionLabel.setBounds(getLocalBounds().reduced(padding).removeFromBottom(15).removeFromRight(100));
        // Explanation of bounds:
        // getLocalBounds().getReduced(padding) : Start with the main padded area
        // .removeFromBottom(15)             : Take a thin 15px strip at the bottom
        // .removeFromRight(100)            : Take the right-most 100px of that strip
}

//==============================================================================
void DynamicsDoctorEditor::buttonClicked (juce::Button* buttonThatWasClicked)
{
    // Attachments handle parameter changes. This callback is mostly for additional logic
    // or if you weren't using attachments for some reason.
    //if (buttonThatWasClicked == &presetSelector) // This check is incorrect, presetSelector is ComboBox
    //{
       // Should be handled by comboBoxChanged
    //}
    if (buttonThatWasClicked == &bypassButton)
    {
        updateUIStatus(); // Update UI immediately on bypass click
    }
    else if (buttonThatWasClicked == &resetLraButton) // <<< ADD THIS ELSE IF
    {
        // The attachment changes the parameter. The processor's parameterChanged
        // will call handleResetLRA() and then set the parameter back to false.
        // We might want immediate UI feedback here if necessary, but timer will catch it.
        // DBG("Reset LRA Button Clicked in Editor");
        updateUIStatus(); // Update UI immediately on reset click
    }
}

//==============================================================================
// Listener Callbacks (Optional but useful for immediate feedback)

void DynamicsDoctorEditor::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &presetSelector)
    {
       updateUIStatus();
    }
}

//==============================================================================
// Timer Callback (Essential for polling processor analysis results)
void DynamicsDoctorEditor::timerCallback()
{
    updateUIStatus();
}


//==============================================================================
// Private Helper Function to Update UI Elements
void DynamicsDoctorEditor::updateUIStatus()
{
    // Get the current status from the processor (thread-safe read due to atomic)
    auto status = processorRef.getCurrentStatus();

    // Update the Traffic Light component
    trafficLight.setStatus (status);

    // Update the main Status Label text and colour using helpers from Constants.h
    statusLabel.setText (getStatusMessage(status), juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, getStatusColour(status));

    // Determine if the plugin is currently bypassed
    const bool isBypassed = (status == DynamicsStatus::Bypassed);

    // Update value labels only if not bypassed
    if (!isBypassed)
    {
        // Fetch Peak from parameter
        auto peakParam = valueTreeState.getRawParameterValue(ParameterIDs::peak.getParamID());
        if (peakParam != nullptr)
        {
            peakValueLabel.setText ("Peak: " + juce::String(peakParam->load(), 1) + " dBFS", juce::dontSendNotification);
        } else {
            peakValueLabel.setText("Peak: --- dBFS", juce::dontSendNotification);
        }
        
        // Fetch LRA value from reporting parameter (which processor updates from libebur128)
                auto lraReportingParam = valueTreeState.getRawParameterValue(ParameterIDs::lra.getParamID());
                if (lraReportingParam != nullptr) {
                    lraValueLabel.setText ("LRA: " + juce::String(lraReportingParam->load(), 1) + " LU", juce::dontSendNotification);
                } else {
                     lraValueLabel.setText ("LRA: --- LU", juce::dontSendNotification);
                }

        // Update preset info label using the updated DynamicsPreset structure
        int presetIndex = presetSelector.getSelectedId() - 1; // IDs are 1-based
        if (presetIndex >= 0 && static_cast<size_t>(presetIndex) < presets.size())
        {
            const auto& selectedPreset = presets[presetIndex];
            // --- NEW TEXT FOR PRESET INFO LABEL ---
                    // Displaying the target LRA range for the selected genre
                    juce::String infoText = selectedPreset.label + " (Target LRA: ";
                    infoText += juce::String(selectedPreset.targetLraMin, 1) + " LU - ";
                    infoText += juce::String(selectedPreset.targetLraMax, 1) + " LU)";
                    presetInfoLabel.setText (infoText, juce::dontSendNotification);
                    // --- END OF NEW TEXT ---

                    /*
                    // Optional: If you ALSO want to show the Red/Amber thresholds here:
                    juce::String detailedInfo = selectedPreset.label;
                    detailedInfo += " (Target: " + juce::String(selectedPreset.targetLraMin, 1) + "-" + juce::String(selectedPreset.targetLraMax, 1) + " LU";
                    detailedInfo += " | Red < " + juce::String(selectedPreset.lraThresholdRed, 1);
                    detailedInfo += ", Amber < " + juce::String(selectedPreset.lraThresholdAmber, 1) + ")";
                    presetInfoLabel.setText(detailedInfo, juce::dontSendNotification);
                    */
        }
        else
        {
            presetInfoLabel.setText ("Invalid Preset Selected", juce::dontSendNotification);
        }

        peakValueLabel.setVisible(true);
        lraValueLabel.setVisible(true);
        presetInfoLabel.setVisible(true);
    }
    else // Is Bypassed
    {
         peakValueLabel.setVisible(false);
         lraValueLabel.setVisible(false);
         presetInfoLabel.setVisible(false);
    }

     presetSelector.setEnabled(!isBypassed);
     presetLabel.setEnabled(!isBypassed);
     presetLabel.setColour(juce::Label::textColourId,
                           isBypassed ? Palette::DisabledText : Palette::Foreground);
     bypassLabel.setEnabled(true);
     bypassLabel.setColour(juce::Label::textColourId, Palette::Foreground);
     resetLraButton.setEnabled(!isBypassed); // <<< Optionally disable reset button when bypassed
}
