#pragma once
#include <JuceHeader.h>
#include "IndustrialLookAndFeel.h"

/**
 *  ModeSelector — RE-201 style large rotary selector dial.
 *
 *  12 positions arranged on a 300° arc (gap at the bottom).
 *  Click or drag anywhere on/around the dial to select a position.
 *
 *  Positions 1-7  → echo only  (white/amber ticks)
 *  Positions 8-12 → reverb     (red ticks)
 */
class ModeSelector : public juce::Component
{
public:
    static constexpr int NUM_MODES = 12;

    std::function<void (int)> onModeChanged;

    ModeSelector() = default;

    void setMode (int mode, bool sendCallback = false)
    {
        mode = juce::jlimit (0, NUM_MODES - 1, mode);
        currentMode = mode;
        repaint();
        if (sendCallback && onModeChanged) onModeChanged (mode);
    }

    int getMode() const noexcept { return currentMode; }

    void paint (juce::Graphics& g) override
    {
        const float W  = (float) getWidth();
        const float H  = (float) getHeight();
        const float cx = W * 0.5f;
        const float cy = H * 0.48f;
        const float dialR = juce::jmin (W, H) * 0.38f;

        // ── Arc parameters: 300° sweep, gap at the bottom
        // arcStart = 120° (lower-left), arcEnd = 420° (= 60°, lower-right)
        // going through 270° (top) as the mid-point (mode 6-7)
        const float arcStart = juce::MathConstants<float>::pi * (2.f / 3.f);   // 120°
        const float arcEnd   = juce::MathConstants<float>::pi * (7.f / 3.f);   // 420° = 60°
        const float arcRange = arcEnd - arcStart;                               // 300°

        // ── Drop shadow
        g.setColour (juce::Colours::black.withAlpha (0.70f));
        g.fillEllipse (cx - dialR - 1.f, cy - dialR + 5.f,
                       (dialR + 1.f) * 2.f, (dialR + 1.f) * 2.f);

        // ── Outer chrome bezel ring
        {
            juce::ColourGradient bezel (
                juce::Colour (0xFF686868), cx - dialR, cy - dialR,
                juce::Colour (0xFF181818), cx + dialR, cy + dialR, false);
            g.setGradientFill (bezel);
            g.fillEllipse (cx - dialR - 5.f, cy - dialR - 5.f,
                           (dialR + 5.f) * 2.f, (dialR + 5.f) * 2.f);
        }

        // ── Knob body — brushed aluminium radial gradient
        {
            juce::ColourGradient body (
                juce::Colour (0xFF888888), cx - dialR * 0.35f, cy - dialR * 0.35f,
                juce::Colour (0xFF282828), cx + dialR * 0.6f,  cy + dialR * 0.7f,
                false);
            body.addColour (0.4, juce::Colour (0xFF585858));
            g.setGradientFill (body);
            g.fillEllipse (cx - dialR, cy - dialR, dialR * 2.f, dialR * 2.f);
        }

        // ── Knurling lines around edge
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        for (int i = 0; i < 64; ++i)
        {
            const float a = i * juce::MathConstants<float>::twoPi / 64.f;
            g.drawLine (cx + std::cos (a) * dialR * 0.80f, cy + std::sin (a) * dialR * 0.80f,
                        cx + std::cos (a) * dialR * 0.97f, cy + std::sin (a) * dialR * 0.97f, 1.f);
        }

        // ── Inner flat face (smaller inset disc)
        {
            juce::ColourGradient face (
                juce::Colour (0xFF6A6A6A), cx - dialR * 0.2f, cy - dialR * 0.2f,
                juce::Colour (0xFF303030), cx + dialR * 0.4f,  cy + dialR * 0.5f,
                false);
            g.setGradientFill (face);
            g.fillEllipse (cx - dialR * 0.65f, cy - dialR * 0.65f,
                           dialR * 1.30f, dialR * 1.30f);
        }

        // ── Top highlight
        {
            juce::ColourGradient hl (
                juce::Colours::white.withAlpha (0.18f), cx, cy - dialR,
                juce::Colours::transparentBlack,         cx, cy, false);
            g.setGradientFill (hl);
            g.fillEllipse (cx - dialR, cy - dialR, dialR * 2.f, dialR * 2.f);
        }

        // ── Position notches + mode number labels around the bezel
        const float notchInner = dialR + 4.f;
        const float notchOuter = dialR + 14.f;
        const float labelR     = dialR + 26.f;
        const float fontSize   = juce::jmax (8.f, dialR * 0.14f);

        for (int i = 0; i < NUM_MODES; ++i)
        {
            const float t    = (float) i / (float) (NUM_MODES - 1);
            const float ang  = arcStart + t * arcRange;
            const float sinA = std::sin (ang), cosA = std::cos (ang);

            const bool isReverb   = (i >= 7);
            const bool isSelected = (i == currentMode);

            // Tick mark
            const float r1 = notchInner;
            const float r2 = notchOuter + (isSelected ? 4.f : 0.f);
            juce::Colour tickCol = isReverb
                ? (isSelected ? juce::Colour (0xFFFF5533) : juce::Colour (0xFF773322))
                : (isSelected ? juce::Colour (0xFFFFCC00) : juce::Colour (0xFF777777));
            g.setColour (tickCol);
            g.drawLine (cx + cosA * r1, cy + sinA * r1,
                        cx + cosA * r2, cy + sinA * r2,
                        isSelected ? 3.f : 1.5f);

            // Mode number
            g.setFont (juce::Font (juce::FontOptions ("Arial", fontSize,
                                   isSelected ? juce::Font::bold : juce::Font::plain)));
            g.setColour (isReverb
                ? (isSelected ? juce::Colour (0xFFFF6644) : juce::Colour (0xFF663322))
                : (isSelected ? juce::Colour (0xFFFFCC00) : juce::Colour (0xFF666666)));
            const float lx = cx + cosA * labelR;
            const float ly = cy + sinA * labelR;
            g.drawText (juce::String (i + 1),
                        (int) (lx - 13.f), (int) (ly - 8.f), 26, 16,
                        juce::Justification::centred);
        }

        // ── Pointer / indicator from center outward
        {
            const float t    = (float) currentMode / (float) (NUM_MODES - 1);
            const float ang  = arcStart + t * arcRange;
            const float sinA = std::sin (ang), cosA = std::cos (ang);

            // Amber line
            g.setColour (juce::Colour (IndustrialLookAndFeel::COL_AMBER));
            g.drawLine (cx + cosA * dialR * 0.06f, cy + sinA * dialR * 0.06f,
                        cx + cosA * dialR * 0.62f, cy + sinA * dialR * 0.62f, 3.5f);

            // White indicator dot near knob rim
            const float dotX = cx + cosA * dialR * 0.76f;
            const float dotY = cy + sinA * dialR * 0.76f;
            g.setColour (juce::Colours::white);
            g.fillEllipse (dotX - 4.5f, dotY - 4.5f, 9.f, 9.f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawEllipse (dotX - 4.5f, dotY - 4.5f, 9.f, 9.f, 1.f);
        }

        // ── "MODE" label in the lower-center of the face
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (fontSize * 0.85f));
        g.setColour (juce::Colour (0xFF888888));
        g.drawText ("MODE",
                    (int) (cx - 22.f), (int) (cy + dialR * 0.15f), 44, 14,
                    juce::Justification::centred);

        // ── Center pivot cap
        g.setColour (juce::Colour (0xFF282828));
        g.fillEllipse (cx - 9.f, cy - 9.f, 18.f, 18.f);
        g.setColour (juce::Colour (0xFF606060));
        g.fillEllipse (cx - 6.f, cy - 6.f, 12.f, 12.f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (cx - 6.f, cy - 6.f, 12.f, 12.f, 1.f);

        // ── Outer rim
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (cx - dialR, cy - dialR, dialR * 2.f, dialR * 2.f, 2.f);

        // ── Reverb / Echo zone labels around the gap
        g.setFont (IndustrialLookAndFeel::getIndustrialFont (6.5f));
        {
            // "ECHO" label at lower-left of arc
            const float angL = arcStart - 0.08f;
            g.setColour (juce::Colour (0xFF557799));
            g.drawText ("ECHO",
                        (int) (cx + std::cos (angL) * (dialR + 36.f) - 18.f),
                        (int) (cy + std::sin (angL) * (dialR + 36.f) - 6.f),
                        36, 12, juce::Justification::centred);

            // "REVERB" label at lower-right of arc
            const float angR = arcEnd + 0.08f;
            g.setColour (juce::Colour (0xFF995544));
            g.drawText ("REV",
                        (int) (cx + std::cos (angR) * (dialR + 36.f) - 18.f),
                        (int) (cy + std::sin (angR) * (dialR + 36.f) - 6.f),
                        36, 12, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { selectFromPoint (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { selectFromPoint (e.position); }
    void resized()  override {}

private:
    int currentMode = 0;

    void selectFromPoint (juce::Point<float> pt)
    {
        const float cx = getWidth()  * 0.5f;
        const float cy = getHeight() * 0.48f;

        float ang = std::atan2 (pt.y - cy, pt.x - cx);

        const float arcStart = juce::MathConstants<float>::pi * (2.f / 3.f);
        const float arcEnd   = juce::MathConstants<float>::pi * (7.f / 3.f);
        const float arcRange = arcEnd - arcStart;
        const float twoPi    = juce::MathConstants<float>::twoPi;

        // Normalise into [arcStart, arcEnd]
        while (ang < arcStart) ang += twoPi;
        if (ang > arcEnd) return;  // click in the forbidden bottom zone

        const float t      = (ang - arcStart) / arcRange;
        const int   mode   = juce::roundToInt (t * (float) (NUM_MODES - 1));
        const int   clamped = juce::jlimit (0, NUM_MODES - 1, mode);

        if (clamped != currentMode)
        {
            currentMode = clamped;
            repaint();
            if (onModeChanged) onModeChanged (currentMode);
        }
    }
};
