#pragma once

// Include necessary JUCE modules and standard headers explicitly
#include <juce_core/juce_core.h>          // For juce::String, juce::Identifier
#include <juce_graphics/juce_graphics.h>  // For juce::Colour, juce::Colours
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>                         // For std::vector
#include <string>

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
    Ok,       // Green
    Reduced,  // Amber
    Loss,     // Red
    Bypassed,
    Measuring // New Listening State
};

//------------------------------------------------------------------------------
// Preset Definition
// Structure holding information for each dynamic range reference standard.
//------------------------------------------------------------------------------
struct DynamicsPreset
{
    std::string id;         // Unique identifier string for APVTS
    juce::String label;     // Display name for the UI in the ComboBox

    // LRA thresholds for traffic light logic (all values in LU)
    float lraThresholdRed;    // Below this LRA value, light is RED
    float lraThresholdAmber;  // Below this LRA value (and >= lraThresholdRed), light is AMBER
                              // Above or equal to this value, light is GREEN
    // For user display/info:
    float targetLraMin;       // The minimum LU of the genre's target range
    float targetLraMax;       // The maximum LU of the genre's target range
};

// Define the available presets with new LRA thresholds
const std::vector<DynamicsPreset> presets = {
    // id,         label,            lraThresholdRed, lraThresholdAmber, targetLraMin, targetLraMax
    { "edm",       "EDM/Club",           3.0f,            3.6f,              3.0f,         8.0f },
        { "pop_rock",  "Pop/Rock",           4.0f,            4.8f,              4.0f,         9.0f },
        { "classical", "Classical/Acoustic", 6.0f,            7.2f,              6.0f,        22.0f }
    };
    
    // The ParameterDefaults::preset might need adjustment if the default preset changes index.

//------------------------------------------------------------------------------
// Parameter Definitions
// Defines identifiers and default values for plugin parameters.
//------------------------------------------------------------------------------
namespace ParameterIDs
{
     // const juce::ParameterID bypass { "bypass", 1 }; // Use struct
     const juce::ParameterID preset { "preset", 1 }; // Use struct
     const juce::ParameterID peak   { "peak",   1 }; // Use struct
     const juce::ParameterID lra    { "lra",    1 }; // Use struct
     const juce::ParameterID resetLra { "resetLra", 1 };
}

// ParameterDefaults update based on new preset list if necessary
namespace ParameterDefaults
{
    // const bool bypass = false;
    const int  preset = 1;
    const float peak = -100.0f;
    const float lra = 0.0f; // Default LRA value (e.g., a high value to start Green)
    constexpr float LRA_MEASURING_DURATION = 6.0f; // Duration to measure LRA in seconds
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
        case DynamicsStatus::Ok:      return Palette::Ok;       // Green
        case DynamicsStatus::Reduced: return Palette::Reduced;  // Amber
        case DynamicsStatus::Loss:    return Palette::Loss;     // Red
        case DynamicsStatus::Measuring: return Palette::Secondary.withAlpha(0.6f);   // Dimmed foregroun for measuring
        case DynamicsStatus::Bypassed:/* fallthrough */
        default:                      return Palette::Muted; // Default case handles Bypassed and any unexpected values
    }
}

inline juce::String getStatusMessage (DynamicsStatus status)
{
    switch (status)
    {
        case DynamicsStatus::Ok:      return "Dynamics: OK";
        case DynamicsStatus::Reduced: return "Dynamics: Reduced";
        case DynamicsStatus::Loss:    return "Dynamics: Loss Risk";
        case DynamicsStatus::Measuring: return "Measuring LRA..."; // New Listening Message
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
            return Palette::Muted.withAlpha (BypassedAlpha);
        
        if (currentActualStatus == DynamicsStatus::Measuring)
        {
            // Example: All lights dim/off, or a specific "measuring" pattern
            // For simplicity here, let's make them look like "inactive" when measuring
            return Palette::Background.brighter(InactiveBackgroundBrightnessFactor);
        }

        if (currentActualStatus == lightTargetStatus)
            return getStatusColour (lightTargetStatus);

        return Palette::Background.brighter(InactiveBackgroundBrightnessFactor);
    }

    // getLightBorderThickness remains the same

    inline juce::Colour getLightBorderColour (DynamicsStatus lightTargetStatus, DynamicsStatus currentActualStatus)
    {
         if (currentActualStatus == DynamicsStatus::Bypassed)
            return Palette::Muted.darker(BypassedBorderDarkenFactor);

         if (currentActualStatus == DynamicsStatus::Measuring)
         {
             // Example: Same border as inactive
             return Palette::Foreground.withAlpha(InactiveAlpha);
         }

        if (currentActualStatus == lightTargetStatus)
            return getStatusColour (lightTargetStatus).darker(ActiveBorderDarkenFactor);

        return Palette::Foreground.withAlpha(InactiveAlpha);
    }
} // namespace TrafficLightMetrics
