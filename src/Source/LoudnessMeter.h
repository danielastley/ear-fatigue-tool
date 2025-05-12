#pragma once

#include <juce_audio_basics/juce_audio_basics.h> // For juce::AudioBuffer
#include "ebur128.h" // <<< Crucial: Include the C library's header

/**
    A C++ wrapper around the libebur128 C library to simplify loudness
    measurements according to EBU R128 / ITU-R BS.1770.
*/
class LoudnessMeter
{
public:
    LoudnessMeter();
    ~LoudnessMeter();

    /**
        Initialises the loudness meter with the current sample rate and channel count.
        Must be called before processing audio.
        @param sampleRate The audio sample rate in Hz.
        @param numChannels The number of audio channels (e.g., 1 for mono, 2 for stereo).
        @param maxSamplesPerBlock The maximum expected block size.
    */
    void prepare(double sampleRate, int numChannels, int maxSamplesPerBlock);

    /** Resets the internal state of the loudness meter. */
    void reset();

    /**
        Processes a block of audio samples.
        @param buffer The audio buffer containing the samples to process.
    */
    void processBlock(const juce::AudioBuffer<float>& buffer);

    /**
        Gets the Short-Term loudness in LUFS.
        Call this after processBlock.
        @return Short-Term loudness in LUFS, or a very negative value if not enough data.
    */
    float getShortTermLoudness() const;

    /**
        Gets the Momentary loudness in LUFS.
        Call this after processBlock.
        @return Momentary loudness in LUFS, or a very negative value if not enough data.
    */
    float getMomentaryLoudness() const;


    /**
        Gets the Loudness Range (LRA) in LU.
        Call this after processBlock.
        @return Loudness Range in LU, or 0.0 if not enough data.
    */
    float getLoudnessRange() const;

    // Optional: Add getters for Integrated Loudness if needed later.
    // float getIntegratedLoudness() const;
    // Optional: Add getter for True Peak if needed later.
    // float getTruePeak() const;


private:
    ebur128_state* state = nullptr; // Pointer to the libebur128 state object
    int currentNumChannels = 0;
    double currentSampleRate = 0.0;

    // To store results if libebur128 doesn't update them every block
    // mutable allows these to be updated in const getter methods if we fetch from lib on demand
    mutable float lastShortTermLUFS = -144.0f; // A very low initial value
    mutable float lastMomentaryLUFS = -144.0f;
    mutable float lastLRA = 0.0f;
    
    std::vector<float> tempInterleaveBuffer; // <<< Member Variable for interleavedData


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeter)
};
