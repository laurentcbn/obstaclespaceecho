#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/IndustrialLookAndFeel.h"
#include "UI/VUMeter.h"
#include "UI/ModeSelector.h"
#include "UI/TapeReelComponent.h"
#include "UI/OscilloscopeComponent.h"

/**
 *  SpaceEchoAudioProcessorEditor  v1.3 — Roland RE-201 faithful layout
 *
 *  960 × 460 px — three-section panel:
 *
 *  ┌─────────────────────────────────────────────────────────────────────────┐
 *  │  HEADER  ● OBSTACLE  SPACE ECHO  [FREEZE] [PING-PONG]       [TEST] v1.2│ h=52
 *  ├──────────────┬──────────────────┬──────────────────────────────────────┤
 *  │  BLACK LEFT  │  CENTER (dark)   │  GREEN RIGHT PANEL                   │
 *  │  w=200       │  w=220           │  w=540                               │
 *  │              │                  │                                      │
 *  │  VU IN  OUT  │  ┌────────────┐  │  Row 1: BASS·TREBLE·ECHO·REVERB     │
 *  │              │  │  MODE DIAL │  │  Row 2: WOW·SAT·NOISE·SHIMMER       │
 *  │  INPUT·RATE  │  │  (rotary)  │  │                                      │
 *  │  INTENSITY   │  └────────────┘  │                                      │
 *  │              │   [OSCILLOSCOPE] │                                      │
 *  ├──────────────┴──────────────────┴──────────────────────────────────────┤
 *  │  FOOTER  [TAPE REELS]                                              v1.2 │ h=50
 *  └─────────────────────────────────────────────────────────────────────────┘
 */
class SpaceEchoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Timer
{
public:
    explicit SpaceEchoAudioProcessorEditor (SpaceEchoAudioProcessor&);
    ~SpaceEchoAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

private:
    SpaceEchoAudioProcessor& processor;
    IndustrialLookAndFeel     lnf;

    // ── Dimensions ───────────────────────────────────────────────────
    static constexpr int W = 960;
    static constexpr int H = 460;

    // ── VU meters ─────────────────────────────────────────────────────
    VUMeter vuIn  { VUMeter::Orientation::Vertical, "IN"  };
    VUMeter vuOut { VUMeter::Orientation::Vertical, "OUT" };

    // ── Tape reels animation ───────────────────────────────────────────
    TapeReelComponent tapeReels;

    // ── Oscilloscope ──────────────────────────────────────────────────
    OscilloscopeComponent oscilloscope;

    // ── Test tone button ──────────────────────────────────────────────
    juce::TextButton testToneBtn { "TEST" };

    // ── FREEZE / PING-PONG / SYNC toggle buttons ─────────────────────
    juce::TextButton freezeBtn    { "FREEZE"    };
    juce::TextButton pingpongBtn  { "PING-PONG" };
    juce::TextButton syncBtn      { "SYNC"      };

    std::unique_ptr<juce::ButtonParameterAttachment> freezeAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> pingpongAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> syncAttachment;

    // ── Mode selector ─────────────────────────────────────────────────
    ModeSelector modeSelector;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    // ── Knobs (sliders in rotary mode) ────────────────────────────────
    struct LabelledKnob
    {
        juce::Slider knob;
        juce::Label  label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;

        LabelledKnob (const juce::String& labelText,
                      juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& paramId)
        {
            knob.setSliderStyle (juce::Slider::Rotary);
            knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
            label.setText (labelText, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            attachment = std::make_unique<juce::SliderParameterAttachment> (
                *apvts.getParameter (paramId), knob);
        }
    };

    // Row A
    std::unique_ptr<LabelledKnob> knobInput;
    std::unique_ptr<LabelledKnob> knobRepeat;
    std::unique_ptr<LabelledKnob> knobIntensity;
    std::unique_ptr<LabelledKnob> knobBass;
    std::unique_ptr<LabelledKnob> knobTreble;
    // Row B
    std::unique_ptr<LabelledKnob> knobWow;
    std::unique_ptr<LabelledKnob> knobSat;
    std::unique_ptr<LabelledKnob> knobEcho;
    std::unique_ptr<LabelledKnob> knobReverb;
    std::unique_ptr<LabelledKnob> knobNoise;
    std::unique_ptr<LabelledKnob> knobShimmer;
    // Sync division (left panel, below RATE)
    std::unique_ptr<LabelledKnob> knobSyncDiv;

    // ── Layout helpers ────────────────────────────────────────────────
    void addKnob (LabelledKnob& k);
    void layoutKnobRow (std::initializer_list<LabelledKnob*> knobs,
                        juce::Rectangle<int> rowBounds, int margin = 8);

    void styliseToggleButton (juce::TextButton& btn,
                              juce::Colour offBg, juce::Colour onBg,
                              juce::Colour offText, juce::Colour onText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceEchoAudioProcessorEditor)
};
