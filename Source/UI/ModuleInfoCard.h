#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../UITheme.h"
#include "../Localization.h"
#include "../Graph/ModuleFactory.h"
#include "SidebarComponent.h"

/**
    Adobe Photoshop / Logic Pro style floating Rich Module Info Panel.
    Displays in-depth DSP explanations, chain placement recommendations,
    parameter breakdowns, and an "Add to Canvas" shortcut.
*/
class ModuleInfoCard : public juce::Component
{
public:
    struct ModuleInfoData
    {
        juce::String type;
        juce::String category;
        juce::String tagline;
        juce::String overviewTr;
        juce::String overviewEn;
        juce::String placementTr;
        juce::String placementEn;
        juce::String tipsTr;
        juce::String tipsEn;
    };

    ModuleInfoCard (const juce::String& moduleType, 
                    std::function<void(const juce::String&)> onAddCallback,
                    std::function<void()> onCloseCallback)
        : currentType (moduleType), onAdd (std::move (onAddCallback)), onClose (std::move (onCloseCallback))
    {
        addAndMakeVisible (addBtn);
        addBtn.onClick = [this]
        {
            if (onAdd != nullptr)
                onAdd (currentType);
        };

        addAndMakeVisible (closeBtn);
        closeBtn.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };

        updateInfo (moduleType);
    }

    void updateInfo (const juce::String& type)
    {
        currentType = type;
        info = getInfoForType (type);
        updateLocalizedText();
        repaint();
    }

    void updateLocalizedText()
    {
        bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;
        addBtn.setButtonText (isTr ? juce::String (juce::CharPointer_UTF8 ("+ TUVALE EKLE")) : "+ ADD TO CANVAS");
        addBtn.setColour (juce::TextButton::buttonColourId, UITheme::appleBlue);
        addBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);

        closeBtn.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9c\x95"));
        closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0x33ffffff));
        closeBtn.setColour (juce::TextButton::textColourOffId, UITheme::textSecondary);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;

        // 1. Soft 3D Drop Shadow
        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 4.0f), 12.0f);

        // 2. Apple Dark Glass Card Background
        juce::ColourGradient bgGrad (juce::Colour (0xf4202026), 0, bounds.getY(),
                                     juce::Colour (0xf8141418), 0, bounds.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (bounds, 10.0f);

        // Card Border & Specular Rim
        g.setColour (juce::Colour (0x33ffffff));
        g.drawRoundedRectangle (bounds, 10.0f, 1.2f);
        g.setColour (UITheme::specularRim);
        g.drawLine (bounds.getX() + 10.0f, bounds.getY() + 1.0f, bounds.getRight() - 10.0f, bounds.getY() + 1.0f, 1.0f);

        // 3. Header Section (Icon, Title, Category Pill)
        float headerH = 56.0f;
        auto header = bounds.removeFromTop (headerH);

        // Header Background
        g.setColour (juce::Colour (0x22ffffff));
        g.fillRoundedRectangle (header, 10.0f);
        g.fillRect (header.getX(), header.getBottom() - 10.0f, header.getWidth(), 10.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawHorizontalLine ((int) header.getBottom(), header.getX(), header.getRight());

        // Header Icon Box
        auto iconBox = header.removeFromLeft (50.0f).reduced (10.0f, 13.0f);
        juce::Colour catCol = UITheme::appleBlue;
        if (info.category.equalsIgnoreCase ("Dynamics"))     catCol = juce::Colour (0xffa855f7);
        else if (info.category.equalsIgnoreCase ("Frequency"))catCol = juce::Colour (0xff06b6d4);
        else if (info.category.equalsIgnoreCase ("Cleanup"))  catCol = juce::Colour (0xff38bdf8);
        else if (info.category.equalsIgnoreCase ("Vocal Tone") || info.category.equalsIgnoreCase ("Radio Tone")) catCol = juce::Colour (0xffc084fc);

        LibraryItem::drawModuleIcon (g, info.type, iconBox, catCol);

        // Module Title & Category Tag
        auto titleArea = header.withTrimmedRight (36.0f).withTrimmedTop (8.0f);
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (14.0f, true));
        g.drawText (info.type, titleArea.removeFromTop (18.0f), juce::Justification::centredLeft);

        auto badgeArea = titleArea.removeFromTop (16.0f).withWidth (64.0f);
        g.setColour (catCol.withAlpha (0.25f));
        g.fillRoundedRectangle (badgeArea, 3.0f);
        g.setColour (catCol);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText (info.category.toUpperCase(), badgeArea, juce::Justification::centred);

        // Subtitle / Tagline on header right
        auto subArea = titleArea.withTrimmedLeft (70.0f);
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (10.0f));
        g.drawText (info.tagline, subArea, juce::Justification::centredLeft, true);

        // 4. Body Content (Structured Sections)
        auto content = bounds.reduced (14.0f, 10.0f).withTrimmedBottom (36.0f);

        auto drawSection = [&] (const juce::String& sectionTitle, const juce::String& bodyText, juce::Rectangle<float>& area)
        {
            if (area.getHeight() < 20.0f) return;

            // Section Header
            g.setColour (UITheme::appleBlue);
            g.setFont (UITheme::getFont (10.5f, true));
            g.drawText (sectionTitle, area.removeFromTop (16.0f), juce::Justification::centredLeft);

            // Body paragraph with word wrapping
            g.setColour (UITheme::textSecondary);
            g.setFont (UITheme::getFont (11.0f));
            juce::AttributedString as;
            as.append (bodyText, UITheme::getFont (11.0f), UITheme::textSecondary);
            as.setLineSpacing (1.25f);
            juce::TextLayout tl;
            tl.createLayout (as, area.getWidth());
            tl.draw (g, juce::Rectangle<float> (area.getX(), area.getY(), (float) area.getWidth(), tl.getHeight()));
            area.removeFromTop (tl.getHeight() + 8.0f);
        };

        // Section 1: Overview
        drawSection (isTr ? juce::String (juce::CharPointer_UTF8 ("NE \xc4\xb0\xc5\x9e\x45 YARAR?")) : "OVERVIEW",
                     isTr ? info.overviewTr : info.overviewEn, content);

        // Section 2: Chain Placement
        drawSection (isTr ? juce::String (juce::CharPointer_UTF8 ("S\xc4\xb0NYAL Z\xc4\xb0NC\xc4\xb0R\xc4\xb0NDEK\xc4\xb0 YER\xc4\xb0")) : "RECOMMENDED PLACEMENT",
                     isTr ? info.placementTr : info.placementEn, content);

        // Section 3: Pro Tips
        drawSection (isTr ? juce::String (juce::CharPointer_UTF8 ("\xc4\xb0PU\xc3\x87LARI VE KULLANIM")) : "PRO TIPS",
                     isTr ? info.tipsTr : info.tipsEn, content);
    }

    void resized() override
    {
        closeBtn.setBounds (getWidth() - 32, 12, 22, 22);
        addBtn.setBounds (14, getHeight() - 40, getWidth() - 28, 28);
    }

    static ModuleInfoData getInfoForType (const juce::String& type)
    {
        ModuleInfoData d;
        d.type = type;
        d.category = ModuleFactory::instance().getCategory (type);

        if (type.equalsIgnoreCase ("Gain"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Hassas Kazan\xc3" "\xa7" " ve Seviye Ayarlay\xc4" "\xb1" "c\xc4" "\xb1" ""));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Sinyalin ses seviyesini hassas dB kademeleriyle art\xc4" "\xb1" "ran veya azaltan saf kazan\xc3" "\xa7" " d\xc3" "\xbc" "\xc4" "\x9f" "\xc3" "\xbc" "m\xc3" "\xbc" "."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Pure linear gain trim module with polarity invert and smooth zero-crossing volume management."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Herhangi bir mod\xc3" "\xbc" "l\xc3" "\xbc" "n giri\xc5" "\x9f" "ini veya \xc3" "\xa7" "\xc4" "\xb1" "k\xc4" "\xb1" "\xc5" "\x9f" "\xc4" "\xb1" "n\xc4" "\xb1" " dengelemek (Gain Staging) i\xc3" "\xa7" "in zincirin her yerinde kullan\xc4" "\xb1" "labilir."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Use at any stage to balance levels between complex nonlinear DSP nodes."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Mod\xc3" "\xbc" "ller aras\xc4" "\xb1" "na ekleyerek bir sonraki efekti (\xc3" "\xb6" "zellikle kompres\xc3" "\xb6" "r ve doygunluk) ideal seviyede besleyin."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Maintain consistent dynamic headroom by trimming loud outputs before hitting character compressors."));
        }
        else if (type.equalsIgnoreCase ("Crossover Splitter") || type.equalsIgnoreCase ("Frequency Splitter"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("3 Bantl" "\xc4\xb1" " LR4 Mod" "\xc3\xbc" "ler Frekans B" "\xc3\xb6" "l" "\xc3\xbc" "c" "\xc3\xbc" " (1 IN -> 3 OUT)"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Giri" "\xc5\x9f" " sesini Linkwitz-Riley 4. Derece (24 dB/oct) faz do" "\xc4\x9f" "rulu" "\xc4\x9f" "uyla Low, Mid ve High olmak " "\xc3\xbc" "zere 3 ayr" "\xc4\xb1" " frekans jak" "\xc4\xb1" "na b" "\xc3\xb6" "ler. Her frekans band" "\xc4\xb1" "n" "\xc4\xb1" " ba" "\xc4\x9f" "\xc4\xb1" "ms" "\xc4\xb1" "z mod" "\xc3\xbc" "l zincirlerine y" "\xc3\xb6" "nlendirmenizi sa" "\xc4\x9f" "lar."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Splits incoming stereo audio into three discrete frequency bands (Low, Mid, High) via phase-perfect Linkwitz-Riley 4th order (24 dB/oct) crossover filters."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Paralel ve " "\xc3\xa7" "ok bantl" "\xc4\xb1" " (multiband) i" "\xc5\x9f" "leme zincirlerinin en ba" "\xc5\x9f" "\xc4\xb1" "nda kullan" "\xc4\xb1" "l" "\xc4\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Place at the beginning of modular multiband processing chains."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("LOW " "\xc3\xa7" "\xc4\xb1" "k" "\xc4\xb1" "\xc5\x9f" "\xc4\xb1" "n" "\xc4\xb1" " temiz b" "\xc4\xb1" "rak" "\xc4\xb1" "p, MID " "\xc3\xa7" "\xc4\xb1" "k" "\xc4\xb1" "\xc5\x9f" "\xc4\xb1" "na kompres" "\xc3\xb6" "r, HIGH " "\xc3\xa7" "\xc4\xb1" "k" "\xc4\xb1" "\xc5\x9f" "\xc4\xb1" "na Aural Exciter ba" "\xc4\x9f" "lay" "\xc4\xb1" "p ard" "\xc4\xb1" "ndan Frequency Joiner ile birle" "\xc5\x9f" "tirebilirsiniz."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Process LOW cleanly, compress MID, and excite HIGH separately before recombining with Frequency Joiner."));
        }
        else if (type.equalsIgnoreCase ("Crossover Joiner") || type.equalsIgnoreCase ("Frequency Joiner"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("3 Bantl" "\xc4\xb1" " LR4 Mod" "\xc3\xbc" "ler Frekans Birle" "\xc5\x9f" "tirici (3 IN -> 1 OUT)"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Frekans Ay" "\xc4\xb1" "r" "\xc4\xb1" "c" "\xc4\xb1" "'dan (Splitter) ayr" "\xc4\xb1" "lan veya farkl" "\xc4\xb1" " mod" "\xc3\xbc" "llerden ge" "\xc3\xa7" "en Low, Mid ve High frekans sinyallerini ba" "\xc4\x9f" "\xc4\xb1" "ms" "\xc4\xb1" "z kazan" "\xc3\xa7" " (Gain) ve sessize alma (Mute) denetimiyle tek bir ana stereo " "\xc3\xa7" "\xc4\xb1" "k" "\xc4\xb1" "\xc5\x9f" "ta m" "\xc3\xbc" "kemmel faz uyumuyla birle" "\xc5\x9f" "tirir."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Recombines discrete Low, Mid, and High frequency band signals into a unified stereo output with per-band gain trims and mute switches."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Frekans Ay" "\xc4\xb1" "r" "\xc4\xb1" "c" "\xc4\xb1" " ile ba" "\xc5\x9f" "layan " "\xc3\xa7" "ok bantl" "\xc4\xb1" " i" "\xc5\x9f" "leme zincirlerinin sonunda, ana " "\xc3\xa7" "\xc4\xb1" "k" "\xc4\xb1" "\xc5\x9f" "tan hemen " "\xc3\xb6" "nce yer al" "\xc4\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Place at the end of multiband subchains to sum the processed frequency bands."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Her band" "\xc4\xb1" "n seviyesini ayr" "\xc4\xb1" " ayr" "\xc4\xb1" " dengelerken, istenmeyen bir band" "\xc4\xb1" " tek t" "\xc4\xb1" "kla Mute butonundan kapatabilirsiniz."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Trim relative band levels or mute unwanted bands with a single click."));
        }
        else if (type.equalsIgnoreCase ("3-Band EQ") || type.equalsIgnoreCase ("Three-Band EQ"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("3 Bantl" "\xc4\xb1" " M" "\xc3\xbc" "zikal Ton Ekolayzeri"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("D" "\xc3\xbc" "\xc5\x9f" "\xc3\xbc" "k frekans (Low Shelf), orta frekans (Mid Peaking Bell) ve y" "\xc3\xbc" "ksek frekans (High Shelf) ile h" "\xc4\xb1" "zl" "\xc4\xb1" " ve m" "\xc3\xbc" "zikal ton " "\xc5\x9f" "ekillendirme sa" "\xc4\x9f" "lar. Canl" "\xc4\xb1" " frekans yan" "\xc4\xb1" "t e" "\xc4\x9f" "risi ve kazanc" "\xc4\xb1" " do" "\xc4\x9f" "rudan grafikten s" "\xc3\xbc" "r" "\xc3\xbc" "kleyerek ayarlama imkan" "\xc4\xb1" " sunar."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("3-band musical equalizer featuring Low Shelf, Mid Peaking Bell, and High Shelf with interactive visualizer curve."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Genel vokal veya enstr" "\xc3\xbc" "man ton dengesini h" "\xc4\xb1" "zl" "\xc4\xb1" "ca oturtmak i" "\xc3\xa7" "in zincirin herhangi bir noktas" "\xc4\xb1" "nda kullan" "\xc4\xb1" "labilir."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Use anywhere in the chain for swift, musical broad-stroke equalization."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Vokale s" "\xc4\xb1" "cakl" "\xc4\xb1" "k katmak i" "\xc3\xa7" "in Low band" "\xc4\xb1" "n" "\xc4\xb1" " hafif" "\xc3\xa7" "e y" "\xc3\xbc" "kseltin, burundan gelen kutu t" "\xc4\xb1" "n" "\xc4\xb1" "s" "\xc4\xb1" "n" "\xc4\xb1" " almak i" "\xc3\xa7" "in 800 Hz civar" "\xc4\xb1" "n" "\xc4\xb1" " k" "\xc4\xb1" "s" "\xc4\xb1" "n ve High band" "\xc4\xb1" "yla hava kat" "\xc4\xb1" "n."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Boost low shelf for warmth, dip mid bell around 800 Hz to remove boxiness, and lift high shelf for air."));
        }
        else if (type.equalsIgnoreCase ("Spatial 3D") || type.equalsIgnoreCase ("3D Spatializer") || type.equalsIgnoreCase ("Spatial Realm"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("3D Binaural Akustik Ses Uzay" "\xc4\xb1" " & " "\xc3\x87" "ok Bantl" "\xc4\xb1" " Panner"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Sesi 3 boyutlu sanal bir odada dinleyicinin (Listener) etraf" "\xc4\xb1" "nda 360 derece konumland" "\xc4\xb1" "ran yeni nesil binaural ses i" "\xc5\x9f" "lemcisi. Kulak kep" "\xc3\xa7" "esi (Pinna) y" "\xc3\xbc" "kseklik alg" "\xc4\xb1" "s" "\xc4\xb1" ", kafa g" "\xc3\xb6" "lgeleme (ILD), zaman gecikmesi (ITD), oda erken yans" "\xc4\xb1" "malar" "\xc4\xb1" " ve otomatik 3D y" "\xc3\xb6" "r" "\xc3\xbc" "nge (Orbit) hareketleri sunar. Sesi frekans bantlar" "\xc4\xb1" "na ay" "\xc4\xb1" "rarak Low, Mid ve High frekanslar" "\xc4\xb1" "n" "\xc4\xb1" " uzayda farkl" "\xc4\xb1" " noktalara da" "\xc4\x9f" "\xc4\xb1" "tabilirsiniz."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Next-generation 3D binaural spatializer placing audio sources in an interactive 3D acoustic room. Features HRTF pinna elevation filters, ITD/ILD, 6-surface early reflections, multiband frequency splitting, and 360-degree tempo-synced orbit automation."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Vokal katmanlar" "\xc4\xb1" "na, backing vokallere veya alan efektlerine (Reverb/Delay) 3 boyutlu sinematik derinlik ve hareket katmak i" "\xc3\xa7" "in zincirin sonlar" "\xc4\xb1" "nda kullan" "\xc4\xb1" "l" "\xc4\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Place downstream of spatial effects or on vocal double/backing tracks to create immersive 3D surround movement."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Multi-Band Spatial modunu se" "\xc3\xa7" "erek Low band" "\xc4\xb1" "n" "\xc4\xb1" " merkezde sabit tutun, High band" "\xc4\xb1" "n" "\xc4\xb1" " ise 360 Orbit modunda ba" "\xc5\x9f" "\xc4\xb1" "n" "\xc4\xb1" "z" "\xc4\xb1" "n etraf" "\xc4\xb1" "nda d" "\xc3\xb6" "nd" "\xc3\xbc" "rerek b" "\xc3\xbc" "y" "\xc3\xbc" "leyici bir sinematik etki yarat" "\xc4\xb1" "n."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Use Multi-Band Spatial mode to keep Low frequencies anchored in the center while orbiting High frequencies around the listener's head."));
        }
        else if (type.equalsIgnoreCase ("Compressor"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("St\xc3" "\xbc" "dyo S\xc4" "\xb1" "n\xc4" "\xb1" "f\xc4" "\xb1" " Vokal Dinamik Kompres\xc3" "\xb6" "r\xc3" "\xbc" ""));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Vokalin en k\xc4" "\xb1" "s\xc4" "\xb1" "k k\xc4" "\xb1" "s\xc4" "\xb1" "mlar\xc4" "\xb1" " ile en y\xc3" "\xbc" "ksek ba\xc4" "\x9f" "\xc4" "\xb1" "r\xc4" "\xb1" "\xc5" "\x9f" "lar\xc4" "\xb1" " aras\xc4" "\xb1" "ndaki ses fark\xc4" "\xb1" "n\xc4" "\xb1" " dengeleyerek vokalinizin mikste her an net ve \xc3" "\xb6" "nde duyulmas\xc4" "\xb1" "n\xc4" "\xb1" " sa\xc4" "\x9f" "lar."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("High-precision studio vocal compressor designed to clamp peaks and lift subtle body dynamics seamlessly."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Temizlik a\xc5" "\x9f" "amas\xc4" "\xb1" "ndan sonra, EQ veya doygunluk \xc3" "\xb6" "ncesinde yer al\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Position after noise cleanup and surgical EQ to stabilize vocal loudness."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("4:1 oran\xc4" "\xb1" "nda, 3-6 dB Gain Reduction yakalayacak \xc5" "\x9f" "ekilde Threshold ayarlayarak modern ve s\xc4" "\xb1" "k\xc4" "\xb1" " bir vokal elde edin."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Target 3-6 dB of gain reduction on average peaks with a medium attack for natural punch."));
        }
        else if (type.equalsIgnoreCase ("Upward Compressor"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Vokal OTT & D\xc3" "\xbc" "\xc5" "\x9f" "\xc3" "\xbc" "k Seviye Detay Y\xc3" "\xbc" "kseltici"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Vokaldeki f\xc4" "\xb1" "s\xc4" "\xb1" "lt\xc4" "\xb1" "lar\xc4" "\xb1" ", nefes ayr\xc4" "\xb1" "nt\xc4" "\xb1" "lar\xc4" "\xb1" "n\xc4" "\xb1" " ve al\xc3" "\xa7" "ak seviyeli duyulmayan detaylar\xc4" "\xb1" " yukar\xc4" "\xb1" " kald\xc4" "\xb1" "ran (Upward Compression) ve patlayan tepe noktalar\xc4" "\xb1" "n\xc4" "\xb1" " dizginleyen \xc3" "\xa7" "oklu dinamik i\xc5" "\x9f" "lemci."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Dual upward and downward compressor. Lifts low-level whispers and micro-details while clamping wild peaks."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("G\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" " temizleme ad\xc4" "\xb1" "mlar\xc4" "\xb1" "ndan sonra, ana vokal katman\xc4" "\xb1" "nda kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Insert after cleanup stages to bring forward hidden articulation and intimate breath presence."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Upward Amount ayar\xc4" "\xb1" "n\xc4" "\xb1" " a\xc3" "\xa7" "t\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" "n\xc4" "\xb1" "zda dip g\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" "s\xc3" "\xbc" "n\xc3" "\xbc" "n de y\xc3" "\xbc" "kselmemesi i\xc3" "\xa7" "in bu mod\xc3" "\xbc" "lden \xc3" "\xb6" "nce Noise Suppression veya Gate kullanman\xc4" "\xb1" "z \xc3" "\xb6" "nerilir."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Always pair with pre-cleanup (Noise Suppression) to avoid bringing up room background noise."));
        }
        else if (type.equalsIgnoreCase ("Limiter"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Tavan S\xc4" "\xb1" "n\xc4" "\xb1" "rlay\xc4" "\xb1" "c\xc4" "\xb1" " & Dijital K\xc4" "\xb1" "rp\xc4" "\xb1" "lma Engelleyici"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Sesin belirlenen tavan (Ceiling) s\xc4" "\xb1" "n\xc4" "\xb1" "r\xc4" "\xb1" "n\xc4" "\xb1" " a\xc5" "\x9f" "mas\xc4" "\xb1" "n\xc4" "\xb1" " engelleyen tu\xc4" "\x9f" "la duvar (brickwall) s\xc4" "\xb1" "n\xc4" "\xb1" "rlay\xc4" "\xb1" "c\xc4" "\xb1" ". Dijital bozulmalar\xc4" "\xb1" " (clipping) kesin olarak \xc3" "\xb6" "nler."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Ultra-transparent brickwall limiter with adjustable ceiling and smooth lookahead release."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Her zaman sinyal zincirinin en sonunda (Audio Out \xc3" "\xb6" "ncesi) yer almal\xc4" "\xb1" "d\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Place at the very end of your mastering or vocal bus chain as final guardrail."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Ceiling de\xc4" "\x9f" "erini -0.5 dB veya -1.0 dB ayarlayarak platformlara y\xc3" "\xbc" "klerken inter-sample peak bozulmalar\xc4" "\xb1" "n\xc4" "\xb1" " engelleyin."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Set ceiling to -0.5 dB to prevent inter-sample clip distortions during streaming conversion."));
        }
        else if (type.equalsIgnoreCase ("Gate"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Ak\xc4" "\xb1" "ll\xc4" "\xb1" " G\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" " Kap\xc4" "\xb1" "s\xc4" "\xb1" " (Noise Gate)"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Konu\xc5" "\x9f" "mad\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" "n\xc4" "\xb1" "z veya \xc5" "\x9f" "ark\xc4" "\xb1" " s\xc3" "\xb6" "ylemedi\xc4" "\x9f" "iniz anlarda mikrofonu tamamen sessize alarak arka plan seslerini ve dip g\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" "s\xc3" "\xbc" "n\xc3" "\xbc" " keser."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Attenuates microphone bleed and background ambience when voice drops below threshold."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Zincirin ba\xc5" "\x9f" "\xc4" "\xb1" "nda, temizlik a\xc5" "\x9f" "amas\xc4" "\xb1" "nda kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Position near the beginning of the chain to silence mic idle bleed."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Threshold seviyesini sesiniz kesildi\xc4" "\x9f" "inde kapanacak, konu\xc5" "\x9f" "tu\xc4" "\x9f" "unuzda ise sesin ilk harfini yutmayacak hassasiyette ayarlay\xc4" "\xb1" "n."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Set hold and release times gently so word endings don't get abruptly chopped off."));
        }
        else if (type.equalsIgnoreCase ("AGC"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Otomatik Kazan\xc3" "\xa7" " ve Seviye Dengeleyici"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Mikrofona yakla\xc5" "\x9f" "\xc4" "\xb1" "p uzakla\xc5" "\x9f" "san\xc4" "\xb1" "z bile ses seviyenizi s\xc3" "\xbc" "rekli analiz ederek hedef ses \xc5" "\x9f" "iddetinde (Target LUFS/dB) sabit tutar."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Real-time automatic leveling processor maintaining consistent broadcast loudness."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Canl\xc4" "\xb1" " yay\xc4" "\xb1" "nlarda veya podcastlerde kompres\xc3" "\xb6" "r yerine ya da \xc3" "\xb6" "ncesinde kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Ideal for live streaming and podcasting to level out moving vocalists."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Yay\xc4" "\xb1" "nc\xc4" "\xb1" "lar ve podcast sunucular\xc4" "\xb1" " i\xc3" "\xa7" "in ses ini\xc5" "\x9f" " \xc3" "\xa7" "\xc4" "\xb1" "k\xc4" "\xb1" "\xc5" "\x9f" "lar\xc4" "\xb1" "n\xc4" "\xb1" " s\xc4" "\xb1" "f\xc4" "\xb1" "ra indiren hayat kurtar\xc4" "\xb1" "c\xc4" "\xb1" " bir mod\xc3" "\xbc" "ld\xc3" "\xbc" "r."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Pair with a limiter downstream to catch instantaneous vocal spikes."));
        }
        else if (type.equalsIgnoreCase ("Parametric EQ"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("7 Bant Parametrik Cerrahi ve Ton Ekolayzeri"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Vokalinizdeki fazlal\xc4" "\xb1" "k frekanslar\xc4" "\xb1" " kesip parlakl\xc4" "\xb1" "k ve g\xc3" "\xb6" "vde eklemenizi sa\xc4" "\x9f" "layan 7 bantl\xc4" "\xb1" " parametrik ekolayz\xc4" "\xb1" "r. Canl\xc4" "\xb1" " spektrum analiz\xc3" "\xb6" "r\xc3" "\xbc" " (RTA) ile sesinizi g\xc3" "\xb6" "rerek \xc5" "\x9f" "ekillendirebilirsiniz."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("7-band surgical & tonal parametric EQ with real-time spectrum analysis, precise Q curves, Low-Cut and High-Cut filters."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Zincirin ba\xc5" "\x9f" "\xc4" "\xb1" "nda dip g\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" " ve patlamalar\xc4" "\xb1" " kesmek i\xc3" "\xa7" "in (Low-Cut) veya kompres\xc3" "\xb6" "r sonras\xc4" "\xb1" "nda nihai vokal rengini ayarlamak i\xc3" "\xa7" "in kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Place at the start of the chain to clean sub rumble, or after compression to sculpt final vocal tonal curve."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("80-100 Hz alt\xc4" "\xb1" "na Low-Cut atarak mikrofondaki bo\xc4" "\x9f" "uklu\xc4" "\x9f" "u temizleyin; 10-12 kHz civar\xc4" "\xb1" "na hafif High-Shelf vererek vokale modern radyo parlakl\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" " kat\xc4" "\xb1" "n."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Apply a Low-Cut below 80-100 Hz to remove mud, and boost gently with a High-Shelf around 10-12 kHz for instant vocal air."));
        }
        else if (type.equalsIgnoreCase ("Spectral Clarity"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Ak\xc4" "\xb1" "ll\xc4" "\xb1" " Dinamik Rezonans & \xc3" "\x87" "\xc4" "\xb1" "nlama Temizleyici"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Soothe2 mant\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" "yla \xc3" "\xa7" "al\xc4" "\xb1" "\xc5" "\x9f" "an ak\xc4" "\xb1" "ll\xc4" "\xb1" " rezonans temizleyici. Mikrofondan veya k\xc3" "\xb6" "t\xc3" "\xbc" " oda akusti\xc4" "\x9f" "inden kaynaklanan kulak t\xc4" "\xb1" "rmalay\xc4" "\xb1" "c\xc4" "\xb1" ", teneke gibi \xc3" "\xa7" "\xc4" "\xb1" "nlayan frekanslar\xc4" "\xb1" " otomatik olarak bulur ve ses geldik\xc3" "\xa7" "e dinamik olarak bast\xc4" "\xb1" "r\xc4" "\xb1" "r."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Smart dynamic spectral resonance suppressor inspired by Soothe2. Automatically identifies and suppresses harsh ringing frequencies in real time."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Kompres\xc3" "\xb6" "rden hemen sonra kullan\xc4" "\xb1" "lmas\xc4" "\xb1" " tavsiye edilir; kompres\xc3" "\xb6" "r\xc3" "\xbc" "n a\xc3" "\xa7" "\xc4" "\xb1" "\xc4" "\x9f" "a \xc3" "\xa7" "\xc4" "\xb1" "kard\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" " gizli bat\xc4" "\xb1" "c\xc4" "\xb1" " \xc3" "\xa7" "\xc4" "\xb1" "nlamalar\xc4" "\xb1" " vokali matla\xc5" "\x9f" "t\xc4" "\xb1" "rmadan yok eder."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Best placed right after compression to eliminate hidden resonant peaks unearthed by dynamic processing."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Amount de\xc4" "\x9f" "erini %30-%50 aras\xc4" "\xb1" "nda tutarak sesin do\xc4" "\x9f" "all\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" "n\xc4" "\xb1" " koruyabilir, tizlerdeki sivri rezonanslar\xc4" "\xb1" " ipeksi bir p\xc3" "\xbc" "r\xc3" "\xbc" "zs\xc3" "\xbc" "zl\xc3" "\xbc" "\xc4" "\x9f" "e kavu\xc5" "\x9f" "turabilirsiniz."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Set Amount between 30% and 50% to maintain natural warmth while turning harsh high-end into a silky smooth texture."));
        }
        else if (type.equalsIgnoreCase ("Dynamic EQ"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Dinamik Ekolayzer & Rezonans Kontrol\xc3" "\xbc" ""));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Frekans bantlar\xc4" "\xb1" "n\xc4" "\xb1" " yaln\xc4" "\xb1" "zca o frekanstaki ses belirli bir e\xc5" "\x9f" "i\xc4" "\x9f" "i a\xc5" "\x9f" "t\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" "nda k\xc4" "\xb1" "san veya art\xc4" "\xb1" "ran dinamik filtreleme mod\xc3" "\xbc" "l\xc3" "\xbc" "."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Compresses or expands frequency bands selectively only when incoming signal crosses threshold."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Statik EQ'nun yetersiz kald\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" " de\xc4" "\x9f" "i\xc5" "\x9f" "ken tonlu vokallerde kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Use where static EQ causes dullness on quieter passages."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Vokalist ba\xc4" "\x9f" "\xc4" "\xb1" "rd\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" "nda kula\xc4" "\x9f" "\xc4" "\xb1" " t\xc4" "\xb1" "rmalayan 3-4 kHz b\xc3" "\xb6" "lgesini yaln\xc4" "\xb1" "zca gerekti\xc4" "\x9f" "inde bast\xc4" "\xb1" "rmak i\xc3" "\xa7" "in birebirdir."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Tame harsh vocal shouts at 3-4 kHz dynamically without hollowing out quiet vocal phrases."));
        }
        else if (type.equalsIgnoreCase ("De-Esser"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Vokal Sibilans ve Bat\xc4" "\xb1" "c\xc4" "\xb1" " 'S' Frekans\xc4" "\xb1" " Bast\xc4" "\xb1" "r\xc4" "\xb1" "c\xc4" "\xb1" ""));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Vokallerdeki bat\xc4" "\xb1" "c\xc4" "\xb1" " 'S', '\xc5" "\x9e" "', '\xc3" "\x87" "', 'Z' gibi t\xc4" "\xb1" "slamalar\xc4" "\xb1" " ve mikrofon sibilans patlamalar\xc4" "\xb1" "n\xc4" "\xb1" " vokalin genel parlakl\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" "n\xc4" "\xb1" " ve enerjisini \xc3" "\xb6" "ld\xc3" "\xbc" "rmeden temizler."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Next-gen De-Esser featuring Spectral-Ratio detection and zero-phase distortion dynamic peaking filters."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Kompres\xc3" "\xb6" "r ve EQ'dan sonra veya hemen \xc3" "\xb6" "nce yerle\xc5" "\x9f" "tirilebilir."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Position before or after tonal compression to tame harsh vocal fricatives."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Processing kadran\xc4" "\xb1" "n\xc4" "\xb1" " vokale g\xc3" "\xb6" "re ayarlay\xc4" "\xb1" "n; sibilanslar\xc4" "\xb1" "n kulak t\xc4" "\xb1" "rmalamayacak kadar yumu\xc5" "\x9f" "ad\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" " noktada b\xc4" "\xb1" "rak\xc4" "\xb1" "n."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Adjust the Processing dial until harsh 'S' sounds soften without dulling the overall vocal clarity."));
        }
        else if (type.equalsIgnoreCase ("Noise Suppression"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("RNNoise Yapay Zeka Sinir A\xc4" "\x9f" "\xc4" "\xb1" " G\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" " Engelleyici"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Derin \xc3" "\xb6" "\xc4" "\x9f" "renme yapay zeka sinir a\xc4" "\x9f" "\xc4" "\xb1" "yla (RNNoise) \xc3" "\xa7" "al\xc4" "\xb1" "\xc5" "\x9f" "an g\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" " engelleyici. Bilgisayar fan\xc4" "\xb1" ", klima, mikrofon dip t\xc4" "\xb1" "slamas\xc4" "\xb1" " ve ortam g\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" "s\xc3" "\xbc" "n\xc3" "\xbc" " vokal sesini bozmadan gecikmesiz temizler."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Deep learning neural noise suppression. Eliminates room ambient hum, computer fans, and preamp hiss with zero latency."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Kesinlikle zincirin en ba\xc5" "\x9f" "\xc4" "\xb1" "nda, Audio In mod\xc3" "\xbc" "l\xc3" "\xbc" "n\xc3" "\xbc" "n hemen ard\xc4" "\xb1" "ndan kullan\xc4" "\xb1" "lmal\xc4" "\xb1" "d\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Must be placed at the very start of the signal chain directly following Audio In."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("G\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" "y\xc3" "\xbc" " en ba\xc5" "\x9f" "ta temizlemek, sonraki kompres\xc3" "\xb6" "r ve doygunluk mod\xc3" "\xbc" "llerinin dip g\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" "s\xc3" "\xbc" "n\xc3" "\xbc" " katlayarak y\xc3" "\xbc" "kseltmesini \xc3" "\xb6" "nler."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Denoising at the earliest stage prevents downstream compressors from boosting background artifacts."));
        }
        else if (type.equalsIgnoreCase ("De-reverb"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Oda Yank\xc4" "\xb1" "s\xc4" "\xb1" " ve Akustik Yans\xc4" "\xb1" "ma Kurutucu"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Yal\xc4" "\xb1" "t\xc4" "\xb1" "ms\xc4" "\xb1" "z veya yank\xc4" "\xb1" "l\xc4" "\xb1" " odalarda kaydedilen seslerdeki oda \xc3" "\xa7" "\xc4" "\xb1" "nlamas\xc4" "\xb1" "n\xc4" "\xb1" " ve yank\xc4" "\xb1" " kuyru\xc4" "\x9f" "unu kurutur; sesin sanki profesyonel ses yal\xc4" "\xb1" "t\xc4" "\xb1" "ml\xc4" "\xb1" " st\xc3" "\xbc" "dyoda kaydedilmi\xc5" "\x9f" " gibi do\xc4" "\x9f" "rudan kula\xc4" "\x9f" "\xc4" "\xb1" "n dibinde duyulmas\xc4" "\xb1" "n\xc4" "\xb1" " sa\xc4" "\x9f" "lar."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Acoustic reflection suppressor. Strips room ambience and boxy acoustic reverberation from untreated rooms."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Noise Suppression mod\xc3" "\xbc" "l\xc3" "\xbc" "n\xc3" "\xbc" "n hemen ard\xc4" "\xb1" "ndan, EQ ve kompres\xc3" "\xb6" "r \xc3" "\xb6" "ncesinde kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Insert right after noise suppression before dynamic shaping."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Oda yank\xc4" "\xb1" "s\xc4" "\xb1" "n\xc4" "\xb1" " sildikten sonra vokalinize kendi kaliteli reverb efektinizi \xc3" "\xa7" "ok daha berrak \xc5" "\x9f" "ekilde ekleyebilirsiniz."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Creates a dry, intimate canvas so you can add high-quality algorithmic reverb later with full clarity."));
        }
        else if (type.equalsIgnoreCase ("De-Breath"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Ak\xc4" "\xb1" "ll\xc4" "\xb1" " Nefes & \xc4" "\xb0" "\xc3" "\xa7" "e \xc3" "\x87" "eki\xc5" "\x9f" " Sesi Azalt\xc4" "\xb1" "c\xc4" "\xb1" ""));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("C\xc3" "\xbc" "mle aralar\xc4" "\xb1" "nda istemsizce mikrofona giren y\xc3" "\xbc" "ksek sesli derin nefes alma ve i\xc3" "\xa7" "e \xc3" "\xa7" "eki\xc5" "\x9f" " seslerini otomatik olarak tespit edip bast\xc4" "\xb1" "r\xc4" "\xb1" "r."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Intelligent breath and inhalation detector. Automatically detects and suppresses prominent gasp and breath artifacts between vocal phrases."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("G\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" " temizleme (Noise Suppression) sonras\xc4" "\xb1" "nda veya kompres\xc3" "\xb6" "r \xc3" "\xb6" "ncesinde kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Position after initial noise cleanup and before dynamic compression."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Nefes seslerini tamamen silmek yerine seviyesini hafif\xc3" "\xa7" "e d\xc3" "\xbc" "\xc5" "\x9f" "\xc3" "\xbc" "rerek vokaldeki insani ve do\xc4" "\x9f" "al havay\xc4" "\xb1" " koruyabilirsiniz."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Tame breath levels by 6-12 dB rather than completely gating them to maintain natural vocal humanity."));
        }
        else if (type.equalsIgnoreCase ("De-Click"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("A\xc4" "\x9f" "\xc4" "\xb1" "z \xc5" "\x9e" "ap\xc4" "\xb1" "rt\xc4" "\xb1" "s\xc4" "\xb1" " ve \xc3" "\x87" "\xc4" "\xb1" "t\xc4" "\xb1" "rt\xc4" "\xb1" " Giderici"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Konu\xc5" "\x9f" "ma veya \xc5" "\x9f" "ark\xc4" "\xb1" " esnas\xc4" "\xb1" "nda a\xc4" "\x9f" "\xc4" "\xb1" "z kurulu\xc4" "\x9f" "undan, dudak ve dil hareketlerinden kaynaklanan rahats\xc4" "\xb1" "z edici mikro \xc3" "\xa7" "\xc4" "\xb1" "t\xc4" "\xb1" "rt\xc4" "\xb1" " ve t\xc4" "\xb1" "klama seslerini an\xc4" "\xb1" "nda temizler."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Surgically detects and repairs fast transient mouth noises, saliva ticks, and digital pop artifacts."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Zincirin ba\xc5" "\x9f" "\xc4" "\xb1" "nda, g\xc3" "\xbc" "r\xc3" "\xbc" "lt\xc3" "\xbc" " engelleme ile birlikte kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Insert at the very beginning alongside noise reduction."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Podcast, seslendirme ve yak\xc4" "\xb1" "n mikrofon kay\xc4" "\xb1" "tlar\xc4" "\xb1" "nda m\xc3" "\xbc" "kemmel netlik sa\xc4" "\x9f" "lar."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Crucial for close-mic voiceovers, spoken word, and intimate acoustic vocal tracks."));
        }
        else if (type.equalsIgnoreCase ("De-Plosive"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Mikrofon Patlamas\xc4" "\xb1" " (P-B-T R\xc3" "\xbc" "zgar\xc4" "\xb1" ") Filtresi"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("'P', 'B', 'T' gibi harflerde mikrofona \xc3" "\xa7" "arpan hava ak\xc4" "\xb1" "m\xc4" "\xb1" "n\xc4" "\xb1" "n yaratt\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" " d\xc3" "\xbc" "\xc5" "\x9f" "\xc3" "\xbc" "k frekansl\xc4" "\xb1" " bo\xc4" "\x9f" "uk patlama ve u\xc4" "\x9f" "ultu seslerini dinamik olarak emer."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Dynamically attenuates low-frequency energy bursts caused by air blasts on microphone diaphragms."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Zincirin giri\xc5" "\x9f" "inde, Audio In mod\xc3" "\xbc" "l\xc3" "\xbc" "n\xc3" "\xbc" "n hemen sonras\xc4" "\xb1" "nda kullan\xc4" "\xb1" "lmal\xc4" "\xb1" "d\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Place immediately after Audio Input to protect subsequent compressors from low-end overload."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Pop-filter kullan\xc4" "\xb1" "lmayan kay\xc4" "\xb1" "tlarda mikrofon patlamalar\xc4" "\xb1" "n\xc4" "\xb1" " yok etmek i\xc3" "\xa7" "in vazge\xc3" "\xa7" "ilmezdir."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Essential when recording without a physical hardware pop filter."));
        }
        else if (type.equalsIgnoreCase ("Pitch Correction"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Ger\xc3" "\xa7" "ek Zamanl\xc4" "\xb1" " Vokal Ton D\xc3" "\xbc" "zeltici (Autotune)"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Detone olunan notalar\xc4" "\xb1" " an\xc4" "\xb1" "nda se\xc3" "\xa7" "ilen m\xc3" "\xbc" "zikal diziye (Gam ve K\xc3" "\xb6" "k nota) \xc3" "\xa7" "eken profesyonel tonlama algoritmas\xc4" "\xb1" "."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Low-latency pitch correction engine with musical scale quantization, correction speed, and humanize vibrato controls."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Genellikle temizlik a\xc5" "\x9f" "amas\xc4" "\xb1" "ndan hemen sonra, reverb ve delay efektlerinden \xc3" "\xb6" "nce kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Insert after cleanup stages and prior to spatial time-based effects."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Speed de\xc4" "\x9f" "erini h\xc4" "\xb1" "zland\xc4" "\xb1" "rarak modern Trap/Pop robotik efektini yakalayabilir veya yava\xc5" "\x9f" "lat\xc4" "\xb1" "p \xc5" "\x9f" "effaf ve do\xc4" "\x9f" "al bir d\xc3" "\xbc" "zeltme elde edebilirsiniz."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Set fast Retune Speed for modern hard-tuning or medium-slow speeds for natural transparent pitch correction."));
        }
        else if (type.equalsIgnoreCase ("Saturator"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("T\xc3" "\xbc" "p & Bant Harmonik Doygunluk / Karakter"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Vokale vintage analog s\xc4" "\xb1" "cakl\xc4" "\xb1" "k, harmonik zenginlik ve miksin i\xc3" "\xa7" "inde parlamas\xc4" "\xb1" "n\xc4" "\xb1" " sa\xc4" "\x9f" "layan tatl\xc4" "\xb1" " bir g\xc3" "\xb6" "vde katar."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Warm analog saturation generator adding odd and even harmonic overtones for perceived loudness and presence."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Kompres\xc3" "\xb6" "rden sonra veya EQ \xc3" "\xb6" "ncesinde vokal tonunu renklendirmek i\xc3" "\xa7" "in kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Place after dynamic control to introduce harmonic glue and presence."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Az miktarda Tube veya Tape doygunlu\xc4" "\x9f" "u, vokali 'so\xc4" "\x9f" "uk dijital' hissiyattan kurtar\xc4" "\xb1" "p profesyonel st\xc3" "\xbc" "dyo alb\xc3" "\xbc" "m sound'una kavu\xc5" "\x9f" "turur."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Blend subtly using the Drive dial to make vocals sit effortlessly on top of a dense instrumental mix."));
        }
        else if (type.equalsIgnoreCase ("Doubler"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Stereo Vokal Katlay\xc4" "\xb1" "c\xc4" "\xb1" " & Koro Efekti"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Tek bir vokal kanal\xc4" "\xb1" "ndan k\xc3" "\xbc" "\xc3" "\xa7" "\xc3" "\xbc" "k zamanlama ve perde mikro-kaymalar\xc4" "\xb1" " yaratarak sanki ayn\xc4" "\xb1" " anda iki vokalist s\xc3" "\xb6" "yl\xc3" "\xbc" "yormu\xc5" "\x9f" " gibi geni\xc5" "\x9f" " bir stereo hissi yarat\xc4" "\xb1" "r."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Generates natural psychoacoustic doubling width using micro-pitch shifts and tiny decorrelated time offsets."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Ana vokal zincirinde veya nakaratlarda zenginlik katmak i\xc3" "\xa7" "in kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Apply on vocal hooks, choruses, or backing layers for immersive wide vocal presence."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Detune ve Width ayarlar\xc4" "\xb1" "n\xc4" "\xb1" " dengeli tutarak ana vokal oda\xc4" "\x9f" "\xc4" "\xb1" "n\xc4" "\xb1" " kaybetmeden geni\xc5" "\x9f" "lik kazand\xc4" "\xb1" "r\xc4" "\xb1" "n."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Keep the lead center image intact by balancing wet mix subtly behind the main dry signal."));
        }
        else if (type.equalsIgnoreCase ("Delay"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Stereo / Ping-Pong Eko ve Gecikme"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Vokal c\xc3" "\xbc" "mlelerini m\xc3" "\xbc" "zi\xc4" "\x9f" "in temposuna (BPM) uygun olarak sa\xc4" "\x9f" "-sol kanallarda yank\xc4" "\xb1" "land\xc4" "\xb1" "ran stereo gecikme i\xc5" "\x9f" "lemcisi."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Digital & tape style delay with host tempo sync, stereo offset, ping-pong bouncing, and filtering."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Zincirin sonlar\xc4" "\xb1" "na do\xc4" "\x9f" "ru, Reverb \xc3" "\xb6" "ncesinde veya paralel hatta kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Position near the end of the chain before or parallel to reverb."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("High-Cut filtresini kullanarak delay tekrarlar\xc4" "\xb1" "n\xc4" "\xb1" " hafif matla\xc5" "\x9f" "t\xc4" "\xb1" "r\xc4" "\xb1" "n; b\xc3" "\xb6" "ylece ana vokali \xc3" "\xb6" "rtmeden arkada derinlik hissi yarat\xc4" "\xb1" "r."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Use the high-cut damping filter on delay repeats so echoes stay behind lead vocals without cluttering."));
        }
        else if (type.equalsIgnoreCase ("Reverb"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Algoritmik St\xc3" "\xbc" "dyo Alan\xc4" "\xb1" " ve Akustik Yank\xc4" "\xb1" ""));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Vokali kuru bir odadan \xc3" "\xa7" "\xc4" "\xb1" "kar\xc4" "\xb1" "p profesyonel bir konser salonu, geni\xc5" "\x9f" " st\xc3" "\xbc" "dyo odas\xc4" "\xb1" " (Plate/Room) veya b\xc3" "\xbc" "y\xc3" "\xbc" "leyici bir mekana yerle\xc5" "\x9f" "tiren akustik derinlik motoru."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Lush spatial reverb processor with room size, pre-delay, damping, and width controls."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Sinyal zincirinin en sonunda (Limiter \xc3" "\xb6" "ncesinde) kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Place at the end of the processing chain to create atmospheric 3D space."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Decay s\xc3" "\xbc" "resini \xc5" "\x9f" "ark\xc4" "\xb1" "n\xc4" "\xb1" "n h\xc4" "\xb1" "z\xc4" "\xb1" "na g\xc3" "\xb6" "re ayarlay\xc4" "\xb1" "n; vokalin anla\xc5" "\x9f" "\xc4" "\xb1" "l\xc4" "\xb1" "rl\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" "n\xc4" "\xb1" " kaybetmemek i\xc3" "\xa7" "in Pre-Delay de\xc4" "\x9f" "erini 20-40 ms aras\xc4" "\xb1" "nda tutun."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Set Pre-Delay between 20-40 ms to preserve vocal transient punch before the reverb tail begins."));
        }
        else if (type.equalsIgnoreCase ("Stereo Width"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("Stereo Geni\xc5" "\x9f" "letici & Monouyumluluk Kontrol\xc3" "\xbc" ""));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Sesin stereo sahnedeki geni\xc5" "\x9f" "li\xc4" "\x9f" "ini art\xc4" "\xb1" "ran veya mono uyumlulu\xc4" "\x9f" "unu denetleyen Mid/Side i\xc5" "\x9f" "lemci."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Precise Mid/Side stereo field expander with mono bass collapse and side-gain boosting."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Efektlerin ard\xc4" "\xb1" "ndan, miks dengesini a\xc3" "\xa7" "mak i\xc3" "\xa7" "in kullan\xc4" "\xb1" "l\xc4" "\xb1" "r."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Insert downstream of spatial effects to fine-tune final stereo spread."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Mono bas uyumlulu\xc4" "\x9f" "unu korumak i\xc3" "\xa7" "in d\xc3" "\xbc" "\xc5" "\x9f" "\xc3" "\xbc" "k frekanslar\xc4" "\xb1" " merkezde tutmaya \xc3" "\xb6" "zen g\xc3" "\xb6" "sterin."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Always monitor mono sum compatibility to ensure wide vocals don't phase-cancel on mobile speakers."));
        }
        else if (type.equalsIgnoreCase ("Container"))
        {
            d.tagline = juce::String (juce::CharPointer_UTF8 ("\xc3" "\x87" "oklu Mod\xc3" "\xbc" "l Raf\xc4" "\xb1" " & Efekt Paketi"));
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Birden fazla mod\xc3" "\xbc" "l\xc3" "\xbc" " (\xc3" "\xb6" "rn. RNNoise + De-Reverb + De-Breath) tek bir kutu i\xc3" "\xa7" "ine toplayarak tuvaldeki kalabal\xc4" "\xb1" "\xc4" "\x9f" "\xc4" "\xb1" " \xc3" "\xb6" "nler. Kendi \xc3" "\xb6" "zel ismini (\xc3" "\xb6" "rn. 'Pre-FX') ve genel etki d\xc3" "\xbc" "zeyi (Mix %) kadran\xc4" "\xb1" "n\xc4" "\xb1" " bar\xc4" "\xb1" "nd\xc4" "\xb1" "r\xc4" "\xb1" "r."));
            d.overviewEn = juce::String (juce::CharPointer_UTF8 ("Encapsulates multiple chained processing modules into a single compact node with custom naming, serialized preset state, and global Wet/Dry mix dial."));
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("Zincirinizdeki herhangi bir a\xc5" "\x9f" "amay\xc4" "\xb1" " kompakt bir blok halinde gruplamak i\xc3" "\xa7" "in kullanabilirsiniz."));
            d.placementEn = juce::String (juce::CharPointer_UTF8 ("Group complex multi-node subchains into a neat single block on the canvas."));
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Tuvaldeki bir mod\xc3" "\xbc" "l\xc3" "\xbc" " s\xc3" "\xbc" "r\xc3" "\xbc" "kleyip do\xc4" "\x9f" "rudan Container'\xc4" "\xb1" "n \xc3" "\xbc" "zerine b\xc4" "\xb1" "rakarak i\xc3" "\xa7" "ine ekleyebilir veya mod\xc3" "\xbc" "lleri se\xc3" "\xa7" "ip sa\xc4" "\x9f" " t\xc4" "\xb1" "k ile paketleyebilirsiniz."));
            d.tipsEn = juce::String (juce::CharPointer_UTF8 ("Drag any module directly onto the Container to adopt it, or select multiple nodes and use right-click 'Pack Selected Modules'."));
        }
        else
        {
            d.tagline = "Modular Voice Studio DSP Module";
            d.overviewTr = juce::String (juce::CharPointer_UTF8 ("Profesyonel ses ve vokal i\xc5\x9fleme mod\xc3\xbcl\xc3\xbc."));
            d.overviewEn = "Professional audio and vocal processing DSP module.";
            d.placementTr = juce::String (juce::CharPointer_UTF8 ("\xc4\xb0stedi\xc4\x9finiz sinyal zinciri konumunda kullanabilirsiniz."));
            d.placementEn = "Place anywhere in your signal chain.";
            d.tipsTr = juce::String (juce::CharPointer_UTF8 ("Ayarlar\xc4\xb1 miksinize g\xc3\xb6re kulakla dinleyerek optimize edin."));
            d.tipsEn = "Tune parameters by ear to fit your mix.";
        }
        return d;
    }

private:
    juce::String currentType;
    ModuleInfoData info;
    std::function<void(const juce::String&)> onAdd;
    std::function<void()> onClose;
    juce::TextButton addBtn;
    juce::TextButton closeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleInfoCard)
};
