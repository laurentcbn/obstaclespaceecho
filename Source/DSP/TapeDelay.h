#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

/**
 *  TapeDelay – Simulates a 3-head tape delay loop (Roland RE-201 style).
 *
 *  Features:
 *   • Circular buffer with Catmull-Rom cubic interpolation
 *   • Wow (0.4 Hz) and double flutter (8 Hz / 13.7 Hz) LFOs
 *   • tanh soft saturation on the record head
 *   • One-pole lowpass + one-pole highpass (DC removal) on each playback head
 *   • Stereo instances should use different wowPhase seeds for natural spread
 */
class TapeDelay
{
public:
    static constexpr int NUM_HEADS = 3;

    // Physical head spacing ratios (RE-201 approximation)
    static constexpr std::array<float, 3> HEAD_RATIOS = { 1.0f, 1.475f, 2.625f };

    struct HeadOutputs { std::array<float, NUM_HEADS> heads = {}; };

    // -----------------------------------------------------------------
    void prepare (double newSampleRate, float maxDelayMs = 750.0f, float wowSeedPhase = 0.0f)
    {
        sampleRate = newSampleRate;
        bufferSize = static_cast<int> (maxDelayMs / 1000.0 * sampleRate) + 4096;
        buffer.assign (bufferSize, 0.0f);
        writePos = 0;

        // LFO init
        wowPhase     = wowSeedPhase;
        wowInc       = 0.4f  / static_cast<float> (sampleRate);
        flutterPhase = 0.0f;
        flutterInc   = 8.0f  / static_cast<float> (sampleRate);
        flutter2Phase = 0.37f;
        flutter2Inc  = 13.7f / static_cast<float> (sampleRate);

        // Filter states
        lpState.fill  (0.0f);
        hpState.fill  (0.0f);

        // HP coefficient: one-pole highpass at ~30 Hz (DC removal)
        const float hpFreq = 30.0f;
        hpCoeff = 1.0f - (juce::MathConstants<float>::twoPi
                         * hpFreq / static_cast<float> (sampleRate));

        setLowpassFrequency (5800.0f);
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        lpState.fill (0.0f);
        hpState.fill (0.0f);
    }

    /** When frozen, the write head stops — the buffer loops infinitely. */
    void setFrozen (bool shouldFreeze) noexcept { frozen = shouldFreeze; }

    /**
     *  Process one sample.
     *  @param input            Dry input sample
     *  @param baseDelaySamples Delay in samples for head 1 (others scaled by HEAD_RATIOS)
     *  @param feedbackSignal   Pre-computed feedback from previous step (caller maintains state)
     *  @param wowFlutterAmt    0..1 amount of pitch modulation
     *  @param saturationAmt    0..1 tape saturation drive
     */
    HeadOutputs process (float input,
                         float baseDelaySamples,
                         float feedbackSignal,
                         float wowFlutterAmt,
                         float saturationAmt)
    {
        // ── Wow & Flutter modulation ──────────────────────────────────
        const float wow  = std::sin (wowPhase * juce::MathConstants<float>::twoPi);
        advancePhase (wowPhase, wowInc);

        const float flt1 = std::sin (flutterPhase * juce::MathConstants<float>::twoPi);
        advancePhase (flutterPhase, flutterInc);

        const float flt2 = std::sin (flutter2Phase * juce::MathConstants<float>::twoPi);
        advancePhase (flutter2Phase, flutter2Inc);

        const float mod = (wow * 0.007f * 0.6f +
                           flt1 * 0.003f * 0.3f +
                           flt2 * 0.002f * 0.1f) * wowFlutterAmt;

        // ── Write (record head) ───────────────────────────────────────
        float toWrite = saturate (input + feedbackSignal, saturationAmt);
        if (! frozen)
            buffer[writePos] = toWrite;

        // ── Read (playback heads) ─────────────────────────────────────
        HeadOutputs out;
        for (int h = 0; h < NUM_HEADS; ++h)
        {
            float delay = baseDelaySamples * HEAD_RATIOS[h] * (1.0f + mod);
            delay = juce::jlimit (1.0f, static_cast<float> (bufferSize - 4), delay);

            float raw = readCubic (delay);
            raw = onePoleLP (raw, lpState[h]);
            raw = onePoleHP (raw, hpState[h]);
            out.heads[h] = raw;
        }

        if (++writePos >= bufferSize) writePos = 0;
        return out;
    }

    void setLowpassFrequency (float hz)
    {
        lpCoeff = std::exp (-juce::MathConstants<float>::twoPi
                            * hz / static_cast<float> (sampleRate));
    }

private:
    std::vector<float> buffer;
    int  bufferSize = 0;
    int  writePos   = 0;
    double sampleRate = 44100.0;
    bool frozen = false;

    float wowPhase = 0.f,  wowInc = 0.f;
    float flutterPhase = 0.f, flutterInc = 0.f;
    float flutter2Phase = 0.f, flutter2Inc = 0.f;

    std::array<float, NUM_HEADS> lpState = {};
    std::array<float, NUM_HEADS> hpState = {};

    float lpCoeff = 0.5f;
    float hpCoeff = 0.999f;

    static void advancePhase (float& ph, float inc)
    {
        ph += inc;
        if (ph >= 1.0f) ph -= 1.0f;
    }

    // One-pole lowpass
    float onePoleLP (float x, float& s) const
    {
        s = s * lpCoeff + x * (1.0f - lpCoeff);
        return s;
    }

    // One-pole highpass (DC removal)
    float onePoleHP (float x, float& s) const
    {
        float y = x - s;
        s = s * hpCoeff + x * (1.0f - hpCoeff);
        return y;
    }

    // Catmull-Rom cubic interpolation
    float readCubic (float delaySamples) const
    {
        float rPos = static_cast<float> (writePos) - delaySamples;
        while (rPos < 0.0f) rPos += static_cast<float> (bufferSize);

        int i1 = static_cast<int> (rPos) % bufferSize;
        float t = rPos - std::floor (rPos);

        int im1 = (i1 - 1 + bufferSize) % bufferSize;
        int  i2 = (i1 + 1) % bufferSize;
        int  i3 = (i1 + 2) % bufferSize;

        const float y0 = buffer[im1], y1 = buffer[i1];
        const float y2 = buffer[i2],  y3 = buffer[i3];

        const float a0 = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
        const float a1 =        y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
        const float a2 = -0.5f*y0             + 0.5f*y2;

        return ((a0*t + a1)*t + a2)*t + y1;
    }

    // tanh soft saturation, drive-normalised
    static float saturate (float x, float amount)
    {
        if (amount < 0.001f) return x;
        const float drive   = 1.0f + amount * 5.0f;
        const float norm    = 1.0f / std::tanh (drive);
        return std::tanh (x * drive) * norm;
    }
};
