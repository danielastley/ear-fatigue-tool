#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Constants.h"

class TrafficLightComponent : public juce::Component
{
public:
    TrafficLightComponent();
    ~TrafficLightComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setStatus (DynamicsStatus newStatus);

private:
    void paintLight (juce::Graphics& g, const juce::Rectangle<float>& bounds, DynamicsStatus lightStatus);

    DynamicsStatus currentStatus = DynamicsStatus::Bypassed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrafficLightComponent)
};
