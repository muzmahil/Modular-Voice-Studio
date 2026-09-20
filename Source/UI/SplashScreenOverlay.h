#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../UITheme.h"
#include <BinaryData.h>

class SplashScreenOverlay : public juce::Component,
                            private juce::Timer
{
public:
    explicit SplashScreenOverlay (std::function<void()> onDismissedCallback = nullptr)
        : onDismissed (std::move (onDismissedCallback))
    {
        setAlwaysOnTop (true);
        startTimerHz (60);
    }

    ~SplashScreenOverlay() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // 1. Dark Vignette Background with Animated Opacity
        g.setColour (juce::Colour (0xf80b0b0e).withAlpha (currentAlpha));
        g.fillRect (bounds);

        if (currentAlpha <= 0.01f)
            return;

        // 2. Central Frosted Glass Card
        float cardW = juce::jmin (440.0f, bounds.getWidth() - 40.0f);
        float cardH = 260.0f;
        auto card = juce::Rectangle<float> (bounds.getCentreX() - cardW * 0.5f,
                                            bounds.getCentreY() - cardH * 0.5f,
                                            cardW, cardH);

        // Soft Outer Shadow
        g.setColour (juce::Colours::black.withAlpha (0.45f * currentAlpha));
        g.fillRoundedRectangle (card.translated (0.0f, 6.0f), 16.0f);

        // Card Glass Gradient
        juce::ColourGradient cardGrad (juce::Colour (0xff1f1f26).withAlpha (currentAlpha), card.getX(), card.getY(),
                                       juce::Colour (0xff141418).withAlpha (currentAlpha), card.getX(), card.getBottom(), false);
        g.setGradientFill (cardGrad);
        g.fillRoundedRectangle (card, 14.0f);

        // Subtle Card Border
        g.setColour (juce::Colour (0x40ffffff).withAlpha (0.25f * currentAlpha));
        g.drawRoundedRectangle (card, 14.0f, 1.2f);

        // Specular Top Rim
        g.setColour (UITheme::specularRim.withAlpha (currentAlpha));
        g.drawLine (card.getX() + 14.0f, card.getY() + 1.0f, card.getRight() - 14.0f, card.getY() + 1.0f, 1.0f);

        // 3. Glowing Studio Logo (Modular Patch Plug + Sonic Waves or Uploaded Icon)
        float logoCenterY = card.getY() + 64.0f;
        float logoRadius = 28.0f;

        // Logo Glow Halo
        g.setColour (UITheme::appleBlue.withAlpha ((0.20f + 0.10f * pulsePhase) * currentAlpha));
        g.fillEllipse (card.getCentreX() - logoRadius - 8.0f, logoCenterY - logoRadius - 8.0f,
                       (logoRadius + 8.0f) * 2.0f, (logoRadius + 8.0f) * 2.0f);

        auto logoImg = juce::ImageCache::getFromMemory (BinaryData::icon_png, BinaryData::icon_pngSize);
        if (logoImg.isValid())
        {
            auto logoRect = juce::Rectangle<float> (card.getCentreX() - logoRadius, logoCenterY - logoRadius,
                                                    logoRadius * 2.0f, logoRadius * 2.0f);
            g.setOpacity (currentAlpha);
            g.drawImageWithin (logoImg, (int) logoRect.getX(), (int) logoRect.getY(), (int) logoRect.getWidth(), (int) logoRect.getHeight(),
                               juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            // Fallback Logo Base Circle
            juce::ColourGradient logoGrad (UITheme::appleBlue.withAlpha (currentAlpha), card.getCentreX(), logoCenterY - logoRadius,
                                           juce::Colour (0xff0a58ca).withAlpha (currentAlpha), card.getCentreX(), logoCenterY + logoRadius, false);
            g.setGradientFill (logoGrad);
            g.fillEllipse (card.getCentreX() - logoRadius, logoCenterY - logoRadius, logoRadius * 2.0f, logoRadius * 2.0f);

            // Modular Jack Icon inside logo
            g.setColour (juce::Colours::white.withAlpha (currentAlpha));
            juce::Path lp;
            lp.addRoundedRectangle (card.getCentreX() - 4.5f, logoCenterY - 10.0f, 9.0f, 14.0f, 2.0f);
            lp.addRoundedRectangle (card.getCentreX() - 1.5f, logoCenterY + 4.0f, 3.0f, 7.0f, 1.0f);
            g.strokePath (lp, juce::PathStrokeType (1.8f));
            g.fillEllipse (card.getCentreX() - 2.0f, logoCenterY - 4.0f, 4.0f, 4.0f);
        }

        // 4. Main Title
        g.setColour (UITheme::textPrimary.withAlpha (currentAlpha));
        g.setFont (UITheme::getFont (19.0f, true));
        g.drawText ("Modular Voice Studio",
                    juce::Rectangle<float> (card.getX(), logoCenterY + 36.0f, card.getWidth(), 26.0f),
                    juce::Justification::centred);

        // 5. Subtitle
        g.setColour (UITheme::textSecondary.withAlpha (currentAlpha));
        g.setFont (UITheme::getFont (12.0f));
        g.drawText ("Next-Gen Modular Audio & Vocal DSP Environment",
                    juce::Rectangle<float> (card.getX(), logoCenterY + 64.0f, card.getWidth(), 18.0f),
                    juce::Justification::centred);

        // 6. Pro Version Capsule Badge
        auto badge = juce::Rectangle<float> (card.getCentreX() - 46.0f, logoCenterY + 92.0f, 92.0f, 18.0f);
        g.setColour (juce::Colour (0x28ffffff).withAlpha (0.35f * currentAlpha));
        g.fillRoundedRectangle (badge, 9.0f);
        g.setColour (UITheme::appleBlue.withAlpha (currentAlpha));
        g.drawRoundedRectangle (badge, 9.0f, 1.0f);

        g.setColour (UITheme::appleBlue.withAlpha (currentAlpha));
        g.setFont (UITheme::getFont (10.0f, true));
        g.drawText ("RELEASE PRO", badge, juce::Justification::centred);

        // 7. Loading Progress Track
        float trackW = card.getWidth() - 80.0f;
        float trackH = 3.0f;
        float trackX = card.getX() + 40.0f;
        float trackY = card.getBottom() - 28.0f;

        g.setColour (juce::Colour (0xff25252c).withAlpha (currentAlpha));
        g.fillRoundedRectangle (trackX, trackY, trackW, trackH, 1.5f);

        float progress = juce::jlimit (0.0f, 1.0f, elapsedFrames / 45.0f);
        if (progress > 0.01f)
        {
            g.setColour (UITheme::appleBlue.withAlpha (currentAlpha));
            g.fillRoundedRectangle (trackX, trackY, trackW * progress, trackH, 1.5f);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dismissImmediately();
    }

    void dismissImmediately()
    {
        isFadingOut = true;
        currentAlpha = 0.0f;
        if (onDismissed != nullptr)
            onDismissed();
        setVisible (false);
    }

private:
    void timerCallback() override
    {
        elapsedFrames++;
        pulsePhase = 0.5f + 0.5f * std::sin (elapsedFrames * 0.12f);

        // Hold for ~65 frames (~1.1 seconds), then fade out over 20 frames
        if (elapsedFrames > 65)
        {
            isFadingOut = true;
            currentAlpha = juce::jmax (0.0f, currentAlpha - 0.07f);
            if (currentAlpha <= 0.001f)
            {
                stopTimer();
                if (onDismissed != nullptr)
                    onDismissed();
                setVisible (false);
            }
        }
        else
        {
            currentAlpha = juce::jmin (1.0f, currentAlpha + 0.12f);
        }

        repaint();
    }

    std::function<void()> onDismissed;
    int elapsedFrames = 0;
    float currentAlpha = 0.0f;
    float pulsePhase = 0.0f;
    bool isFadingOut = false;
};
