#pragma once
#include <JuceHeader.h>

/**
 *  IndustrialLookAndFeel  — Roland RE-201 faithful aesthetic.
 *
 *  Palette:
 *   • Near-black body + dark military-green panel
 *   • Brushed-aluminium knobs with white dot indicator
 *   • Amber VU needle, red power LED, white labels
 */
class IndustrialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // ── Colour constants ───────────────────────────────────────────────
    static constexpr uint32_t COL_BODY       = 0xFF111111;
    static constexpr uint32_t COL_PANEL_BG   = 0xFF1A1A1A;
    static constexpr uint32_t COL_GREEN_HI   = 0xFF264826;
    static constexpr uint32_t COL_GREEN_LO   = 0xFF1A3218;
    static constexpr uint32_t COL_CHROME_HI  = 0xFFA8A8A8;
    static constexpr uint32_t COL_CHROME_LO  = 0xFF484848;
    static constexpr uint32_t COL_WHITE      = 0xFFE8E8E8;
    static constexpr uint32_t COL_AMBER      = 0xFFF0A000;
    static constexpr uint32_t COL_GREEN      = 0xFF44CC44;
    static constexpr uint32_t COL_RED        = 0xFFCC3333;
    static constexpr uint32_t COL_ORANGE     = 0xFFE07020; // kept for compat
    static constexpr uint32_t COL_LABEL      = 0xFFDDDDDD;
    static constexpr uint32_t COL_LABEL_DIM  = 0xFF777777;

    IndustrialLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId,               juce::Colour (COL_WHITE));
        setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (COL_WHITE));
        setColour (juce::Slider::backgroundColourId,          juce::Colour (0xFF333333));
        setColour (juce::Label::textColourId,                 juce::Colour (COL_LABEL));
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (COL_BODY));
    }

    // ─────────────────────────────────────────────────────────────────
    //  Rotary knob — brushed aluminium, white dot indicator
    // ─────────────────────────────────────────────────────────────────
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int w, int h,
                           float sliderPos,
                           float startAngle, float endAngle,
                           juce::Slider& /*slider*/) override
    {
        const float cx = x + w * 0.5f;
        const float cy = y + h * 0.5f;
        const float r  = juce::jmin (w, h) * 0.44f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        // ── Drop shadow
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (cx - r - 2.f, cy - r + 2.f, (r + 2.f) * 2.f, (r + 2.f) * 2.f);

        // ── Outer ring (scale arc background)
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, r + 3.f, r + 3.f, 0.f, startAngle, endAngle, true);
            g.setColour (juce::Colour (0xFF2A2A2A));
            g.strokePath (arc, juce::PathStrokeType (4.f));
        }

        // ── Value arc (filled, dim white)
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, r + 3.f, r + 3.f, 0.f, startAngle, angle, true);
            g.setColour (juce::Colour (COL_WHITE).withAlpha (0.35f));
            g.strokePath (arc, juce::PathStrokeType (4.f));
        }

        // ── Tick marks around perimeter
        for (int i = 0; i <= 10; ++i)
        {
            const float t   = (float) i / 10.f;
            const float a   = startAngle + t * (endAngle - startAngle);
            const float r1  = r + 6.f;
            const float r2  = r + (i % 5 == 0 ? 12.f : 9.f);
            const float lw  = (i % 5 == 0) ? 1.8f : 1.f;
            g.setColour (juce::Colour (COL_LABEL_DIM));
            g.drawLine (cx + std::sin (a) * r1, cy - std::cos (a) * r1,
                        cx + std::sin (a) * r2, cy - std::cos (a) * r2, lw);
        }

        // ── Knob body — brushed aluminium radial gradient
        {
            juce::ColourGradient body (
                juce::Colour (0xFF888888), cx - r * 0.35f, cy - r * 0.35f,
                juce::Colour (0xFF282828), cx + r * 0.6f,  cy + r * 0.7f,
                false);
            body.addColour (0.4, juce::Colour (0xFF585858));
            g.setGradientFill (body);
            g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
        }

        // ── Knurling lines around edge
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        for (int i = 0; i < 48; ++i)
        {
            const float a = i * juce::MathConstants<float>::twoPi / 48.f;
            g.drawLine (cx + std::cos (a) * r * 0.78f, cy + std::sin (a) * r * 0.78f,
                        cx + std::cos (a) * r * 0.97f, cy + std::sin (a) * r * 0.97f, 1.f);
        }

        // ── Inner flat face
        {
            juce::ColourGradient face (
                juce::Colour (0xFF6A6A6A), cx - r * 0.2f, cy - r * 0.2f,
                juce::Colour (0xFF303030), cx + r * 0.4f,  cy + r * 0.5f,
                false);
            g.setGradientFill (face);
            g.fillEllipse (cx - r * 0.70f, cy - r * 0.70f, r * 1.40f, r * 1.40f);
        }

        // ── White indicator dot
        const float dotR = 3.5f;
        const float dotX = cx + std::sin (angle) * r * 0.52f;
        const float dotY = cy - std::cos (angle) * r * 0.52f;
        g.setColour (juce::Colours::white);
        g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.f, dotR * 2.f);

        // ── Top highlight
        {
            juce::ColourGradient hl (
                juce::Colours::white.withAlpha (0.20f), cx, cy - r,
                juce::Colours::transparentBlack,         cx, cy,
                false);
            g.setGradientFill (hl);
            g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
        }

        // ── Rim
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (cx - r, cy - r, r * 2.f, r * 2.f, 1.5f);
    }

    // ─────────────────────────────────────────────────────────────────
    //  Button drawing
    // ─────────────────────────────────────────────────────────────────
    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                               const juce::Colour& bgCol,
                               bool isOver, bool isDown) override
    {
        auto b = btn.getLocalBounds().toFloat().reduced (1.f);
        g.setColour (bgCol);
        g.fillRoundedRectangle (b, 4.f);
        if (isOver)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (b, 4.f);
        }
        if (isDown)
        {
            g.setColour (juce::Colours::black.withAlpha (0.15f));
            g.fillRoundedRectangle (b, 4.f);
        }
        g.setColour (juce::Colour (0xFF404040));
        g.drawRoundedRectangle (b, 4.f, 1.f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                         bool /*isOver*/, bool /*isDown*/) override
    {
        g.setColour (btn.findColour (btn.getToggleState()
            ? juce::TextButton::textColourOnId
            : juce::TextButton::textColourOffId));
        g.setFont (getIndustrialFont (10.f));
        g.drawFittedText (btn.getButtonText(), btn.getLocalBounds(),
                          juce::Justification::centred, 1);
    }

    void drawLabel (juce::Graphics& g, juce::Label& lbl) override
    {
        g.fillAll (lbl.findColour (juce::Label::backgroundColourId));
        if (! lbl.isBeingEdited())
        {
            g.setColour (lbl.findColour (juce::Label::textColourId));
            g.setFont   (lbl.getFont());
            g.drawFittedText (lbl.getText(),
                              lbl.getLocalBounds(),
                              lbl.getJustificationType(), 2);
        }
    }

    // ─────────────────────────────────────────────────────────────────
    //  Static helpers
    // ─────────────────────────────────────────────────────────────────
    static juce::Font getIndustrialFont (float size = 12.f)
    {
        return juce::Font (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold);
    }

    /** Dark military-green panel (RE-201 right section) — hammertone finish. */
    static void drawGreenPanel (juce::Graphics& g, juce::Rectangle<float> b)
    {
        // ── Base gradient
        juce::ColourGradient grad (
            juce::Colour (COL_GREEN_HI), b.getX(), b.getY(),
            juce::Colour (COL_GREEN_LO), b.getX(), b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (b);

        // ── Dense horizontal texture lines (hammertone / factory paint)
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        for (float yy = b.getY() + 1.f; yy < b.getBottom(); yy += 3.f)
            g.drawHorizontalLine ((int) yy, b.getX() + 1.f, b.getRight() - 1.f);

        // ── Secondary bright lines (micro-relief between dark lines)
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        for (float yy = b.getY() + 2.f; yy < b.getBottom(); yy += 3.f)
            g.drawHorizontalLine ((int) yy, b.getX() + 1.f, b.getRight() - 1.f);

        // ── Inner top shadow (panel appears recessed)
        {
            juce::ColourGradient innerShadow (
                juce::Colours::black.withAlpha (0.22f), b.getX(), b.getY(),
                juce::Colours::transparentBlack,        b.getX(), b.getY() + 18.f, false);
            g.setGradientFill (innerShadow);
            g.fillRect (b);
        }

        // ── Left-edge specular (light catching the bevel)
        {
            juce::ColourGradient leftSpec (
                juce::Colours::white.withAlpha (0.09f), b.getX(), b.getY(),
                juce::Colours::transparentBlack,         b.getX() + 7.f, b.getY(), false);
            g.setGradientFill (leftSpec);
            g.fillRect (b.getX(), b.getY(), 7.f, b.getHeight());
        }

        // ── Top edge highlight (machined edge)
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawLine (b.getX(), b.getY(), b.getRight(), b.getY(), 1.5f);

        // ── Outer border
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRect (b, 1.5f);
    }

    /** Dark aluminium panel (RE-201 left/center section) — brushed finish. */
    static void drawMetalPanel (juce::Graphics& g, juce::Rectangle<float> b,
                                juce::Colour base = juce::Colour (0xFF1E1E1E))
    {
        // ── Base gradient
        juce::ColourGradient grad (
            base.brighter (0.10f), b.getX(), b.getY(),
            base.darker   (0.15f), b.getX(), b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, 3.f);

        // ── Brushed aluminium horizontal lines
        g.setColour (juce::Colours::white.withAlpha (0.026f));
        for (float yy = b.getY() + 1.f; yy < b.getBottom(); yy += 2.f)
            g.drawHorizontalLine ((int) yy, b.getX() + 1.f, b.getRight() - 1.f);

        // ── Inner top shadow (panel depth / recessed feel)
        {
            juce::ColourGradient innerShadow (
                juce::Colours::black.withAlpha (0.20f), b.getX(), b.getY(),
                juce::Colours::transparentBlack,        b.getX(), b.getY() + 16.f, false);
            g.setGradientFill (innerShadow);
            g.fillRoundedRectangle (b, 3.f);
        }

        // ── Top-left bevel highlight (raised edge illusion)
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawLine (b.getX() + 3.f, b.getY() + 1.f, b.getRight() - 3.f, b.getY() + 1.f, 1.f);
        g.drawLine (b.getX() + 1.f, b.getY() + 3.f, b.getX() + 1.f, b.getBottom() - 3.f, 1.f);

        // ── Outer border
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (b, 3.f, 1.5f);
    }

    /** Classic indicator LED. */
    static void drawLED (juce::Graphics& g, float cx, float cy, float r,
                         juce::Colour colour, bool lit)
    {
        // Bezel
        g.setColour (juce::Colour (0xFF090909));
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);

        // Halo
        if (lit)
        {
            juce::ColourGradient halo (colour.withAlpha (0.45f), cx, cy,
                                       juce::Colours::transparentBlack, cx + r * 2.8f, cy, true);
            g.setGradientFill (halo);
            g.fillEllipse (cx - r * 2.8f, cy - r * 2.8f, r * 5.6f, r * 5.6f);
        }

        // Dome
        const float gr = r * 0.82f;
        juce::ColourGradient dome (
            lit ? colour.brighter (0.5f) : colour.withAlpha (0.2f), cx - gr * 0.3f, cy - gr * 0.5f,
            lit ? colour.darker   (0.2f) : colour.withAlpha (0.08f), cx + gr * 0.3f, cy + gr * 0.3f,
            false);
        g.setGradientFill (dome);
        g.fillEllipse (cx - gr, cy - gr, gr * 2.f, gr * 2.f);

        // Specular
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.5f : 0.12f));
        g.fillEllipse (cx - gr * 0.42f, cy - gr * 0.60f, gr * 0.48f, gr * 0.28f);
    }
};
