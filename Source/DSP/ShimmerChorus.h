#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

/**
 *  ShimmerChorus — 4-voice long-delay modulated chorus that adds an
 *  ethereal, shimmering character to the spring reverb tail.
 *
 *  Each voice:
 *   • Delay 35–80 ms (slightly detuned per voice)
 *   • Slow LFO 0.05–0.17 Hz for lush modulation
 *   • Mild feedback (0.45) building into shimmer
 *   • Linear interpolation for smooth modulation
 *
 *  amount 0..1  (0 = bypass, 1 = full effect)
 */
class ShimmerChorus
{
public:
    static constexpr int NUM_VOICES = 4;

    void prepare (double sampleRate)
    {
        sr = static_cast<float> (sampleRate);

        constexpr float basesMs [NUM_VOICES] = { 37.f, 51.f, 64.f, 79.f };
        constexpr float lfosHz  [NUM_VOICES] = { 0.053f, 0.083f, 0.131f, 0.173f };
        constexpr float depthMs [NUM_VOICES] = { 8.f,  10.f,  12.f,   9.f };

        bufSize = static_cast<int> (0.12f * sr) + 8; // 120 ms max

        for (int v = 0; v < NUM_VOICES; ++v)
        {
            bufs[v].assign (bufSize, 0.f);
            wPos[v]    = 0;
            baseD[v]   = basesMs[v] * 0.001f * sr;
            lfoPhase[v]= static_cast<float> (v) / NUM_VOICES;
            lfoInc[v]  = lfosHz[v] / sr;
            lfoDepth[v]= depthMs[v] * 0.001f * sr;
            fbState[v] = 0.f;
        }
    }

    void reset()
    {
        for (int v = 0; v < NUM_VOICES; ++v)
        {
            std::fill (bufs[v].begin(), bufs[v].end(), 0.f);
            wPos[v]    = 0;
            lfoPhase[v]= static_cast<float> (v) / NUM_VOICES;
            fbState[v] = 0.f;
        }
    }

    /** Process one sample.  Returns shimmer output. */
    float process (float x, float amount) noexcept
    {
        if (amount < 0.001f)
            return 0.f;

        float out = 0.f;

        for (int v = 0; v < NUM_VOICES; ++v)
        {
            // LFO
            const float lfoVal = std::sin (lfoPhase[v] * juce::MathConstants<float>::twoPi);
            lfoPhase[v] += lfoInc[v];
            if (lfoPhase[v] >= 1.f) lfoPhase[v] -= 1.f;

            const float dSamples = juce::jlimit (1.f, static_cast<float> (bufSize - 2),
                                                 baseD[v] + lfoVal * lfoDepth[v]);

            // Write (input + mild feedback for buildup)
            bufs[v][wPos[v]] = x + fbState[v] * 0.45f;

            // Linear interpolated read
            float rPos = static_cast<float> (wPos[v]) - dSamples;
            while (rPos < 0.f) rPos += static_cast<float> (bufSize);

            const int   ri0  = static_cast<int> (rPos) % bufSize;
            const int   ri1  = (ri0 + 1) % bufSize;
            const float frac = rPos - std::floor (rPos);
            const float samp = bufs[v][ri0] * (1.f - frac) + bufs[v][ri1] * frac;

            fbState[v] = samp;
            out += samp;

            wPos[v] = (wPos[v] + 1) % bufSize;
        }

        return out * (amount * 0.6f / static_cast<float> (NUM_VOICES));
    }

private:
    float sr      = 44100.f;
    int   bufSize = 0;

    std::array<std::vector<float>, NUM_VOICES> bufs;
    std::array<int,   NUM_VOICES> wPos     = {};
    std::array<float, NUM_VOICES> baseD    = {};
    std::array<float, NUM_VOICES> lfoPhase = {};
    std::array<float, NUM_VOICES> lfoInc   = {};
    std::array<float, NUM_VOICES> lfoDepth = {};
    std::array<float, NUM_VOICES> fbState  = {};
};
