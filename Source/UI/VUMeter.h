#pragma once
#include <JuceHeader.h>
#include "IndustrialLookAndFeel.h"
#include <atomic>
#include <cmath>

/**
 *  VUMeter — Analog needle VU meter, RE-201 faithful style.
 *
 *  Semi-circular arc display with:
 *   • Scale: -20 .. 0 .. +3 VU
 *   • Orange/amber needle with VU ballistics (~300 ms)
 *   • Peak hold (3 s)
 *   • Black matte face, aluminium bezel
 *   • "VU" label and channel label
 */
class VUMeter : public juce::Component, public juce::Timer
{
public:
    // Keep Orientation for API compatibility (editor uses this)
    enum class Orientation { Vertical, Horizontal };

    VUMeter (Orientation = Orientation::Vertical, const juce::String& lbl = {})
        : channelLabel (lbl)
    {
        startTimerHz (30);
    }

    ~VUMeter() override { stopTimer(); }

    /** Called from editor timer at ~30 Hz with linear RMS level. */
    void setLevel (float linearRMS) noexcept
    {
        const float db = juce::Decibels::gainToDecibels (std::abs (linearRMS), -60.f);
        // Map -20..+3 dB to 0..1
        const float norm = juce::jmap (db, -20.f, 3.f, 0.f, 1.f);
        targetLevel.store (juce::jlimit (0.f, 1.f, norm));
    }

    void paint (juce::Graphics& g) override
    {
        const float W  = (float) getWidth();
        const float H  = (float) getHeight();

        // ── Aluminium bezel ───────────────────────────────────────────
        {
            juce::ColourGradient bezel (
                juce::Colour (0xFF888888), 0.f, 0.f,
                juce::Colour (0xFF383838), W,   H, false);
            g.setGradientFill (bezel);
            g.fillRoundedRectangle (0.f, 0.f, W, H, 5.f);
        }

        // ── Black face ────────────────────────────────────────────────
        const juce::Rectangle<float> face (4.f, 4.f, W - 8.f, H - 8.f);
        g.setColour (juce::Colour (0xFF0C0C0C));
        g.fillRoundedRectangle (face, 3.f);

        // Arc parameters (semi-circle opening downward → needles sweep up)
        const float cx    = W * 0.5f;
        const float arcY  = H * 0.88f;          // pivot below centre
        const float arcR  = W * 0.48f;
        const float angStart = juce::MathConstants<float>::pi * 1.20f; // ~216°
        const float angEnd   = juce::MathConstants<float>::pi * 1.80f; // ~324° (= -36°)
        const float angRange = angEnd - angStart;

        // ── Scale arc (background) ────────────────────────────────────
        {
            juce::Path arc;
            arc.addCentredArc (cx, arcY, arcR, arcR, 0.f, angStart, angEnd, true);
            g.setColour (juce::Colour (0xFF222222));
            g.strokePath (arc, juce::PathStrokeType (2.f));
        }

        // ── Scale markings ────────────────────────────────────────────
        // VU scale: -20, -10, -7, -5, -3, -1, 0, +1, +2, +3
        const float scaleDb[]  = { -20.f, -10.f, -7.f, -5.f, -3.f, -1.f, 0.f, 1.f, 2.f, 3.f };
        const char* scaleLabel[]= { "-20", "-10",  "-7", "-5",  "-3",  "",   "0",  "",  "",  "+3" };
        const int   nScale     = 10;

        for (int i = 0; i < nScale; ++i)
        {
            const float norm = juce::jmap (scaleDb[i], -20.f, 3.f, 0.f, 1.f);
            const float ang  = angStart + norm * angRange;
            const float sinA = std::sin (ang), cosA = std::cos (ang);

            // Tick
            const bool  bigTick = (scaleDb[i] <= -10.f || scaleDb[i] >= 0.f);
            const float r1 = arcR - (bigTick ? 10.f : 7.f);
            const float r2 = arcR + 2.f;
            g.setColour (scaleDb[i] >= 0.f ? juce::Colour (0xFFDD3333)
                                           : juce::Colour (0xFF888888));
            g.drawLine (cx + sinA * r1, arcY + cosA * r1,
                        cx + sinA * r2, arcY + cosA * r2,
                        bigTick ? 2.f : 1.2f);

            // Label
            if (scaleLabel[i][0] != '\0')
            {
                const float lx = cx + sinA * (r1 - 9.f);
                const float ly = arcY + cosA * (r1 - 9.f);
                g.setFont (juce::Font (juce::FontOptions ("Arial", 6.5f, juce::Font::plain)));
                g.setColour (scaleDb[i] >= 0.f ? juce::Colour (0xFFEE4444)
                                               : juce::Colour (0xFF999999));
                g.drawText (scaleLabel[i],
                            (int) (lx - 12.f), (int) (ly - 6.f), 24, 12,
                            juce::Justification::centred);
            }
        }

        // ── Coloured zones (red zone 0..+3) ──────────────────────────
        {
            const float ang0  = angStart + juce::jmap (0.f,  -20.f, 3.f, 0.f, 1.f) * angRange;
            juce::Path redArc;
            redArc.addCentredArc (cx, arcY, arcR - 3.f, arcR - 3.f, 0.f, ang0, angEnd, true);
            g.setColour (juce::Colour (0x55DD2222));
            g.strokePath (redArc, juce::PathStrokeType (5.f));
        }

        // ── Needle ────────────────────────────────────────────────────
        {
            const float ang  = angStart + needlePos * angRange;
            const float len  = arcR - 6.f;
            const float sinA = std::sin (ang), cosA = std::cos (ang);

            // Shadow
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawLine (cx + 1.f, arcY + 1.f,
                        cx + sinA * (len - 1.f) + 1.f, arcY + cosA * (len - 1.f) + 1.f, 1.5f);

            // Needle body
            g.setColour (juce::Colour (IndustrialLookAndFeel::COL_AMBER));
            g.drawLine (cx, arcY, cx + sinA * len, arcY + cosA * len, 1.8f);

            // Pivot dot
            g.setColour (juce::Colour (0xFF555555));
            g.fillEllipse (cx - 5.f, arcY - 5.f, 10.f, 10.f);
            g.setColour (juce::Colour (0xFF888888));
            g.fillEllipse (cx - 3.f, arcY - 3.f, 6.f, 6.f);
        }

        // ── Peak hold tick ────────────────────────────────────────────
        if (peakHold > 0.02f)
        {
            const float ang  = angStart + peakHold * angRange;
            const float sinA = std::sin (ang), cosA = std::cos (ang);
            g.setColour (juce::Colour (IndustrialLookAndFeel::COL_AMBER).withAlpha (0.7f));
            g.drawLine (cx + sinA * (arcR - 13.f), arcY + cosA * (arcR - 13.f),
                        cx + sinA * (arcR - 3.f),  arcY + cosA * (arcR - 3.f), 2.f);
        }

        // ── "VU" label ────────────────────────────────────────────────
        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.f, juce::Font::bold)));
        g.setColour (juce::Colour (0xFF666666));
        g.drawText ("VU", (int) (cx - 12.f), (int) (arcY - arcR * 0.35f), 24, 12,
                    juce::Justification::centred);

        // ── Channel label ─────────────────────────────────────────────
        if (channelLabel.isNotEmpty())
        {
            g.setFont (IndustrialLookAndFeel::getIndustrialFont (8.f));
            g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM));
            g.drawText (channelLabel, 0, (int) (H - 14.f), (int) W, 12,
                        juce::Justification::centred);
        }
    }

    void resized() override {}

    void timerCallback() override
    {
        const float target = targetLevel.load();

        // VU ballistics: ~300 ms attack & release at 30 Hz → coeff ≈ 0.10
        needlePos += (target - needlePos) * 0.10f;

        // Peak hold
        if (target >= peakHold)
        {
            peakHold    = target;
            peakHoldAge = 0;
        }
        else if (++peakHoldAge > 90) // ~3 s
            peakHold = std::max (0.f, peakHold - 0.004f);

        repaint();
    }

private:
    juce::String channelLabel;

    std::atomic<float> targetLevel { 0.f };
    float needlePos  = 0.f;
    float peakHold   = 0.f;
    int   peakHoldAge= 0;
};
