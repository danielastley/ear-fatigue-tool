#pragma once

// Include necessary JUCE modules and standard headers explicitly
#include <juce_core/juce_core.h>          // For juce::String, juce::Identifier
#include <juce_graphics/juce_graphics.h>  // For juce::Colour, juce::Colours
#include <vector>                         // For std::vector

//------------------------------------------------------------------------------
// Colour Palette
// Defines the core colours used throughout the plugin UI.
//------------------------------------------------------------------------------
namespace Palette
{
    // Using juce::Colour::fromString is fine. Ensure alpha is specified (FF prefix for opaque).
    const juce::Colour Primary        = juce::Colour::fromString ("FF333333"); // Dark grey background
    const juce::Colour Secondary      = juce::Colour::fromString ("FFDDDDDD"); // Light grey foreground/text
    const juce::Colour Ok             = juce::Colour::fromString ("FF008080"); // Teal accent
    const juce::Colour Reduced        = juce::Colour::fromString ("FFFFA500"); // Orange accent
    const juce::Colour Loss           = juce::Colour::fromString ("FFFF0000"); // Red accent
    const juce::Colour Muted          = juce::Colours::grey;                    // Grey for bypassed/inactive
    const juce::Colour DisabledText   = Muted.withAlpha(0.6f);               // Example for disabled text

    // Convenience aliases matching original naming if preferred elsewhere
    const juce::Colour Background     = Primary;
    const juce::Colour Foreground     = Secondary;
}

//------------------------------------------------------------------------------
// Status Definition
// Represents the different dynamic states the plugin can report.
//------------------------------------------------------------------------------
enum class DynamicsStatus
{
    Ok,
    Reduced,
    Loss,
    Bypassed
};

//------------------------------------------------------------------------------
// Preset Definition
// Structure holding information for each dynamic range reference standard.
//------------------------------------------------------------------------------
struct DynamicsPreset
{
    juce::String id;            // Unique identifier string
    juce::String label;         // Display name for the UI
    float minDiffOk;        // Minimum Peak-LUFS difference (dB) to be considered 'Ok'
    float minDiffReduced;   // Minimum Peak-LUFS difference (dB) to be considered 'Reduced'
};

// Define the available presets
// Using a const std::vector is standard and appropriate here.
const std::vector<DynamicsPreset> presets = {
    { "lufs-20", "LUFS -20 Reference", 12.0f, 6.0f }, // Example: Added "Reference" to label
    { "lufs-14", "LUFS -14 Reference", 6.0f, 3.0f },
    { "lufs-12", "LUFS -12 Reference", 3.0f, 1.5f }
    // Add more presets here if needed
};

//------------------------------------------------------------------------------
// Parameter Definitions
// Defines identifiers and default values for plugin parameters.
//------------------------------------------------------------------------------
namespace ParameterIDs
{
    // Using juce::Identifier can be slightly more efficient than juce::String
    // for parameter lookups, but juce::String is also perfectly fine.
    // Choose one style and be consistent. Let's stick to String as per original.
    const juce::String bypass { "bypass" };
    const juce::String preset { "preset" };
    const juce::String peak   { "peak" };   // Reporting parameter: True Peak (dBFS)
    const juce::String lufs   { "lufs" };   // Reporting parameter: Integrated LUFS
}

namespace ParameterDefaults
{
    // Use constexpr for compile-time constants where possible
    constexpr bool bypass = false;
    constexpr int  preset = 0; // Default preset index (references `presets` vector)
    // Note: Default values for reporting parameters (peak/lufs) are set in createParameterLayout
}

//------------------------------------------------------------------------------
// Status Helper Functions
// Provide UI elements with appropriate colours and text based on status.
// Defined inline as they are small and used in a header.
//------------------------------------------------------------------------------
inline juce::Colour getStatusColour (DynamicsStatus status)
{
    switch (status)
    {
        case DynamicsStatus::Ok:      return Palette::Ok;
        case DynamicsStatus::Reduced: return Palette::Reduced;
        case DynamicsStatus::Loss:    return Palette::Loss;
        case DynamicsStatus::Bypassed:/* fallthrough */ // Explicit fallthrough comment if needed
        default:                      return Palette::Muted; // Default case handles Bypassed and any unexpected values
    }
}

inline juce::String getStatusMessage (DynamicsStatus status)
{
    switch (status)
    {
        case DynamicsStatus::Ok:      return "Dynamics: OK"; // Slightly shorter messages
        case DynamicsStatus::Reduced: return "Dynamics: Reduced";
        case DynamicsStatus::Loss:    return "Dynamics: Loss Risk"; // Changed message slightly
        case DynamicsStatus::Bypassed:/* fallthrough */
        default:                      return "Monitoring Bypassed";
    }
}

//------------------------------------------------------------------------------
// Traffic Light Helper Functions (Specific to TrafficLightComponent)
// Provide colours and styles for the traffic light display based on status.
//------------------------------------------------------------------------------
namespace TrafficLightMetrics
{
    // Make constants related to the traffic light easily configurable
    constexpr float LightBorderThickness = 2.0f; // Thinner border?
    constexpr float BypassedAlpha = 0.3f;
    constexpr float InactiveAlpha = 0.3f;
    constexpr float ActiveBorderDarkenFactor = 0.5f;
    constexpr float InactiveBackgroundBrightnessFactor = 0.2f;
    constexpr float BypassedBorderDarkenFactor = 0.3f;


    inline juce::Colour getLightColour (DynamicsStatus lightTargetStatus, DynamicsStatus currentActualStatus)
    {
        if (currentActualStatus == DynamicsStatus::Bypassed)
            return Palette::Muted.withAlpha (BypassedAlpha); // Dimmed grey for bypassed

        // If the current actual status matches the status this light represents, use the status colour
        if (currentActualStatus == lightTargetStatus)
            return getStatusColour (lightTargetStatus); // Use the main status colour (Ok, Reduced, Loss)

        // Otherwise, the light is inactive
        return Palette::Background.brighter(InactiveBackgroundBrightnessFactor); // Slightly lighter than background
    }

    inline float getLightBorderThickness()
    {
        return LightBorderThickness;
    }

    inline juce::Colour getLightBorderColour (DynamicsStatus lightTargetStatus, DynamicsStatus currentActualStatus)
    {
         if (currentActualStatus == DynamicsStatus::Bypassed)
            return Palette::Muted.darker(BypassedBorderDarkenFactor);

        // If this light is the active one, use a darkened version of its status colour
        if (currentActualStatus == lightTargetStatus)
            return getStatusColour (lightTargetStatus).darker(ActiveBorderDarkenFactor);

        // Otherwise, it's an inactive border
        return Palette::Foreground.withAlpha(InactiveAlpha); // Dimmed foreground colour
    }
} // namespace TrafficLightMetrics
