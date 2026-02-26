#pragma once
#include <JuceHeader.h>
#include <cstdint>

/**
 *  TapeNoise — Generates filtered tape hiss.
 *
 *  Uses a fast xorshift32 PRNG, then a bandpass filter (HP ~200 Hz, LP ~8 kHz)
 *  to shape the noise into the classic "tape hiss" frequency band.
 *  amount 0..1 — scaled so it is subtle at 0.3, noticeable at 0.7.
 */
class TapeNoise
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;

        // One-pole HP at 200 Hz  (removes low rumble)
        hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 200.f / sr);

        // One-pole LP at 8000 Hz (removes ultra-high crackle)
        lpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 8000.f / sr);

        reset();
    }

    void reset()
    {
        hpState = 0.f;
        lpState = 0.f;
    }

    /** Call once per sample.  Returns noise scaled by amount. */
    float process (float amount) noexcept
    {
        if (amount < 0.001f)
            return 0.f;

        // Fast xorshift32 PRNG
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        // Normalise to [-1, 1]
        const float noise = static_cast<float> (static_cast<int32_t> (state)) * 4.6566e-10f;

        // Low-pass
        lpState = lpCoeff * lpState + (1.f - lpCoeff) * noise;

        // High-pass (band = LP - HP state)
        const float hp = lpState - hpState;
        hpState = hpCoeff * hpState + (1.f - hpCoeff) * lpState;

        return hp * amount * 0.04f;
    }

private:
    float    sr      = 44100.f;
    float    hpCoeff = 0.999f, lpCoeff = 0.5f;
    float    hpState = 0.f,    lpState = 0.f;
    uint32_t state   = 0xDEAD1337u; // xorshift32 seed
};
