#pragma once

// Include necessary JUCE module headers explicitly
#include <juce_gui_basics/juce_gui_basics.h> // Provides juce::Component, juce::Graphics, etc.

// Include project-specific headers required for definitions used in this header
#include "Constants.h" // Provides DynamicsStatus enum and potentially helper functions signatures if needed publicly

//==============================================================================
/**
    A custom JUCE component that visually represents a status using three
    vertically arranged "lights" (ellipses), similar to a traffic light.

    The active light corresponds to the current DynamicsStatus.
*/
class TrafficLightComponent : public juce::Component
{
public:
    //==============================================================================
    /** Constructor. */
    TrafficLightComponent();

    /** Destructor. */
    ~TrafficLightComponent() override;

    //==============================================================================
    /** @internal */
    void paint (juce::Graphics& g) override;

    /** @internal */
    void resized() override;

    //==============================================================================
    /**
        Updates the visual state of the traffic light component.

        @param newStatus The status to display (Ok, Reduced, Loss, Bypassed).
                       This determines which light is "active" and the overall appearance.
    */
    void setStatus (DynamicsStatus newStatus);

private:
    //==============================================================================
    /**
        Helper function called by paint() to draw a single light ellipse.

        @param g                The graphics context to draw into.
        @param bounds           The rectangular area where the light should be drawn.
        @param lightTargetStatus The status level this specific light represents (e.g., Ok, Reduced, Loss).
    */
    void paintLight (juce::Graphics& g, const juce::Rectangle<float>& bounds, DynamicsStatus lightTargetStatus);

    // The current status being displayed by the component. Initialized to Bypassed.
    DynamicsStatus currentStatus = DynamicsStatus::Bypassed;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrafficLightComponent)
};
