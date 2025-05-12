#include "LoudnessMeter.h"
#include <juce_core/system/juce_PlatformDefs.h> // For jassert, etc.
// #include <juce_core/logging/juce_Logger.h>  // For DBG
#include <limits> // For std::numeric_limits
#include <vector> // Added for std::vector

LoudnessMeter::LoudnessMeter()
{
    // The ebur128_state will be initialized in prepare()
}

LoudnessMeter::~LoudnessMeter()
{
    if (state != nullptr)
    {
        ebur128_destroy(&state); // Clean up the libebur128 state
        state = nullptr;
    }
}
// ==============================================================================
void LoudnessMeter::prepare(double sampleRate, int numChannels, int /*maxSamplesPerBlock*/)
{
    // If already initialized, destroy the old state first
    if (state != nullptr)
    {
        ebur128_destroy(&state);
        state = nullptr;
    }

    currentSampleRate = sampleRate;
    currentNumChannels = numChannels;

    // Initialize libebur128
    // EBUR128_MODE_S for Short-Term, EBUR128_MODE_M for Momentary, EBUR128_MODE_LRA for Loudness Range
    // We can combine modes using bitwise OR.
    // Let's enable all modes we plan to use: Short-Term, Momentary, and LRA.
    // Integrated loudness (EBUR128_MODE_I) is also often useful.
    unsigned int mode = EBUR128_MODE_S | EBUR128_MODE_M | EBUR128_MODE_LRA; // Add EBUR128_MODE_I if you want integrated

    state = ebur128_init(static_cast<unsigned>(numChannels),
                         static_cast<unsigned>(sampleRate),
                         mode);

    if (state == nullptr)
    {
        jassertfalse; // "Failed to initialize libebur128!"
        return; // Early exit if init failed
    }

    // For simple mono/stereo, the default usually works. Example for stereo:
    if (numChannels == 1 && state != nullptr) {
        ebur128_set_channel(state, 0, EBUR128_CENTER); // Or EBUR128_DUAL_MONO
    } else if (numChannels == 2 && state != nullptr) {
        ebur128_set_channel(state, 0, EBUR128_LEFT);
        ebur128_set_channel(state, 1, EBUR128_RIGHT);
    }
    // Add more ebur128_set_channel calls for other layouts if supporting more than stereo.

    reset(); // Reset internal values
}

// ===============================================================================
void LoudnessMeter::reset()
{
    lastShortTermLUFS = -144.0f; // Or -std::numeric_limits<float>::infinity();
    lastMomentaryLUFS = -144.0f; // Or -std::numeric_limits<float>::infinity();
    lastLRA = 0.0f;
    // Note: A 'harder' reset for libebur128's internal LRA history would be to call prepare() again.
    // For now, this resets our cached values. libebur128's LRA is continuous.
}

// ===============================================================================
void LoudnessMeter::processBlock(const juce::AudioBuffer<float>& buffer)
{
    if (state == nullptr || buffer.getNumSamples() == 0)
    {
        return; // Not prepared or empty buffer
    }
    
    // Ensure channel count matches what libebur128 was initialized with
    jassert (buffer.getNumChannels() == currentNumChannels);
    if (buffer.getNumChannels() != currentNumChannels)
    {
        DBG("LoudnessMeter::processBlock channel count mismatch! Expected: " << currentNumChannels << " Got: " << buffer.getNumChannels());
        return; // Mismatch, can't process
    }
        
        const int numFrames = buffer.getNumSamples(); // In libebur128, 'frames means samples per channel.
        const int numChans = currentNumChannels;
    
    // A temporary vector to hold the interleaved audio data.
    // This will be allocated on each call in this version
    std::vector<float> interleavedData;
    interleavedData.resize(static_cast<size_t>(numFrames * numChans)); // static_cast for size_t
    
    // Interleaving logic:
    for (int i = 0; i < numFrames; ++i) // Outer loop: Iterate through each sample index (frame). 'i' is the sample index
    {
        for (int ch = 0; ch < numChans; ++ch) // Inner Loop: Iterate through each channel for that sample index. 'ch' is the channel index.
        {
            // 1. Get the read pointer for the CURRENT channel 'ch'
            const float* channelReadPtr = buffer.getReadPointer(ch);
            
            // 2. Get the sample at the CURRENT sample index 'i' from that channel
            float sampleValue = channelReadPtr[i];
            
            // 3. Place this sampleValue into the correct position in the interleavedData buffer
            interleavedData[static_cast<size_t>(i * numChans + ch)] = sampleValue;
        }
    }
    
    // Add inerleaved frames to libebur128
    int err = ebur128_add_frames_float(state, interleavedData.data(), static_cast<size_t>(numFrames));
    
    if (err != 0)
    {
        DBG("libebur128_add_frames_float error code: " << err);
        // In a release build, you might want to log this or handle it more gracefully than just asserting. For debugging, an assert is fine.
        jassertfalse;  // Error in libeur128 processing
        
    }
    
    // Note: The getter methods will be called from PluginProcessor to retrieve values.
    // We don't necessarily need to update lastShortTermLUFS etc. here, as the getters
    // will fetch the latest from libebur128.
}

// ===========================================================================
float LoudnessMeter::getShortTermLoudness() const
{
    if (state == nullptr) return -std::numeric_limits<float>::infinity();;
    double lufs_s;
    // EBUR128_LOUDNESS_SHORTTERM uses a 3-second window
    if (ebur128_loudness_shortterm(state, &lufs_s) == 0) { // 0 means success
        lastShortTermLUFS = static_cast<float>(lufs_s); // Update mutable cache
        return (lufs_s < -140.0) ? -std::numeric_limits<float>::infinity() : static_cast<float>(lufs_s); // Handle very low values
    }
    // DBG("Error getting short-term loudness from libebur128");
    return lastShortTermLUFS; // Return cached value on error
}

// ============================================================================
float LoudnessMeter::getMomentaryLoudness() const
{
    if (state == nullptr) return -std::numeric_limits<float>::infinity();;
    double lufs_m;
    // EBUR128_LOUDNESS_MOMENTARY uses a 400ms window
    if (ebur128_loudness_momentary(state, &lufs_m) == 0) { // 0 means success
        lastMomentaryLUFS = static_cast<float>(lufs_m); // Update mutable cache
        return (lufs_m < -140.0) ? -std::numeric_limits<float>::infinity() : static_cast<float>(lufs_m); // Handle very low values
    }
    // DBG("Error getting momentary loudness from libebur128");
    return lastMomentaryLUFS; // Return cached value on error
}

// ============================================================================
float LoudnessMeter::getLoudnessRange() const
{
    if (state == nullptr) return 0.0f;  // LRA is typically positive or zero
    double lra_val; // Renamed to avoid conflict with member 'last LRA'
        if (ebur128_loudness_range(state, &lra_val) == 0) { // 0 means success
        lastLRA = static_cast<float>(lra_val);
        return static_cast<float>(lra_val);
    }
    // DBG ("Error getting loudness range from libebur128");
    return lastLRA; // Return cached value on error
}
