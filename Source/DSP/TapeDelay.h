#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

/**
 *  TapeDelay – Simulates a 3-head tape delay loop (Roland RE-201 style).
 *
 *  v1.4 DSP improvements:
 *   • Per-head speed-dependent head-gap loss (playback LPs darken with distance + slow speed)
 *   • Organic wow & flutter — periodic LFOs + filtered random noise (irregular, lively)
 *   • Head bump — gentle bandpass resonance at ~150 Hz (warmth, magnetic presence)
 *   • Asymmetric tape saturation — dominant 2nd harmonic (warm, even-order, like magnetic oxide)
 *   • Catmull-Rom cubic interpolation, DC-removal HP per head, FREEZE support
 */
class TapeDelay
{
public:
    static constexpr int NUM_HEADS = 3;

    // Physical head spacing ratios (RE-201 approximation)
    static constexpr std::array<float, 3> HEAD_RATIOS = { 1.0f, 1.475f, 2.625f };

    struct HeadOutputs { std::array<float, NUM_HEADS> heads = {}; };

    // ─────────────────────────────────────────────────────────────────
    void prepare (double newSampleRate, float maxDelayMs = 750.0f, float wowSeedPhase = 0.0f)
    {
        sampleRate = newSampleRate;
        bufferSize = static_cast<int> (maxDelayMs / 1000.0 * sampleRate) + 4096;
        buffer.assign (bufferSize, 0.0f);
        writePos = 0;

        // ── LFO initialisation ──────────────────────────────────────
        wowPhase      = wowSeedPhase;
        wowInc        = 0.4f  / static_cast<float> (sampleRate);
        flutterPhase  = 0.0f;
        flutterInc    = 8.0f  / static_cast<float> (sampleRate);
        flutter2Phase = 0.37f;
        flutter2Inc   = 13.7f / static_cast<float> (sampleRate);

        // ── Random flutter state ────────────────────────────────────
        randState     = 2463534242u;
        randomFlutter = 0.f;

        // ── Filter states ───────────────────────────────────────────
        headLpState.fill (0.f);
        bumpHiState.fill (0.f);
        bumpLoState.fill (0.f);
        hpState.fill     (0.f);

        // HP: one-pole at 30 Hz (DC removal)
        hpCoeff = std::exp (-juce::MathConstants<float>::twoPi
                            * 30.f / static_cast<float> (sampleRate));

        // Reference delay at 150 ms (used for speed-dependent LP scaling)
        refDelaySamples = 0.150f * static_cast<float> (sampleRate);
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.f);
        writePos = 0;
        randomFlutter = 0.f;
        headLpState.fill (0.f);
        bumpHiState.fill (0.f);
        bumpLoState.fill (0.f);
        hpState.fill     (0.f);
    }

    /** When frozen, the write head stops — the buffer loops infinitely. */
    void setFrozen (bool shouldFreeze) noexcept { frozen = shouldFreeze; }

    /**
     *  Process one sample.
     *  @param input            Dry input sample
     *  @param baseDelaySamples Delay in samples for head 1 (others × HEAD_RATIOS)
     *  @param feedbackSignal   Pre-computed feedback (caller maintains state)
     *  @param wowFlutterAmt    0..1 — amount of pitch modulation
     *  @param saturationAmt    0..1 — tape saturation drive
     */
    HeadOutputs process (float input,
                         float baseDelaySamples,
                         float feedbackSignal,
                         float wowFlutterAmt,
                         float saturationAmt)
    {
        const float sr = static_cast<float> (sampleRate);

        // ── 1. Organic wow & flutter ──────────────────────────────────
        //    Periodic LFOs + low-frequency filtered noise → natural irregularity

        const float wow  = std::sin (wowPhase  * juce::MathConstants<float>::twoPi);
        advancePhase (wowPhase, wowInc);

        const float flt1 = std::sin (flutterPhase  * juce::MathConstants<float>::twoPi);
        advancePhase (flutterPhase, flutterInc);

        const float flt2 = std::sin (flutter2Phase * juce::MathConstants<float>::twoPi);
        advancePhase (flutter2Phase, flutter2Inc);

        // xorshift32 noise → LP-filtered to ~5 Hz → organic random flutter
        randState ^= randState << 13;
        randState ^= randState >> 17;
        randState ^= randState << 5;
        const float rNoise = static_cast<float> (static_cast<int32_t> (randState)) * 4.656e-10f;
        randomFlutter += 0.000713f * (rNoise - randomFlutter); // LP ≈ 5 Hz at 44100 Hz

        const float mod = (wow  * 0.0042f          // 0.4 Hz wow
                         + flt1 * 0.0009f          // 8 Hz flutter
                         + flt2 * 0.0002f          // 13.7 Hz flutter
                         + randomFlutter * 0.025f) // organic random component
                        * wowFlutterAmt;

        // ── 2. Write (record head) — asymmetric tape saturation ───────
        float toWrite = saturate (input + feedbackSignal, saturationAmt);
        if (! frozen)
            buffer[writePos] = toWrite;

        // ── 3. Read (playback heads) + per-head processing ────────────
        //
        //    Per head:
        //      a) Catmull-Rom read
        //      b) Head-gap loss LP  (speed-dependent, darker for far heads)
        //      c) DC removal HP
        //      d) Head bump        (bandpass ≈ 150 Hz, magnetic presence)

        // Speed ratio: slower tape (long delay) → darker playback
        const float speedRatio = refDelaySamples / juce::jmax (1.f, baseDelaySamples);

        // Base head-gap cutoff frequencies at reference speed (150 ms)
        // Each further head = additional oxide pass = lower cutoff
        static constexpr float HEAD_BASE_FC[NUM_HEADS] = { 7000.f, 5200.f, 3800.f };

        // Pre-compute bump filter increments (fixed, speed-independent)
        // bumpHi ≈ 270 Hz: inc = 1 − exp(−2π·270/sr)
        // bumpLo ≈  85 Hz: inc = 1 − exp(−2π· 85/sr)
        const float bumpHiInc = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 270.f / sr);
        const float bumpLoInc = 1.f - std::exp (-juce::MathConstants<float>::twoPi *  85.f / sr);

        HeadOutputs out;
        for (int h = 0; h < NUM_HEADS; ++h)
        {
            // a) Catmull-Rom read with wow/flutter modulation
            float delay = baseDelaySamples * HEAD_RATIOS[h] * (1.f + mod);
            delay = juce::jlimit (1.f, static_cast<float> (bufferSize - 4), delay);
            float raw = readCubic (delay);

            // b) Head-gap loss LP — speed-dependent + per-head darkening
            const float fc  = juce::jlimit (1800.f, 9000.f, HEAD_BASE_FC[h] * speedRatio);
            const float lpc = std::exp (-juce::MathConstants<float>::twoPi * fc / sr);
            headLpState[h]  = lpc * headLpState[h] + (1.f - lpc) * raw;
            raw = headLpState[h];

            // c) DC removal (one-pole HP at 30 Hz)
            {
                const float y = raw - hpState[h];
                hpState[h] = hpCoeff * hpState[h] + (1.f - hpCoeff) * raw;
                raw = y;
            }

            // d) Head bump: bandpass around 150 Hz → warm low-mid presence
            bumpHiState[h] += bumpHiInc * (raw - bumpHiState[h]); // LP at 270 Hz
            bumpLoState[h] += bumpLoInc * (raw - bumpLoState[h]); // LP at  85 Hz
            raw += (bumpHiState[h] - bumpLoState[h]) * 0.28f;     // +bandpass mix

            out.heads[h] = raw;
        }

        if (++writePos >= bufferSize) writePos = 0;
        return out;
    }

private:
    std::vector<float> buffer;
    int    bufferSize = 0;
    int    writePos   = 0;
    double sampleRate = 44100.0;
    bool   frozen     = false;

    // LFO
    float wowPhase = 0.f,      wowInc       = 0.f;
    float flutterPhase = 0.f,  flutterInc   = 0.f;
    float flutter2Phase = 0.f, flutter2Inc  = 0.f;

    // Organic flutter noise
    uint32_t randState     = 2463534242u;
    float    randomFlutter = 0.f;

    // Per-head filter states
    std::array<float, NUM_HEADS> headLpState = {}; // head-gap LP
    std::array<float, NUM_HEADS> bumpHiState = {}; // head bump LP hi
    std::array<float, NUM_HEADS> bumpLoState = {}; // head bump LP lo
    std::array<float, NUM_HEADS> hpState     = {}; // DC removal HP

    float hpCoeff         = 0.999f;
    float refDelaySamples = 6615.f; // 150 ms @ 44100 Hz

    // ─────────────────────────────────────────────────────────────────
    static void advancePhase (float& ph, float inc) noexcept
    {
        ph += inc;
        if (ph >= 1.f) ph -= 1.f;
    }

    // ─────────────────────────────────────────────────────────────────
    // Catmull-Rom cubic interpolation
    float readCubic (float delaySamples) const noexcept
    {
        float rPos = static_cast<float> (writePos) - delaySamples;
        while (rPos < 0.f) rPos += static_cast<float> (bufferSize);

        const int i1 = static_cast<int> (rPos) % bufferSize;
        const float t = rPos - std::floor (rPos);

        const int im1 = (i1 - 1 + bufferSize) % bufferSize;
        const int  i2 = (i1 + 1) % bufferSize;
        const int  i3 = (i1 + 2) % bufferSize;

        const float y0 = buffer[im1], y1 = buffer[i1];
        const float y2 = buffer[i2],  y3 = buffer[i3];

        const float a0 = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
        const float a1 =        y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
        const float a2 = -0.5f*y0             + 0.5f*y2;

        return ((a0*t + a1)*t + a2)*t + y1;
    }

    // ─────────────────────────────────────────────────────────────────
    // Asymmetric tape saturation — generates dominant 2nd harmonic.
    //
    // Positive half: x / (1 + 1.12·|x|)   → softer knee (tape oxide compresses gently)
    // Negative half: x / (1 + 0.88·|x|)   → harder knee (magnetic hysteresis asymmetry)
    //
    // The asymmetry between the two halves creates even-order harmonics (2nd, 4th)
    // which sound warm and musical rather than harsh — characteristic of tape.
    //
    // Unity gain for small signals (both halves → x/(1+|x|) ≈ x for |x| << 1).
    //
    static float saturate (float x, float amount) noexcept
    {
        if (amount < 0.001f) return x;

        const float drive = 1.f + amount * 4.5f;
        const float xd    = x * drive;

        // Asymmetric soft-clip waveshaper
        float y;
        if (xd >= 0.f)
            y = xd / (1.f + 1.12f * xd);   // positive: soft (dominant 2nd harmonic)
        else
            y = xd / (1.f - 0.88f * xd);   // negative: slightly harder

        // Unity-gain normalisation (at small xd, y ≈ xd → y/drive ≈ x)
        return y / drive;
    }
};
