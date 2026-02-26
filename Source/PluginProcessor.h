#pragma once
#include <JuceHeader.h>
#include "DSP/TapeDelay.h"
#include "DSP/SpringReverb.h"
#include "DSP/TapeNoise.h"
#include "DSP/ShimmerChorus.h"
#include <array>
#include <atomic>

class SpaceEchoAudioProcessor : public juce::AudioProcessor
{
public:
    // ── Mode table ──────────────────────────────────────────────────────
    struct ModeConfig
    {
        bool heads[TapeDelay::NUM_HEADS];
        bool reverb;
    };

    static const ModeConfig MODE_TABLE[12];

    // ── Oscilloscope ring buffer size ─────────────────────────────────
    static constexpr int SCOPE_SIZE = 512;

    // ── Construction ──────────────────────────────────────────────────
    SpaceEchoAudioProcessor();
    ~SpaceEchoAudioProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()   const override { return false; }
    bool producesMidi()  const override { return false; }
    bool isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int  getNumPrograms()               override { return 1; }
    int  getCurrentProgram()            override { return 0; }
    void setCurrentProgram (int)        override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── Parameter tree ────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Metering ──────────────────────────────────────────────────────
    float getInputLevel()  const noexcept { return inputLevelL.load(); }
    float getOutputLevel() const noexcept { return outputLevelL.load(); }

    // ── Test tone ─────────────────────────────────────────────────────
    void setTestTone (bool e) noexcept { testToneEnabled.store (e); }
    bool isTestToneEnabled() const noexcept { return testToneEnabled.load(); }

    // ── Oscilloscope (UI reads at ~30 Hz) ────────────────────────────
    const float* getScopeBuffer()   const noexcept { return scopeBuffer.data(); }
    int          getScopeWritePos() const noexcept
        { return scopeWritePos.load (std::memory_order_relaxed); }

private:
    // ── DSP objects ───────────────────────────────────────────────────
    TapeDelay    tapeL, tapeR;
    SpringReverb springL, springR;
    TapeNoise    noiseL, noiseR;
    ShimmerChorus shimmerL, shimmerR; // granular +1-octave pitch shifter

    // IIR shelving EQ (inside feedback path)
    juce::dsp::IIR::Filter<float> bassL,   bassR;
    juce::dsp::IIR::Filter<float> trebleL, trebleR;
    float cachedBassDb   = 9999.f; // for change detection
    float cachedTrebleDb = 9999.f;

    // One-sample feedback
    float feedbackL = 0.f, feedbackR = 0.f;

    // Shimmer feedback (pitch-shifted reverb tail fed back into reverb input)
    float shimFeedL = 0.f, shimFeedR = 0.f;

    double currentSampleRate = 44100.0;

    // ── Per-sample parameter smoothing (eliminates zipper noise) ─────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smInputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smIntensity;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smEchoLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smReverbLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smWowFlutter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smSaturation;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smTapeNoise;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smShimmer;

    // ── Test tone ─────────────────────────────────────────────────────
    std::atomic<bool> testToneEnabled { false };
    float testTonePhase   = 0.f;
    float testTonePhase2  = 0.f;
    float testToneTrigger = 0.f;

    // ── Level meters ──────────────────────────────────────────────────
    std::atomic<float> inputLevelL  { 0.f };
    std::atomic<float> outputLevelL { 0.f };

    // ── Oscilloscope ring buffer ──────────────────────────────────────
    std::array<float, SCOPE_SIZE> scopeBuffer = {};
    std::atomic<int>              scopeWritePos { 0 };

    // ── Helpers ───────────────────────────────────────────────────────
    void updateEQ (float bassDb, float trebleDb);

    /** Soft clipper: tanh-based, transparent below ~0 dBFS, hard limit above. */
    static float softClip (float x) noexcept
    {
        // Gain staging: reduce to ~0.9 to leave headroom, then tanh
        return std::tanh (x * 0.9f) / 0.9f;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceEchoAudioProcessor)
};
