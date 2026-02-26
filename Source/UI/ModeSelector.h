#pragma once
#include <JuceHeader.h>
#include "IndustrialLookAndFeel.h"

/**
 *  ModeSelector
 *
 *  12-position rotary switch that mimics the RE-201 mode dial.
 *  Rendered as a row of illuminated push buttons (1-12).
 *  Clicking a button sends a callback and lights the corresponding LED.
 *
 *  Labels per mode:
 *   1-7  : Echo only   (increasing head combinations)
 *   8-11 : Echo + Spring Reverb
 *   12   : Spring Reverb only
 */
class ModeSelector : public juce::Component
{
public:
    static constexpr int NUM_MODES = 12;

    // Mode descriptions (tooltip text)
    inline static const char* MODE_LABELS[NUM_MODES] = {
        "H1",  "H2",  "H3",
        "H1+2","H1+3","H2+3",
        "H1+2+3",
        "H1+R","H2+R","H3+R",
        "ALL+R","REV"
    };

    std::function<void (int)> onModeChanged;

    ModeSelector()
    {
        for (int i = 0; i < NUM_MODES; ++i)
        {
            auto* btn = buttons.add (new juce::TextButton (juce::String (i + 1)));
            btn->setTooltip (MODE_LABELS[i]);
            btn->setClickingTogglesState (false);
            btn->onClick = [this, i]
            {
                currentMode = i;
                for (auto* b : buttons) b->setToggleState (false, juce::dontSendNotification);
                buttons[i]->setToggleState (true, juce::dontSendNotification);
                repaint();
                if (onModeChanged) onModeChanged (i);
            };
            addAndMakeVisible (btn);
        }
        // Select mode 0 by default
        buttons[0]->setToggleState (true, juce::dontSendNotification);
    }

    void setMode (int mode, bool sendCallback = false)
    {
        mode = juce::jlimit (0, NUM_MODES - 1, mode);
        currentMode = mode;
        for (int i = 0; i < NUM_MODES; ++i)
            buttons[i]->setToggleState (i == mode, juce::dontSendNotification);
        repaint();
        if (sendCallback && onModeChanged) onModeChanged (mode);
    }

    int getMode() const { return currentMode; }

    void resized() override
    {
        const auto b      = getLocalBounds();
        const int  btnW   = b.getWidth()  / NUM_MODES;
        const int  btnH   = b.getHeight();
        const int  margin = 3;

        for (int i = 0; i < NUM_MODES; ++i)
            buttons[i]->setBounds (i * btnW + margin, margin,
                                   btnW - margin * 2, btnH - margin * 2);
    }

    void paint (juce::Graphics& g) override
    {
        // Background strip
        IndustrialLookAndFeel::drawMetalPanel (g, getLocalBounds().toFloat().reduced (1.f),
                                               juce::Colour (0xFF181818));

        // Section divider between pure-echo (1-7) and reverb modes (8-12)
        const auto b    = getLocalBounds().toFloat();
        const float divX = b.getX() + b.getWidth() * (7.f / NUM_MODES);
        g.setColour (juce::Colour (IndustrialLookAndFeel::COL_ORANGE).withAlpha (0.5f));
        g.drawLine (divX, b.getY() + 6.f, divX, b.getBottom() - 6.f, 1.5f);

        // LED dots above each button
        const int btnW = getWidth() / NUM_MODES;
        for (int i = 0; i < NUM_MODES; ++i)
        {
            const bool lit = (i == currentMode);
            const float cx = b.getX() + (i + 0.5f) * btnW;
            const float cy = b.getY() + 5.f;

            juce::Colour ledCol = (i >= 7) ? juce::Colour (0xFFFF8800)
                                           : juce::Colour (0xFF00CCFF);
            IndustrialLookAndFeel::drawLED (g, cx, cy, 3.5f, ledCol, lit);
        }
    }

    void lookAndFeelChanged() override { applyButtonStyle(); }

    void applyButtonStyle()
    {
        for (int i = 0; i < NUM_MODES; ++i)
        {
            auto* btn = buttons[i];
            btn->setColour (juce::TextButton::buttonColourId,
                            juce::Colour (0xFF1E1E1E));
            btn->setColour (juce::TextButton::buttonOnColourId,
                            juce::Colour (IndustrialLookAndFeel::COL_ORANGE).withAlpha (0.8f));
            btn->setColour (juce::TextButton::textColourOffId,
                            juce::Colour (IndustrialLookAndFeel::COL_LABEL_DIM));
            btn->setColour (juce::TextButton::textColourOnId,
                            juce::Colours::white);
        }
    }

    void parentHierarchyChanged() override { applyButtonStyle(); }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    int currentMode = 0;
};
