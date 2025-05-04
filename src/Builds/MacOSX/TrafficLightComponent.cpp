#include "TrafficLightComponent.h"

TrafficLightComponent::TrafficLightComponent()
{
    // In your constructor, you might want to add child components if needed
    // For this simple component, we just need painting.
}

TrafficLightComponent::~TrafficLightComponent()
{
    // Destructor
}

void TrafficLightComponent::paint (juce::Graphics& g)
{
    // Optional: Fill background if needed, though the parent usually handles this
    // g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto bounds = getLocalBounds().toFloat();
    float lightHeight = bounds.getHeight() / 3.0f;
    float spacing = 5.0f; // Spacing between lights
    float diameter = juce::jmin (lightHeight - spacing, bounds.getWidth() - spacing * 2.0f);
    float x = bounds.getCentreX() - diameter / 2.0f;

    juce::Rectangle<float> topLightBounds (x, spacing, diameter, diameter);
    juce::Rectangle<float> midLightBounds (x, topLightBounds.getBottom() + spacing, diameter, diameter);
    juce::Rectangle<float> botLightBounds (x, midLightBounds.getBottom() + spacing, diameter, diameter);

    paintLight (g, topLightBounds, DynamicsStatus::Loss);    // Red light at the top
    paintLight (g, midLightBounds, DynamicsStatus::Reduced); // Orange light in the middle
    paintLight (g, botLightBounds, DynamicsStatus::Ok);      // Green light at the bottom
}

void TrafficLightComponent::paintLight (juce::Graphics& g, const juce::Rectangle<float>& bounds, DynamicsStatus lightStatus)
{
    juce::Colour fillColour = getLightColour (lightStatus, currentStatus);
    juce::Colour borderColour = getLightBorderColour (lightStatus, currentStatus);
    float borderThickness = getLightBorderThickness();

    g.setColour (fillColour);
    g.fillEllipse (bounds);

    g.setColour (borderColour);
    g.drawEllipse (bounds, borderThickness);
}


void TrafficLightComponent::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains.
}

void TrafficLightComponent::setStatus (DynamicsStatus newStatus)
{
    if (currentStatus != newStatus)
    {
        currentStatus = newStatus;
        repaint(); // Trigger a repaint to update the visuals
    }
}
