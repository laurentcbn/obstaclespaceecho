#pragma once
#include <JuceHeader.h>
#include "IndustrialLookAndFeel.h"

/**
 *  VUMeter
 *
 *  Segment-bar VU meter with:
 *   • Green segments (safe zone)
 *   • Amber segments (hot zone)
 *   • Red peak segment (clip)
 *   • Peak hold dot
 *   • Smooth RMS decay (adjustable time constant)
 */
class VUMeter : public juce::Component, public juce::Timer
{
public:
    enum class Orientation { Vertical, Horizontal };

    VUMeter (Orientation ori = Orientation::Vertical, const juce::String& lbl = {})
        : orientation (ori), label (lbl)
    {
        startTimerHz (30);
    }

    ~VUMeter() override { stopTimer(); }

    /** Thread-safe: called from audio thread */
    void setLevel (float linearRMS) noexcept
    {
        // Convert to dB range  (-60..+6)
        const float db = juce::Decibels::gainToDecibels (std::abs (linearRMS), -60.0f);
        const float norm = juce::jmap (db, -60.0f, 6.0f, 0.0f, 1.0f);
        targetLevel.store (juce::jlimit (0.0f, 1.0f, norm));
    }

    void paint (juce::Graphics& g) override
    {
        const auto b  = getLocalBounds().toFloat().reduced (2.f);
        const int  ns = numSegments;

        const bool vert = (orientation == Orientation::Vertical);
        const float segW = vert ? b.getWidth()
                                : b.getWidth()  / ns;
        const float segH = vert ? b.getHeight() / ns
                                : b.getHeight();
        const float gap  = 2.f;

        const float lvl  = displayLevel;
        const int   litN = static_cast<int> (lvl * ns);

        // ── Segments ──────────────────────────────────────────────────
        for (int i = 0; i < ns; ++i)
        {
            juce::Rectangle<float> seg;
            if (vert)
            {
                // Bottom = segment 0
                float segTop = b.getBottom() - (i + 1) * segH + gap * 0.5f;
                seg = { b.getX(), segTop, segW, segH - gap };
            }
            else
            {
                seg = { b.getX() + i * segW + gap * 0.5f, b.getY(), segW - gap, segH };
            }

            const bool lit = (i < litN);
            juce::Colour segCol;

            if (i >= ns - 1)          segCol = juce::Colour (IndustrialLookAndFeel::COL_RED);
            else if (i >= ns - 4)     segCol = juce::Colour (IndustrialLookAndFeel::COL_AMBER);
            else                      segCol = juce::Colour (IndustrialLookAndFeel::COL_GREEN);

            if (!lit) segCol = segCol.withAlpha (0.12f);

            g.setColour (segCol);
            g.fillRoundedRectangle (seg, 1.5f);

            if (lit)
            {
                // Inner highlight
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (seg.withHeight (seg.getHeight() * 0.35f), 1.5f);
            }
        }

        // ── Peak hold ─────────────────────────────────────────────────
        if (peakHold > 0.01f)
        {
            const int peakSeg = static_cast<int> (peakHold * ns);
            const int clamped = juce::jlimit (0, ns - 1, peakSeg);

            juce::Colour pkCol = (clamped >= ns - 1)
                ? juce::Colour (IndustrialLookAndFeel::COL_RED)
                : juce::Colour (IndustrialLookAndFeel::COL_AMBER);

            juce::Rectangle<float> seg;
            if (vert)
            {
                float segTop = b.getBottom() - (clamped + 1) * segH + gap * 0.5f;
                seg = { b.getX() + segW * 0.2f, segTop + segH * 0.35f,
                        segW * 0.6f, segH * 0.3f };
            }
            else
            {
                seg = { b.getX() + clamped * segW + gap * 0.5f + segW * 0.3f,
                        b.getY() + segH * 0.2f,
                        segW * 0.4f, segH * 0.6f };
            }
            g.setColour (pkCol.withAlpha (0.9f));
            g.fillRoundedRectangle (seg, 1.f);
        }

        // ── Label ─────────────────────────────────────────────────────
        if (label.isNotEmpty())
        {
            g.setFont (IndustrialLookAndFeel::getIndustrialFont (9.f));
            g.setColour (juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM));
            g.drawText (label, getLocalBounds(), juce::Justification::centredTop);
        }
    }

    void resized() override {}

    void timerCallback() override
    {
        const float target = targetLevel.load();
        const float decay  = 0.92f; // ~30 ms decay at 30 Hz

        displayLevel = std::max (displayLevel * decay, target);

        // Peak hold
        if (target >= peakHold)
        {
            peakHold    = target;
            peakHoldAge = 0;
        }
        else if (++peakHoldAge > 90) // ~3s hold
        {
            peakHold = std::max (0.0f, peakHold - 0.005f);
        }

        repaint();
    }

private:
    Orientation   orientation;
    juce::String  label;
    int           numSegments = 24;

    std::atomic<float> targetLevel { 0.0f };
    float              displayLevel = 0.0f;
    float              peakHold     = 0.0f;
    int                peakHoldAge  = 0;
};
