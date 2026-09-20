#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <vector>
#include <mutex>

class OscilloscopeModuleEditor;

class OscilloscopeModule : public ModuleProcessor
{
public:
    static constexpr int bufferSize = 1024;

    OscilloscopeModule()
        : ModuleProcessor ("Oscilloscope", createLayout())
    {
        waveformBuffer.resize (bufferSize, 0.0f);
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
            return;

        const float* readPtr = buffer.getReadPointer (0);
        int numSamples = buffer.getNumSamples();

        std::lock_guard<std::mutex> lock (bufferMutex);
        for (int i = 0; i < numSamples; ++i)
        {
            waveformBuffer[writeIndex] = readPtr[i];
            writeIndex = (writeIndex + 1) % bufferSize;
        }
    }

    // Thread-safe copy of recent waveform history
    void getWaveformData (std::vector<float>& dest)
    {
        dest.resize (bufferSize);
        std::lock_guard<std::mutex> lock (bufferMutex);

        int idx = writeIndex;
        for (int i = 0; i < bufferSize; ++i)
        {
            dest[i] = waveformBuffer[(idx + i) % bufferSize];
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "scale", 1 },
                "Scale",
                juce::NormalisableRange<float> (0.2f, 5.0f, 0.05f),
                1.0f,
                juce::AudioParameterFloatAttributes().withLabel ("x")
            )
        };
    }

    std::vector<float> waveformBuffer;
    int writeIndex = 0;
    std::mutex bufferMutex;
};

// =========================================================================
// --- OSCILLOSCOPE MODULE EDITOR (POPUP WINDOW) ---
// =========================================================================
class OscilloscopeModuleEditor : public juce::AudioProcessorEditor,
                                 public juce::Timer
{
public:
    explicit OscilloscopeModuleEditor (OscilloscopeModule& p)
        : juce::AudioProcessorEditor (p), osc(p)
    {
        setSize (360, 220);
        startTimerHz (60); // 60 FPS ultra-smooth refresh
    }

    ~OscilloscopeModuleEditor() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (UITheme::bgCanvas);

        auto bounds = getLocalBounds().toFloat().reduced (12.0f);

        // Header Title
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (12.0f, true));
        g.drawText ("Oscilloscope / Waveform Monitor", bounds.removeFromTop (22.0f), juce::Justification::centredLeft);

        // CRT Screen Bezel
        auto screen = bounds.reduced (2.0f, 4.0f);
        g.setColour (juce::Colour (0xff0a0c10));
        g.fillRoundedRectangle (screen, 6.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screen, 6.0f, 1.0f);

        // Subtle CRT Reticle Grid
        g.setColour (juce::Colour (0x1830d158));
        float midY = screen.getCentreY();
        g.drawLine (screen.getX(), midY, screen.getRight(), midY, 1.0f);
        float midX = screen.getCentreX();
        g.drawLine (midX, screen.getY(), midX, screen.getBottom(), 1.0f);

        for (float y = screen.getY(); y < screen.getBottom(); y += 20.0f)
            g.drawLine (screen.getX(), y, screen.getRight(), y, 0.5f);
        for (float x = screen.getX(); x < screen.getRight(); x += 25.0f)
            g.drawLine (x, screen.getY(), x, screen.getBottom(), 0.5f);

        // Fetch waveform points
        std::vector<float> points;
        osc.getWaveformData (points);

        if (points.empty()) return;

        juce::Path wavePath;
        float h = screen.getHeight() * 0.45f;
        float stepX = screen.getWidth() / (float) points.size();

        wavePath.startNewSubPath (screen.getX(), midY - points[0] * h);
        for (size_t i = 1; i < points.size(); ++i)
        {
            float px = screen.getX() + (float) i * stepX;
            float py = midY - points[i] * h;
            wavePath.lineTo (px, py);
        }

        // 1. Glowing Phosphor Cyan/Green Aura
        g.setColour (juce::Colour (0x3330d158));
        g.strokePath (wavePath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Solid Phosphor Beam
        g.setColour (juce::Colour (0xff30d158));
        g.strokePath (wavePath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Hot Core Beam (White/Green highlight)
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.strokePath (wavePath, juce::PathStrokeType (0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    OscilloscopeModule& osc;
};

inline juce::AudioProcessorEditor* OscilloscopeModule::createEditor()
{
    return new OscilloscopeModuleEditor (*this);
}
