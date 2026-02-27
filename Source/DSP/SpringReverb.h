#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

/**
 *  SpringReverb – Schroeder-based reverberator tuned for spring character.
 *
 *  Differs from a room reverb:
 *   • Shorter delay lines → metallic / high-density
 *   • Comb filters with damping → "boing" decay
 *   • Series allpass → diffusion / chirp
 *   • Gentle pre-delay (~8 ms)
 *
 *  v1.5: Added "boing" attack resonator
 *   • Digital resonator at 1200 Hz (spring mechanical resonance)
 *   • ~200 ms exponential decay — characteristic metallic "boing" ringing
 *   • Normalised input gain → unity contribution at resonance
 */
class SpringReverb
{
public:
    static constexpr int NUM_COMBS   = 8;
    static constexpr int NUM_ALLPASS = 4;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        // Comb filter delay times (ms) – spring-tuned, mutually prime
        const float combMs[NUM_COMBS] = {
            25.31f, 26.94f, 28.96f, 30.75f,
            32.25f, 33.84f, 35.28f, 36.80f
        };
        // Allpass delay times (ms)
        const float apMs[NUM_ALLPASS] = { 5.10f, 7.73f, 10.00f, 12.61f };
        // Pre-delay (~8 ms)
        const float preMs = 8.0f;

        auto msToSamples = [&] (float ms) {
            return static_cast<int> (ms * 0.001 * sampleRate) + 1;
        };

        // Pre-delay
        preDelayBuf.assign (msToSamples (preMs), 0.0f);
        preDelayPos = 0;

        for (int i = 0; i < NUM_COMBS; ++i)
        {
            combBufs[i].assign (msToSamples (combMs[i]), 0.0f);
            combPos[i] = 0;
            combState[i] = 0.0f;
        }
        for (int i = 0; i < NUM_ALLPASS; ++i)
        {
            apBufs[i].assign (msToSamples (apMs[i]), 0.0f);
            apPos[i] = 0;
        }

        // ── "Boing" resonator (spring mechanical resonance at ~1200 Hz) ─
        //
        //   Digital resonator:  y[n] = A1·y[n-1] + A2·y[n-2] + B0·x[n]
        //   Poles at  z = r·e^{±jω₀}
        //     A1 = 2r·cos(ω₀),  A2 = −r²
        //
        //   Decay time τ = 200 ms  →  pole bandwidth  BW = 1/(π·τ) ≈ 1.59 Hz
        //   r = exp(−π·BW/sr)
        //
        //   Normalised input gain:
        //     Peak gain at ω₀ ≈ 1 / (2·(1−r)·sin(ω₀))
        //     So  B0 = 2·(1−r)·sin(ω₀)  gives unity peak gain.
        {
            const float sr_f  = static_cast<float> (sampleRate);
            const float f0    = 1200.f;
            const float tau   = 0.200f; // seconds
            const float bw    = 1.f / (juce::MathConstants<float>::pi * tau);
            const float r     = std::exp (-juce::MathConstants<float>::pi * bw / sr_f);
            const float w0    = juce::MathConstants<float>::twoPi * f0 / sr_f;

            boingA1   = 2.f * r * std::cos (w0);
            boingA2   = -(r * r);
            boingB0   = 2.f * (1.f - r) * std::sin (w0); // normalised for unity peak gain
            boingY1   = 0.f;
            boingY2   = 0.f;
        }

        setSize    (0.5f);
        setDamping (0.5f);
    }

    void reset()
    {
        std::fill (preDelayBuf.begin(), preDelayBuf.end(), 0.0f);
        preDelayPos = 0;
        for (int i = 0; i < NUM_COMBS;   ++i) { std::fill (combBufs[i].begin(), combBufs[i].end(), 0.0f); combState[i] = 0.0f; combPos[i] = 0; }
        for (int i = 0; i < NUM_ALLPASS; ++i) { std::fill (apBufs[i].begin(),   apBufs[i].end(),   0.0f); apPos[i] = 0; }
        boingY1 = boingY2 = 0.f;
    }

    float process (float input)
    {
        // ── Pre-delay ─────────────────────────────────────────────────
        float delayed = preDelayBuf[preDelayPos];
        preDelayBuf[preDelayPos] = input;
        if (++preDelayPos >= static_cast<int> (preDelayBuf.size())) preDelayPos = 0;

        // ── Parallel comb filters ─────────────────────────────────────
        float combSum = 0.0f;
        for (int i = 0; i < NUM_COMBS; ++i)
        {
            auto& buf  = combBufs[i];
            int   bsz  = static_cast<int> (buf.size());

            float d = buf[combPos[i]];
            // Lowpass-in-the-loop (tone / damping)
            combState[i] = d * (1.0f - damp) + combState[i] * damp;
            buf[combPos[i]] = delayed + combState[i] * roomCoeff;
            if (++combPos[i] >= bsz) combPos[i] = 0;
            combSum += d;
        }
        float out = combSum * (1.0f / NUM_COMBS) * 0.7f;

        // ── Series allpass filters ────────────────────────────────────
        for (int i = 0; i < NUM_ALLPASS; ++i)
        {
            auto& buf  = apBufs[i];
            int   bsz  = static_cast<int> (buf.size());
            float d    = buf[apPos[i]];
            float v    = out + d * (-0.5f);
            buf[apPos[i]] = out + d * 0.5f;
            if (++apPos[i] >= bsz) apPos[i] = 0;
            out = d + v * (-0.5f);
        }

        // ── "Boing" resonator — spring mechanical resonance ───────────
        // The resonator is fed by the pre-delayed input and rings at 1200 Hz
        // with a ~200 ms decay, adding the characteristic spring "boing" attack.
        // Mixed at 8% so it colours the reverb tail without overpowering it.
        {
            const float boingOut = boingA1 * boingY1 + boingA2 * boingY2 + delayed * boingB0;
            boingY2 = boingY1;
            boingY1 = boingOut;
            out += boingOut * 0.08f;
        }

        return out;
    }

    /** 0..1 — controls decay time */
    void setSize (float s)    { roomCoeff = 0.70f + s * 0.27f; }

    /** 0..1 — controls high-frequency damping */
    void setDamping (float d) { damp = d * 0.45f; }

private:
    double sampleRate = 44100.0;

    std::vector<float>                    preDelayBuf;
    int                                   preDelayPos = 0;

    std::array<std::vector<float>, NUM_COMBS>   combBufs;
    std::array<int,   NUM_COMBS>                combPos   = {};
    std::array<float, NUM_COMBS>                combState = {};

    std::array<std::vector<float>, NUM_ALLPASS> apBufs;
    std::array<int, NUM_ALLPASS>                apPos = {};

    float roomCoeff = 0.84f;
    float damp      = 0.20f;

    // ── Boing resonator state & coefficients ─────────────────────────
    float boingA1 = 0.f, boingA2 = 0.f; // IIR pole coefficients
    float boingB0 = 0.f;                 // normalised input gain
    float boingY1 = 0.f, boingY2 = 0.f; // delay-line state
};
