#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

/**
 *  ShimmerChorus — Granular pitch shifter (+1 octave) for shimmer reverb.
 *
 *  Algorithm: two overlapping grains, each reading the delay buffer at 2×
 *  speed (= +1 octave), windowed with Hanning to avoid clicks at crossfades.
 *
 *  Two grains are offset by half a grain cycle so their windows complement
 *  each other — when one fades in the other fades out, giving a continuous
 *  output with no silent gaps.
 *
 *  This is the same core algorithm as Valhalla's pitch-shifted reverbs.
 *
 *  Usage inside shimmer reverb:
 *    // Each sample:
 *    revL = spring.process(dry + shimmerFeedback);
 *    shimmerFeedback = shifter.process(revL) * amount;
 *
 *  amount 0..1
 */
class ShimmerChorus
{
public:
    static constexpr int GRAIN = 4096;  // ~93 ms at 44.1 kHz — smooth crossfades
    static constexpr int BUF   = GRAIN * 4; // circular buffer, must be > 3×GRAIN

    void prepare (double /*sampleRate*/)
    {
        reset();
    }

    void reset()
    {
        buf.fill (0.f);
        wPos = 0;

        // Grain 2 starts halfway through its cycle so windows complement grain 1
        r1 = -(float) GRAIN;
        r2 = -(float) GRAIN * 0.5f;
    }

    /**
     *  Process one sample.
     *  Returns the pitch-shifted (+1 octave) version of x, scaled by amount.
     */
    float process (float x, float amount) noexcept
    {
        if (amount < 0.001f)
            return 0.f;

        // ── Write to circular buffer ──────────────────────────────────
        buf[wPos & (BUF - 1)] = x;

        // ── Compute Hanning window for each grain ─────────────────────
        // phase = (r - (wPos - GRAIN)) / GRAIN  →  0..1 as r goes (wPos-GRAIN)..wPos
        const float phase1 = juce::jlimit (0.f, 1.f,
            (r1 - static_cast<float> (wPos) + static_cast<float> (GRAIN))
            / static_cast<float> (GRAIN));

        const float phase2 = juce::jlimit (0.f, 1.f,
            (r2 - static_cast<float> (wPos) + static_cast<float> (GRAIN))
            / static_cast<float> (GRAIN));

        const float w1 = 0.5f - 0.5f * std::cos (phase1 * juce::MathConstants<float>::twoPi);
        const float w2 = 0.5f - 0.5f * std::cos (phase2 * juce::MathConstants<float>::twoPi);

        // ── Read both grains with linear interpolation ────────────────
        const float s1 = readLinear (r1);
        const float s2 = readLinear (r2);

        const float out = s1 * w1 + s2 * w2;

        // ── Advance read pointers at 2× write speed (= +1 octave) ─────
        r1 += 2.f;
        r2 += 2.f;
        ++wPos;

        // ── Reset grains once they have caught up with the write head ──
        // (phase = 1  ↔  r == wPos)
        if (r1 >= static_cast<float> (wPos))
            r1 = static_cast<float> (wPos) - static_cast<float> (GRAIN);

        if (r2 >= static_cast<float> (wPos))
            r2 = static_cast<float> (wPos) - static_cast<float> (GRAIN);

        return out * amount;
    }

private:
    std::array<float, BUF> buf = {};
    int   wPos = 0;
    float r1   = 0.f, r2 = 0.f;

    /** Linear-interpolated read from the circular buffer. */
    float readLinear (float pos) const noexcept
    {
        // Wrap into buffer range
        float p = pos;
        while (p < 0.f)        p += static_cast<float> (BUF);
        while (p >= static_cast<float> (BUF)) p -= static_cast<float> (BUF);

        const int   i0   = static_cast<int> (p) & (BUF - 1);
        const int   i1   = (i0 + 1) & (BUF - 1);
        const float frac = p - std::floor (p);

        return buf[i0] * (1.f - frac) + buf[i1] * frac;
    }
};
