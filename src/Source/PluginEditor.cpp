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
    // bypassLabel.setFont (juce::FontOptions(14.0f)); // Use direct height overload
   //  bypassLabel.setJustificationType (juce::Justification::centredRight);
   //  bypassLabel.attachToComponent(&bypassButton, true); // Label to the left
   //  addAndMakeVisible (bypassLabel);

    // Use LookAndFeel for toggle button style instead of text
    // bypassButton.setButtonText("");
    // bypassButton.setTooltip ("Bypass dynamics monitoring");
    // addAndMakeVisible (bypassButton);
    // bypassButton.addListener (this); // Listen for immediate user clicks

    // Attachment: Connects the Button state (on/off) to the APVTS parameter
    //bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
     //   valueTreeState, ParameterIDs::bypass.getParamID(), bypassButton); // Use getParamID()


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
      // resetLraButton.addListener(this); // Optional: for direct click feedback if needed beyond parameter
    // resetLraButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, ParameterIDs::resetLra.getParamID(), resetLraButton);
    
    resetLraButton.onClick = [this]()
        {
            DBG("=============================================================="); // Separator
            DBG("EDITOR: resetLraButton.onClick lambda - Entered.");

            auto* resetParamRaw = valueTreeState.getParameter(ParameterIDs::resetLra.getParamID()); // Use your .getParamID()

            if (resetParamRaw != nullptr)
            {
                float currentValue = resetParamRaw->getValue();
                DBG("EDITOR: resetLra param (" << ParameterIDs::resetLra.getParamID() << ") current value BEFORE setting: "
                    << currentValue << (currentValue > 0.5f ? " (TRUE)" : " (FALSE)"));

                DBG("EDITOR: Attempting to set resetLra param to TRUE (1.0f).");
                resetParamRaw->setValueNotifyingHost(1.0f);

                float valueAfterSet = resetParamRaw->getValue();
                DBG("EDITOR: resetLra param value AFTER setValueNotifyingHost(1.0f): "
                    << valueAfterSet << (valueAfterSet > 0.5f ? " (TRUE)" : " (FALSE)"));
            }
            else
            {
                DBG("EDITOR: ERROR - resetLra parameter NOT FOUND in valueTreeState!");
                jassertfalse;
            }
            DBG("EDITOR: resetLraButton.onClick lambda - Exiting.");
            DBG("==============================================================");
        };
    
    
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
    // bypassButton.removeListener(this);
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
    auto topArea = bounds.removeFromTop(bounds.getHeight() * 0.50f); // adjusted
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
    
    // Add extra space above the Preset adn Bypass controls
    controlArea.removeFromTop(20); // Add 20px gap above the controls
    auto presetRow = controlArea.removeFromTop(controlHeight);
    // controlArea.removeFromTop(controlGap); // Add gap
    // auto bypassRow = controlArea.removeFromTop(controlHeight);

    // Preset Row: Label (implicit via attachToComponent) + ComboBox
    presetSelector.setBounds(presetRow.withLeft(presetRow.getX() + labelWidth).reduced(5, 0));
    // Bypass Row: Label (implicit via attachToComponent) + ToggleButton
    // bypassButton.setBounds(bypassRow.withLeft(bypassRow.getX() + labelWidth).reduced(5, 0));


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
    
    if (buttonThatWasClicked == &resetLraButton) // <<< ADD THIS ELSE IF
    {
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
    auto status = processorRef.getCurrentStatus();
    trafficLight.setStatus(status); // Your TrafficLightComponent handles ::Measuring based on Constants.h
    statusLabel.setText(getStatusMessage(status), juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, getStatusColour(status));

    // const bool isBypassed = (status == DynamicsStatus::Bypassed);
    const bool isPluginConsideredActive = true; // Plugin is always active unless host bypasses
    const bool isMeasuring = (status == DynamicsStatus::Measuring);

    //if (isBypassed)
    //{
     //    peakValueLabel.setVisible(false);
       //  lraValueLabel.setVisible(false);
       //  presetInfoLabel.setVisible(false);
       //  presetSelector.setEnabled(false);
        // presetLabel.setEnabled(false);
        // resetLraButton.setEnabled(false);
    // }
    if (isMeasuring)
    {
        peakValueLabel.setText("Peak: Receiving...", juce::dontSendNotification); // Or "--- dBFS"
        lraValueLabel.setText("LRA: Measuring...", juce::dontSendNotification);
        // Optionally show preset info even while measuring
        int presetIndex = presetSelector.getSelectedId() - 1;
        if (presetIndex >= 0 && static_cast<size_t>(presetIndex) < presets.size()) {
            const auto& selectedPreset = presets[presetIndex];
            juce::String infoText = selectedPreset.label + " (Target LRA: ";
            infoText += juce::String(selectedPreset.targetLraMin, 1) + " LU - ";
            infoText += juce::String(selectedPreset.targetLraMax, 1) + " LU)";
            presetInfoLabel.setText(infoText, juce::dontSendNotification);
        } else {
            presetInfoLabel.setText("Select Preset", juce::dontSendNotification);
        }

        peakValueLabel.setVisible(true);
        lraValueLabel.setVisible(true);
        presetInfoLabel.setVisible(true);
        presetSelector.setEnabled(true); // Allow preset changes while measuring
        presetLabel.setEnabled(true);
        resetLraButton.setEnabled(true); // Allow reset even while measuring
    }
    else // Not Bypassed (always true from interal param view) and Not Measuring (Ok, Reduced, Loss)
    {
        auto peakParamPtr = valueTreeState.getRawParameterValue(ParameterIDs::peak.getParamID());
        if (peakParamPtr != nullptr) {
            peakValueLabel.setText ("Peak: " + juce::String(peakParamPtr->load(), 1) + " dBFS", juce::dontSendNotification);
        } else {
            peakValueLabel.setText("Peak: --- dBFS", juce::dontSendNotification);
        }
        
        auto lraReportingParam = valueTreeState.getRawParameterValue(ParameterIDs::lra.getParamID());
        if (lraReportingParam != nullptr) {
            lraValueLabel.setText ("LRA: " + juce::String(lraReportingParam->load(), 1) + " LU", juce::dontSendNotification);
        } else {
             lraValueLabel.setText ("LRA: --- LU", juce::dontSendNotification);
        }

        int presetIndex = presetSelector.getSelectedId() - 1;
        if (presetIndex >= 0 && static_cast<size_t>(presetIndex) < presets.size()) {
            // ... (your existing preset info label logic) ...
            const auto& selectedPreset = presets[presetIndex];
            juce::String infoText = selectedPreset.label + " (Target LRA: ";
            infoText += juce::String(selectedPreset.targetLraMin, 1) + " LU - ";
            infoText += juce::String(selectedPreset.targetLraMax, 1) + " LU)";
            presetInfoLabel.setText (infoText, juce::dontSendNotification);
        } else {
            presetInfoLabel.setText ("Invalid Preset Selected", juce::dontSendNotification);
        }

        peakValueLabel.setVisible(true);
        lraValueLabel.setVisible(true);
        presetInfoLabel.setVisible(true);
        presetSelector.setEnabled(true);
        presetLabel.setEnabled(true);
        resetLraButton.setEnabled(true);
    }

    // General enabled state for labels that are always active (like bypass label)
    presetLabel.setColour(juce::Label::textColourId,Palette::Foreground);
    // bypassLabel.setEnabled(true); // Bypass label itself is always enabled
    // bypassLabel.setColour(juce::Label::textColourId, Palette::Foreground);
    
}

