#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: style a toggle button with custom on/off colours
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::styliseToggleButton (
    juce::TextButton& btn,
    juce::Colour offBg, juce::Colour onBg,
    juce::Colour offText, juce::Colour onText)
{
    btn.setClickingTogglesState (true);
    btn.setColour (juce::TextButton::buttonColourId,   offBg);
    btn.setColour (juce::TextButton::buttonOnColourId, onBg);
    btn.setColour (juce::TextButton::textColourOffId,  offText);
    btn.setColour (juce::TextButton::textColourOnId,   onText);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────
SpaceEchoAudioProcessorEditor::SpaceEchoAudioProcessorEditor (SpaceEchoAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    auto& apvts = processor.apvts;

    // ── Row A knobs ────────────────────────────────────────────────────
    knobInput    = std::make_unique<LabelledKnob> ("INPUT",     apvts, "inputGain");
    knobRepeat   = std::make_unique<LabelledKnob> ("RATE",      apvts, "repeatRate");
    knobIntensity= std::make_unique<LabelledKnob> ("INTENSITY", apvts, "intensity");
    knobBass     = std::make_unique<LabelledKnob> ("BASS",      apvts, "bass");
    knobTreble   = std::make_unique<LabelledKnob> ("TREBLE",    apvts, "treble");

    // ── Row B knobs ────────────────────────────────────────────────────
    knobWow    = std::make_unique<LabelledKnob> ("WOW/FLT",   apvts, "wowFlutter");
    knobSat    = std::make_unique<LabelledKnob> ("SATURATE",  apvts, "saturation");
    knobEcho   = std::make_unique<LabelledKnob> ("ECHO LVL",  apvts, "echoLevel");
    knobReverb = std::make_unique<LabelledKnob> ("REVERB LVL",apvts, "reverbLevel");
    knobNoise  = std::make_unique<LabelledKnob> ("NOISE",     apvts, "tapeNoise");
    knobShimmer= std::make_unique<LabelledKnob> ("SHIMMER",   apvts, "shimmer");

    addKnob (*knobInput);    addKnob (*knobRepeat);   addKnob (*knobIntensity);
    addKnob (*knobBass);     addKnob (*knobTreble);
    addKnob (*knobWow);      addKnob (*knobSat);      addKnob (*knobEcho);
    addKnob (*knobReverb);   addKnob (*knobNoise);    addKnob (*knobShimmer);

    // ── VU meters ──────────────────────────────────────────────────────
    addAndMakeVisible (vuIn);
    addAndMakeVisible (vuOut);

    // ── Tape reel animation ────────────────────────────────────────────
    addAndMakeVisible (tapeReels);

    // ── Oscilloscope ──────────────────────────────────────────────────
    addAndMakeVisible (oscilloscope);

    // ── Test tone button ───────────────────────────────────────────────
    styliseToggleButton (testToneBtn,
        juce::Colour (0xFF2A2A2A), juce::Colour (0xFFCC4400),
        juce::Colour (0xFFE07020), juce::Colours::white);
    testToneBtn.onClick = [this] { processor.setTestTone (testToneBtn.getToggleState()); };
    addAndMakeVisible (testToneBtn);

    // ── FREEZE button ──────────────────────────────────────────────────
    styliseToggleButton (freezeBtn,
        juce::Colour (0xFF1A2A3A), juce::Colour (0xFF0055CC),
        juce::Colour (0xFF6699CC), juce::Colours::white);
    addAndMakeVisible (freezeBtn);

    freezeAttachment = std::make_unique<juce::ButtonParameterAttachment> (
        *apvts.getParameter ("freeze"), freezeBtn, apvts.undoManager);

    // ── PING-PONG button ───────────────────────────────────────────────
    styliseToggleButton (pingpongBtn,
        juce::Colour (0xFF2A1A3A), juce::Colour (0xFF7700CC),
        juce::Colour (0xFFAA66CC), juce::Colours::white);
    addAndMakeVisible (pingpongBtn);

    pingpongAttachment = std::make_unique<juce::ButtonParameterAttachment> (
        *apvts.getParameter ("pingpong"), pingpongBtn, apvts.undoManager);

    // ── Mode selector ──────────────────────────────────────────────────
    addAndMakeVisible (modeSelector);

    modeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("mode"),
        [this] (float v) { modeSelector.setMode (static_cast<int> (v)); },
        apvts.undoManager);

    modeSelector.onModeChanged = [this] (int m)
    {
        if (auto* param = processor.apvts.getParameter ("mode"))
        {
            const float norm = param->convertTo0to1 (static_cast<float> (m));
            param->setValueNotifyingHost (norm);
        }
    };

    // ── Style knob labels and text boxes ──────────────────────────────
    auto styliseLabel = [&] (juce::Label& l)
    {
        l.setFont (IndustrialLookAndFeel::getIndustrialFont (10.f));
        l.setColour (juce::Label::textColourId, juce::Colour (IndustrialLookAndFeel::COL_LABEL));
        l.setJustificationType (juce::Justification::centred);
    };

    auto styliseKnob = [&] (juce::Slider& s)
    {
        s.setColour (juce::Slider::textBoxTextColourId,     juce::Colour (IndustrialLookAndFeel::COL_AMBER));
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xFF0A0A0A));
        s.setColour (juce::Slider::textBoxOutlineColourId,  juce::Colour (0xFF333333));
    };

    for (auto* k : { knobInput.get(), knobRepeat.get(), knobIntensity.get(),
                     knobBass.get(), knobTreble.get(), knobWow.get(), knobSat.get(),
                     knobEcho.get(), knobReverb.get(), knobNoise.get(), knobShimmer.get() })
    {
        styliseLabel (k->label);
        styliseKnob  (k->knob);
    }

    setResizable (false, false);
    setSize (W, H);
    startTimerHz (30);
}

SpaceEchoAudioProcessorEditor::~SpaceEchoAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
//  addKnob helper
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::addKnob (LabelledKnob& k)
{
    addAndMakeVisible (k.knob);
    addAndMakeVisible (k.label);
}

// ─────────────────────────────────────────────────────────────────────────────
//  paint
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float fW = static_cast<float> (W);
    const float fH = static_cast<float> (H);

    // ── Main gradient background ──────────────────────────────────────
    {
        juce::ColourGradient bg (
            juce::Colour (0xFF1C1C1C), 0.f, 0.f,
            juce::Colour (0xFF0E0E0E), 0.f, fH,
            false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    // ── HEADER panel (h=72) ───────────────────────────────────────────
    {
        IndustrialLookAndFeel::drawMetalPanel (g,
            juce::Rectangle<float> (0.f, 0.f, fW, 72.f),
            juce::Colour (0xFF252525));

        // Power LED
        IndustrialLookAndFeel::drawLED (g, 28.f, 36.f, 7.f,
            juce::Colour (IndustrialLookAndFeel::COL_GREEN), true);

        // Brand – "OBSTACLE" small / "SPACE ECHO" large
        const float tx = 46.f;
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_ORANGE).withAlpha (0.6f));
        g.drawText ("OBSTACLE", juce::Rectangle<float> (tx, 10.f, 200.f, 14.f),
                    juce::Justification::centredLeft);

        g.setFont (juce::Font (juce::FontOptions ("Arial", 24.f, juce::Font::bold)));
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_ORANGE));
        g.drawText ("SPACE ECHO", juce::Rectangle<float> (tx, 26.f, 260.f, 30.f),
                    juce::Justification::centredLeft);

        // Version
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM).withAlpha (0.5f));
        g.drawText ("v1.1", juce::Rectangle<float> (fW - 52.f, 0.f, 44.f, 72.f),
                    juce::Justification::centred);

        // Screws
        for (auto [sx, sy] : std::initializer_list<std::pair<float,float>> {
                { 14.f, 14.f }, { fW - 14.f, 14.f },
                { 14.f, 58.f }, { fW - 14.f, 58.f } })
        {
            g.setColour (juce::Colour (0xFF3A3A3A));
            g.fillEllipse (sx - 4.f, sy - 4.f, 8.f, 8.f);
            g.setColour (juce::Colour (0xFF0A0A0A));
            g.drawEllipse (sx - 4.f, sy - 4.f, 8.f, 8.f, 1.f);
            g.setColour (juce::Colour (0xFF555555));
            g.drawLine (sx - 2.5f, sy, sx + 2.5f, sy, 1.f);
            g.drawLine (sx, sy - 2.5f, sx, sy + 2.5f, 1.f);
        }
    }

    // ── LEFT panel (VU meters + tape reels) ──────────────────────────
    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (6.f, 78.f, 90.f, 328.f),
        juce::Colour (0xFF1A1A1A));

    // ── CENTRE knob panels (Row A + Row B) ────────────────────────────
    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (102.f, 78.f, 596.f, 158.f),
        juce::Colour (0xFF202020));

    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (102.f, 244.f, 596.f, 158.f),
        juce::Colour (0xFF202020));

    // Section labels
    g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
    g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM));
    g.drawText ("─── TAPE  CONTROLS ───",
                juce::Rectangle<float> (104.f, 78.f, 280.f, 18.f),
                juce::Justification::centredLeft);
    g.drawText ("─── SIGNAL  PATH ───",
                juce::Rectangle<float> (104.f, 244.f, 260.f, 18.f),
                juce::Justification::centredLeft);

    // ── RIGHT panel (oscilloscope) ────────────────────────────────────
    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (704.f, 78.f, 250.f, 328.f),
        juce::Colour (0xFF151515));

    // Oscilloscope label
    g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
    g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM));
    g.drawText ("WAVEFORM",
                juce::Rectangle<float> (704.f, 78.f, 250.f, 16.f),
                juce::Justification::centred);

    // ── MODE strip ────────────────────────────────────────────────────
    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (6.f, 412.f, fW - 12.f, 60.f),
        juce::Colour (0xFF181818));

    g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
    g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM));
    g.drawText ("MODE SELECT", juce::Rectangle<float> (12.f, 413.f, 90.f, 14.f),
                juce::Justification::centredLeft);

    // ── Bottom screws ─────────────────────────────────────────────────
    for (auto [sx, sy] : std::initializer_list<std::pair<float,float>> {
            { 18.f, fH - 18.f }, { fW - 18.f, fH - 18.f } })
    {
        g.setColour (juce::Colour (0xFF3A3A3A));
        g.fillEllipse (sx - 4.f, sy - 4.f, 8.f, 8.f);
        g.setColour (juce::Colour (0xFF0A0A0A));
        g.drawEllipse (sx - 4.f, sy - 4.f, 8.f, 8.f, 1.f);
        g.setColour (juce::Colour (0xFF555555));
        g.drawLine (sx - 2.5f, sy, sx + 2.5f, sy, 1.f);
        g.drawLine (sx, sy - 2.5f, sx, sy + 2.5f, 1.f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  resized
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::resized()
{
    // ── Header buttons ────────────────────────────────────────────────
    // FREEZE / PING-PONG in the centre of the header
    freezeBtn  .setBounds (330, 14, 100, 42);
    pingpongBtn.setBounds (440, 14, 120, 42);
    testToneBtn.setBounds (W - 182, 14, 110, 42);

    // ── VU meters ─────────────────────────────────────────────────────
    vuIn .setBounds (10,  86, 32, 250);
    vuOut.setBounds (58,  86, 32, 250);

    // ── Tape reels ────────────────────────────────────────────────────
    tapeReels.setBounds (8, 340, 88, 62);

    // ── Oscilloscope ──────────────────────────────────────────────────
    oscilloscope.setBounds (710, 98, 238, 148);

    // ── Knob Row A (5 knobs: Input, Rate, Intensity, Bass, Treble) ────
    const juce::Rectangle<int> rowA (104, 82, 594, 154);
    layoutKnobRow ({ knobInput.get(), knobRepeat.get(), knobIntensity.get(),
                     knobBass.get(), knobTreble.get() }, rowA);

    // ── Knob Row B (6 knobs: Wow, Sat, Echo, Reverb, Noise, Shimmer) ─
    const juce::Rectangle<int> rowB (104, 248, 594, 154);
    layoutKnobRow ({ knobWow.get(), knobSat.get(), knobEcho.get(),
                     knobReverb.get(), knobNoise.get(), knobShimmer.get() }, rowB);

    // ── Mode selector ─────────────────────────────────────────────────
    modeSelector.setBounds (10, 424, W - 20, 40);
}

// ─────────────────────────────────────────────────────────────────────────────
//  layoutKnobRow
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::layoutKnobRow (
    std::initializer_list<LabelledKnob*> knobs,
    juce::Rectangle<int> rowBounds,
    int margin)
{
    const int n = static_cast<int> (knobs.size());
    if (n == 0) return;

    const int slotW  = rowBounds.getWidth() / n;
    const int labelH = 18;
    int idx = 0;

    for (auto* k : knobs)
    {
        const int x  = rowBounds.getX() + idx * slotW + margin;
        const int y  = rowBounds.getY() + margin;
        const int kW = slotW - margin * 2;
        const int kH = rowBounds.getHeight() - labelH - margin * 2;

        k->knob .setBounds (x, y, kW, kH);
        k->label.setBounds (x, y + kH, kW, labelH);
        ++idx;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  timerCallback — update meters + oscilloscope + reel animation
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::timerCallback()
{
    // ── VU meters ─────────────────────────────────────────────────────
    vuIn .setLevel (processor.getInputLevel());
    vuOut.setLevel (processor.getOutputLevel());

    // ── Oscilloscope ──────────────────────────────────────────────────
    oscilloscope.refresh (processor.getScopeBuffer(),
                          SpaceEchoAudioProcessor::SCOPE_SIZE,
                          processor.getScopeWritePos());

    // ── Tape reel rotation ────────────────────────────────────────────
    const float rateMs   = *processor.apvts.getRawParameterValue ("repeatRate");
    const bool  frozen   = *processor.apvts.getRawParameterValue ("freeze") > 0.5f;

    // Angular speed: faster RATE → slower-looking reel spin (tape consumed)
    // Base: 2 rotations/sec at 150ms, scaled by rate
    const float rps      = 1.5f / (rateMs * 0.001f); // rotations per second
    const float dAngle   = rps * juce::MathConstants<float>::twoPi / 30.f; // 30 fps

    tapeReels.setFrozen (frozen);
    tapeReels.advance (dAngle);
}
