#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 *  TapeReelComponent — Animated tape reels displayed on the panel.
 *
 *  Shows two reels (supply + takeup) with copper flanges, hub, spokes and
 *  a tape strip between them.  Rotation is driven by the host via advance().
 */
class TapeReelComponent : public juce::Component
{
public:
    TapeReelComponent() = default;

    /** Call from a timer (e.g. 30 Hz) with the angular delta per frame. */
    void advance (float deltaAngle)
    {
        if (! frozen)
        {
            angle1 += deltaAngle;
            angle2 -= deltaAngle * 0.73f; // supply reel rotates slightly slower / opposite
        }
        repaint();
    }

    void setFrozen (bool f) noexcept { frozen = f; }

    // ─────────────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        const float w  = static_cast<float> (getWidth());
        const float h  = static_cast<float> (getHeight());
        const float cy = h * 0.54f;
        const float r  = std::min (w * 0.23f, h * 0.44f);

        // Background
        g.setColour (juce::Colour (0xFF141414));
        g.fillRoundedRectangle (0.f, 0.f, w, h, 4.f);

        const float cx1 = w * 0.28f;
        const float cx2 = w * 0.72f;

        drawTape (g, cx1, cx2, cy, r);
        drawReel (g, cx1, cy, r, angle1);
        drawReel (g, cx2, cy, r, angle2);

        // FREEZE indicator
        if (frozen)
        {
            g.setColour (juce::Colour (0x99FF4400));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 8.f, juce::Font::bold)));
            g.drawText ("FREEZE", juce::Rectangle<float> (0.f, 0.f, w, 12.f),
                        juce::Justification::centred);
        }
    }

private:
    float angle1 = 0.f;
    float angle2 = 0.f;
    bool  frozen = false;

    // ── Draw a single reel at (cx, cy) with radius r and rotation angle ──
    void drawReel (juce::Graphics& g, float cx, float cy, float r, float angle)
    {
        // Outer flange (copper gradient)
        {
            juce::ColourGradient grad (
                juce::Colour (0xFF9A6820), cx - r, cy - r,
                juce::Colour (0xFF5A3A00), cx + r, cy + r,
                false);
            g.setGradientFill (grad);
            g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
        }

        // Rim shadow
        g.setColour (juce::Colour (0xFF2A1800));
        g.drawEllipse (cx - r, cy - r, r * 2.f, r * 2.f, 2.5f);

        // Inner dark area (between flanges)
        const float innerR = r * 0.82f;
        g.setColour (juce::Colour (0xFF0E0E0E));
        g.fillEllipse (cx - innerR, cy - innerR, innerR * 2.f, innerR * 2.f);

        // Tape wound on reel (charcoal circle)
        const float tapeR = r * 0.52f;
        g.setColour (juce::Colour (0xFF1E1008));
        g.fillEllipse (cx - tapeR, cy - tapeR, tapeR * 2.f, tapeR * 2.f);

        // Spokes (4 spokes)
        const float hubR  = r * 0.18f;
        const float spokeOuter = r * 0.75f;
        for (int i = 0; i < 4; ++i)
        {
            const float a = angle + static_cast<float> (i) * juce::MathConstants<float>::halfPi;
            const float cosA = std::cos (a);
            const float sinA = std::sin (a);
            g.setColour (juce::Colour (0xFF7A5210));
            g.drawLine (cx + cosA * hubR, cy + sinA * hubR,
                        cx + cosA * spokeOuter, cy + sinA * spokeOuter, 3.5f);
        }

        // Hub
        g.setColour (juce::Colour (0xFF2A2A2A));
        g.fillEllipse (cx - hubR, cy - hubR, hubR * 2.f, hubR * 2.f);
        g.setColour (juce::Colour (0xFF4A4A4A));
        g.drawEllipse (cx - hubR, cy - hubR, hubR * 2.f, hubR * 2.f, 1.5f);

        // Centre hole
        const float holeR = r * 0.08f;
        g.setColour (juce::Colour (0xFF080808));
        g.fillEllipse (cx - holeR, cy - holeR, holeR * 2.f, holeR * 2.f);

        // Highlight arc on flange
        {
            juce::Path arc;
            arc.addArc (cx - r * 0.88f, cy - r * 0.88f,
                        r * 1.76f, r * 1.76f,
                        angle - 0.6f, angle + 0.4f, true);
            g.setColour (juce::Colour (0x30FFDD99));
            g.strokePath (arc, juce::PathStrokeType (2.f));
        }
    }

    // ── Draw tape strip between the two reels ────────────────────────
    void drawTape (juce::Graphics& g, float cx1, float cx2, float cy, float r)
    {
        const float tapeH  = 8.f;
        const float margin = r * 0.82f; // touch inner flange
        const float x1 = cx1 + margin;
        const float x2 = cx2 - margin;

        if (x2 <= x1) return;

        // Tape body (dark brown)
        g.setColour (juce::Colour (0xFF1A0A00));
        g.fillRect (x1, cy - tapeH * 0.5f, x2 - x1, tapeH);

        // Tape edge highlights
        g.setColour (juce::Colour (0xFF3A2000));
        g.fillRect (x1, cy - tapeH * 0.5f, x2 - x1, 1.5f);
        g.fillRect (x1, cy + tapeH * 0.5f - 1.5f, x2 - x1, 1.5f);
    }
};
