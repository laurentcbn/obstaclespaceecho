#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Mode table
// ─────────────────────────────────────────────────────────────────────────────
const SpaceEchoAudioProcessor::ModeConfig SpaceEchoAudioProcessor::MODE_TABLE[12] =
{
    {{ true,  false, false }, false }, // 1  – H1
    {{ false,  true, false }, false }, // 2  – H2
    {{ false, false,  true }, false }, // 3  – H3
    {{ true,   true, false }, false }, // 4  – H1+H2
    {{ true,  false,  true }, false }, // 5  – H1+H3
    {{ false,  true,  true }, false }, // 6  – H2+H3
    {{ true,   true,  true }, false }, // 7  – ALL
    {{ true,  false, false },  true }, // 8  – H1+Reverb
    {{ false,  true, false },  true }, // 9  – H2+Reverb
    {{ false, false,  true },  true }, // 10 – H3+Reverb
    {{ true,   true,  true },  true }, // 11 – ALL+Reverb
    {{ false, false, false },  true }, // 12 – Reverb only
};

// ─────────────────────────────────────────────────────────────────────────────
//  Parameter layout
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
SpaceEchoAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto makeFloat = [&] (const juce::String& id, const juce::String& name,
                          float lo, float hi, float def,
                          std::function<juce::String (float, int)> strFn = nullptr)
    {
        juce::NormalisableRange<float> range (lo, hi);
        if (strFn)
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, 1 }, name, range, def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (strFn)));
        else
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, 1 }, name, range, def));
    };

    makeFloat ("inputGain",   "Input Gain",    0.0f,  1.0f,  0.70f);
    makeFloat ("repeatRate",  "Repeat Rate",   20.f,  500.f, 150.f,
               [] (float v, int) { return juce::String ((int) v) + " ms"; });
    makeFloat ("intensity",   "Intensity",     0.0f,  0.95f, 0.40f);
    makeFloat ("bass",        "Bass",         -12.0f, 12.0f, 0.0f,
               [] (float v, int) { return juce::String (v, 1) + " dB"; });
    makeFloat ("treble",      "Treble",       -12.0f, 12.0f, 0.0f,
               [] (float v, int) { return juce::String (v, 1) + " dB"; });
    makeFloat ("echoLevel",   "Echo Level",    0.0f,  1.0f,  0.70f);
    makeFloat ("reverbLevel", "Reverb Level",  0.0f,  1.0f,  0.50f);
    makeFloat ("wowFlutter",  "Wow / Flutter", 0.0f,  1.0f,  0.30f);
    makeFloat ("saturation",  "Saturation",    0.0f,  1.0f,  0.30f);

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "mode", 1 }, "Mode", 0, 11, 0));

    makeFloat ("tapeNoise", "Tape Noise", 0.0f, 1.0f, 0.15f);
    makeFloat ("shimmer",   "Shimmer",    0.0f, 1.0f, 0.0f);

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "freeze",   1 }, "Freeze",    false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "pingpong", 1 }, "Ping-Pong", false));

    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
SpaceEchoAudioProcessor::SpaceEchoAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SpaceEchoState", createParameterLayout())
{
}

SpaceEchoAudioProcessor::~SpaceEchoAudioProcessor() {}

// ─────────────────────────────────────────────────────────────────────────────
//  Prepare
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    feedbackL = feedbackR = 0.f;
    shimFeedL = shimFeedR = 0.f;

    tapeL.prepare (sampleRate, 750.f, 0.0f);
    tapeR.prepare (sampleRate, 750.f, 0.37f);

    springL.prepare (sampleRate);
    springR.prepare (sampleRate);

    noiseL.prepare (sampleRate);
    noiseR.prepare (sampleRate);

    shimmerL.prepare (sampleRate);
    shimmerR.prepare (sampleRate);

    // Shelving EQ filters
    juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
    bassL.prepare  (spec); bassL.reset();
    bassR.prepare  (spec); bassR.reset();
    trebleL.prepare(spec); trebleL.reset();
    trebleR.prepare(spec); trebleR.reset();
    cachedBassDb   = 9999.f; // force first-frame update
    cachedTrebleDb = 9999.f;
    updateEQ (0.f, 0.f);

    // ── Parameter smoothers (20 ms ramp — eliminates zipper noise) ───
    const double rampSec = 0.020;
    auto initSmoother = [&] (auto& sm, const juce::String& id)
    {
        sm.reset (sampleRate, rampSec);
        sm.setCurrentAndTargetValue (*apvts.getRawParameterValue (id));
    };

    initSmoother (smInputGain,   "inputGain");
    initSmoother (smIntensity,   "intensity");
    initSmoother (smEchoLevel,   "echoLevel");
    initSmoother (smReverbLevel, "reverbLevel");
    initSmoother (smWowFlutter,  "wowFlutter");
    initSmoother (smSaturation,  "saturation");
    initSmoother (smTapeNoise,   "tapeNoise");
    initSmoother (smShimmer,     "shimmer");

    scopeBuffer.fill (0.f);
    scopeWritePos.store (0, std::memory_order_relaxed);
}

void SpaceEchoAudioProcessor::releaseResources()
{
    tapeL.reset(); tapeR.reset();
    springL.reset(); springR.reset();
    noiseL.reset(); noiseR.reset();
    shimmerL.reset(); shimmerR.reset();
}

// ─────────────────────────────────────────────────────────────────────────────
//  EQ update — called only when coefficients change
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessor::updateEQ (float bassDb, float trebleDb)
{
    if (bassDb == cachedBassDb && trebleDb == cachedTrebleDb)
        return;

    cachedBassDb   = bassDb;
    cachedTrebleDb = trebleDb;

    const double sr = currentSampleRate;
    *bassL.coefficients   = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sr, 200.0, 0.7, juce::Decibels::decibelsToGain (bassDb));
    *bassR.coefficients   = *bassL.coefficients;

    *trebleL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sr, 3000.0, 0.7, juce::Decibels::decibelsToGain (trebleDb));
    *trebleR.coefficients = *trebleL.coefficients;
}

// ─────────────────────────────────────────────────────────────────────────────
//  processBlock
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    // ── Block-rate params (bool / int / EQ) ──────────────────────────
    const float bassDb   = *apvts.getRawParameterValue ("bass");
    const float trebleDb = *apvts.getRawParameterValue ("treble");
    const int   mode     = (int) *apvts.getRawParameterValue ("mode");
    const bool  frozen   = *apvts.getRawParameterValue ("freeze")   > 0.5f;
    const bool  pingpong = *apvts.getRawParameterValue ("pingpong") > 0.5f;
    const float repeatMs = *apvts.getRawParameterValue ("repeatRate");

    updateEQ (bassDb, trebleDb);
    tapeL.setFrozen (frozen);
    tapeR.setFrozen (frozen);

    // Reverb parameters (fixed for now, could expose later)
    springL.setSize    (0.65f); springR.setSize    (0.65f);
    springL.setDamping (0.35f); springR.setDamping (0.35f);

    // ── Set smoother targets (interpolated per-sample below) ──────────
    smInputGain  .setTargetValue (*apvts.getRawParameterValue ("inputGain"));
    smIntensity  .setTargetValue (*apvts.getRawParameterValue ("intensity"));
    smEchoLevel  .setTargetValue (*apvts.getRawParameterValue ("echoLevel"));
    smReverbLevel.setTargetValue (*apvts.getRawParameterValue ("reverbLevel"));
    smWowFlutter .setTargetValue (*apvts.getRawParameterValue ("wowFlutter"));
    smSaturation .setTargetValue (*apvts.getRawParameterValue ("saturation"));
    smTapeNoise  .setTargetValue (*apvts.getRawParameterValue ("tapeNoise"));
    smShimmer    .setTargetValue (*apvts.getRawParameterValue ("shimmer"));

    const auto& mc = MODE_TABLE[juce::jlimit (0, 11, mode)];
    int numHeads = 0;
    for (int h = 0; h < TapeDelay::NUM_HEADS; ++h)
        if (mc.heads[h]) ++numHeads;

    const float baseDelay = repeatMs * 0.001f * (float) currentSampleRate;

    // Stereo output setup
    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    float inAcc = 0.f, outAcc = 0.f;

    // Test tone state
    const bool  testOn    = testToneEnabled.load();
    const float sr_f      = (float) currentSampleRate;
    const float pulseLen  = sr_f * 1.5f;

    // Scope write position (local for this block)
    int scopePos = scopeWritePos.load (std::memory_order_relaxed);

    // ── Per-sample loop ───────────────────────────────────────────────
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // Smoothed parameter values — no zipper noise
        const float gain   = smInputGain  .getNextValue();
        const float intens = smIntensity  .getNextValue();
        const float echoLv = smEchoLevel  .getNextValue();
        const float revLv  = smReverbLevel.getNextValue();
        const float wow    = smWowFlutter .getNextValue();
        const float sat    = smSaturation .getNextValue();
        const float noise  = smTapeNoise  .getNextValue();
        const float shim   = smShimmer    .getNextValue();

        float inL = left[i]  * gain;
        float inR = right[i] * gain;

        // ── Test tone ─────────────────────────────────────────────────
        if (testOn)
        {
            testToneTrigger += 1.f;
            if (testToneTrigger >= pulseLen)
            {
                testToneTrigger = 0.f;
                testTonePhase   = 0.f;
                testTonePhase2  = 0.f;
            }
            const float env = (testToneTrigger < 4.f)
                              ? testToneTrigger * 0.25f
                              : std::exp (-5.f * testToneTrigger / sr_f);
            const float s1 = std::sin (testTonePhase  * juce::MathConstants<float>::twoPi);
            const float s2 = std::sin (testTonePhase2 * juce::MathConstants<float>::twoPi);
            testTonePhase  += 440.f / sr_f; if (testTonePhase  >= 1.f) testTonePhase  -= 1.f;
            testTonePhase2 += 554.f / sr_f; if (testTonePhase2 >= 1.f) testTonePhase2 -= 1.f;
            const float tone = (s1 * 0.6f + s2 * 0.4f) * env * 0.4f;
            inL += tone; inR += tone;
        }

        // ── Tape noise injection ──────────────────────────────────────
        inL += noiseL.process (noise);
        inR += noiseR.process (noise);

        inAcc += std::abs (inL);

        // ── Tape delay ────────────────────────────────────────────────
        auto headsL = tapeL.process (inL, baseDelay, feedbackL, wow, sat);
        auto headsR = tapeR.process (inR, baseDelay, feedbackR, wow, sat);

        // ── Sum active heads ──────────────────────────────────────────
        float echoL = 0.f, echoR = 0.f;
        for (int h = 0; h < TapeDelay::NUM_HEADS; ++h)
        {
            if (mc.heads[h])
            {
                echoL += headsL.heads[h];
                echoR += headsR.heads[h];
            }
        }
        if (numHeads > 0)
        {
            echoL /= (float) numHeads;
            echoR /= (float) numHeads;
        }

        // ── EQ on echo feedback path ──────────────────────────────────
        echoL = bassL.processSample   (echoL);
        echoL = trebleL.processSample (echoL);
        echoR = bassR.processSample   (echoR);
        echoR = trebleR.processSample (echoR);

        // ── Feedback (with optional ping-pong) ────────────────────────
        if (pingpong)
        {
            feedbackL = echoR * intens;
            feedbackR = echoL * intens;
        }
        else
        {
            feedbackL = echoL * intens;
            feedbackR = echoR * intens;
        }

        // ── Spring reverb + shimmer feedback loop ─────────────────────
        // Architecture: reverb feeds into pitch shifter, pitch shifter
        // feeds back into reverb — creates an endless rising shimmer.
        float revL = 0.f, revR = 0.f;
        if (mc.reverb)
        {
            revL = springL.process (inL + echoL * 0.15f + shimFeedL);
            revR = springR.process (inR + echoR * 0.15f + shimFeedR);

            // Update shimmer feedback (granular +1 oct pitch shifted reverb)
            shimFeedL = shimmerL.process (revL, shim) * 0.8f;
            shimFeedR = shimmerR.process (revR, shim) * 0.8f;
        }
        else
        {
            shimFeedL = shimFeedR = 0.f;
        }

        // ── Output mix ────────────────────────────────────────────────
        const float mixL = inL + echoL * echoLv + revL * revLv;
        const float mixR = inR + echoR * echoLv + revR * revLv;

        // ── Soft limiter (transparent below 0 dBFS, prevents digital clip) ──
        const float outL = softClip (mixL);
        const float outR = softClip (mixR);

        left[i]  = outL;
        right[i] = outR;
        outAcc  += std::abs (outL);

        // ── Oscilloscope ──────────────────────────────────────────────
        scopeBuffer[scopePos] = outL;
        scopePos = (scopePos + 1) % SCOPE_SIZE;
    }

    scopeWritePos.store (scopePos, std::memory_order_relaxed);

    const float inv = 1.f / (float) std::max (1, buffer.getNumSamples());
    inputLevelL .store (inAcc  * inv);
    outputLevelL.store (outAcc * inv);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Buses layout
// ─────────────────────────────────────────────────────────────────────────────
bool SpaceEchoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

// ─────────────────────────────────────────────────────────────────────────────
//  State persistence
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SpaceEchoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Editor / Factory
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* SpaceEchoAudioProcessor::createEditor()
{
    return new SpaceEchoAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpaceEchoAudioProcessor();
}
