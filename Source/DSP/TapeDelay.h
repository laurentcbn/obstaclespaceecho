#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

/**
 *  TapeDelay – Simulates a 3-head tape delay loop (Roland RE-201 style).
 *
 *  v1.5 additions (on top of v1.4):
 *   • Motor drift     — ultra-slow LFO (0.05 Hz), always-on long-term pitch wobble
 *   • Print-through   — ghost echo at delay×0.92 (magnetic bleed from adjacent tape layer)
 *   • Inter-head crosstalk — 1.5 % adjacent-head bleed (realistic oxide proximity)
 *   • Dropout simulation  — rare brief amplitude dips (~2–3 / min), tape wear character
 *
 *  v1.4 features:
 *   • Per-head speed-dependent head-gap loss (playback darkens with distance + slow speed)
 *   • Organic wow & flutter — periodic LFOs + filtered random noise
 *   • Head bump — gentle bandpass resonance at ~150 Hz
 *   • Asymmetric tape saturation — dominant 2nd harmonic
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

        const float sr = static_cast<float> (sampleRate);

        // ── LFO initialisation ──────────────────────────────────────
        wowPhase      = wowSeedPhase;
        wowInc        = 0.4f  / sr;
        flutterPhase  = 0.0f;
        flutterInc    = 8.0f  / sr;
        flutter2Phase = 0.37f;
        flutter2Inc   = 13.7f / sr;

        // ── Motor drift (very slow long-term speed instability) ─────
        // 0.05 Hz LFO, ±0.15% pitch — always on, independent of wow/flutter
        driftPhase = 0.f;
        driftInc   = 0.05f / sr;

        // ── Random flutter state ────────────────────────────────────
        randState     = 2463534242u;
        randomFlutter = 0.f;

        // ── Dropout state ───────────────────────────────────────────
        // Initial blank period of ~2 s before first possible dropout
        dropRandState = 1234567891u ^ static_cast<uint32_t> (newSampleRate);
        dropoutTimer  = static_cast<uint32_t> (2.0 * newSampleRate);
        dropoutLen    = 0u;
        dropoutGain   = 1.f;

        // ── Filter states ───────────────────────────────────────────
        headLpState.fill (0.f);
        bumpHiState.fill (0.f);
        bumpLoState.fill (0.f);
        hpState.fill     (0.f);

        // HP: one-pole at 30 Hz (DC removal)
        hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 30.f / sr);

        // Reference delay at 150 ms (used for speed-dependent LP scaling)
        refDelaySamples = 0.150f * sr;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.f);
        writePos      = 0;
        randomFlutter = 0.f;
        dropoutGain   = 1.f;
        dropoutLen    = 0u;
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
        randomFlutter += 0.000713f * (rNoise - randomFlutter); // LP ≈ 5 Hz at 44100

        const float mod = (wow  * 0.0042f          // 0.4 Hz wow
                         + flt1 * 0.0009f          // 8 Hz flutter
                         + flt2 * 0.0002f          // 13.7 Hz flutter
                         + randomFlutter * 0.025f) // organic random component
                        * wowFlutterAmt;

        // ── 2. Motor drift — ultra-slow LFO (always-on) ───────────────
        // 0.05 Hz, ±0.15% pitch — simulates motor speed instability
        const float drift = std::sin (driftPhase * juce::MathConstants<float>::twoPi) * 0.0015f;
        advancePhase (driftPhase, driftInc);

        const float totalMod = mod + drift;

        // ── 3. Dropout simulation ──────────────────────────────────────
        // Rare amplitude dips (~2–3/min) simulating worn tape oxide
        if (dropoutLen > 0)
        {
            // Gradual recovery: time constant ≈ 250 samples (~5.7 ms @ 44.1 kHz)
            dropoutGain += (1.f - dropoutGain) * 0.004f;
            --dropoutLen;
            if (dropoutLen == 0) dropoutGain = 1.f;
        }
        else if (dropoutTimer == 0)
        {
            // xorshift for dropout RNG
            dropRandState ^= dropRandState << 13;
            dropRandState ^= dropRandState >> 17;
            dropRandState ^= dropRandState << 5;

            // Dropout: gain dips to 0.25–0.50
            dropoutGain = 0.25f + (dropRandState & 0xFFu) * (0.25f / 255.f);
            // Duration: ~30–75 ms
            dropoutLen  = 1323u + (dropRandState >> 8 & 0x7FFu);

            // Next dropout in 15–35 s (at 44.1 kHz ≈ 662000–1543000 samples)
            dropRandState ^= dropRandState << 13;
            dropRandState ^= dropRandState >> 17;
            dropRandState ^= dropRandState << 5;
            dropoutTimer = static_cast<uint32_t> (sr * 15.f) + (dropRandState & 0xFFFFFu);
        }
        else
        {
            --dropoutTimer;
        }

        // ── 4. Write (record head) — asymmetric tape saturation ────────
        float toWrite = saturate (input + feedbackSignal, saturationAmt);
        if (! frozen)
            buffer[writePos] = toWrite;

        // ── 5. Read (playback heads) + per-head processing ─────────────
        const float speedRatio = refDelaySamples / juce::jmax (1.f, baseDelaySamples);

        // Base head-gap cutoff frequencies at reference speed (150 ms)
        static constexpr float HEAD_BASE_FC[NUM_HEADS] = { 7000.f, 5200.f, 3800.f };

        const float bumpHiInc = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 270.f / sr);
        const float bumpLoInc = 1.f - std::exp (-juce::MathConstants<float>::twoPi *  85.f / sr);

        HeadOutputs out;
        for (int h = 0; h < NUM_HEADS; ++h)
        {
            // a) Catmull-Rom read with combined modulation (wow/flutter + motor drift)
            float delay = baseDelaySamples * HEAD_RATIOS[h] * (1.f + totalMod);
            delay = juce::jlimit (1.f, static_cast<float> (bufferSize - 4), delay);
            float raw = readCubic (delay);

            // b) Dropout — tape oxide wear affects playback amplitude
            raw *= dropoutGain;

            // c) Print-through — faint ghost echo at 92% of the main delay
            //    Magnetic bleed from adjacent tape layers creates a subtle pre-echo
            //    ~35 dB below the main signal (≈ gain 0.018)
            {
                const float ptDelay = juce::jlimit (1.f, static_cast<float> (bufferSize - 4),
                                                    delay * 0.92f);
                raw += readCubic (ptDelay) * 0.018f;
            }

            // d) Head-gap loss LP — speed-dependent + per-head darkening
            const float fc  = juce::jlimit (1800.f, 9000.f, HEAD_BASE_FC[h] * speedRatio);
            const float lpc = std::exp (-juce::MathConstants<float>::twoPi * fc / sr);
            headLpState[h]  = lpc * headLpState[h] + (1.f - lpc) * raw;
            raw = headLpState[h];

            // e) DC removal (one-pole HP at 30 Hz)
            {
                const float y = raw - hpState[h];
                hpState[h] = hpCoeff * hpState[h] + (1.f - hpCoeff) * raw;
                raw = y;
            }

            // f) Head bump: bandpass around 150 Hz → warm low-mid presence
            bumpHiState[h] += bumpHiInc * (raw - bumpHiState[h]); // LP at 270 Hz
            bumpLoState[h] += bumpLoInc * (raw - bumpLoState[h]); // LP at  85 Hz
            raw += (bumpHiState[h] - bumpLoState[h]) * 0.28f;     // +bandpass mix

            out.heads[h] = raw;
        }

        // ── 6. Inter-head crosstalk — 1.5% adjacent-head bleed ─────────
        // Simulates magnetic cross-talk between physically adjacent record/play heads
        {
            const std::array<float, NUM_HEADS> orig = out.heads;
            for (int h = 0; h < NUM_HEADS; ++h)
            {
                if (h > 0)             out.heads[h] += orig[h - 1] * 0.015f;
                if (h < NUM_HEADS - 1) out.heads[h] += orig[h + 1] * 0.015f;
            }
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

    // Motor drift (ultra-slow, always-on)
    float driftPhase = 0.f,    driftInc     = 0.f;

    // Organic flutter noise
    uint32_t randState     = 2463534242u;
    float    randomFlutter = 0.f;

    // Dropout state
    uint32_t dropRandState = 1234567891u;
    uint32_t dropoutTimer  = 88200u;   // samples until next dropout event
    uint32_t dropoutLen    = 0u;       // samples remaining in current dropout
    float    dropoutGain   = 1.f;      // current playback amplitude (1 = no dropout)

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
    static float saturate (float x, float amount) noexcept
    {
        if (amount < 0.001f) return x;

        const float drive = 1.f + amount * 4.5f;
        const float xd    = x * drive;

        float y;
        if (xd >= 0.f)
            y = xd / (1.f + 1.12f * xd);   // positive: soft (dominant 2nd harmonic)
        else
            y = xd / (1.f - 0.88f * xd);   // negative: slightly harder

        return y / drive;
    }
};
