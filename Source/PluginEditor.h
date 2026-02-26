#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/IndustrialLookAndFeel.h"
#include "UI/VUMeter.h"
#include "UI/ModeSelector.h"

/**
 *  SpaceEchoAudioProcessorEditor
 *
 *  900 × 460 px industrial panel layout:
 *
 *  ┌─────────────────────────────────────────────────────────────────────────┐
 *  │  HEADER  – logo / power LED / brand                          (h = 60)  │
 *  ├───────────┬─────────────────────────────────────────────────────────────┤
 *  │  VU       │  Row A knobs: Input · RepeatRate · Intensity · Bass · Treble│
 *  │  meters   ├─────────────────────────────────────────────────────────────┤
 *  │  (w=90)   │  Row B knobs: WowFlt · Saturation · EchoLevel · ReverbLevel │
 *  ├───────────┴─────────────────────────────────────────────────────────────┤
 *  │  MODE SELECTOR  – 12 buttons                                 (h = 60)  │
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

    void timerCallback() override; // updates VU meters

private:
    SpaceEchoAudioProcessor& processor;
    IndustrialLookAndFeel     lnf;

    // ── VU meters ─────────────────────────────────────────────────────
    VUMeter vuIn  { VUMeter::Orientation::Vertical, "IN"  };
    VUMeter vuOut { VUMeter::Orientation::Vertical, "OUT" };

    // ── Test tone button ──────────────────────────────────────────────
    juce::TextButton testToneBtn { "TEST" };

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

    std::unique_ptr<LabelledKnob> knobInput;
    std::unique_ptr<LabelledKnob> knobRepeat;
    std::unique_ptr<LabelledKnob> knobIntensity;
    std::unique_ptr<LabelledKnob> knobBass;
    std::unique_ptr<LabelledKnob> knobTreble;
    std::unique_ptr<LabelledKnob> knobWow;
    std::unique_ptr<LabelledKnob> knobSat;
    std::unique_ptr<LabelledKnob> knobEcho;
    std::unique_ptr<LabelledKnob> knobReverb;

    // ── Layout helpers ────────────────────────────────────────────────
    void addKnob (LabelledKnob& k);
    void layoutKnobRow (std::initializer_list<LabelledKnob*> knobs,
                        juce::Rectangle<int> rowBounds, int margin = 8);

    static constexpr int W = 900;
    static constexpr int H = 460;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceEchoAudioProcessorEditor)
};
