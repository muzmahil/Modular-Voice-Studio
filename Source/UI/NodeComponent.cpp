#include "NodeComponent.h"
#include "NodeCanvas.h"
#include <cmath>
#include "../UITheme.h"
#include "../Graph/ModuleFactory.h"
#include "../Graph/MixWrapperProcessor.h"
#include "../Modules/ContainerModule.h"
#include "../Modules/CrossoverSplitterModule.h"
#include "../Modules/CrossoverJoinerModule.h"
#include "../Modules/ThreeBandEQModule.h"
#include "../Modules/Spatial3DModule.h"
#include "../Localization.h"
#include <juce_gui_extra/juce_gui_extra.h> // PopupMenu ve DocumentWindow için gerekli

class ModuleEditorWindow : public juce::DocumentWindow
{
public:
    static void openForProcessor (juce::AudioProcessor* proc)
    {
        if (proc == nullptr) return;

        // 1. If a window for this processor is already open anywhere, bring it to front!
        for (auto* w : getActiveWindows())
        {
            if (w != nullptr && w->processor == proc)
            {
                w->toFront (true);
                w->setVisible (true);
                return; // NEVER create a duplicate!
            }
        }

        // 2. Otherwise create a single window
        new ModuleEditorWindow (proc);
    }

    static void closeForProcessor (juce::AudioProcessor* proc)
    {
        if (proc == nullptr) return;

        auto& list = getActiveWindows();
        for (int i = list.size(); --i >= 0;)
        {
            if (auto* w = list[i])
            {
                if (w->processor == proc)
                    delete w;
            }
        }
    }

    ModuleEditorWindow (juce::AudioProcessor* proc)
        : DocumentWindow (proc->getName(),
                          juce::Colour (0xff18181a),
                          juce::DocumentWindow::closeButton),
          processor (proc)
    {
        getActiveWindows().add (this);
        setUsingNativeTitleBar (true);
        setContentOwned (proc->createEditorIfNeeded(), true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~ModuleEditorWindow() override
    {
        getActiveWindows().removeAllInstancesOf (this);
    }

    void closeButtonPressed() override
    {
        delete this;
    }

private:
    juce::AudioProcessor* processor;

    static juce::Array<ModuleEditorWindow*>& getActiveWindows()
    {
        static juce::Array<ModuleEditorWindow*> list;
        return list;
    }
};

NodeComponent::NodeComponent(NodeCanvas &owner, juce::AudioProcessorGraph::NodeID nodeId, juce::String label,
                             bool hasInputPin, bool hasOutputPin, bool isSystemNode)
    : canvas(owner), nodeID(nodeId), displayName(std::move(label)),
      showInputPin(hasInputPin), showOutputPin(hasOutputPin), systemNode(isSystemNode)
{
    auto titleFont = UITheme::getFont (11.0f);
    
    // Calculate the maximum width of the translated title across all languages to prevent truncation
    float maxTextWidth = 0.0f;
    auto& lm = LocalizationManager::instance();
    std::vector<Language> languages { Language::English, Language::Turkish, Language::Russian, Language::Japanese };
    for (auto lang : languages)
    {
        juce::String key = displayName;
        if (displayName == "Audio In")       key = "AUDIO_IN";
        else if (displayName == "Audio Out") key = "AUDIO_OUT";
        
        juce::String trans = lm.getForLanguage (key, lang);
        maxTextWidth = juce::jmax (maxTextWidth, titleFont.getStringWidthFloat (trans));
    }

    int minWidth = systemNode ? 134 : (isSplitterNode() || isJoinerNode() ? 210 : 156);
    int extraPadding = systemNode ? 70 : (isSplitterNode() || isJoinerNode() ? 100 : 110);
    int calculatedWidth = juce::jmax (minWidth, (int) std::ceil (maxTextWidth) + extraPadding);

    if (isSplitterNode() || isJoinerNode())
        setSize (juce::jmax (210, calculatedWidth), 108);
    else
        setSize (calculatedWidth, 74);
}

NodeComponent::~NodeComponent() = default;

juce::Rectangle<float> NodeComponent::getMixDialBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f, 6.0f);
    float pbX = bounds.getRight() - 17.0f;
    return juce::Rectangle<float> (pbX - 21.0f, bounds.getY() + 4.0f, 15.0f, 15.0f);
}

void NodeComponent::paint(juce::Graphics &g)
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f, 6.0f);
    const float corner = 7.0f;

    auto *graphNode = canvas.getGraphNode(nodeID);
    isBypassed = graphNode ? graphNode->isBypassed() : false;
    juce::String colorName = graphNode ? graphNode->properties["color"].toString() : "grey";

    juce::Colour accent = juce::Colour(0xff8a8a8f);
    if (colorName == "red")
        accent = juce::Colour(0xffe25858);
    if (colorName == "blue")
        accent = juce::Colour(0xff4fa3e0);
    if (colorName == "yellow")
        accent = juce::Colour(0xffe0c34f);
    if (colorName == "green")
        accent = juce::Colour(0xff5fcf7a);
    if (colorName == "purple")
        accent = juce::Colour(0xffb37ae0);
    if (colorName == "orange")
        accent = juce::Colour(0xffe0904f);

    if (isBypassed)
        accent = accent.withSaturation(0.15f).darker(0.2f);

    // --- 1. SİSTEM NODELARI (Audio In / Out) ---
    if (systemNode) 
    {
        bool isAudioIn = displayName.containsIgnoreCase ("in");
        float liveLevel = isAudioIn ? canvas.getInputLevel() : canvas.getOutputLevel();

        // macOS Soft Drop Shadow
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), corner);

        // Apple Card Surface
        g.setColour (UITheme::cardSurface);
        g.fillRoundedRectangle (bounds, corner);

        // Header Strip
        auto headerRect = bounds.withHeight (23.0f);
        g.setColour (UITheme::cardHeader);
        g.fillRoundedRectangle (headerRect, corner);
        g.fillRect (headerRect.getX(), headerRect.getY() + 10.0f, headerRect.getWidth(), 13.0f);
        
        // Hairline divider
        g.setColour (UITheme::strokeHairline);
        g.fillRect (bounds.getX(), bounds.getY() + 23.0f, bounds.getWidth(), 1.0f);

        // Apple Specular Keyline (Top rim reflection)
        g.setColour (UITheme::specularRim);
        g.drawLine (bounds.getX() + corner, bounds.getY() + 0.5f, bounds.getRight() - corner, bounds.getY() + 0.5f, 1.0f);

        // Apple Status Indicator LED (for Audio In)
        bool hasSignal = liveLevel > 0.005f;
        juce::Colour ledColor = hasSignal ? UITheme::textPrimary : UITheme::textTertiary;
        if (isAudioIn)
        {
            g.setColour (ledColor);
            g.fillEllipse (bounds.getX() + 8.0f, bounds.getY() + 9.0f, 5.0f, 5.0f);
        }

        // Title in Inter Typography (Left-anchored)
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (11.0f, true));
        auto titleArea = bounds.withHeight (23.0f).withTrimmedLeft (isAudioIn ? 18.0f : 32.0f).withTrimmedRight (46.0f);
        juce::String translatedTitle = displayName;
        if (displayName == "Audio In")        translatedTitle = tr ("AUDIO_IN");
        else if (displayName == "Audio Out")  translatedTitle = tr ("AUDIO_OUT");
        else                                  translatedTitle = tr (displayName);
        g.drawText (translatedTitle, titleArea, juce::Justification::centredLeft);

        // Apple Frosted Pill Badge (IN 1-2 / OUT 1-2 or live dB level)
        float db = juce::Decibels::gainToDecibels (liveLevel, -60.0f);
        juce::String badgeText = isAudioIn ? "IN 1-2" : "OUT 1-2";
        if (liveLevel > 0.002f)
            badgeText = db >= 0.0f ? ("+" + juce::String (db, 1) + " dB") : (juce::String (db, 1) + " dB");

        auto badgeArea = bounds.withHeight (23.0f).removeFromRight (48.0f).reduced (2.0f, 4.5f);
        g.setColour (juce::Colour (0x20ffffff));
        g.fillRoundedRectangle (badgeArea, 3.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (badgeArea, 3.0f, 1.0f);
        g.setColour (db >= 0.0f ? UITheme::appleRed : (db >= -12.0f ? UITheme::appleYellow : UITheme::textSecondary));
        g.setFont (UITheme::getFont (8.0f));
        g.drawText (badgeText, badgeArea, juce::Justification::centred);

        // --- LOWER AREA: RECESSED DIGITAL SCREEN (L / R) ---
        auto meterScreen = bounds.withTrimmedTop (25.0f).withTrimmedBottom (6.0f).reduced (4.0f, 0.0f);
        g.setColour (UITheme::displayRecessed);
        g.fillRoundedRectangle (meterScreen, 3.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (meterScreen, 3.0f, 1.0f);

        // L & R Channel Bars
        float barH = 5.0f;
        float barY1 = meterScreen.getY() + 5.0f;
        float barY2 = meterScreen.getY() + 15.0f;
        float barX = meterScreen.getX() + 16.0f;
        float barW = meterScreen.getWidth() - 22.0f;

        // "L" and "R" labels in Inter font
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.0f));
        g.drawText ("L", juce::Rectangle<float> (meterScreen.getX() + 3.0f, barY1 - 2.0f, 10.0f, barH + 2.0f), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (meterScreen.getX() + 3.0f, barY2 - 2.0f, 10.0f, barH + 2.0f), juce::Justification::centred);

        // Track Backgrounds
        g.setColour (juce::Colour (0xff141416));
        g.fillRoundedRectangle (barX, barY1, barW, barH, 1.5f);
        g.fillRoundedRectangle (barX, barY2, barW, barH, 1.5f);

        // Live Meter Levels (Pro Audio Green -> Yellow -> Red gradient)
        float normL = juce::jlimit (0.0f, 1.0f, liveLevel * 1.25f);
        float normR = juce::jlimit (0.0f, 1.0f, liveLevel * 1.15f);

        auto drawMeterChannel = [&] (float yPos, float norm)
        {
            if (norm <= 0.001f) return;
            float activeW = barW * norm;
            
            juce::ColourGradient grad (
                UITheme::appleGreen, barX, yPos,
                UITheme::appleRed, barX + barW, yPos,
                false
            );
            grad.addColour (0.0,   UITheme::appleGreen);
            grad.addColour (0.68,  UITheme::appleGreen);
            grad.addColour (0.80,  UITheme::appleYellow);
            grad.addColour (0.90,  juce::Colour (0xffff9f0a));
            grad.addColour (0.952, UITheme::appleRed);
            grad.addColour (1.0,   juce::Colour (0xffff3b30));

            g.setGradientFill (grad);
            g.fillRoundedRectangle (barX, yPos, activeW, barH, 1.5f);

            // Segment cuts
            g.setColour (juce::Colour (0x35000000));
            for (float sx = barX + 4.0f; sx < barX + activeW; sx += 4.0f)
                g.drawVerticalLine ((int) sx, yPos, yPos + barH);
        };

        drawMeterChannel (barY1, normL);
        drawMeterChannel (barY2, normR);

        // Outer keyline border
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }
    // --- 2. MODÜL NODELARI ---
    else
    {
        // macOS Soft Drop Shadow
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), corner);

        // Apple Card Surface
        g.setColour (UITheme::cardSurface);
        g.fillRoundedRectangle (bounds, corner);

        // Header Strip
        auto headerRect = bounds.withHeight (23.0f);
        g.setColour (UITheme::cardHeader);
        g.fillRoundedRectangle (headerRect, corner);
        g.fillRect (headerRect.getX(), headerRect.getY() + 10.0f, headerRect.getWidth(), 13.0f);

        if (colorName != "grey")
        {
            g.setColour (accent.withAlpha (0.22f));
            g.fillRoundedRectangle (headerRect, corner);
            g.fillRect (headerRect.getX(), headerRect.getY() + 10.0f, headerRect.getWidth(), 13.0f);

            // Vibrant top accent rim line
            g.setColour (accent);
            g.drawLine (bounds.getX() + corner, bounds.getY() + 1.0f, bounds.getRight() - corner, bounds.getY() + 1.0f, 2.0f);
        }

        // Hairline divider
        g.setColour (UITheme::strokeHairline);
        g.fillRect (bounds.getX(), bounds.getY() + 23.0f, bounds.getWidth(), 1.0f);

        // Apple Specular Keyline (Top rim reflection)
        g.setColour (UITheme::specularRim);
        g.drawLine (bounds.getX() + corner, bounds.getY() + 0.5f, bounds.getRight() - corner, bounds.getY() + 0.5f, 1.0f);

        // Apple Tactile Power Button with clear ON/OFF color status (Positioned at Top-Right away from pins)
        float pbSize = 13.0f;
        float pbX = bounds.getRight() - 17.0f;
        float pbY = bounds.getY() + 5.0f;

        // When ACTIVE: Luminous studio blue with soft glowing halo
        // When BYPASSED: Clear muted graphite
        juce::Colour powerColor = isBypassed ? juce::Colour (0xff52525a) : UITheme::appleBlue;

        if (! isBypassed)
        {
            // Luminous ambient glow behind active power icon
            g.setColour (UITheme::appleBlue.withAlpha (0.18f));
            g.fillEllipse (pbX - 2.0f, pbY - 2.0f, pbSize + 4.0f, pbSize + 4.0f);
        }
        else
        {
            // Subtle dark recessed ring when bypassed
            g.setColour (juce::Colour (0x25000000));
            g.fillEllipse (pbX - 2.0f, pbY - 2.0f, pbSize + 4.0f, pbSize + 4.0f);
        }

        g.setColour (powerColor);
        juce::Path powerArc;
        powerArc.addCentredArc (pbX + pbSize / 2, pbY + pbSize / 2, pbSize / 2 - 1.5f, pbSize / 2 - 1.5f,
                                0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath (powerArc, juce::PathStrokeType (1.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        g.drawLine (pbX + pbSize / 2, pbY + 1.0f, pbX + pbSize / 2, pbY + pbSize / 2 + 1.0f, 1.8f);

        // --- FL STUDIO STYLE MINI MIX DIAL (DRY/WET LEVEL) ---
        auto mixBounds = getMixDialBounds();
        float currentMix = 1.0f;
        if (graphNode != nullptr && graphNode->properties.contains ("mix"))
            currentMix = (float) graphNode->properties["mix"];

        // Dial background
        g.setColour (juce::Colour (0xff18181c));
        g.fillEllipse (mixBounds);
        g.setColour (UITheme::strokeHairline);
        g.drawEllipse (mixBounds, 0.8f);

        // Circular track arc (inactive portion)
        float dialCentreX = mixBounds.getCentreX();
        float dialCentreY = mixBounds.getCentreY();
        float dialRadius  = mixBounds.getWidth() * 0.5f - 1.5f;

        juce::Path trackArc;
        trackArc.addCentredArc (dialCentreX, dialCentreY, dialRadius, dialRadius, 0.0f,
                                -juce::MathConstants<float>::pi * 0.75f,
                                 juce::MathConstants<float>::pi * 0.75f, true);
        g.setColour (juce::Colour (0x28ffffff));
        g.strokePath (trackArc, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active mix arc (Apple Green)
        if (currentMix > 0.01f)
        {
            float startAngle = -juce::MathConstants<float>::pi * 0.75f;
            float sweepAngle = currentMix * (juce::MathConstants<float>::pi * 1.5f);
            juce::Path mixArc;
            mixArc.addCentredArc (dialCentreX, dialCentreY, dialRadius, dialRadius, 0.0f,
                                  startAngle, startAngle + sweepAngle, true);

            juce::Colour mixCol = isBypassed ? UITheme::textTertiary : UITheme::appleGreen;
            g.setColour (mixCol);
            g.strokePath (mixArc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Pointer dot
        float angle = -juce::MathConstants<float>::pi * 0.75f + currentMix * (juce::MathConstants<float>::pi * 1.5f);
        float pX = dialCentreX + std::sin (angle) * (dialRadius - 2.0f);
        float pY = dialCentreY - std::cos (angle) * (dialRadius - 2.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (pX - 1.0f, pY - 1.0f, 2.0f, 2.0f);

        // Title in Inter Typography (Left-anchored)
        g.setColour (isBypassed ? UITheme::textTertiary : UITheme::textPrimary);
        g.setFont (UITheme::getFont (11.0f, true));
        auto titleArea = bounds.withHeight (23.0f).withTrimmedLeft (isSplitterNode() || isJoinerNode() ? 12.0f : 32.0f).withTrimmedRight (76.0f);

        auto *liveProc = graphNode ? graphNode->getProcessor() : nullptr;
        if (auto* wrapper = dynamic_cast<MixWrapperProcessor*> (liveProc))
            liveProc = wrapper->getInnerProcessor();

        auto* contProc = dynamic_cast<ContainerModule*> (liveProc);

        juce::String translatedTitle = displayName;
        if (contProc != nullptr)              translatedTitle = contProc->getCustomLabel();
        else if (displayName == "Audio In")   translatedTitle = tr ("AUDIO_IN");
        else if (displayName == "Audio Out")  translatedTitle = tr ("AUDIO_OUT");
        else                                  translatedTitle = tr (displayName);
        
        g.drawText (translatedTitle, titleArea, juce::Justification::centredLeft);

        // Apple Frosted Pill Badge (DYN, FREQ, CLN, TONE, UTIL, CONT, SPLIT, JOIN or BYP)
        auto badgeArea = bounds.withHeight (23.0f).removeFromRight (74.0f).withTrimmedRight (38.0f).reduced (2.0f, 4.5f);
        g.setColour (juce::Colour (0x20ffffff));
        g.fillRoundedRectangle (badgeArea, 3.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (badgeArea, 3.0f, 1.0f);
        
        juce::String cat = ModuleFactory::instance().getCategory (displayName).toUpperCase();
        juce::String badgeText = "DSP";
        if (isSplitterNode())            badgeText = "SPLIT";
        else if (isJoinerNode())         badgeText = "JOIN";
        else if (contProc != nullptr)    badgeText = "RACK";
        else if (cat.contains ("DYN"))   badgeText = "DYN";
        else if (cat.contains ("FREQ"))  badgeText = "FREQ";
        else if (cat.contains ("CLEAN")) badgeText = "CLN";
        else if (cat.contains ("TONE"))  badgeText = "TONE";
        else if (cat.contains ("UTIL"))  badgeText = "UTIL";
        if (isBypassed) badgeText = "BYP";

        g.setColour (isBypassed ? UITheme::appleRed : (colorName != "grey" ? accent : UITheme::textSecondary));
        g.setFont (UITheme::getFont (8.5f));
        g.drawText (badgeText, badgeArea, juce::Justification::centred);

        // --- LOWER AREA: RECESSED DISPLAY SCREEN ---
        auto bodyBounds = bounds.withTrimmedTop (25.0f).withTrimmedBottom (6.0f).reduced (4.0f, 0.0f);
        g.setColour (UITheme::displayRecessed);
        g.fillRoundedRectangle (bodyBounds, 3.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (bodyBounds, 3.0f, 1.0f);

        if (auto* splitter = dynamic_cast<CrossoverSplitterModule*> (liveProc))
        {
            auto innerArea = bodyBounds.reduced (4.0f, 3.0f);
            float centerLeft  = innerArea.getX() + 28.0f;
            float centerRight = innerArea.getRight() - 40.0f;
            float centerWidth = centerRight - centerLeft;

            float rowH = (innerArea.getHeight() - 4.0f) / 3.0f;

            float liveLow  = splitter->getLiveLow();
            float liveMid  = splitter->getLiveMid();
            float liveHigh = splitter->getLiveHigh();

            auto drawSplitterRow = [&] (int rIdx, const juce::String& name, const juce::String& freqRange, juce::Colour col, float liveLvl)
            {
                auto rowRect = juce::Rectangle<float> (centerLeft, innerArea.getY() + (float) rIdx * (rowH + 2.0f), centerWidth, rowH);

                // Row background strip
                g.setColour (juce::Colour (0xff141417));
                g.fillRoundedRectangle (rowRect, 2.5f);
                g.setColour (col.withAlpha (0.25f));
                g.drawRoundedRectangle (rowRect, 2.5f, 0.7f);

                // Label
                g.setColour (isBypassed ? UITheme::textTertiary : col);
                g.setFont (UITheme::getFont (8.0f, true));
                g.drawText (name, rowRect.withTrimmedLeft (5.0f).withTrimmedRight (centerWidth * 0.5f), juce::Justification::centredLeft);

                // Cutoff / Range text
                g.setColour (isBypassed ? UITheme::textTertiary : UITheme::textSecondary);
                g.setFont (UITheme::getFont (7.5f));
                g.drawText (freqRange, rowRect.withTrimmedRight (5.0f).withTrimmedLeft (centerWidth * 0.40f), juce::Justification::centredRight);

                // Mini level indicator line at bottom of row
                if (! isBypassed && liveLvl > 0.002f)
                {
                    float norm = juce::jlimit (0.0f, 1.0f, liveLvl * 1.3f);
                    auto meterLine = rowRect.removeFromBottom (2.0f).reduced (2.0f, 0.0f);
                    g.setColour (col.withAlpha (0.90f));
                    g.fillRoundedRectangle (meterLine.withWidth (meterLine.getWidth() * norm), 1.0f);
                }
            };

            auto* pLowMid  = splitter->getAPVTS().getRawParameterValue ("lowMidFreq");
            auto* pMidHigh = splitter->getAPVTS().getRawParameterValue ("midHighFreq");
            float fLowMid  = pLowMid ? pLowMid->load() : 250.0f;
            float fMidHigh = pMidHigh ? pMidHigh->load() : 3500.0f;

            juce::String sLowMid  = fLowMid >= 1000.0f ? (juce::String (fLowMid / 1000.0f, 1) + "k") : (juce::String ((int) fLowMid) + " Hz");
            juce::String sMidHigh = fMidHigh >= 1000.0f ? (juce::String (fMidHigh / 1000.0f, 1) + "k") : (juce::String ((int) fMidHigh) + " Hz");

            drawSplitterRow (0, "LOW",  "< " + sLowMid,            UITheme::applePurple, liveLow);
            drawSplitterRow (1, "MID",  sLowMid + " - " + sMidHigh, UITheme::appleGreen,  liveMid);
            drawSplitterRow (2, "HIGH", "> " + sMidHigh,           UITheme::appleCyan,   liveHigh);
        }
        else if (auto* joiner = dynamic_cast<CrossoverJoinerModule*> (liveProc))
        {
            auto innerArea = bodyBounds.reduced (4.0f, 3.0f);
            float centerLeft  = innerArea.getX() + 40.0f;
            float centerRight = innerArea.getRight() - 28.0f;
            float centerWidth = centerRight - centerLeft;

            float rowH = (innerArea.getHeight() - 4.0f) / 3.0f;

            float liveLow  = joiner->getLiveLow();
            float liveMid  = joiner->getLiveMid();
            float liveHigh = joiner->getLiveHigh();

            auto drawJoinerRow = [&] (int rIdx, const juce::String& name, const juce::String& gainStr, juce::Colour col, float liveLvl)
            {
                auto rowRect = juce::Rectangle<float> (centerLeft, innerArea.getY() + (float) rIdx * (rowH + 2.0f), centerWidth, rowH);

                // Row background strip
                g.setColour (juce::Colour (0xff141417));
                g.fillRoundedRectangle (rowRect, 2.5f);
                g.setColour (col.withAlpha (0.25f));
                g.drawRoundedRectangle (rowRect, 2.5f, 0.7f);

                // Label
                g.setColour (isBypassed ? UITheme::textTertiary : col);
                g.setFont (UITheme::getFont (8.0f, true));
                g.drawText (name, rowRect.withTrimmedLeft (5.0f).withTrimmedRight (centerWidth * 0.5f), juce::Justification::centredLeft);

                // Gain dB text
                g.setColour (isBypassed ? UITheme::textTertiary : UITheme::textSecondary);
                g.setFont (UITheme::getFont (7.5f));
                g.drawText (gainStr, rowRect.withTrimmedRight (5.0f).withTrimmedLeft (centerWidth * 0.40f), juce::Justification::centredRight);

                // Mini level indicator line at bottom of row
                if (! isBypassed && liveLvl > 0.002f)
                {
                    float norm = juce::jlimit (0.0f, 1.0f, liveLvl * 1.3f);
                    auto meterLine = rowRect.removeFromBottom (2.0f).reduced (2.0f, 0.0f);
                    g.setColour (col.withAlpha (0.90f));
                    g.fillRoundedRectangle (meterLine.withWidth (meterLine.getWidth() * norm), 1.0f);
                }
            };

            auto* pLowGain  = joiner->getAPVTS().getRawParameterValue ("lowGain");
            auto* pMidGain  = joiner->getAPVTS().getRawParameterValue ("midGain");
            auto* pHighGain = joiner->getAPVTS().getRawParameterValue ("highGain");

            float gLow  = pLowGain ? pLowGain->load() : 0.0f;
            float gMid  = pMidGain ? pMidGain->load() : 0.0f;
            float gHigh = pHighGain ? pHighGain->load() : 0.0f;

            auto formatDb = [] (float val) -> juce::String {
                return (val >= 0.0f ? "+" : "") + juce::String (val, 1) + " dB";
            };

            drawJoinerRow (0, "LOW",  formatDb (gLow),  UITheme::applePurple, liveLow);
            drawJoinerRow (1, "MID",  formatDb (gMid),  UITheme::appleGreen,  liveMid);
            drawJoinerRow (2, "HIGH", formatDb (gHigh), UITheme::appleCyan,   liveHigh);
        }
        else if (contProc != nullptr)
        {
            g.setColour (UITheme::appleBlue);
            g.setFont (UITheme::getFont (9.5f, true));
            int numStacked = contProc->getNumInnerModules();
            juce::String stackText = juce::String (numStacked) + (numStacked == 1 ? " Chained Module" : " Chained Modules");
            g.drawText (stackText, bodyBounds.withTrimmedBottom (14.0f), juce::Justification::centred);

            g.setColour (UITheme::textTertiary);
            g.setFont (UITheme::getFont (8.5f));
            g.drawText ("Double-click to open Rack", bodyBounds.withTrimmedTop (16.0f), juce::Justification::centred);
        }
        else if (auto* eqProc = dynamic_cast<ThreeBandEQModule*> (liveProc))
        {
            // Mini 3-Band EQ Display (Low, Mid, High columns with gain values)
            auto* pLowGain  = eqProc->getAPVTS().getRawParameterValue ("lowGain");
            auto* pMidGain  = eqProc->getAPVTS().getRawParameterValue ("midGain");
            auto* pHighGain = eqProc->getAPVTS().getRawParameterValue ("highGain");

            float gLow  = pLowGain  ? pLowGain->load()  : 0.0f;
            float gMid  = pMidGain  ? pMidGain->load()  : 0.0f;
            float gHigh = pHighGain ? pHighGain->load() : 0.0f;

            auto drawBandCol = [&] (int colIdx, const juce::String& name, float gainDb, juce::Colour col)
            {
                float colW = (bodyBounds.getWidth() - 8.0f) / 3.0f;
                auto colRect = juce::Rectangle<float> (bodyBounds.getX() + 4.0f + (float) colIdx * colW,
                                                       bodyBounds.getY() + 1.0f,
                                                       colW - 3.0f,
                                                       bodyBounds.getHeight() - 2.0f);

                g.setColour (juce::Colour (0xff141417));
                g.fillRoundedRectangle (colRect, 2.5f);
                g.setColour (col.withAlpha (0.22f));
                g.drawRoundedRectangle (colRect, 2.5f, 0.7f);

                // Name
                g.setColour (isBypassed ? UITheme::textTertiary : col);
                g.setFont (UITheme::getFont (7.5f, true));
                g.drawText (name, colRect.removeFromTop (12.0f), juce::Justification::centred);

                // Gain readout
                juce::String valStr = (gainDb >= 0.0f ? "+" : "") + juce::String (gainDb, 1);
                g.setColour (isBypassed ? UITheme::textTertiary : UITheme::textPrimary);
                g.setFont (UITheme::getFont (7.5f));
                g.drawText (valStr, colRect.removeFromBottom (11.0f), juce::Justification::centred);

                // Vertical mini gain fill
                float zeroY = colRect.getCentreY();
                float normGain = juce::jlimit (-1.0f, 1.0f, gainDb / 15.0f);
                float barH = (colRect.getHeight() * 0.45f) * std::abs (normGain);

                if (barH > 0.5f)
                {
                    g.setColour (isBypassed ? UITheme::textTertiary : col.withAlpha (0.85f));
                    if (normGain > 0.0f)
                        g.fillRoundedRectangle (colRect.getX() + 3.0f, zeroY - barH, colRect.getWidth() - 6.0f, barH, 1.0f);
                    else
                        g.fillRoundedRectangle (colRect.getX() + 3.0f, zeroY, colRect.getWidth() - 6.0f, barH, 1.0f);
                }

                // Center zero indicator
                g.setColour (juce::Colour (0x40ffffff));
                g.drawHorizontalLine ((int) zeroY, colRect.getX() + 2.0f, colRect.getRight() - 2.0f);
            };

            drawBandCol (0, "LOW",  gLow,  UITheme::applePurple);
            drawBandCol (1, "MID",  gMid,  UITheme::appleGreen);
            drawBandCol (2, "HIGH", gHigh, UITheme::appleCyan);
        }
        else if (auto* spatialProc = dynamic_cast<Spatial3DModule*> (liveProc))
        {
            // Mini 3D Radar Compass on canvas node
            auto radarRect = bodyBounds.reduced (4.0f, 2.0f);
            float rcx = radarRect.getX() + 24.0f;
            float rcy = radarRect.getCentreY();
            float radRadius = 16.0f;

            // Radar background circle
            g.setColour (juce::Colour (0xff101016));
            g.fillEllipse (rcx - radRadius, rcy - radRadius, radRadius * 2.0f, radRadius * 2.0f);
            g.setColour (UITheme::appleBlue.withAlpha (0.40f));
            g.drawEllipse (rcx - radRadius, rcy - radRadius, radRadius * 2.0f, radRadius * 2.0f, 1.0f);
            g.drawEllipse (rcx - radRadius * 0.5f, rcy - radRadius * 0.5f, radRadius, radRadius, 0.7f);

            // Crosshairs
            g.setColour (juce::Colour (0x22ffffff));
            g.drawHorizontalLine ((int) rcy, rcx - radRadius, rcx + radRadius);
            g.drawVerticalLine ((int) rcx, rcy - radRadius, rcy + radRadius);

            // Center Listener
            g.setColour (UITheme::appleBlue);
            g.fillEllipse (rcx - 2.5f, rcy - 2.5f, 5.0f, 5.0f);

            // Live Emitter Position
            auto pos = spatialProc->getCurrentEmitterPos();
            float eScreenX = rcx + juce::jlimit (-radRadius + 2.0f, radRadius - 2.0f, pos.x * (radRadius * 0.75f));
            float eScreenY = rcy - juce::jlimit (-radRadius + 2.0f, radRadius - 2.0f, pos.y * (radRadius * 0.75f));

            g.setColour (UITheme::appleCyan.withAlpha (0.50f));
            g.drawLine (rcx, rcy, eScreenX, eScreenY, 1.0f);

            g.setColour (UITheme::appleCyan);
            g.fillEllipse (eScreenX - 3.0f, eScreenY - 3.0f, 6.0f, 6.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (eScreenX - 1.0f, eScreenY - 1.0f, 2.0f, 2.0f);

            // Text Info on right
            auto infoRect = radarRect.withTrimmedLeft (50.0f);
            g.setColour (isBypassed ? UITheme::textTertiary : UITheme::textPrimary);
            g.setFont (UITheme::getFont (8.5f, true));
            g.drawText ("3D SPATIAL REALM", infoRect.removeFromTop (14.0f), juce::Justification::centredLeft);

            g.setColour (UITheme::textSecondary);
            g.setFont (UITheme::getFont (8.0f));
            juce::String posStr = "X: " + juce::String (pos.x, 2) + "  Y: " + juce::String (pos.y, 2) + "  Z: " + juce::String (pos.z, 2);
            g.drawText (posStr, infoRect, juce::Justification::centredLeft);
        }
        else
        {
            static const juce::Array<juce::AudioProcessorParameter *> dummyParams;
            auto &params = liveProc ? liveProc->getParameters() : dummyParams;

            if (params.isEmpty())
            {
                g.setColour (UITheme::textTertiary);
                g.setFont (UITheme::getFont (9.0f));
                g.drawText (isBypassed ? "Bypassed" : "Active Processing", bodyBounds, juce::Justification::centred);
            }
            else
            {
                int rowsToShow = juce::jmin (2, params.size());
                float rowHeight = bodyBounds.getHeight() / (float) rowsToShow;

                for (int i = 0; i < rowsToShow; ++i)
                {
                    auto* param = params[i];
                    float verticalReduction = rowsToShow == 1 ? 3.0f : 1.0f;
                    auto row = bodyBounds.removeFromTop (rowHeight).reduced (6.0f, verticalReduction);

                    float norm = param->getValue(); // 0..1
                    auto barTrack = row.removeFromBottom (rowsToShow == 1 ? 5.0f : 3.5f);

                    // Track Background
                    g.setColour (juce::Colour (0xff141416));
                    g.fillRoundedRectangle (barTrack, 1.5f);

                    if (norm > 0.003f)
                    {
                        // Apple System Blue or System Green level bar
                        juce::Colour barCol = isBypassed ? UITheme::textTertiary : UITheme::appleGreen;
                        g.setColour (barCol);
                        float activeW = barTrack.getWidth() * norm;
                        g.fillRoundedRectangle (barTrack.withWidth (activeW), 1.5f);

                        // Segment cuts
                        g.setColour (juce::Colour (0x30000000));
                        for (float sx = barTrack.getX() + 4.0f; sx < barTrack.getX() + activeW; sx += 4.0f)
                            g.drawVerticalLine ((int) sx, barTrack.getY(), barTrack.getBottom());
                    }

                    // Parameter name on left, value on right in Inter font
                    g.setColour (isBypassed ? UITheme::textTertiary : UITheme::textSecondary);
                    g.setFont (UITheme::getFont (8.5f));
                    g.drawText (param->getName (14), row, juce::Justification::centredLeft);

                    g.setColour (isBypassed ? UITheme::textTertiary : UITheme::textPrimary);
                    g.drawText (param->getCurrentValueAsText(), row, juce::Justification::centredRight);
                }
            }
        }

        // Outer keyline border
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    // Apple Selection Focus Ring & Container Drop Target Feedback
    if (canvas.getContainerDropTarget() == nodeID)
    {
        g.setColour (juce::Colour (0xfff59e0b));
        g.drawRoundedRectangle (bounds.expanded (3.0f), corner + 3.0f, 2.5f);

        auto bannerRect = bounds.reduced (4.0f, 4.0f);
        g.setColour (juce::Colour (0xee18181b));
        g.fillRoundedRectangle (bannerRect, 3.0f);
        g.setColour (juce::Colour (0xfff59e0b));
        g.drawRoundedRectangle (bannerRect, 3.0f, 1.2f);
        g.setFont (UITheme::getFont (9.0f, true));
        bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;
        g.drawText (isTr ? juce::String (juce::CharPointer_UTF8 ("+ PAKETE DAH\xc4\xb0L ET")) : "+ INSERT INTO CONTAINER",
                    bannerRect, juce::Justification::centred);
    }
    else if (canvas.isNodeSelected (nodeID))
    {
        g.setColour (UITheme::appleBlue);
        g.drawRoundedRectangle (bounds.expanded (2.0f), corner + 2.0f, 1.8f);
    }

    if (canvas.getDraggedOverNodeID() == nodeID)
    {
        g.setColour (juce::Colour (0xfff59e0b));
        g.drawRoundedRectangle (bounds.expanded (2.0f), corner + 2.0f, 2.0f);
    }

    // --- 3. PINLER ---
    auto drawPin = [&](bool isInput, int pinIdx)
    {
        int chan = pinIndexToChannel (isInput, pinIdx);
        auto pinCenter = getPinPosition (isInput, chan) - getPosition().toFloat();
        const float socketSize = 10.0f;
        juce::Rectangle<float> socket(socketSize, socketSize);
        socket.setCentre(pinCenter);

        bool connected = canvas.isPinConnected(nodeID, isInput, chan);

        juce::Colour portColor = getPinColour (isInput, pinIdx);

        // Hardware Socket Bezel (Sleek Studio Titanium / Neutral Metal)
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (socket.translated (0.0f, 1.0f));

        // Metallic Bezel Ring
        g.setColour (juce::Colour (0xff222228));
        g.fillEllipse (socket);

        // Illuminated Bezel Ring
        g.setColour (connected ? portColor : portColor.withAlpha (0.55f));
        g.drawEllipse (socket, 1.2f);

        // Top specular rim
        g.setColour (juce::Colour (0x50ffffff));
        g.drawEllipse (socket.reduced (0.5f), 0.6f);

        // Deep Recessed Jack Barrel Hole
        auto barrel = socket.reduced (2.4f);
        g.setColour (juce::Colour (0xff0a0a0d));
        g.fillEllipse (barrel);

        if (connected)
        {
            auto plugCore = barrel.reduced (0.4f);
            g.setColour (portColor);
            g.fillEllipse (plugCore);

            g.setColour (juce::Colours::white.withAlpha (0.90f));
            g.fillEllipse (plugCore.getX() + 1.0f, plugCore.getY() + 1.0f, 1.5f, 1.5f);
        }
        else
        {
            g.setColour (portColor.withAlpha (0.80f));
            g.fillEllipse (barrel.getCentreX() - 1.0f, barrel.getCentreY() - 1.0f, 2.0f, 2.0f);
        }

        // Micro Frosted Pill Badge ("IN", "OUT", "LOW", "MID", "HIGH")
        juce::String pinText = getPinName (isInput, pinIdx);
        float badgeWidth = pinText.length() > 2 ? 26.0f : (isInput ? 16.0f : 21.0f);
        auto badgeRect = isInput 
            ? juce::Rectangle<float> (pinCenter.x + 5.0f, pinCenter.y - 5.0f, badgeWidth, 10.0f)
            : juce::Rectangle<float> (pinCenter.x - (badgeWidth + 5.0f), pinCenter.y - 5.0f, badgeWidth, 10.0f);

        g.setColour (portColor.withAlpha (0.18f));
        g.fillRoundedRectangle (badgeRect, 2.5f);
        g.setColour (portColor.withAlpha (0.50f));
        g.drawRoundedRectangle (badgeRect, 2.5f, 0.8f);

        g.setColour (portColor);
        g.setFont (UITheme::getFont (7.0f, true));
        g.drawText (pinText, badgeRect, juce::Justification::centred);
    };

    int numIn = getNumPins (true);
    for (int p = 0; p < numIn; ++p)
        drawPin (true, p);

    int numOut = getNumPins (false);
    for (int p = 0; p < numOut; ++p)
        drawPin (false, p);
}

juce::Rectangle<int> NodeComponent::getRepaintBounds() const
{
    return getBounds().expanded(24);
}

int NodeComponent::hitTestPin(juce::Point<int> localPos, bool &isInput) const
{
    auto worldPos = localPos.toFloat() + getPosition().toFloat();
    int numIn = getNumPins (true);
    for (int p = 0; p < numIn; ++p)
    {
        int chan = pinIndexToChannel (true, p);
        if (getPinPosition(true, chan).getDistanceFrom(worldPos) < pinHitRadius)
        {
            isInput = true;
            return p;
        }
    }
    int numOut = getNumPins (false);
    for (int p = 0; p < numOut; ++p)
    {
        int chan = pinIndexToChannel (false, p);
        if (getPinPosition(false, chan).getDistanceFrom(worldPos) < pinHitRadius)
        {
            isInput = false;
            return p;
        }
    }
    return -1;
}

void NodeComponent::mouseDown(const juce::MouseEvent &e)
{
    bool isInput = false;

    // --- YENİ EKLENEN: POWER BUTONU TIKLAMA KONTROLÜ ---
    if (!systemNode)
    {
        auto bounds = getLocalBounds().toFloat().reduced (3.0f, 6.0f);
        // Butonun bulunduğu koordinatları kapsayan görünmez bir tıklama alanı (Hitbox - Sağ Üst)
        juce::Rectangle<float> powerButtonArea (bounds.getRight() - 20.0f, bounds.getY(), 20.0f, 23.0f);

        if (powerButtonArea.contains(e.getPosition().toFloat()) && e.mods.isLeftButtonDown())
        {
            if (auto *gNode = canvas.getGraphNode(nodeID))
            {
                gNode->setBypassed(!gNode->isBypassed());
                repaint(); // Rengi anında güncelle
            }
            return; // İşlemi burada kes, kablo sürüklemeyi tetikleme
        }

        // --- FL STUDIO STYLE MINI MIX DIAL ---
        auto mixBounds = getMixDialBounds();
        if (mixBounds.expanded (3.0f).contains (e.getPosition().toFloat()) && e.mods.isLeftButtonDown())
        {
            isDraggingMix = true;
            dragStartPos = e.getPosition();
            if (auto* gNode = canvas.getGraphNode (nodeID))
            {
                dragStartMix = 1.0f;
                if (gNode->properties.contains ("mix"))
                    dragStartMix = (float) gNode->properties["mix"];
            }
            return;
        }
    }

    int hitPinIdx = hitTestPin (e.getPosition(), isInput);
    if (hitPinIdx >= 0)
    {
        int chan = pinIndexToChannel (isInput, hitPinIdx);
        if (e.mods.isRightButtonDown())
        {
            rightClickCandidate = true;
        }
        else
        {
            // If already connected, unplug this end and drag it to replug
            if (! canvas.startReplugCable (*this, isInput, chan))
            {
                canvas.beginCableDrag (*this, isInput, chan);
            }
        }
        return;
    }

    // Node sağ tık adayı
    if (e.mods.isRightButtonDown())
    {
        rightClickCandidate = true;
        return;
    }

    toFront(true);
    if (e.mods.isShiftDown())
    {
        canvas.toggleNodeSelection (nodeID);
    }
    else
    {
        if (! canvas.isNodeSelected (nodeID))
            canvas.addSelectedNode (nodeID, true);
    }
    dragger.startDraggingComponent(this, e);
}

void NodeComponent::mouseDrag(const juce::MouseEvent &e)
{
    if (isDraggingMix)
    {
        int deltaY = dragStartPos.y - e.getPosition().y;
        float newMix = juce::jlimit (0.0f, 1.0f, dragStartMix + (float) deltaY / 100.0f);
        if (auto* gNode = canvas.getGraphNode (nodeID))
        {
            gNode->properties.set ("mix", newMix);
            if (auto* wrapper = dynamic_cast<MixWrapperProcessor*> (gNode->getProcessor()))
                wrapper->setMixLevel (newMix);
        }
        repaint();
        return;
    }

    if (canvas.isDraggingCable())
    {
        canvas.updateCableDrag(e.getEventRelativeTo(&canvas).getPosition());
        return;
    }

    auto oldPos = getPosition();
    auto oldBounds = getRepaintBounds();
    dragger.dragComponent(this, e, nullptr);

    if (canvas.isSnapToGrid())
    {
        constexpr int grid = NodeCanvas::snapGridSize;
        int gx = (int)std::lround((float)getX() / (float)grid) * grid;
        int gy = (int)std::lround((float)getY() / (float)grid) * grid;
        setTopLeftPosition(gx, gy);
    }

    auto delta = getPosition() - oldPos;

    // Multi-selection: synchronously drag all other selected nodes by the same delta!
    if (canvas.getSelectedNodeIDs().size() > 1 && (delta.x != 0 || delta.y != 0))
    {
        for (auto selID : canvas.getSelectedNodeIDs())
        {
            if (selID != nodeID)
            {
                if (auto* other = canvas.findComponentFor (selID))
                {
                    auto oldOtherBounds = other->getRepaintBounds();
                    other->setTopLeftPosition (other->getPosition() + delta);
                    canvas.notifyNodeMoved (*other, oldOtherBounds);
                }
            }
        }
    }

    canvas.notifyNodeMoved(*this, oldBounds);

    // Dynamic Container Drag-Over Detection
    if (! systemNode && ! isContainerNode())
    {
        auto myBounds = getBounds();
        juce::AudioProcessorGraph::NodeID targetContainerID;

        for (int i = 0; i < canvas.getNumChildComponents(); ++i)
        {
            if (auto* otherNode = dynamic_cast<NodeComponent*> (canvas.getChildComponent (i)))
            {
                if (otherNode != this && otherNode->isContainerNode())
                {
                    if (otherNode->getBounds().intersects (myBounds))
                    {
                        targetContainerID = otherNode->getNodeID();
                        break;
                    }
                }
            }
        }

        canvas.setContainerDropTarget (targetContainerID, targetContainerID.uid != 0 ? nodeID : juce::AudioProcessorGraph::NodeID());
    }
}

bool NodeComponent::isContainerNode() const
{
    if (auto* gNode = canvas.getGraphNode (nodeID))
    {
        auto* liveProc = gNode->getProcessor();
        return dynamic_cast<ContainerModule*> (MixWrapperProcessor::getActualProcessor (liveProc)) != nullptr;
    }
    return false;
}

bool NodeComponent::isSplitterNode() const
{
    if (displayName.containsIgnoreCase ("Splitter") || displayName == "Frequency Splitter" || displayName == "Crossover Splitter")
        return true;
    if (auto* gNode = canvas.getGraphNode (nodeID))
    {
        auto type = gNode->properties["type"].toString();
        return type.containsIgnoreCase ("Splitter") || type == "Frequency Splitter" || type == "Crossover Splitter";
    }
    return false;
}

bool NodeComponent::isJoinerNode() const
{
    if (displayName.containsIgnoreCase ("Joiner") || displayName == "Frequency Joiner" || displayName == "Crossover Joiner")
        return true;
    if (auto* gNode = canvas.getGraphNode (nodeID))
    {
        auto type = gNode->properties["type"].toString();
        return type.containsIgnoreCase ("Joiner") || type == "Frequency Joiner" || type == "Crossover Joiner";
    }
    return false;
}

int NodeComponent::getNumPins (bool isInput) const
{
    if (systemNode)
    {
        if (isInput && !showInputPin) return 0;
        if (!isInput && !showOutputPin) return 0;
        return 1;
    }
    if (isSplitterNode())
        return isInput ? 1 : 3;
    if (isJoinerNode())
        return isInput ? 3 : 1;

    return 1;
}

juce::String NodeComponent::getPinName (bool isInput, int pinIndex) const
{
    if (isSplitterNode() && !isInput)
    {
        if (pinIndex == 0) return "LOW";
        if (pinIndex == 1) return "MID";
        if (pinIndex == 2) return "HIGH";
    }
    if (isJoinerNode() && isInput)
    {
        if (pinIndex == 0) return "LOW";
        if (pinIndex == 1) return "MID";
        if (pinIndex == 2) return "HIGH";
    }
    return isInput ? "IN" : "OUT";
}

juce::Colour NodeComponent::getPinColour (bool isInput, int pinIndex) const
{
    if (isSplitterNode() && !isInput)
    {
        if (pinIndex == 0) return UITheme::applePurple;
        if (pinIndex == 1) return UITheme::appleGreen;
        if (pinIndex == 2) return UITheme::appleCyan;
    }
    if (isJoinerNode() && isInput)
    {
        if (pinIndex == 0) return UITheme::applePurple;
        if (pinIndex == 1) return UITheme::appleGreen;
        if (pinIndex == 2) return UITheme::appleCyan;
    }
    return UITheme::appleBlue;
}

int NodeComponent::pinIndexToChannel (bool isInput, int pinIndex) const
{
    if (isSplitterNode() && !isInput)
        return pinIndex * 2;
    if (isJoinerNode() && isInput)
        return pinIndex * 2;
    return 0;
}

int NodeComponent::channelToPinIndex (bool isInput, int channel) const
{
    if (isSplitterNode() && !isInput)
    {
        if (channel >= 4) return 2;
        if (channel == 2 || channel == 3) return 1;
        return 0;
    }
    if (isJoinerNode() && isInput)
    {
        if (channel >= 4) return 2;
        if (channel == 2 || channel == 3) return 1;
        return 0;
    }
    return 0;
}

void NodeComponent::mouseUp(const juce::MouseEvent &e)
{
    if (canvas.getContainerDropTarget().uid != 0 && canvas.getDraggedOverNodeID() == nodeID)
    {
        auto targetID = canvas.getContainerDropTarget();
        canvas.setContainerDropTarget ({}, {});
        canvas.adoptNodeIntoContainer (nodeID, targetID);
        return;
    }

    if (isDraggingMix)
    {
        isDraggingMix = false;
        return;
    }

    if (rightClickCandidate || e.mods.isPopupMenu())
    {
        bool wasRightClick = rightClickCandidate;
        rightClickCandidate = false;

        if (wasRightClick && e.getDistanceFromDragStart() < 6)
        {
            bool isInputPin = false;
            int hitPin = hitTestPin (e.getPosition(), isInputPin);
            if (hitPin >= 0)
            {
                int chan = pinIndexToChannel (isInputPin, hitPin);
                canvas.disconnectPin (nodeID, isInputPin, chan);
            }
            else
            {
                canvas.showNodeContextMenu (*this);
            }
            return;
        }
    }

    if (canvas.isDraggingCable())
    {
        auto pos = e.getEventRelativeTo(&canvas).getPosition();
        canvas.endCableDrag(pos);
    }
}
void NodeComponent::openEditorWindow()
{
    if (auto node = canvas.getGraphNode(nodeID))
    {
        if (auto *proc = MixWrapperProcessor::getActualProcessor (node->getProcessor()))
        {
            if (auto* cont = dynamic_cast<ContainerModule*> (proc))
            {
                cont->onExtractRequested = [this, nId = nodeID] (int idx) {
                    canvas.extractModuleFromContainer (nId, idx);
                };
            }

            if (proc->hasEditor())
            {
                ModuleEditorWindow::openForProcessor (proc);
            }
        }
    }
}

void NodeComponent::mouseDoubleClick(const juce::MouseEvent &e)
{
    if (!systemNode && getMixDialBounds().expanded (3.0f).contains (e.getPosition().toFloat()))
    {
        // Double-click resets mix level to 100% (FL Studio behavior)
        if (auto* gNode = canvas.getGraphNode (nodeID))
        {
            gNode->properties.set ("mix", 1.0f);
            if (auto* wrapper = dynamic_cast<MixWrapperProcessor*> (gNode->getProcessor()))
                wrapper->setMixLevel (1.0f);
        }
        repaint();
        return;
    }

    bool isInput;
    if (hitTestPin(e.getPosition(), isInput) < 0)
    {
        openEditorWindow();
    }
}

juce::Point<float> NodeComponent::getPinPosition(bool isInput, int channelOrPinIndex) const
{
    auto b = getLocalBounds().toFloat().reduced (3.0f, 6.0f);

    if (isSplitterNode())
    {
        if (!isInput)
        {
            int idx = channelToPinIndex (false, channelOrPinIndex);
            float x = b.getRight() - 9.0f;
            float y = b.getY() + 38.0f + (float)idx * 24.0f;
            return getPosition().toFloat() + juce::Point<float>(x, y);
        }
        else
        {
            float x = b.getX() + 9.0f;
            float y = b.getY() + 62.0f;
            return getPosition().toFloat() + juce::Point<float>(x, y);
        }
    }

    if (isJoinerNode())
    {
        if (isInput)
        {
            int idx = channelToPinIndex (true, channelOrPinIndex);
            float x = b.getX() + 9.0f;
            float y = b.getY() + 38.0f + (float)idx * 24.0f;
            return getPosition().toFloat() + juce::Point<float>(x, y);
        }
        else
        {
            float x = b.getRight() - 9.0f;
            float y = b.getY() + 62.0f;
            return getPosition().toFloat() + juce::Point<float>(x, y);
        }
    }

    float x = isInput ? 12.0f : (float)getWidth() - 12.0f;
    float y = isInput ? 6.0f : (float)getHeight() - 6.0f;
    return getPosition().toFloat() + juce::Point<float>(x, y);
}