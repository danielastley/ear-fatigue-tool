#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Colour Palette (approximating the web version)
// Primary: Dark grey (#333333)
const juce::Colour colourPrimary = juce::Colour::fromString ("FF333333");
// Secondary: Light grey (#DDDDDD)
const juce::Colour colourSecondary = juce::Colour::fromString ("FFDDDDDD");
// Accent/Ok: Teal (#008080)
const juce::Colour colourOk = juce::Colour::fromString ("FF008080");
// Reduced: Orange (#FFA500)
const juce::Colour colourReduced = juce::Colour::fromString ("FFFFA500");
// Loss: Red (#FF0000)
const juce::Colour colourLoss = juce::Colour::fromString ("FFFF0000");
// Muted/Bypassed: Grey
const juce::Colour colourMuted = juce::Colours::grey;
const juce::Colour colourBackground = colourPrimary; // Main background
const juce::Colour colourForeground = colourSecondary; // Main text/elements

enum class DynamicsStatus
{
    Ok,
    Reduced,
    Loss,
    Bypassed
};

struct DynamicsPreset
{
    juce::String id;
    juce::String label;
    float minDiffOk;
    float minDiffReduced;
};

// Define the presets
const std::vector<DynamicsPreset> presets = {
    { "lufs-20", "LUFS -20", 12.0f, 6.0f },
    { "lufs-14", "LUFS -14", 6.0f, 3.0f },
    { "lufs-12", "LUFS -12", 3.0f, 1.5f }
};

namespace ParameterIDs
{
    const juce::String bypass { "bypass" };
    const juce::String preset { "preset" };
    const juce::String peak   { "peak" };   // For reporting values to host/UI
    const juce::String lufs   { "lufs" };   // For reporting values to host/UI
}

namespace ParameterDefaults
{
    const bool bypass = false;
    const int  preset = 0; // Index of the default preset
}

inline juce::Colour getStatusColour (DynamicsStatus status)
{
    switch (status)
    {
        case DynamicsStatus::Ok:      return colourOk;
        case DynamicsStatus::Reduced: return colourReduced;
        case DynamicsStatus::Loss:    return colourLoss;
        case DynamicsStatus::Bypassed:
        default:                      return colourMuted;
    }
}

inline juce::String getStatusMessage (DynamicsStatus status)
{
    switch (status)
    {
        case DynamicsStatus::Ok:      return "Dynamics in Range";
        case DynamicsStatus::Reduced: return "Dynamics Reduced";
        case DynamicsStatus::Loss:    return "Dynamics Loss (Ear Fatigue Likely)";
        case DynamicsStatus::Bypassed:
        default:                      return "Monitoring Bypassed";
    }
}

inline juce::Colour getLightColour (DynamicsStatus lightTargetStatus, DynamicsStatus currentStatus)
{
    if (currentStatus == DynamicsStatus::Bypassed)
        return colourMuted.withAlpha (0.3f); // Dimmed grey for bypassed

    if (currentStatus == lightTargetStatus)
        return getStatusColour (lightTargetStatus);

    // Inactive light
    return colourBackground.brighter(0.2f); // Slightly lighter than background
}

inline float getLightBorderThickness() { return 4.0f; }

inline juce::Colour getLightBorderColour (DynamicsStatus lightTargetStatus, DynamicsStatus currentStatus)
{
     if (currentStatus == DynamicsStatus::Bypassed)
        return colourMuted.darker(0.3f);

    if (currentStatus == lightTargetStatus)
        return getStatusColour (lightTargetStatus).darker(0.5f); // Darker border for active

    // Inactive border
    return colourForeground.withAlpha(0.3f); // Dimmed border
}
