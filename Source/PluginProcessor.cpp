#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Mode table   (12 positions, RE-201 style)
// ─────────────────────────────────────────────────────────────────────────────
const SpaceEchoAudioProcessor::ModeConfig SpaceEchoAudioProcessor::MODE_TABLE[12] =
{
    // heads[0]=H1  heads[1]=H2  heads[2]=H3  reverb
    {{ true,  false, false }, false }, // 1  – H1
    {{ false,  true, false }, false }, // 2  – H2
    {{ false, false,  true }, false }, // 3  – H3
    {{ true,   true, false }, false }, // 4  – H1 + H2
    {{ true,  false,  true }, false }, // 5  – H1 + H3
    {{ false,  true,  true }, false }, // 6  – H2 + H3
    {{ true,   true,  true }, false }, // 7  – ALL heads
    {{ true,  false, false },  true }, // 8  – H1 + Reverb
    {{ false,  true, false },  true }, // 9  – H2 + Reverb
    {{ false, false,  true },  true }, // 10 – H3 + Reverb
    {{ true,   true,  true },  true }, // 11 – ALL + Reverb
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

    // Original parameters
    makeFloat ("inputGain",   "Input Gain",   0.0f,  1.0f,  0.70f);
    makeFloat ("repeatRate",  "Repeat Rate",  20.f,  500.f, 150.f,
               [] (float v, int) { return juce::String (static_cast<int> (v)) + " ms"; });
    makeFloat ("intensity",   "Intensity",    0.0f,  0.95f, 0.40f);
    makeFloat ("bass",        "Bass",        -12.0f, 12.0f, 0.0f,
               [] (float v, int) { return juce::String (v, 1) + " dB"; });
    makeFloat ("treble",      "Treble",      -12.0f, 12.0f, 0.0f,
               [] (float v, int) { return juce::String (v, 1) + " dB"; });
    makeFloat ("echoLevel",   "Echo Level",   0.0f,  1.0f,  0.70f);
    makeFloat ("reverbLevel", "Reverb Level", 0.0f,  1.0f,  0.50f);
    makeFloat ("wowFlutter",  "Wow / Flutter",0.0f,  1.0f,  0.30f);
    makeFloat ("saturation",  "Saturation",   0.0f,  1.0f,  0.30f);

    // Mode (int)
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "mode", 1 }, "Mode", 0, 11, 0));

    // ── New v1.1 parameters ───────────────────────────────────────────
    makeFloat ("tapeNoise", "Tape Noise",  0.0f, 1.0f, 0.15f);
    makeFloat ("shimmer",   "Shimmer",     0.0f, 1.0f, 0.0f);

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

    // Different wow-phase seeds for L / R → natural stereo spread
    tapeL.prepare (sampleRate, 750.f, 0.0f);
    tapeR.prepare (sampleRate, 750.f, 0.37f);

    springL.prepare (sampleRate);
    springR.prepare (sampleRate);

    noiseL.prepare (sampleRate);
    noiseR.prepare (sampleRate);

    shimmerL.prepare (sampleRate);
    shimmerR.prepare (sampleRate);

    // Prepare shelving filters
    juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
    bassL.prepare  (spec); bassL.reset();
    bassR.prepare  (spec); bassR.reset();
    trebleL.prepare(spec); trebleL.reset();
    trebleR.prepare(spec); trebleR.reset();

    // Apply initial EQ (flat)
    updateEQ (0.f, 0.f);

    // Clear scope buffer
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
//  EQ update (called each block when params change)
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessor::updateEQ (float bassDb, float trebleDb)
{
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

    // ── Read parameters ───────────────────────────────────────────────
    const float inputGain   = *apvts.getRawParameterValue ("inputGain");
    const float repeatRateMs= *apvts.getRawParameterValue ("repeatRate");
    const float intensity   = *apvts.getRawParameterValue ("intensity");
    const float echoLevel   = *apvts.getRawParameterValue ("echoLevel");
    const float reverbLevel = *apvts.getRawParameterValue ("reverbLevel");
    const float wowFlutter  = *apvts.getRawParameterValue ("wowFlutter");
    const float saturation  = *apvts.getRawParameterValue ("saturation");
    const float bassDb      = *apvts.getRawParameterValue ("bass");
    const float trebleDb    = *apvts.getRawParameterValue ("treble");
    const int   mode        = static_cast<int> (*apvts.getRawParameterValue ("mode"));
    const float tapeNoise   = *apvts.getRawParameterValue ("tapeNoise");
    const float shimmer     = *apvts.getRawParameterValue ("shimmer");
    const bool  frozen      = *apvts.getRawParameterValue ("freeze")   > 0.5f;
    const bool  pingpong    = *apvts.getRawParameterValue ("pingpong") > 0.5f;

    // ── Apply freeze to tape delay ────────────────────────────────────
    tapeL.setFrozen (frozen);
    tapeR.setFrozen (frozen);

    // ── EQ update (once per block) ────────────────────────────────────
    updateEQ (bassDb, trebleDb);

    // ── Spring reverb parameters ──────────────────────────────────────
    springL.setSize    (0.65f);
    springR.setSize    (0.65f);
    springL.setDamping (0.35f);
    springR.setDamping (0.35f);

    // ── Mode config ───────────────────────────────────────────────────
    const auto& mc = MODE_TABLE[juce::jlimit (0, 11, mode)];

    int numActiveHeads = 0;
    for (int h = 0; h < TapeDelay::NUM_HEADS; ++h)
        if (mc.heads[h]) ++numActiveHeads;

    const float baseDelaySamples = repeatRateMs * 0.001f
                                   * static_cast<float> (currentSampleRate);

    // ── Ensure stereo output ──────────────────────────────────────────
    const int totalOut = getTotalNumOutputChannels();
    const int totalIn  = getTotalNumInputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    // ── Metering accumulators ─────────────────────────────────────────
    float inAcc = 0.f, outAcc = 0.f;

    // ── Test tone params ──────────────────────────────────────────────
    const bool   testOn    = testToneEnabled.load();
    const float  sr_f      = static_cast<float> (currentSampleRate);
    const float  pulseLen  = sr_f * 1.5f;
    const float  noteFreq1 = 440.f; // A4
    const float  noteFreq2 = 554.f; // C#5 → A major chord

    // ── Scope write position ──────────────────────────────────────────
    int scopePos = scopeWritePos.load (std::memory_order_relaxed);

    // ── Sample loop ───────────────────────────────────────────────────
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float inL = left[i]  * inputGain;
        float inR = right[i] * inputGain;

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
                              ? (testToneTrigger * 0.25f)
                              : std::exp (-5.f * testToneTrigger / sr_f);

            const float s1 = std::sin (testTonePhase  * juce::MathConstants<float>::twoPi);
            const float s2 = std::sin (testTonePhase2 * juce::MathConstants<float>::twoPi);
            testTonePhase  += noteFreq1 / sr_f;
            testTonePhase2 += noteFreq2 / sr_f;
            if (testTonePhase  >= 1.f) testTonePhase  -= 1.f;
            if (testTonePhase2 >= 1.f) testTonePhase2 -= 1.f;

            const float tone = (s1 * 0.6f + s2 * 0.4f) * env * 0.4f;
            inL += tone;
            inR += tone;
        }

        // ── Tape noise injection ──────────────────────────────────────
        inL += noiseL.process (tapeNoise);
        inR += noiseR.process (tapeNoise);

        inAcc += std::abs (inL);

        // ── Tape delay ────────────────────────────────────────────────
        auto headsL = tapeL.process (inL, baseDelaySamples, feedbackL, wowFlutter, saturation);
        auto headsR = tapeR.process (inR, baseDelaySamples, feedbackR, wowFlutter, saturation);

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
        if (numActiveHeads > 0)
        {
            echoL /= static_cast<float> (numActiveHeads);
            echoR /= static_cast<float> (numActiveHeads);
        }

        // ── Bass / treble EQ on echo ──────────────────────────────────
        echoL = bassL.processSample   (echoL);
        echoL = trebleL.processSample (echoL);
        echoR = bassR.processSample   (echoR);
        echoR = trebleR.processSample (echoR);

        // ── Update feedback (with optional ping-pong cross-feed) ──────
        if (pingpong)
        {
            // Cross-feed: L feedback gets R echo, and vice versa
            feedbackL = echoR * intensity;
            feedbackR = echoL * intensity;
        }
        else
        {
            feedbackL = echoL * intensity;
            feedbackR = echoR * intensity;
        }

        // ── Spring reverb ─────────────────────────────────────────────
        float revL = 0.f, revR = 0.f;
        if (mc.reverb)
        {
            revL = springL.process (inL + echoL * 0.15f);
            revR = springR.process (inR + echoR * 0.15f);

            // Shimmer chorus on reverb tail
            if (shimmer > 0.001f)
            {
                revL += shimmerL.process (revL, shimmer);
                revR += shimmerR.process (revR, shimmer);
            }
        }

        // ── Output mix ────────────────────────────────────────────────
        const float outL = inL + echoL * echoLevel + revL * reverbLevel;
        const float outR = inR + echoR * echoLevel + revR * reverbLevel;

        left[i]  = outL;
        right[i] = outR;
        outAcc  += std::abs (outL);

        // ── Write to scope buffer (left channel only) ─────────────────
        scopeBuffer[scopePos] = outL;
        scopePos = (scopePos + 1) % SCOPE_SIZE;
    }

    scopeWritePos.store (scopePos, std::memory_order_relaxed);

    const float inv = 1.f / static_cast<float> (std::max (1, buffer.getNumSamples()));
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

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
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
//  Editor
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* SpaceEchoAudioProcessor::createEditor()
{
    return new SpaceEchoAudioProcessorEditor (*this);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Factory
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpaceEchoAudioProcessor();
}
