#include "TrafficLightComponent.h"
#include "Constants.h" // Needed for DynamicsStatus, helper functions, Palette

//==============================================================================
TrafficLightComponent::TrafficLightComponent()
{
    // Constructor: Initialize member variables if needed.
    // 'currentStatus' is likely initialized in the header declaration
    // (e.g., DynamicsStatus currentStatus = DynamicsStatus::Bypassed;)
    // No child components are added here as this component purely paints itself.
}

TrafficLightComponent::~TrafficLightComponent()
{
    // Destructor: Clean up resources if any were allocated. None in this case.
}

//==============================================================================
void TrafficLightComponent::paint (juce::Graphics& g)
{
    // No need to fill background here if the parent component (Editor) already does.
    // If this component needed transparency or a specific background, you'd fill here.

    // Use floating-point rectangles for potentially smoother drawing
    auto bounds = getLocalBounds().toFloat();

    // Define spacing and calculate optimal diameter based on available space
    constexpr float spacing = 5.0f; // Space between lights and from edges
    const float totalHeightForLights = bounds.getHeight() - (spacing * 4.0f); // 4 gaps (top, middle, middle, bottom)
    const float lightDiameter = juce::jmin (totalHeightForLights / 3.0f, // Max height per light
                                          bounds.getWidth() - (spacing * 2.0f)); // Max width

    // Ensure diameter is positive
    if (lightDiameter <= 0.0f)
        return; // Cannot draw if space is too small

    const float x = bounds.getCentreX() - (lightDiameter / 2.0f);
    float currentY = spacing; // Start Y position

    // Define bounds for each light dynamically
    juce::Rectangle<float> topLightBounds (x, currentY, lightDiameter, lightDiameter);
    currentY += lightDiameter + spacing; // Move Y down for the next light
    juce::Rectangle<float> midLightBounds (x, currentY, lightDiameter, lightDiameter);
    currentY += lightDiameter + spacing; // Move Y down again
    juce::Rectangle<float> botLightBounds (x, currentY, lightDiameter, lightDiameter);

    // Paint each light using the helper function, associating each position with a status level
    // Note the order: typically Red (Loss) at top, Green (Ok) at bottom.
    paintLight (g, topLightBounds, DynamicsStatus::Loss);    // Top light represents 'Loss'
    paintLight (g, midLightBounds, DynamicsStatus::Reduced); // Middle light represents 'Reduced'
    paintLight (g, botLightBounds, DynamicsStatus::Ok);      // Bottom light represents 'Ok'
}

//==============================================================================
void TrafficLightComponent::paintLight (juce::Graphics& g, const juce::Rectangle<float>& bounds, DynamicsStatus lightTargetStatus)
{
    // Retrieve colours and border thickness using the helper functions from Constants.h
    // These helpers determine the appearance based on the light's target status
    // and the component's current overall status.
    // Using the specific namespace clarifies where these helpers come from.
    const juce::Colour fillColour = TrafficLightMetrics::getLightColour (lightTargetStatus, currentStatus);
    const juce::Colour borderColour = TrafficLightMetrics::getLightBorderColour (lightTargetStatus, currentStatus);
    const float borderThickness = TrafficLightMetrics::getLightBorderThickness();

    // Draw the fill
    g.setColour (fillColour);
    g.fillEllipse (bounds);

    // Draw the border
    g.setColour (borderColour);
    g.drawEllipse (bounds, borderThickness);
}

//==============================================================================
void TrafficLightComponent::resized()
{
    // This component doesn't contain child components, so resized() can be empty.
    // The painting logic in paint() dynamically adapts to the component's current bounds.
}

//==============================================================================
void TrafficLightComponent::setStatus (DynamicsStatus newStatus)
{
    // Only update and trigger a repaint if the status has actually changed.
    // This prevents unnecessary repaints if the status is updated repeatedly with the same value.
    if (currentStatus != newStatus)
    {
        currentStatus = newStatus;
        repaint(); // Schedule a repaint to reflect the visual change
    }
}
