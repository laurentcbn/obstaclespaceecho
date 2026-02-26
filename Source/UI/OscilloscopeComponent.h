#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <algorithm>

/**
 *  OscilloscopeComponent — CRT-style phosphor green waveform display.
 *
 *  The audio processor writes to a shared ring buffer (audio thread).
 *  The UI timer reads it (message thread) and calls repaint().
 *
 *  Features:
 *   • Phosphor green glow (double-stroke: wide dim + thin bright)
 *   • Dark scanline grid overlay
 *   • Rounded "CRT screen" bezels
 *   • Auto-gain: normalises displayed signal
 */
class OscilloscopeComponent : public juce::Component
{
public:
    static constexpr int DISPLAY_POINTS = 256;

    OscilloscopeComponent() = default;

    /** Called from the UI timer at ~30 Hz.
     *  data must have at least 'size' valid samples in a ring buffer.
     *  writePos is the index of the most recently written sample. */
    void refresh (const float* data, int size, int writePos)
    {
        // Copy the most recent DISPLAY_POINTS samples (oldest first)
        const int start = (writePos - DISPLAY_POINTS + size) % size;
        for (int i = 0; i < DISPLAY_POINTS; ++i)
            displayBuf[i] = data[(start + i) % size];

        repaint();
    }

    // ─────────────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        const float w = static_cast<float> (getWidth());
        const float h = static_cast<float> (getHeight());

        // ── CRT bezel background ──────────────────────────────────────
        {
            juce::ColourGradient bg (
                juce::Colour (0xFF060F06), 0.f,   0.f,
                juce::Colour (0xFF030803), w * 0.5f, h,
                false);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (0.f, 0.f, w, h, 5.f);
        }

        // ── Grid lines ────────────────────────────────────────────────
        g.setColour (juce::Colour (0xFF0A200A));
        for (int row = 1; row < 4; ++row)
            g.drawHorizontalLine (static_cast<int> (h * static_cast<float> (row) / 4.f),
                                  1.f, w - 1.f);
        for (int col = 1; col < 6; ++col)
            g.drawVerticalLine (static_cast<int> (w * static_cast<float> (col) / 6.f),
                                1.f, h - 1.f);

        // ── Centre baseline ───────────────────────────────────────────
        g.setColour (juce::Colour (0xFF143014));
        g.drawHorizontalLine (static_cast<int> (h * 0.5f), 1.f, w - 1.f);

        // ── Waveform path ─────────────────────────────────────────────
        // Auto-gain
        float peak = 0.01f;
        for (auto s : displayBuf)
            peak = std::max (peak, std::abs (s));
        const float gain   = 0.85f / peak;
        const float halfH  = h * 0.5f;

        juce::Path path;
        for (int i = 0; i < DISPLAY_POINTS; ++i)
        {
            const float x = w * static_cast<float> (i) / static_cast<float> (DISPLAY_POINTS - 1);
            const float y = halfH - displayBuf[i] * gain * halfH;
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo          (x, y);
        }

        // Glow layer (wide, dim)
        g.setColour (juce::Colour (0x2800DD44));
        g.strokePath (path, juce::PathStrokeType (4.5f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Mid glow
        g.setColour (juce::Colour (0x6000FF55));
        g.strokePath (path, juce::PathStrokeType (2.5f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Bright core
        g.setColour (juce::Colour (0xFF44FF88));
        g.strokePath (path, juce::PathStrokeType (1.2f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ── Scanlines overlay ─────────────────────────────────────────
        for (int y = 0; y < getHeight(); y += 3)
        {
            g.setColour (juce::Colour (0x08000000));
            g.fillRect (0, y, getWidth(), 1);
        }

        // ── Bezel border ──────────────────────────────────────────────
        g.setColour (juce::Colour (0xFF1E3A1E));
        g.drawRoundedRectangle (0.5f, 0.5f, w - 1.f, h - 1.f, 5.f, 1.5f);
    }

private:
    std::array<float, DISPLAY_POINTS> displayBuf = {};
};
