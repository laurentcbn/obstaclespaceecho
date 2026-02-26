#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────
SpaceEchoAudioProcessorEditor::SpaceEchoAudioProcessorEditor (SpaceEchoAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    // ── Knobs ─────────────────────────────────────────────────────────
    auto& apvts = processor.apvts;

    knobInput    = std::make_unique<LabelledKnob> ("INPUT",      apvts, "inputGain");
    knobRepeat   = std::make_unique<LabelledKnob> ("RATE",       apvts, "repeatRate");
    knobIntensity= std::make_unique<LabelledKnob> ("INTENSITY",  apvts, "intensity");
    knobBass     = std::make_unique<LabelledKnob> ("BASS",       apvts, "bass");
    knobTreble   = std::make_unique<LabelledKnob> ("TREBLE",     apvts, "treble");
    knobWow      = std::make_unique<LabelledKnob> ("WOW/FLT",   apvts, "wowFlutter");
    knobSat      = std::make_unique<LabelledKnob> ("SATURATION", apvts, "saturation");
    knobEcho     = std::make_unique<LabelledKnob> ("ECHO LVL",  apvts, "echoLevel");
    knobReverb   = std::make_unique<LabelledKnob> ("REVB LVL",  apvts, "reverbLevel");

    addKnob (*knobInput);
    addKnob (*knobRepeat);
    addKnob (*knobIntensity);
    addKnob (*knobBass);
    addKnob (*knobTreble);
    addKnob (*knobWow);
    addKnob (*knobSat);
    addKnob (*knobEcho);
    addKnob (*knobReverb);

    // ── VU meters ─────────────────────────────────────────────────────
    addAndMakeVisible (vuIn);
    addAndMakeVisible (vuOut);

    // ── Test tone button — dans le header, côté droit ─────────────────
    testToneBtn.setClickingTogglesState (true);
    testToneBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xFF2A2A2A));
    testToneBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFFCC4400));
    testToneBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xFFE07020));
    testToneBtn.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    testToneBtn.onClick = [this]
    {
        processor.setTestTone (testToneBtn.getToggleState());
    };
    addAndMakeVisible (testToneBtn);

    // ── Mode selector ─────────────────────────────────────────────────
    addAndMakeVisible (modeSelector);

    // Sync mode selector with parameter
    modeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("mode"),
        [this] (float v)
        {
            modeSelector.setMode (static_cast<int> (v));
        },
        apvts.undoManager);

    modeSelector.onModeChanged = [this] (int m)
    {
        if (auto* param = processor.apvts.getParameter ("mode"))
        {
            const float normalised = param->convertTo0to1 (static_cast<float> (m));
            param->setValueNotifyingHost (normalised);
        }
    };

    // Apply style to knob labels
    auto styliseLabel = [&] (juce::Label& l)
    {
        l.setFont (IndustrialLookAndFeel::getIndustrialFont (10.f));
        l.setColour (juce::Label::textColourId, juce::Colour (IndustrialLookAndFeel::COL_LABEL));
        l.setJustificationType (juce::Justification::centred);
    };

    styliseLabel (knobInput->label);
    styliseLabel (knobRepeat->label);
    styliseLabel (knobIntensity->label);
    styliseLabel (knobBass->label);
    styliseLabel (knobTreble->label);
    styliseLabel (knobWow->label);
    styliseLabel (knobSat->label);
    styliseLabel (knobEcho->label);
    styliseLabel (knobReverb->label);

    // Style knob text boxes
    auto styliseKnob = [&] (juce::Slider& s)
    {
        s.setColour (juce::Slider::textBoxTextColourId,     juce::Colour (IndustrialLookAndFeel::COL_AMBER));
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xFF0A0A0A));
        s.setColour (juce::Slider::textBoxOutlineColourId,  juce::Colour (0xFF333333));
    };

    styliseKnob (knobInput->knob);
    styliseKnob (knobRepeat->knob);
    styliseKnob (knobIntensity->knob);
    styliseKnob (knobBass->knob);
    styliseKnob (knobTreble->knob);
    styliseKnob (knobWow->knob);
    styliseKnob (knobSat->knob);
    styliseKnob (knobEcho->knob);
    styliseKnob (knobReverb->knob);

    // ── setSize en dernier (déclenche resized() — tous les composants existent) ──
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
    const auto bounds = getLocalBounds().toFloat();

    // ── Main background ──────────────────────────────────────────────
    juce::ColourGradient bg (
        juce::Colour (0xFF1C1C1C), 0.f, 0.f,
        juce::Colour (0xFF111111), 0.f, static_cast<float> (getHeight()),
        false);
    g.setGradientFill (bg);
    g.fillAll();

    // ── Header panel ─────────────────────────────────────────────────
    {
        const juce::Rectangle<float> header (0.f, 0.f, static_cast<float> (W), 60.f);
        IndustrialLookAndFeel::drawMetalPanel (g, header, juce::Colour (0xFF252525));

        // Power LED (left anchor)
        IndustrialLookAndFeel::drawLED (g, 28.f, 30.f, 7.f,
                                        juce::Colour (IndustrialLookAndFeel::COL_GREEN), true);

        // ── Brand block: "OBSTACLE" (small) / "SPACE ECHO" (large) ──
        const float titleX = 46.f;
        // Company name – small dimmed caps
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_ORANGE).withAlpha (0.6f));
        g.drawText ("OBSTACLE", juce::Rectangle<float> (titleX, 8.f, 200.f, 14.f),
                    juce::Justification::centredLeft);
        // Product name – large bold
        g.setFont (juce::Font (juce::FontOptions ("Arial", 24.f, juce::Font::bold)));
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_ORANGE));
        g.drawText ("SPACE ECHO", juce::Rectangle<float> (titleX, 24.f, 260.f, 28.f),
                    juce::Justification::centredLeft);

        // Version – right edge
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM).withAlpha (0.5f));
        g.drawText ("v1.0", juce::Rectangle<float> (static_cast<float> (W) - 55.f, 0.f, 46.f, 60.f),
                    juce::Justification::centred);

        // Screws corners
        const float sd = 14.f;
        for (auto [sx, sy] : std::initializer_list<std::pair<float,float>> {
                { sd, sd }, { W - sd, sd }, { sd, 60.f - sd }, { W - sd, 60.f - sd } })
        {
            g.setColour (juce::Colour (0xFF3A3A3A));
            g.fillEllipse (sx - 4.f, sy - 4.f, 8.f, 8.f);
            g.setColour (juce::Colour (0xFF0A0A0A));
            g.drawEllipse (sx - 4.f, sy - 4.f, 8.f, 8.f, 1.f);
            // Phillips screw
            g.setColour (juce::Colour (0xFF555555));
            g.drawLine (sx - 2.5f, sy, sx + 2.5f, sy, 1.f);
            g.drawLine (sx, sy - 2.5f, sx, sy + 2.5f, 1.f);
        }
    }

    // ── VU meter panel ───────────────────────────────────────────────
    {
        IndustrialLookAndFeel::drawMetalPanel (g,
            juce::Rectangle<float> (8.f, 68.f, 82.f, 320.f),
            juce::Colour (0xFF1A1A1A));
    }

    // ── Control panels (Row A & B) ────────────────────────────────────
    {
        IndustrialLookAndFeel::drawMetalPanel (g,
            juce::Rectangle<float> (98.f, 68.f, static_cast<float> (W) - 106.f, 155.f),
            juce::Colour (0xFF202020));

        IndustrialLookAndFeel::drawMetalPanel (g,
            juce::Rectangle<float> (98.f, 231.f, static_cast<float> (W) - 106.f, 155.f),
            juce::Colour (0xFF202020));

        // Section labels
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM));
        g.drawText ("─── TAPE  CONTROLS ───",
                    juce::Rectangle<float> (98.f, 68.f, 300.f, 20.f),
                    juce::Justification::centredLeft);
        g.drawText ("─── SIGNAL  PATH ───",
                    juce::Rectangle<float> (98.f, 231.f, 280.f, 20.f),
                    juce::Justification::centredLeft);
    }

    // ── Mode strip ───────────────────────────────────────────────────
    {
        IndustrialLookAndFeel::drawMetalPanel (g,
            juce::Rectangle<float> (8.f, 395.f, static_cast<float> (W) - 16.f, 55.f),
            juce::Colour (0xFF181818));

        g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM));
        g.drawText ("MODE SELECT", juce::Rectangle<float> (14.f, 396.f, 90.f, 14.f),
                    juce::Justification::centredLeft);
    }

    // ── Corner screws (panel) ─────────────────────────────────────────
    for (auto [sx, sy] : std::initializer_list<std::pair<float,float>> {
            { 18.f, static_cast<float> (H) - 18.f },
            { static_cast<float> (W) - 18.f, static_cast<float> (H) - 18.f } })
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
    // ── VU meters ─────────────────────────────────────────────────────
    vuIn .setBounds ( 14,  76, 34, 272);
    vuOut.setBounds ( 52,  76, 34, 272);

    // ── Test tone button — header, côté droit ────────────────────────
    testToneBtn.setBounds (W - 170, 10, 110, 38);

    // ── Knob rows ─────────────────────────────────────────────────────
    // Row A: x starts at 100, y = 70, height = 158
    const juce::Rectangle<int> rowA (100, 72, W - 108, 154);
    layoutKnobRow ({ knobInput.get(),    knobRepeat.get(),
                     knobIntensity.get(), knobBass.get(),
                     knobTreble.get() }, rowA);

    // Row B: y = 232
    const juce::Rectangle<int> rowB (100, 234, W - 108, 154);
    layoutKnobRow ({ knobWow.get(), knobSat.get(),
                     knobEcho.get(), knobReverb.get() }, rowB);

    // ── Mode selector ─────────────────────────────────────────────────
    modeSelector.setBounds (12, 408, W - 24, 38);
}

// ─────────────────────────────────────────────────────────────────────────────
//  layoutKnobRow
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::layoutKnobRow (
    std::initializer_list<LabelledKnob*> knobs,
    juce::Rectangle<int> rowBounds,
    int margin)
{
    const int n    = static_cast<int> (knobs.size());
    if (n == 0) return;

    const int slotW = rowBounds.getWidth() / n;
    const int labelH = 18;

    int idx = 0;
    for (auto* k : knobs)
    {
        const int x     = rowBounds.getX() + idx * slotW + margin;
        const int y     = rowBounds.getY() + margin;
        const int kSize = slotW - margin * 2;
        const int kH    = rowBounds.getHeight() - labelH - margin * 2;

        k->knob .setBounds (x, y, kSize, kH);
        k->label.setBounds (x, y + kH, kSize, labelH);
        ++idx;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  timerCallback — push audio levels to VU meters
// ─────────────────────────────────────────────────────────────────────────────
void SpaceEchoAudioProcessorEditor::timerCallback()
{
    vuIn .setLevel (processor.getInputLevel());
    vuOut.setLevel (processor.getOutputLevel());
}
