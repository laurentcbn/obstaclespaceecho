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
        bool heads[TapeDelay::NUM_HEADS]; ///< which playback heads are active
        bool reverb;                       ///< spring reverb active
    };

    // 12 modes: matches Roland RE-201 mode dial
    static const ModeConfig MODE_TABLE[12];

    // ── Oscilloscope ring buffer ──────────────────────────────────────
    static constexpr int SCOPE_SIZE = 512;

    // ── Construction ─────────────────────────────────────────────────────
    SpaceEchoAudioProcessor();
    ~SpaceEchoAudioProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────────
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

    int  getNumPrograms()             override { return 1; }
    int  getCurrentProgram()          override { return 0; }
    void setCurrentProgram (int)      override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Parameter tree ────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Metering (UI reads these) ─────────────────────────────────────────
    float getInputLevel()  const noexcept { return inputLevelL.load(); }
    float getOutputLevel() const noexcept { return outputLevelL.load(); }

    // ── Test tone (UI toggles this) ───────────────────────────────────────
    void setTestTone (bool enabled) noexcept { testToneEnabled.store (enabled); }
    bool isTestToneEnabled() const noexcept  { return testToneEnabled.load(); }

    // ── Oscilloscope data access (UI reads at 30 Hz) ───────────────────────
    const float* getScopeBuffer()   const noexcept { return scopeBuffer.data(); }
    int          getScopeWritePos() const noexcept { return scopeWritePos.load (std::memory_order_relaxed); }

private:
    // ── DSP objects ───────────────────────────────────────────────────────
    TapeDelay    tapeL, tapeR;
    SpringReverb springL, springR;
    TapeNoise    noiseL, noiseR;
    ShimmerChorus shimmerL, shimmerR;

    // Bass / treble IIR shelving filters (on the echo feedback path)
    juce::dsp::IIR::Filter<float> bassL,   bassR;
    juce::dsp::IIR::Filter<float> trebleL, trebleR;

    // Feedback state (one-sample delay, maintained across calls)
    float feedbackL = 0.f, feedbackR = 0.f;

    // Cached sample rate
    double currentSampleRate = 44100.0;

    // ── Test tone oscillator ───────────────────────────────────────────────
    std::atomic<bool> testToneEnabled { false };
    float testTonePhase    = 0.f; // 0..1
    float testTonePhase2   = 0.f; // second note for chord
    float testToneTrigger  = 0.f; // envelope counter (samples)

    // ── Level meters ──────────────────────────────────────────────────────
    std::atomic<float> inputLevelL  { 0.f };
    std::atomic<float> outputLevelL { 0.f };

    // ── Oscilloscope ring buffer (audio writes, UI reads) ─────────────────
    std::array<float, SCOPE_SIZE> scopeBuffer = {};
    std::atomic<int>              scopeWritePos { 0 };

    // ── Helpers ───────────────────────────────────────────────────────────
    void updateEQ (float bassDb, float trebleDb);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceEchoAudioProcessor)
};
