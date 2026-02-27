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

    // ── Knobs ──────────────────────────────────────────────────────────
    // Left panel
    knobInput    = std::make_unique<LabelledKnob> ("INPUT",     apvts, "inputGain");
    knobRepeat   = std::make_unique<LabelledKnob> ("RATE",      apvts, "repeatRate");
    knobIntensity= std::make_unique<LabelledKnob> ("INTENSITY", apvts, "intensity");

    // Green panel Row 1
    knobBass     = std::make_unique<LabelledKnob> ("BASS",      apvts, "bass");
    knobTreble   = std::make_unique<LabelledKnob> ("TREBLE",    apvts, "treble");
    knobEcho     = std::make_unique<LabelledKnob> ("ECHO LVL",  apvts, "echoLevel");
    knobReverb   = std::make_unique<LabelledKnob> ("REVERB LVL",apvts, "reverbLevel");

    // Green panel Row 2
    knobWow      = std::make_unique<LabelledKnob> ("WOW/FLT",   apvts, "wowFlutter");
    knobSat      = std::make_unique<LabelledKnob> ("SATURATE",  apvts, "saturation");
    knobNoise    = std::make_unique<LabelledKnob> ("NOISE",      apvts, "tapeNoise");
    knobShimmer  = std::make_unique<LabelledKnob> ("SHIMMER",   apvts, "shimmer");

    // Sync division (left panel)
    knobSyncDiv  = std::make_unique<LabelledKnob> ("DIV",        apvts, "syncDiv");

    addKnob (*knobInput);    addKnob (*knobRepeat);   addKnob (*knobIntensity);
    addKnob (*knobBass);     addKnob (*knobTreble);
    addKnob (*knobEcho);     addKnob (*knobReverb);
    addKnob (*knobWow);      addKnob (*knobSat);
    addKnob (*knobNoise);    addKnob (*knobShimmer);
    addKnob (*knobSyncDiv);

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
        juce::Colour (0xFF1A2A3A), juce::Colour (0xFF004EBB),
        juce::Colour (0xFF5588BB), juce::Colours::white);
    addAndMakeVisible (freezeBtn);

    freezeAttachment = std::make_unique<juce::ButtonParameterAttachment> (
        *apvts.getParameter ("freeze"), freezeBtn, apvts.undoManager);

    // ── PING-PONG button ───────────────────────────────────────────────
    styliseToggleButton (pingpongBtn,
        juce::Colour (0xFF2A1A3A), juce::Colour (0xFF6600BB),
        juce::Colour (0xFF9966BB), juce::Colours::white);
    addAndMakeVisible (pingpongBtn);

    pingpongAttachment = std::make_unique<juce::ButtonParameterAttachment> (
        *apvts.getParameter ("pingpong"), pingpongBtn, apvts.undoManager);

    // ── SYNC button ────────────────────────────────────────────────────
    styliseToggleButton (syncBtn,
        juce::Colour (0xFF1A2A1A), juce::Colour (0xFF007722),
        juce::Colour (0xFF44BB66), juce::Colours::white);
    addAndMakeVisible (syncBtn);

    syncAttachment = std::make_unique<juce::ButtonParameterAttachment> (
        *apvts.getParameter ("sync"), syncBtn, apvts.undoManager);

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
        l.setFont (IndustrialLookAndFeel::getIndustrialFont (9.5f));
        l.setColour (juce::Label::textColourId, juce::Colour (IndustrialLookAndFeel::COL_LABEL));
        l.setJustificationType (juce::Justification::centred);
    };

    auto styliseKnob = [&] (juce::Slider& s)
    {
        s.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (IndustrialLookAndFeel::COL_AMBER));
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xFF080808));
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour (0xFF2A2A2A));
    };

    for (auto* k : { knobInput.get(), knobRepeat.get(), knobIntensity.get(),
                     knobBass.get(), knobTreble.get(), knobEcho.get(), knobReverb.get(),
                     knobWow.get(), knobSat.get(), knobNoise.get(), knobShimmer.get(),
                     knobSyncDiv.get() })
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
//  paint  — RE-201 three-section layout  (skeuomorphic)
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float fW = static_cast<float> (W);
    const float fH = static_cast<float> (H);

    // ── Overall body (near-black base coat) ──────────────────────────
    g.setColour (juce::Colour (0xFF0E0E0E));
    g.fillAll();

    // ── Chrome Phillips-head screw (highly detailed, skeuomorphic) ───
    auto drawScrew = [&] (float sx, float sy)
    {
        const float r = 5.8f;

        // Drop shadow below screw
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (sx - r, sy - r + 3.f, r * 2.f, r * 2.f);

        // Chrome bezel ring (gradient top-left bright → bottom-right dark)
        {
            juce::ColourGradient bezel (
                juce::Colour (0xFF909090), sx - r, sy - r,
                juce::Colour (0xFF282828), sx + r, sy + r, false);
            g.setGradientFill (bezel);
            g.fillEllipse (sx - r, sy - r, r * 2.f, r * 2.f);
        }

        // Screw head — recessed disc (dark centre with slight radial gradient)
        {
            const float ir = r * 0.68f;
            juce::ColourGradient head (
                juce::Colour (0xFF333333), sx - ir * 0.5f, sy - ir * 0.5f,
                juce::Colour (0xFF555555), sx + ir * 0.6f, sy + ir * 0.6f, false);
            g.setGradientFill (head);
            g.fillEllipse (sx - ir, sy - ir, ir * 2.f, ir * 2.f);
        }

        // Phillips cross (dark slots)
        const float sl = r * 0.44f;
        g.setColour (juce::Colours::black.withAlpha (0.88f));
        g.drawLine (sx - sl, sy, sx + sl, sy, 1.8f);
        g.drawLine (sx, sy - sl, sx, sy + sl, 1.8f);

        // Phillips slot depth highlight (bright edge of slot)
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (sx - sl, sy - 0.7f, sx + sl, sy - 0.7f, 0.8f);
        g.drawLine (sx - 0.7f, sy - sl, sx - 0.7f, sy + sl, 0.8f);

        // Specular highlight (top-left glint on chrome)
        g.setColour (juce::Colours::white.withAlpha (0.42f));
        g.fillEllipse (sx - r * 0.48f, sy - r * 0.62f, r * 0.50f, r * 0.26f);
    };

    // ─────────────────────────────────────────────────────────────────
    //  HEADER strip  (h = 52)
    // ─────────────────────────────────────────────────────────────────
    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (0.f, 0.f, fW, 52.f),
        juce::Colour (0xFF232323));

    // Power LED — red (unit is ON)
    IndustrialLookAndFeel::drawLED (g, 28.f, 26.f, 7.f,
        juce::Colour (IndustrialLookAndFeel::COL_RED), true);

    // Brand — "OBSTACLE" small + "SPACE ECHO" large (silk-screened look)
    {
        const float tx = 48.f;

        // Shadow layer (silk-screen depth)
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (8.5f));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawText ("OBSTACLE",
                    juce::Rectangle<float> (tx + 1.f, 9.f, 160.f, 12.f),
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM).withAlpha (0.70f));
        g.drawText ("OBSTACLE",
                    juce::Rectangle<float> (tx, 8.f, 160.f, 12.f),
                    juce::Justification::centredLeft);

        g.setFont (juce::Font (juce::FontOptions ("Arial", 21.f, juce::Font::bold)));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawText ("SPACE ECHO",
                    juce::Rectangle<float> (tx + 1.f, 21.f, 244.f, 26.f),
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_WHITE));
        g.drawText ("SPACE ECHO",
                    juce::Rectangle<float> (tx, 20.f, 244.f, 26.f),
                    juce::Justification::centredLeft);
    }

    // Version tag
    g.setFont (IndustrialLookAndFeel::getIndustrialFont (8.f));
    g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM).withAlpha (0.35f));
    g.drawText ("v1.2", juce::Rectangle<float> (fW - 50.f, 0.f, 44.f, 52.f),
                juce::Justification::centred);

    drawScrew (14.f, 26.f);
    drawScrew (fW - 14.f, 26.f);

    // ─────────────────────────────────────────────────────────────────
    //  LEFT section  (x = 0..200, y = 52..410)
    // ─────────────────────────────────────────────────────────────────
    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (0.f, 52.f, 200.f, 358.f),
        juce::Colour (0xFF181818));

    // VU sub-section separator + micro-labels (silk-screened)
    g.setColour (juce::Colour (0xFF2A2A2A));
    g.drawLine (6.f, 183.f, 194.f, 183.f, 1.f);

    // Sync section separator + label (between knob row and sync div)
    g.setColour (juce::Colour (0xFF2A2A2A));
    g.drawLine (6.f, 300.f, 194.f, 300.f, 1.f);
    g.setFont (IndustrialLookAndFeel::getIndustrialFont (7.f));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawText ("SYNC DIV", juce::Rectangle<float> (1.f, 303.f, 198.f, 8.f),
                juce::Justification::centred);
    g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM).withAlpha (0.45f));
    g.drawText ("SYNC DIV", juce::Rectangle<float> (0.f, 302.f, 198.f, 8.f),
                juce::Justification::centred);

    g.setFont (IndustrialLookAndFeel::getIndustrialFont (7.f));
    g.setColour (juce::Colours::black.withAlpha (0.5f));     // shadow
    g.drawText ("IN",  juce::Rectangle<float> (9.f,   55.f, 88.f, 8.f), juce::Justification::centred);
    g.drawText ("OUT", juce::Rectangle<float> (105.f,  55.f, 88.f, 8.f), juce::Justification::centred);
    g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM).withAlpha (0.55f));
    g.drawText ("IN",  juce::Rectangle<float> (8.f,   54.f, 88.f, 8.f), juce::Justification::centred);
    g.drawText ("OUT", juce::Rectangle<float> (104.f,  54.f, 88.f, 8.f), juce::Justification::centred);

    // Corner screws for left panel
    drawScrew (14.f, 64.f);
    drawScrew (186.f, 64.f);
    drawScrew (14.f, 400.f);
    drawScrew (186.f, 400.f);

    // ─────────────────────────────────────────────────────────────────
    //  CENTER section  (x = 200..420, y = 52..410)
    // ─────────────────────────────────────────────────────────────────
    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (200.f, 52.f, 220.f, 358.f),
        juce::Colour (0xFF151515));

    // Separator + label between dial and oscilloscope
    g.setColour (juce::Colour (0xFF252525));
    g.drawLine (204.f, 307.f, 416.f, 307.f, 1.f);

    g.setFont (IndustrialLookAndFeel::getIndustrialFont (7.f));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawText ("WAVEFORM", juce::Rectangle<float> (205.f, 310.f, 216.f, 9.f),
                juce::Justification::centred);
    g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM).withAlpha (0.50f));
    g.drawText ("WAVEFORM", juce::Rectangle<float> (204.f, 309.f, 216.f, 9.f),
                juce::Justification::centred);

    // ─────────────────────────────────────────────────────────────────
    //  RIGHT green section  (x = 420..960, y = 52..410)
    // ─────────────────────────────────────────────────────────────────
    IndustrialLookAndFeel::drawGreenPanel (g,
        juce::Rectangle<float> (420.f, 52.f, 540.f, 358.f));

    // Horizontal groove between knob rows
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (428.f, 219.f, 524.f, 3.f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (222, 428.f, 952.f);

    // Row labels (silk-screened — shadow + label)
    g.setFont (IndustrialLookAndFeel::getIndustrialFont (7.5f));
    for (int pass = 0; pass < 2; ++pass)
    {
        const float off = pass == 0 ? 1.f : 0.f;
        g.setColour (pass == 0 ? juce::Colours::black.withAlpha (0.50f)
                               : juce::Colour (IndustrialLookAndFeel::COL_WHITE).withAlpha (0.22f));
        g.drawText ("EQ & MIX",
                    juce::Rectangle<float> (424.f + off, 54.f + off, 200.f, 9.f),
                    juce::Justification::centredLeft);
        g.drawText ("TAPE MODULATION",
                    juce::Rectangle<float> (424.f + off, 224.f + off, 260.f, 9.f),
                    juce::Justification::centredLeft);
    }

    // Green panel corner screws
    drawScrew (432.f, 64.f);
    drawScrew (948.f, 64.f);
    drawScrew (432.f, 400.f);
    drawScrew (948.f, 400.f);

    // ─────────────────────────────────────────────────────────────────
    //  Section groove seams (recessed physical joints)
    // ─────────────────────────────────────────────────────────────────

    // Header → Main body seam
    g.setColour (juce::Colours::black.withAlpha (0.92f));
    g.fillRect (0.f, 49.f, fW, 4.f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (53, 0.f, fW);

    // Main body → Footer seam
    g.setColour (juce::Colours::black.withAlpha (0.92f));
    g.fillRect (0.f, 407.f, fW, 4.f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (411, 0.f, fW);

    // Left | Center vertical groove seam
    g.setColour (juce::Colours::black.withAlpha (0.92f));
    g.fillRect (197.f, 52.f, 5.f, 358.f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawLine (202.f, 52.f, 202.f, 410.f, 1.f);

    // Center | Green vertical groove seam
    g.setColour (juce::Colours::black.withAlpha (0.92f));
    g.fillRect (417.f, 52.f, 5.f, 358.f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawLine (422.f, 52.f, 422.f, 410.f, 1.f);

    // ─────────────────────────────────────────────────────────────────
    //  FOOTER strip  (y = 410..460, h = 50)
    // ─────────────────────────────────────────────────────────────────
    IndustrialLookAndFeel::drawMetalPanel (g,
        juce::Rectangle<float> (0.f, 410.f, fW, 50.f),
        juce::Colour (0xFF1A1A1A));

    drawScrew (18.f, 435.f);
    drawScrew (fW - 18.f, 435.f);

    // Footer text (engraved look — dark shadow offset below)
    for (int pass = 0; pass < 2; ++pass)
    {
        const float off = pass == 0 ? 1.f : 0.f;
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (7.f));
        g.setColour (pass == 0 ? juce::Colours::black.withAlpha (0.6f)
                               : juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM).withAlpha (0.22f));
        g.drawText ("OBSTACLE SPACE ECHO  ·  RE-201 STYLE TAPE DELAY",
                    juce::Rectangle<float> (200.f + off, 410.f + off, 560.f, 50.f),
                    juce::Justification::centred);
    }

    // ─────────────────────────────────────────────────────────────────
    //  Outer plugin frame / bezel  (physical depth illusion)
    // ─────────────────────────────────────────────────────────────────

    // Top & left bright edge (light source from top-left)
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.drawLine (0.f, 0.f, fW, 0.f, 2.f);
    g.drawLine (0.f, 0.f, 0.f, fH, 2.f);

    // Bottom & right dark edge (shadow)
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawLine (0.f, fH - 1.f, fW, fH - 1.f, 2.f);
    g.drawLine (fW - 1.f, 0.f, fW - 1.f, fH, 2.f);

    // ─────────────────────────────────────────────────────────────────
    //  Vignette overlay  (dark radial gradient on corners)
    // ─────────────────────────────────────────────────────────────────
    {
        juce::ColourGradient vignette (
            juce::Colours::transparentBlack, fW * 0.5f, fH * 0.5f,
            juce::Colours::black.withAlpha (0.38f), 0.f, 0.f,
            true);  // radial = true
        g.setGradientFill (vignette);
        g.fillRect (0.f, 0.f, fW, fH);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  resized  — place all child components
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::resized()
{
    // ── Header buttons ───────────────────────────────────────────────
    freezeBtn  .setBounds (330,     9, 110, 34);
    pingpongBtn.setBounds (448,     9, 130, 34);
    syncBtn    .setBounds (586,     9,  72, 34);
    testToneBtn.setBounds (W - 112, 9, 102, 34);

    // ── LEFT panel ───────────────────────────────────────────────────

    // Analog VU meters (88 × 120, side by side with 8px gap)
    vuIn .setBounds (8,   62, 88, 116);
    vuOut.setBounds (104, 62, 88, 116);

    // Three control knobs: INPUT, RATE, INTENSITY
    layoutKnobRow ({ knobInput.get(), knobRepeat.get(), knobIntensity.get() },
                   { 4, 186, 192, 104 }, 4);

    // Sync division knob — small, centred in the left panel below the knob row
    // Uses the empty space from y=298 to y=408
    {
        const int kW = 80, kH = 60, lblH = 18;
        const int kx = (200 - kW) / 2;   // centered horizontally in 200px panel
        const int ky = 304;
        knobSyncDiv->knob .setBounds (kx, ky,        kW, kH);
        knobSyncDiv->label.setBounds (kx, ky + kH,   kW, lblH);
    }

    // ── CENTER panel ─────────────────────────────────────────────────

    // Large rotary mode selector
    modeSelector.setBounds (200, 54, 220, 248);

    // CRT oscilloscope below the dial
    oscilloscope.setBounds (204, 320, 212, 86);

    // ── RIGHT green panel — 4 + 4 knobs in two rows ──────────────────

    // Row 1: EQ & Mix controls
    layoutKnobRow ({ knobBass.get(), knobTreble.get(), knobEcho.get(), knobReverb.get() },
                   { 420, 64, 540, 153 }, 8);

    // Row 2: Tape modulation controls
    layoutKnobRow ({ knobWow.get(), knobSat.get(), knobNoise.get(), knobShimmer.get() },
                   { 420, 228, 540, 176 }, 8);

    // ── FOOTER ───────────────────────────────────────────────────────

    // Animated tape reels (left of footer)
    tapeReels.setBounds (38, 413, 152, 44);
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
//  timerCallback — update meters, oscilloscope, reel animation
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
    const float rateMs = *processor.apvts.getRawParameterValue ("repeatRate");
    const bool  frozen = *processor.apvts.getRawParameterValue ("freeze") > 0.5f;

    const float rps    = 1.5f / (rateMs * 0.001f);
    const float dAngle = rps * juce::MathConstants<float>::twoPi / 30.f;

    tapeReels.setFrozen (frozen);
    tapeReels.advance (dAngle);
}
