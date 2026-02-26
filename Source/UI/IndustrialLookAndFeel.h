#pragma once
#include <JuceHeader.h>

/**
 *  IndustrialLookAndFeel
 *
 *  Vintage / industrial aesthetic inspired by 1970s rack hardware:
 *   - Charcoal panel background
 *   - Brushed-copper knobs with bright orange indicator
 *   - Warm amber VU segments
 *   - Stencil-style label font
 */
class IndustrialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // ── Brand colours ──────────────────────────────────────────────────
    static constexpr uint32_t COL_PANEL_BG   = 0xFF1A1A1A; // near-black
    static constexpr uint32_t COL_PANEL_MID  = 0xFF242424;
    static constexpr uint32_t COL_PANEL_EDGE = 0xFF0D0D0D;
    static constexpr uint32_t COL_COPPER     = 0xFF8B4513; // knob body
    static constexpr uint32_t COL_STEEL      = 0xFF3C3C3C;
    static constexpr uint32_t COL_ORANGE     = 0xFFE07020; // indicator / accent
    static constexpr uint32_t COL_AMBER      = 0xFFFFAA00; // VU hot
    static constexpr uint32_t COL_GREEN      = 0xFF44AA44; // VU safe
    static constexpr uint32_t COL_RED        = 0xFFDD2222; // VU clip
    static constexpr uint32_t COL_LABEL      = 0xFFCCCCCC;
    static constexpr uint32_t COL_LABEL_DIM  = 0xFF666666;

    IndustrialLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId,            juce::Colour (COL_ORANGE));
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (COL_ORANGE));
        setColour (juce::Slider::backgroundColourId,       juce::Colour (COL_STEEL));
        setColour (juce::Label::textColourId,              juce::Colour (COL_LABEL));
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (COL_PANEL_BG));
    }

    // ── Rotary knob ────────────────────────────────────────────────────
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int w, int h,
                           float sliderPos,
                           float startAngle, float endAngle,
                           juce::Slider& /*slider*/) override
    {
        const float cx = x + w * 0.5f;
        const float cy = y + h * 0.5f;
        const float r  = juce::jmin (w, h) * 0.5f - 4.0f;

        // ── Outer shadow ring
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (cx - r - 3.f, cy - r - 3.f, (r + 3.f) * 2.f, (r + 3.f) * 2.f);

        // ── Knob track arc
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, r + 1.f, r + 1.f, 0.f,
                               startAngle, endAngle, true);
            g.setColour (juce::Colour (COL_STEEL));
            g.strokePath (arc, juce::PathStrokeType (3.f));
        }

        // ── Value arc (filled portion)
        {
            const float angle = startAngle + sliderPos * (endAngle - startAngle);
            juce::Path arc;
            arc.addCentredArc (cx, cy, r + 1.f, r + 1.f, 0.f,
                               startAngle, angle, true);
            g.setColour (juce::Colour (COL_ORANGE).withAlpha (0.8f));
            g.strokePath (arc, juce::PathStrokeType (3.f));
        }

        // ── Knob body — radial gradient (copper / metal)
        {
            const auto bodyr = r - 2.f;
            juce::ColourGradient grad (
                juce::Colour (0xFF6E3010),  cx - bodyr * 0.4f, cy - bodyr * 0.4f,
                juce::Colour (0xFF1A0A00),  cx + bodyr * 0.5f, cy + bodyr * 0.6f,
                true);
            grad.addColour (0.3, juce::Colour (0xFF8B4513));
            grad.addColour (0.6, juce::Colour (0xFF3A1A06));
            g.setGradientFill (grad);
            g.fillEllipse (cx - bodyr, cy - bodyr, bodyr * 2.f, bodyr * 2.f);
        }

        // ── Highlight rim
        {
            const auto bodyr = r - 2.f;
            juce::ColourGradient rimGrad (
                juce::Colours::white.withAlpha (0.15f), cx, cy - bodyr,
                juce::Colours::transparentBlack,        cx, cy + bodyr,
                false);
            g.setGradientFill (rimGrad);
            g.fillEllipse (cx - bodyr, cy - bodyr, bodyr * 2.f, bodyr * 2.f);

            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawEllipse (cx - bodyr, cy - bodyr, bodyr * 2.f, bodyr * 2.f, 1.f);
        }

        // ── Indicator line (bright orange dot + line)
        {
            const float angle = startAngle + sliderPos * (endAngle - startAngle);
            const float ix = cx + (r - 8.f) * std::sin (angle);
            const float iy = cy - (r - 8.f) * std::cos (angle);
            const float ox = cx + (r - 2.f) * std::sin (angle);
            const float oy = cy - (r - 2.f) * std::cos (angle);

            // line
            g.setColour (juce::Colour (COL_ORANGE));
            g.drawLine (ix, iy, ox, oy, 2.5f);

            // bright dot at tip
            g.setColour (juce::Colours::white);
            g.fillEllipse (ox - 2.f, oy - 2.f, 4.f, 4.f);

            // glow
            g.setColour (juce::Colour (COL_ORANGE).withAlpha (0.4f));
            g.fillEllipse (ox - 4.f, oy - 4.f, 8.f, 8.f);
        }
    }

    // ── Label font ─────────────────────────────────────────────────────
    juce::Font getLabelFont (juce::Label& label) override
    {
        return getIndustrialFont (label.getFont().getHeight());
    }

    static juce::Font getIndustrialFont (float height = 12.f)
    {
        // Use built-in monospaced font (looks industrial)
        return juce::Font (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::bold);
    }

    // ── Utility: draw a brushed-metal panel rect ───────────────────────
    static void drawMetalPanel (juce::Graphics& g, juce::Rectangle<float> b,
                                juce::Colour base = juce::Colour (0xFF222222))
    {
        // Background
        juce::ColourGradient grad (base.brighter (0.1f), b.getX(), b.getY(),
                                   base.darker  (0.15f), b.getX(), b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, 4.f);

        // Subtle horizontal brush lines
        g.setColour (juce::Colours::white.withAlpha (0.015f));
        for (float yy = b.getY() + 2.f; yy < b.getBottom(); yy += 3.f)
            g.drawHorizontalLine (static_cast<int> (yy),
                                  b.getX() + 1.f, b.getRight() - 1.f);

        // Bevel edge
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine (b.getX(), b.getY(), b.getRight(), b.getY(), 1.f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 1.f);

        // Rounded outer stroke
        g.setColour (juce::Colour (0xFF0D0D0D));
        g.drawRoundedRectangle (b, 4.f, 1.5f);
    }

    // ── Utility: draw LED ──────────────────────────────────────────────
    static void drawLED (juce::Graphics& g, float cx, float cy, float r,
                         juce::Colour colour, bool lit)
    {
        // Bezel
        g.setColour (juce::Colour (0xFF111111));
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);

        // Glass dome
        const float gr = r * 0.85f;
        if (lit)
        {
            // Glow halo
            juce::ColourGradient halo (colour.withAlpha (0.5f), cx, cy,
                                       juce::Colours::transparentBlack, cx + r * 2.5f, cy, true);
            g.setGradientFill (halo);
            g.fillEllipse (cx - r * 2.5f, cy - r * 2.5f, r * 5.f, r * 5.f);

            juce::ColourGradient dome (colour.brighter (0.6f), cx - gr * 0.3f, cy - gr * 0.5f,
                                       colour.darker   (0.3f), cx + gr * 0.3f, cy + gr * 0.3f,
                                       false);
            g.setGradientFill (dome);
        }
        else
        {
            juce::ColourGradient dome (colour.withAlpha (0.25f).brighter (0.2f), cx - gr * 0.3f, cy - gr * 0.5f,
                                       colour.withAlpha (0.1f),                  cx + gr * 0.3f, cy + gr * 0.3f,
                                       false);
            g.setGradientFill (dome);
        }
        g.fillEllipse (cx - gr, cy - gr, gr * 2.f, gr * 2.f);

        // Specular highlight
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.55f : 0.15f));
        g.fillEllipse (cx - gr * 0.4f, cy - gr * 0.6f, gr * 0.5f, gr * 0.3f);
    }
};
