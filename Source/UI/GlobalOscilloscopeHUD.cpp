#include "GlobalOscilloscopeHUD.h"
#include "../PluginProcessor.h"
#include "../UITheme.h"
#include "../Localization.h"

//==============================================================================
// OscilloscopeDetailView Implementation
//==============================================================================
OscilloscopeDetailView::OscilloscopeDetailView (PluginProcessor& p)
    : processor (p)
{
    fftBufferIn.resize (1024, 0.0f);
    fftBufferOut.resize (1024, 0.0f);
    spectrumIn.resize (256, 0.0f);
    spectrumOut.resize (256, 0.0f);

    auto setupBtn = [this] (juce::TextButton& btn, ViewMode mode)
    {
        btn.setClickingTogglesState (true);
        btn.setRadioGroupId (1001);
        btn.onClick = [this, mode]
        {
            currentMode = mode;
            repaint();
        };
        addAndMakeVisible (btn);
    };

    setupBtn (modeCompareBtn,  CompareDual);
    setupBtn (modeInputBtn,    InputOnly);
    setupBtn (modeOutputBtn,   OutputOnly);
    setupBtn (modeDeltaBtn,    ImpactDelta);
    setupBtn (modeSpectrumBtn, SpectrumFFT);
    modeCompareBtn.setToggleState (true, juce::dontSendNotification);

    modeCompareBtn.setButtonText (tr ("DUAL_COMPARE"));
    modeInputBtn.setButtonText (tr ("INPUT_ONLY"));
    modeOutputBtn.setButtonText (tr ("OUTPUT_ONLY"));
    modeDeltaBtn.setButtonText (tr ("DELTA_IMPACT"));
    modeSpectrumBtn.setButtonText (tr ("SPECTRUM_FFT"));

    freezeBtn.setClickingTogglesState (true);
    freezeBtn.setButtonText (tr ("FREEZE"));
    freezeBtn.onClick = [this]
    {
        isFrozen = freezeBtn.getToggleState();
        freezeBtn.setButtonText (isFrozen ? tr ("FROZEN") : tr ("FREEZE"));
        repaint();
    };
    addAndMakeVisible (freezeBtn);

    addAndMakeVisible (freezeBtn);

    LocalizationManager::instance().addListener (this);

    scaleCombo.addItem ("Scale: 1.0x", 1);
    scaleCombo.addItem ("Scale: 2.0x", 2);
    scaleCombo.addItem ("Scale: 4.0x", 3);
    scaleCombo.setSelectedId (1, juce::dontSendNotification);
    scaleCombo.onChange = [this]
    {
        int id = scaleCombo.getSelectedId();
        verticalScale = (id == 2 ? 2.0f : (id == 3 ? 4.0f : 1.0f));
        repaint();
    };
    addAndMakeVisible (scaleCombo);

    startTimerHz (60);
    setSize (780, 450);
}

OscilloscopeDetailView::~OscilloscopeDetailView()
{
    LocalizationManager::instance().removeListener (this);
    stopTimer();
}

void OscilloscopeDetailView::localizationChanged()
{
    modeCompareBtn.setButtonText (tr ("DUAL_COMPARE"));
    modeInputBtn.setButtonText (tr ("INPUT_ONLY"));
    modeOutputBtn.setButtonText (tr ("OUTPUT_ONLY"));
    modeDeltaBtn.setButtonText (tr ("DELTA_IMPACT"));
    modeSpectrumBtn.setButtonText (tr ("SPECTRUM_FFT"));
    freezeBtn.setButtonText (isFrozen ? tr ("FROZEN") : tr ("FREEZE"));
    repaint();
}

void OscilloscopeDetailView::computeFFT()
{
    if (inputWave.size() < 512 || outputWave.size() < 512)
        return;

    std::fill (fftBufferIn.begin(), fftBufferIn.end(), 0.0f);
    std::fill (fftBufferOut.begin(), fftBufferOut.end(), 0.0f);

    size_t offset = inputWave.size() >= 512 ? inputWave.size() - 512 : 0;
    std::copy_n (inputWave.begin() + (std::ptrdiff_t) offset, 512, fftBufferIn.begin());
    std::copy_n (outputWave.begin() + (std::ptrdiff_t) offset, 512, fftBufferOut.begin());

    window.multiplyWithWindowingTable (fftBufferIn.data(), 512);
    window.multiplyWithWindowingTable (fftBufferOut.data(), 512);

    fftEngine.performFrequencyOnlyForwardTransform (fftBufferIn.data());
    fftEngine.performFrequencyOnlyForwardTransform (fftBufferOut.data());

    for (size_t i = 0; i < 256; ++i)
    {
        float inMag  = fftBufferIn[i];
        float outMag = fftBufferOut[i];
        spectrumIn[i]  = spectrumIn[i]  * 0.65f + inMag  * 0.35f;
        spectrumOut[i] = spectrumOut[i] * 0.65f + outMag * 0.35f;
    }
}

void OscilloscopeDetailView::timerCallback()
{
    if (! isFrozen)
    {
        processor.getWaveformData (inputWave, outputWave);
        computeFFT();
        repaint();
    }
}

void OscilloscopeDetailView::resized()
{
    auto area = getLocalBounds().removeFromTop (36).reduced (8, 4);
    modeCompareBtn.setBounds (area.removeFromLeft (95));
    area.removeFromLeft (4);
    modeInputBtn.setBounds (area.removeFromLeft (80));
    area.removeFromLeft (4);
    modeOutputBtn.setBounds (area.removeFromLeft (85));
    area.removeFromLeft (4);
    modeDeltaBtn.setBounds (area.removeFromLeft (95));
    area.removeFromLeft (4);
    modeSpectrumBtn.setBounds (area.removeFromLeft (100));
    area.removeFromLeft (8);

    freezeBtn.setBounds (area.removeFromLeft (95));
    area.removeFromLeft (8);

    scaleCombo.setBounds (area.removeFromLeft (105));
}

void OscilloscopeDetailView::paint (juce::Graphics& g)
{
    g.fillAll (UITheme::bgCanvas);

    auto bounds = getLocalBounds().toFloat();
    auto topBar = bounds.removeFromTop (36.0f);
    g.setColour (UITheme::bgToolbar);
    g.fillRect (topBar);
    g.setColour (UITheme::strokeHairline);
    g.drawLine (0, topBar.getBottom(), bounds.getRight(), topBar.getBottom(), 1.0f);

    auto screen = bounds.reduced (12.0f, 10.0f);
    auto bottomTelemetry = screen.removeFromBottom (26.0f);

    // CRT Oscilloscope Bezel
    g.setColour (juce::Colour (0xff080a0f));
    g.fillRoundedRectangle (screen, 4.0f);
    g.setColour (UITheme::strokeHairline);
    g.drawRoundedRectangle (screen, 4.0f, 1.2f);

    // Reticle Grid Lines & dB Reference Axis
    float midY = screen.getCentreY();
    float halfH = screen.getHeight() * 0.45f;

    g.setFont (UITheme::getFont (9.0f));
    auto drawGridLine = [&] (float factor, const juce::String& label, bool isZeroAxis = false)
    {
        float y = midY - factor * halfH;
        g.setColour (isZeroAxis ? juce::Colour (0x4030d158) : juce::Colour (0x18ffffff));
        g.drawLine (screen.getX(), y, screen.getRight(), y, isZeroAxis ? 1.0f : 0.6f);

        g.setColour (UITheme::textTertiary);
        g.drawText (label, juce::Rectangle<float> (screen.getX() + 6.0f, y - 9.0f, 70.0f, 18.0f), juce::Justification::centredLeft);
    };

    drawGridLine ( 1.0f, "+0 dB (Peak)");
    drawGridLine ( 0.5f, "-6 dB");
    drawGridLine ( 0.0f, " 0 dB Axis", true);
    drawGridLine (-0.5f, "-6 dB");
    drawGridLine (-1.0f, "-0 dB (Peak)");

    // Vertical time division lines
    int divisions = 8;
    float divW = screen.getWidth() / (float) divisions;
    g.setColour (juce::Colour (0x10ffffff));
    for (int d = 1; d < divisions; ++d)
    {
        float x = screen.getX() + (float) d * divW;
        g.drawLine (x, screen.getY(), x, screen.getBottom(), 0.6f);
    }

    if (inputWave.empty() || outputWave.empty())
        return;

    int numSamples = (int) inputWave.size();
    float stepX = screen.getWidth() / (float) (numSamples - 1);

    // Difference / Dynamic Impact Shading (Where processed signal changed compared to incoming dry signal)
    if (currentMode == CompareDual || currentMode == ImpactDelta)
    {
        for (int i = 0; i < numSamples - 1; i += 2)
        {
            float x1 = screen.getX() + (float) i * stepX;
            float inVal1  = juce::jlimit (-1.0f, 1.0f, inputWave[(size_t) i] * verticalScale);
            float outVal1 = juce::jlimit (-1.0f, 1.0f, outputWave[(size_t) i] * verticalScale);
            float yIn1  = midY - inVal1 * halfH;
            float yOut1 = midY - outVal1 * halfH;

            float diff = outVal1 - inVal1;
            if (std::abs (diff) > 0.015f)
            {
                // Compression / Attenuation (Output < Input): Glowing amber/orange
                // Gain / Harmonic Boost (Output > Input): Electric blue/purple
                juce::Colour fillCol = (diff < 0.0f) ? UITheme::appleOrange.withAlpha (0.24f)
                                                     : UITheme::appleBlue.withAlpha (0.24f);
                g.setColour (fillCol);
                g.drawLine (x1, yIn1, x1, yOut1, 2.0f);
            }
        }
    }

    // Input Waveform Path (Electric Cyan)
    if (currentMode == CompareDual || currentMode == InputOnly)
    {
        juce::Path inPath;
        inPath.startNewSubPath (screen.getX(), midY - juce::jlimit (-1.0f, 1.0f, inputWave[0] * verticalScale) * halfH);
        for (int i = 1; i < numSamples; ++i)
        {
            float px = screen.getX() + (float) i * stepX;
            float py = midY - juce::jlimit (-1.0f, 1.0f, inputWave[(size_t) i] * verticalScale) * halfH;
            inPath.lineTo (px, py);
        }

        // Soft ambient glow
        g.setColour (juce::Colour (0x2800d2ff));
        g.strokePath (inPath, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved));

        // Crisp Beam
        g.setColour (juce::Colour (0xff00d2ff));
        g.strokePath (inPath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
    }

    // Output Waveform Path (Emerald Neon Green)
    if (currentMode == CompareDual || currentMode == OutputOnly)
    {
        juce::Path outPath;
        outPath.startNewSubPath (screen.getX(), midY - juce::jlimit (-1.0f, 1.0f, outputWave[0] * verticalScale) * halfH);
        for (int i = 1; i < numSamples; ++i)
        {
            float px = screen.getX() + (float) i * stepX;
            float py = midY - juce::jlimit (-1.0f, 1.0f, outputWave[(size_t) i] * verticalScale) * halfH;
            outPath.lineTo (px, py);
        }

        // Soft ambient glow
        g.setColour (juce::Colour (0x2830d158));
        g.strokePath (outPath, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved));

        // Crisp Beam
        g.setColour (juce::Colour (0xff30d158));
        g.strokePath (outPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
    }

    // Real-Time 20Hz - 20kHz FFT Spectrum Analyzer
    if (currentMode == SpectrumFFT)
    {
        auto getFreqX = [&] (float freq) -> float
        {
            float minF = 20.0f;
            float maxF = 20000.0f;
            float norm = (std::log10 (juce::jlimit (minF, maxF, freq)) - std::log10 (minF)) / (std::log10 (maxF) - std::log10 (minF));
            return screen.getX() + norm * screen.getWidth();
        };

        const float freqs[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f, 20000.0f };
        const char* fLabels[] = { "50Hz", "100Hz", "250Hz", "500Hz", "1kHz", "2.5k", "5kHz", "10kHz", "20kHz" };

        g.setFont (UITheme::getFont (9.0f));
        for (int fi = 0; fi < 9; ++fi)
        {
            float fx = getFreqX (freqs[fi]);
            g.setColour (juce::Colour (0x1affffff));
            g.drawLine (fx, screen.getY(), fx, screen.getBottom(), 0.6f);
            g.setColour (UITheme::textTertiary);
            g.drawText (fLabels[fi], juce::Rectangle<float> (fx - 18.0f, screen.getBottom() - 16.0f, 36.0f, 14.0f), juce::Justification::centred);
        }

        auto drawSpectrumCurve = [&] (const std::vector<float>& spec, juce::Colour col)
        {
            if (spec.empty()) return;
            juce::Path p;
            float sampleRate = (float) processor.getSampleRate();
            if (sampleRate < 8000.0f) sampleRate = 48000.0f;
            float binWidth = (sampleRate * 0.5f) / (float) spec.size();

            bool started = false;
            for (size_t i = 1; i < spec.size(); ++i)
            {
                float freq = (float) i * binWidth;
                if (freq < 20.0f || freq > 20000.0f) continue;
                float x = getFreqX (freq);
                float magDb = juce::Decibels::gainToDecibels (spec[i] * 0.04f * verticalScale, -90.0f);
                float normY = juce::jmap (magDb, -72.0f, 0.0f, 0.0f, 1.0f);
                normY = juce::jlimit (0.0f, 1.0f, normY);
                float y = screen.getBottom() - normY * (screen.getHeight() - 20.0f) - 10.0f;

                if (! started) { p.startNewSubPath (x, y); started = true; }
                else          { p.lineTo (x, y); }
            }

            g.setColour (col.withAlpha (0.22f));
            g.strokePath (p, juce::PathStrokeType (3.8f, juce::PathStrokeType::curved));
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
        };

        drawSpectrumCurve (spectrumIn,  juce::Colour (0xff00d2ff));
        drawSpectrumCurve (spectrumOut, juce::Colour (0xff30d158));
    }

    // Real-Time Telemetry Bar at Bottom of Screen (Peak, RMS, Crest Factor)
    float inPeak = 0.0f, outPeak = 0.0f;
    float inSumSq = 0.0f, outSumSq = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float inAbs  = std::abs (inputWave[(size_t) i]);
        float outAbs = std::abs (outputWave[(size_t) i]);
        inPeak  = juce::jmax (inPeak, inAbs);
        outPeak = juce::jmax (outPeak, outAbs);
        inSumSq  += inAbs * inAbs;
        outSumSq += outAbs * outAbs;
    }
    float outRms = std::sqrt (outSumSq / (float) numSamples);

    float inDb     = inPeak  > 0.00001f ? 20.0f * std::log10 (inPeak)  : -96.0f;
    float outDb    = outPeak > 0.00001f ? 20.0f * std::log10 (outPeak) : -96.0f;
    float outRmsDb = outRms  > 0.00001f ? 20.0f * std::log10 (outRms)  : -96.0f;
    float crestDb  = outDb - outRmsDb;
    float deltaDb  = outDb - inDb;

    juce::String deltaStr = (deltaDb >= 0.0f ? "+" : "") + juce::String (deltaDb, 1) + " dB";
    juce::String impactType = (deltaDb < -0.5f) ? tr ("COMPRESSION_DESC") : (deltaDb > 0.5f ? tr ("BOOST_DESC") : tr ("TRANSPARENT_DESC"));

    g.setColour (UITheme::cardSurface);
    g.fillRoundedRectangle (bottomTelemetry.reduced (0.0f, 2.0f), 3.0f);
    g.setColour (UITheme::strokeHairline);
    g.drawRoundedRectangle (bottomTelemetry.reduced (0.0f, 2.0f), 3.0f, 1.0f);

    g.setFont (UITheme::getFont (10.0f, true));
    g.setColour (juce::Colour (0xff00d2ff));
    g.drawText (tr ("IN_LABEL") + ": " + juce::String (inDb, 1) + " dB", bottomTelemetry.removeFromLeft (110).reduced (6, 0), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff30d158));
    g.drawText (tr ("OUT_LABEL") + ": " + juce::String (outDb, 1) + " dB (" + tr ("RMS_LABEL") + ": " + juce::String (outRmsDb, 1) + ")", bottomTelemetry.removeFromLeft (180).reduced (6, 0), juce::Justification::centredLeft);

    g.setColour (UITheme::appleOrange);
    g.drawText (tr ("CREST_LABEL") + ": " + juce::String (crestDb, 1) + " dB | " + tr ("DELTA_LABEL") + ": " + deltaStr + " (" + impactType + ")", bottomTelemetry.reduced (6, 0), juce::Justification::centredLeft);
}

//==============================================================================
// OscilloscopeWindow Implementation
//==============================================================================
OscilloscopeWindow::OscilloscopeWindow (PluginProcessor& p)
    : juce::DocumentWindow ("Dual-Trace Waveform Monitor & Audio Analyzer",
                            UITheme::bgCanvas,
                            juce::DocumentWindow::closeButton)
{
    detailView = std::make_unique<OscilloscopeDetailView> (p);
    setContentNonOwned (detailView.get(), true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    centreWithSize (780, 480);
    setVisible (true);
}

void OscilloscopeWindow::closeButtonPressed()
{
    setVisible (false);
}

//==============================================================================
// GlobalOscilloscopeHUD Implementation
//==============================================================================
GlobalOscilloscopeHUD::GlobalOscilloscopeHUD (PluginProcessor& p)
    : processor (p)
{
    startTimerHz (30);
    setTooltip ("Global Waveform Monitor (Dual-Trace: Cyan=Input, Green=Processed). Click to open full Audio Analyzer.");
    LocalizationManager::instance().addListener (this);
}

GlobalOscilloscopeHUD::~GlobalOscilloscopeHUD()
{
    LocalizationManager::instance().removeListener (this);
    stopTimer();
}

void GlobalOscilloscopeHUD::localizationChanged()
{
    repaint();
}

void GlobalOscilloscopeHUD::timerCallback()
{
    processor.getWaveformData (inputWave, outputWave);
    repaint();
}

void GlobalOscilloscopeHUD::openDetailedWindow()
{
    if (activeWindow == nullptr)
        activeWindow = std::make_unique<OscilloscopeWindow> (processor);

    activeWindow->setVisible (true);
    activeWindow->toFront (true);
}

void GlobalOscilloscopeHUD::mouseDown (const juce::MouseEvent& /*e*/)
{
    openDetailedWindow();
}

void GlobalOscilloscopeHUD::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Frosted HUD Glass Material
    g.setColour (UITheme::bgSidebar.withAlpha (0.88f));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (UITheme::strokeHairline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    // 2. Header Bar
    auto header = bounds.removeFromTop (22.0f).reduced (8.0f, 0.0f);
    g.setColour (UITheme::textSecondary);
    g.setFont (UITheme::getFont (8.5f, true));
    g.drawText (tr ("WAVE_MONITOR"), header.removeFromLeft (78.0f), juce::Justification::centredLeft);

    // Dual Trace Legend Dots
    g.setColour (juce::Colour (0xff00d2ff));
    g.drawText (tr ("IN_LABEL"), header.removeFromLeft (18.0f), juce::Justification::centred);

    g.setColour (juce::Colour (0xff30d158));
    g.drawText (tr ("OUT_LABEL"), header.removeFromLeft (24.0f), juce::Justification::centred);

    // Expand Window Icon [ ↗ ]
    g.setColour (UITheme::textTertiary);
    g.setFont (UITheme::getFont (10.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("[ \xe2\x86\x97 ]")), header, juce::Justification::centredRight);

    // 3. CRT Inset Screen
    auto screen = bounds.reduced (6.0f, 4.0f);
    g.setColour (juce::Colour (0xff080a0f));
    g.fillRoundedRectangle (screen, 3.0f);
    g.setColour (UITheme::strokeHairline);
    g.drawRoundedRectangle (screen, 3.0f, 0.8f);

    float midY = screen.getCentreY();
    float halfH = screen.getHeight() * 0.44f;

    // Reticle Center Line
    g.setColour (juce::Colour (0x1effffff));
    g.drawLine (screen.getX(), midY, screen.getRight(), midY, 0.5f);

    if (inputWave.empty() || outputWave.empty())
        return;

    int count = (int) inputWave.size();
    int samplesToDraw = 64;
    int stride = juce::jmax (1, count / samplesToDraw);
    float stepX = screen.getWidth() / (float) (samplesToDraw - 1);

    // Draw Difference Shading (Orange/Red for compression, Blue for boost)
    for (int i = 0; i < samplesToDraw; ++i)
    {
        size_t idx = (size_t) (i * stride);
        if (idx >= inputWave.size() || idx >= outputWave.size()) break;

        float inVal  = juce::jlimit (-1.0f, 1.0f, inputWave[idx]);
        float outVal = juce::jlimit (-1.0f, 1.0f, outputWave[idx]);
        float diff = outVal - inVal;

        if (std::abs (diff) > 0.02f)
        {
            float px = screen.getX() + (float) i * stepX;
            float yIn  = midY - inVal * halfH;
            float yOut = midY - outVal * halfH;
            g.setColour (diff < 0.0f ? UITheme::appleOrange.withAlpha (0.28f) : UITheme::appleBlue.withAlpha (0.28f));
            g.drawLine (px, yIn, px, yOut, 1.5f);
        }
    }

    // Input Wave Path (Cyan)
    juce::Path inPath;
    inPath.startNewSubPath (screen.getX(), midY - juce::jlimit (-1.0f, 1.0f, inputWave[0]) * halfH);
    for (int i = 1; i < samplesToDraw; ++i)
    {
        size_t idx = (size_t) (i * stride);
        if (idx >= inputWave.size()) break;
        float px = screen.getX() + (float) i * stepX;
        float py = midY - juce::jlimit (-1.0f, 1.0f, inputWave[idx]) * halfH;
        inPath.lineTo (px, py);
    }
    g.setColour (juce::Colour (0x3000d2ff));
    g.strokePath (inPath, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved));
    g.setColour (juce::Colour (0xff00d2ff));
    g.strokePath (inPath, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));

    // Output Wave Path (Emerald Green)
    juce::Path outPath;
    outPath.startNewSubPath (screen.getX(), midY - juce::jlimit (-1.0f, 1.0f, outputWave[0]) * halfH);
    for (int i = 1; i < samplesToDraw; ++i)
    {
        size_t idx = (size_t) (i * stride);
        if (idx >= outputWave.size()) break;
        float px = screen.getX() + (float) i * stepX;
        float py = midY - juce::jlimit (-1.0f, 1.0f, outputWave[idx]) * halfH;
        outPath.lineTo (px, py);
    }
    g.setColour (juce::Colour (0x3030d158));
    g.strokePath (outPath, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved));
    g.setColour (juce::Colour (0xff30d158));
    g.strokePath (outPath, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved));
}
