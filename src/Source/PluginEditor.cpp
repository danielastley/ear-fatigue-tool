#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Constants.h" // Constants.h defines your colours

//==============================================================================
DynamicsDoctorEditor::DynamicsDoctorEditor (DynamicsDoctorProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), processorRef (p), valueTreeState (vts)
{
    // Set component names via member initialiser list in the header is preferred,
    // but setting them here is also fine if needed after construction.
    // statusLabel.setName("statusLabel"); // Example if not done in header init list
    // presetSelector.setName("presetSelector");
    // bypassButton.setName("bypassButton");
    // ... etc ...

    // --- Traffic Light ---
    // Set colours or properties if needed, otherwise relies on its own paint routine
    addAndMakeVisible (trafficLight);

    // --- Status Label ---
    // Default text set in header init list, setting font/justification here
    statusLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    statusLabel.setJustificationType (juce::Justification::centred);
    // Initial text can be set here or rely on first timer callback
    statusLabel.setText ("Initializing...", juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    // --- Preset Selector ---
    // Default text set in header init list
    presetLabel.setFont (juce::FontOptions (14.0f));
    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetLabel.attachToComponent(&presetSelector, true); // Label to the left
    addAndMakeVisible (presetLabel);

    presetSelector.setTooltip ("Select the dynamic range reference standard");
    // Populate ComboBox items using the global `presets` array (ensure it's accessible)
    for (int i = 0; i < presets.size(); ++i)
        presetSelector.addItem (presets[i].label, i + 1); // ComboBox IDs must be > 0

    addAndMakeVisible (presetSelector);
    presetSelector.addListener (this); // Listen for immediate user changes (optional with timer)

    // Attachment: Connects the ComboBox value to the APVTS parameter
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, ParameterIDs::preset, presetSelector);

     // --- Bypass Toggle Button ---
    // Default text set in header init list
    bypassLabel.setFont (juce::FontOptions (14.0f));
    bypassLabel.setJustificationType (juce::Justification::centredRight);
    bypassLabel.attachToComponent(&bypassButton, true); // Label to the left
    addAndMakeVisible (bypassLabel);

    // Use LookAndFeel for toggle button style instead of text
    bypassButton.setButtonText("");
    bypassButton.setTooltip ("Bypass dynamics monitoring");
    addAndMakeVisible (bypassButton);
    bypassButton.addListener (this); // Listen for immediate user clicks (optional with timer)

    // Attachment: Connects the Button state (on/off) to the APVTS parameter
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        valueTreeState, ParameterIDs::bypass, bypassButton);


    // --- Value Labels ---
    // Defaults set in header init list, setting font/justification/colour here
    peakValueLabel.setFont (juce::FontOptions(12.0f));
    peakValueLabel.setJustificationType (juce::Justification::centred);
    peakValueLabel.setColour(juce::Label::textColourId, Palette::Foreground.withAlpha(0.7f)); // Use palette
    addAndMakeVisible (peakValueLabel);

    lufsValueLabel.setFont (juce::FontOptions(12.0f));
    lufsValueLabel.setJustificationType (juce::Justification::centred);
    lufsValueLabel.setColour(juce::Label::textColourId, Palette::Foreground.withAlpha(0.7f)); // Use palette
    addAndMakeVisible (lufsValueLabel);

    presetInfoLabel.setFont (juce::FontOptions(12.0f));
    presetInfoLabel.setJustificationType (juce::Justification::centred);
    presetInfoLabel.setColour(juce::Label::textColourId, Palette::Foreground.withAlpha(0.7f)); // Use palette
    addAndMakeVisible (presetInfoLabel);


    // Set editor size - Required call
    setSize (250, 400);

    // Start timer for periodic UI updates (e.g., 15 times per second)
    startTimerHz (15);

    // Perform an initial UI update based on the current processor state
    // This is important for when the editor is first opened.
    updateUIStatus();
}

DynamicsDoctorEditor::~DynamicsDoctorEditor()
{
    // Stop the timer safely
    stopTimer();

    // Remove listeners if they were added.
    // Note: Attachments automatically detach, but explicit listeners need explicit removal.
    presetSelector.removeListener(this);
    bypassButton.removeListener(this);

    // Attachments (`presetAttachment`, `bypassAttachment`) are automatically cleaned up
    // by their std::unique_ptr destructors (RAII).
}

//==============================================================================
void DynamicsDoctorEditor::paint (juce::Graphics& g)
{
    // Fill the background - essential for opaque components
    g.fillAll (Palette::Background); // Use palette colour

    // Optional: Draw custom graphics like borders, dividing lines etc.
    auto bounds = getLocalBounds();
    auto topAreaHeight = bounds.getHeight() * 0.6f; // Match resized logic if needed
    auto lightArea = bounds.removeFromTop(topAreaHeight).reduced(20);

    g.setColour(Palette::Foreground.withAlpha(0.1f)); // Use palette colour
    g.drawRoundedRectangle(lightArea.toFloat(), 5.0f, 2.0f); // Slightly thicker line?

    // Draw separator line
    g.setColour(Palette::Foreground.withAlpha(0.2f)); // Use palette colour
    g.drawLine(bounds.getX(), topAreaHeight, bounds.getRight(), topAreaHeight, 1.0f);
}

void DynamicsDoctorEditor::resized()
{
    // Layout the positions and sizes of child components within the editor bounds.

    // Define some constants for margins/padding
    constexpr int padding = 15;
    constexpr int controlHeight = 25;
    constexpr int labelWidth = 80; // Estimated width for labels next to controls
    constexpr int valueLabelHeight = 20;

    auto bounds = getLocalBounds().reduced(padding); // Overall content area

    // Top section: Status Label and Traffic Light
    auto topArea = bounds.removeFromTop(bounds.getHeight() * 0.6f);
    statusLabel.setBounds(topArea.removeFromTop(40));
    // Add padding around the traffic light within its allocated space
    trafficLight.setBounds(topArea.reduced(10));


    // Bottom section: Controls and Info Labels
    auto bottomArea = bounds; // Remaining space after topArea was removed
    auto infoArea = bottomArea.removeFromBottom(valueLabelHeight * 3 + 10); // Space for 3 labels + padding
    auto controlArea = bottomArea; // Space left for controls

    // Layout Controls (Preset and Bypass) side-by-side or vertically? Let's try vertical.
    controlArea.reduce(10, 5); // Reduce horizontally more, vertically less
    auto presetRow = controlArea.removeFromTop(controlHeight);
    controlArea.removeFromTop(5); // Add gap
    auto bypassRow = controlArea.removeFromTop(controlHeight);

    // Preset Row: Label (implicit via attachToComponent) + ComboBox
    // presetLabel is handled by attachToComponent positioning
    presetSelector.setBounds(presetRow.withLeft(presetRow.getX() + labelWidth).reduced(5, 0));

    // Bypass Row: Label (implicit via attachToComponent) + ToggleButton
    // bypassLabel is handled by attachToComponent positioning
    bypassButton.setBounds(bypassRow.withLeft(bypassRow.getX() + labelWidth).reduced(5, 0));


    // Layout Info Labels (Peak, LUFS, Preset Info)
    infoArea.reduce(0, 5); // Reduce top/bottom padding within info area
    auto peakArea = infoArea.removeFromTop(valueLabelHeight);
    auto lufsArea = infoArea.removeFromTop(valueLabelHeight);
    auto presetInfoArea = infoArea; // Remaining space for preset info

    peakValueLabel.setBounds(peakArea);
    lufsValueLabel.setBounds(lufsArea);
    presetInfoLabel.setBounds(presetInfoArea);
}

//==============================================================================
// Listener Callbacks (Optional but useful for immediate feedback)

void DynamicsDoctorEditor::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &presetSelector)
    {
       // Attachment already updated the processor's parameter state.
       // Call updateUIStatus to reflect the change immediately (e.g., presetInfoLabel).
       updateUIStatus();
    }
}

void DynamicsDoctorEditor::buttonClicked (juce::Button* buttonThatWasClicked)
{
     if (buttonThatWasClicked == &bypassButton)
    {
        // Attachment already updated the processor's parameter state.
        // Call updateUIStatus to reflect the change immediately (enable/disable controls).
        updateUIStatus();
    }
}

//==============================================================================
// Timer Callback (Essential for polling processor analysis results)
void DynamicsDoctorEditor::timerCallback()
{
    // This is called periodically by the timer started in the constructor.
    // It's the primary way to keep the UI synchronized with the processor's
    // internal state (status, peak, LUFS) which changes frequently.
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

    // Update the main Status Label text and colour
    // Assumes getStatusMessage and getStatusColour helper functions exist (maybe in Constants.h or ColourPalette.h?)
    statusLabel.setText (getStatusMessage(status), juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, getStatusColour(status));

    // Determine if the plugin is currently bypassed
    const bool isBypassed = (status == DynamicsStatus::Bypassed);

    // Update value labels only if not bypassed
    if (!isBypassed)
    {
        // Get current peak and LUFS values from the processor's parameters (thread-safe atomic reads)
        // Use the parameter system for values displayed/reported to the host/UI
        auto peakParam = valueTreeState.getRawParameterValue(ParameterIDs::peak);
        auto lufsParam = valueTreeState.getRawParameterValue(ParameterIDs::lufs);

        // Check if parameters are valid before dereferencing/loading
        if (peakParam != nullptr && lufsParam != nullptr)
        {
             float peak = peakParam->load();
             float lufs = lufsParam->load();

             peakValueLabel.setText ("Peak: " + juce::String(peak, 1) + " dBFS", juce::dontSendNotification);
             lufsValueLabel.setText ("LUFS: " + juce::String(lufs, 1) + " LUFS", juce::dontSendNotification);
        }
        else
        {
             // Handle case where parameters might not be ready (shouldn't normally happen after init)
             peakValueLabel.setText ("Peak: --- dBFS", juce::dontSendNotification);
             lufsValueLabel.setText ("LUFS: --- LUFS", juce::dontSendNotification);
        }


        // Update preset info label based on the *currently selected* preset in the ComboBox
        // Subtract 1 because ComboBox IDs are 1-based, while array indices are 0-based
        int presetIndex = presetSelector.getSelectedId() - 1;
        if (presetIndex >= 0 && presetIndex < presets.size())
        {
            const auto& selectedPreset = presets[presetIndex];
            presetInfoLabel.setText (selectedPreset.label + " (Ok >= " + juce::String(selectedPreset.minDiffOk, 1) + "dB, Red >= " + juce::String(selectedPreset.minDiffReduced, 1) + "dB)",
                                     juce::dontSendNotification);
        }
        else
        {
            presetInfoLabel.setText ("Invalid Preset Selected", juce::dontSendNotification);
        }

        // Ensure value labels are visible when not bypassed
        peakValueLabel.setVisible(true);
        lufsValueLabel.setVisible(true);
        presetInfoLabel.setVisible(true);
    }
    else // Is Bypassed
    {
         // Hide or clear value labels when bypassed for clarity
         peakValueLabel.setVisible(false);
         lufsValueLabel.setVisible(false);
         presetInfoLabel.setVisible(false);

         // Alternative: Keep visible but show placeholder text
         // peakValueLabel.setText("Peak: (Bypassed)", juce::dontSendNotification);
         // lufsValueLabel.setText("LUFS: (Bypassed)", juce::dontSendNotification);
         // presetInfoLabel.setText("(Bypassed)", juce::dontSendNotification);
    }


     // Enable/disable controls based on bypass state
     // The preset selector should typically be disabled when bypassed.
     presetSelector.setEnabled(!isBypassed);

     // Also visually indicate disablement on associated labels if desired
     // (Can use setEnabled or change colour via LookAndFeel or directly)
     presetLabel.setEnabled(!isBypassed);
     presetLabel.setColour(juce::Label::textColourId,
                           isBypassed ? Palette::DisabledText : Palette::Foreground);

     // Bypass Button state is handled by the attachment, but the label can be always enabled.
     bypassLabel.setEnabled(true); // Label itself doesn't need disabling
     bypassLabel.setColour(juce::Label::textColourId, Palette::Foreground); // Ensure it's normal colour
}
