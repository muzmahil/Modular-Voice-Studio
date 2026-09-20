#pragma once
#include <juce_core/juce_core.h>
#include <map>
#include <vector>
#include <functional>

enum class Language
{
    English,
    Turkish,
    Russian,
    Japanese
};

class LocalizationManager
{
public:
    static LocalizationManager& instance()
    {
        static LocalizationManager mgr;
        return mgr;
    }

    void setLanguage (Language lang)
    {
        if (currentLanguage == lang)
            return;

        currentLanguage = lang;
        broadcastChange();
    }

    Language getLanguage() const { return currentLanguage; }

    juce::String getLanguageCode() const
    {
        switch (currentLanguage)
        {
            case Language::English:  return "en";
            case Language::Turkish:  return "tr";
            case Language::Russian:  return "ru";
            case Language::Japanese: return "ja";
        }
        return "en";
    }

    void setLanguageFromCode (const juce::String& code)
    {
        if (code == "tr")      setLanguage (Language::Turkish);
        else if (code == "ru") setLanguage (Language::Russian);
        else if (code == "ja") setLanguage (Language::Japanese);
        else                   setLanguage (Language::English);
    }

    juce::String get (const juce::String& key) const
    {
        auto it = translations.find (key);
        if (it != translations.end())
        {
            int idx = static_cast<int> (currentLanguage);
            if (idx >= 0 && idx < (int) it->second.size())
                return it->second[(size_t) idx];
        }
        return key;
    }

    juce::String getForLanguage (const juce::String& key, Language lang) const
    {
        auto it = translations.find (key);
        if (it != translations.end())
        {
            int idx = static_cast<int> (lang);
            if (idx >= 0 && idx < (int) it->second.size())
                return it->second[(size_t) idx];
        }
        return key;
    }

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void localizationChanged() = 0;
    };

    void addListener (Listener* listener)
    {
        listeners.add (listener);
    }

    void removeListener (Listener* listener)
    {
        listeners.remove (listener);
    }

    void broadcastChange()
    {
        listeners.call ([] (Listener& l) { l.localizationChanged(); });
    }

private:
    LocalizationManager()
    {
        initDictionary();
    }

    Language currentLanguage = Language::English;
    std::map<juce::String, std::vector<juce::String>> translations;
    juce::ListenerList<Listener> listeners;

    static juce::String toJuceString (const char* s)
    {
        return juce::String (juce::CharPointer_UTF8 (s));
    }

    static juce::String toJuceString (const char8_t* s)
    {
        return juce::String (juce::CharPointer_UTF8 ((const char*) s));
    }

    static juce::String toJuceString (const juce::String& s)
    {
        return s;
    }

    template <typename S1, typename S2, typename S3, typename S4, typename S5>
    void add (const S1& key, const S2& en, const S3& tr, const S4& ru, const S5& ja)
    {
        translations[toJuceString (key)] = {
            toJuceString (en),
            toJuceString (tr),
            toJuceString (ru),
            toJuceString (ja)
        };
    }

    void initDictionary()
    {
        // ALL non-ASCII strings encoded as explicit UTF-8 hex escapes.
        // This is guaranteed to work regardless of MSVC source codepage or /utf-8 flag.
        add("LIBRARY", "Library",
            juce::String(juce::CharPointer_UTF8("K" "\xc3" "\xbc" "t" "\xc3" "\xbc" "phane")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x91" "\xd0" "\xb8" "\xd0" "\xb1" "\xd0" "\xbb" "\xd0" "\xb8" "\xd0" "\xbe" "\xd1" "\x82" "\xd0" "\xb5" "\xd0" "\xba" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa9" "\xe3" "\x82" "\xa4" "\xe3" "\x83" "\x96" "\xe3" "\x83" "\xa9" "\xe3" "\x83" "\xaa")));
        add("INSPECTOR", "Inspector",
            juce::String(juce::CharPointer_UTF8("Denet" "\xc3" "\xa7" "i")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x98" "\xd0" "\xbd" "\xd1" "\x81" "\xd0" "\xbf" "\xd0" "\xb5" "\xd0" "\xba" "\xd1" "\x82" "\xd0" "\xbe" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xa4" "\xe3" "\x83" "\xb3" "\xe3" "\x82" "\xb9" "\xe3" "\x83" "\x9a" "\xe3" "\x82" "\xaf" "\xe3" "\x82" "\xbf" "\xe3" "\x83" "\xbc")));
        add("NAVIGATOR", "Navigator",
            "Gezgin",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9d" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xb8" "\xd0" "\xb3" "\xd0" "\xb0" "\xd1" "\x82" "\xd0" "\xbe" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x8a" "\xe3" "\x83" "\x93" "\xe3" "\x82" "\xb2" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xbf" "\xe3" "\x83" "\xbc")));
        add("WAVE_MONITOR", "WAVE MONITOR",
            juce::String(juce::CharPointer_UTF8("DALGA MON" "\xc4" "\xb0" "T" "\xc3" "\x96" "R" "\xc3" "\x9c")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9c" "\xd0" "\x9e" "\xd0" "\x9d" "\xd0" "\x98" "\xd0" "\xa2" "\xd0" "\x9e" "\xd0" "\xa0" " " "\xd0" "\x92" "\xd0" "\x9e" "\xd0" "\x9b" "\xd0" "\x9d")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\xb3" "\xa2" "\xe5" "\xbd" "\xa2" "\xe3" "\x83" "\xa2" "\xe3" "\x83" "\x8b" "\xe3" "\x82" "\xbf" "\xe3" "\x83" "\xbc")));
        add("SEARCH_MODULES", "Search modules...",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3" "\xbc" "llerde ara...")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xbe" "\xd0" "\xb8" "\xd1" "\x81" "\xd0" "\xba" " " "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xb4" "\xd1" "\x83" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xb9" "...")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe6" "\xa4" "\x9c" "\xe7" "\xb4" "\xa2" "...")));
        add("VOCAL_MODULES", "VOCAL MODULES",
            juce::String(juce::CharPointer_UTF8("VOKAL MOD" "\xc3" "\x9c" "LLER" "\xc4" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd0" "\x9e" "\xd0" "\x9a" "\xd0" "\x90" "\xd0" "\x9b" "\xd0" "\xac" "\xd0" "\x9d" "\xd0" "\xab" "\xd0" "\x95" " " "\xd0" "\x9c" "\xd0" "\x9e" "\xd0" "\x94" "\xd0" "\xa3" "\xd0" "\x9b" "\xd0" "\x98")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x9c" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xab" "\xe3" "\x83" "\xab" "\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab")));
        add("NO_MODULE_SELECTED", "No module selected.",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3" "\xbc" "l se" "\xc3" "\xa7" "ilmedi.")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9c" "\xd0" "\xbe" "\xd0" "\xb4" "\xd1" "\x83" "\xd0" "\xbb" "\xd1" "\x8c" " " "\xd0" "\xbd" "\xd0" "\xb5" " " "\xd0" "\xb2" "\xd1" "\x8b" "\xd0" "\xb1" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xbd" ".")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab" "\xe3" "\x81" "\x8c" "\xe9" "\x81" "\xb8" "\xe6" "\x8a" "\x9e" "\xe3" "\x81" "\x95" "\xe3" "\x82" "\x8c" "\xe3" "\x81" "\xa6" "\xe3" "\x81" "\x84" "\xe3" "\x81" "\xbe" "\xe3" "\x81" "\x9b" "\xe3" "\x82" "\x93" "\xe3" "\x80" "\x82")));
        add("CLICK_NODE_TO_INSPECT", "Click any node on canvas to quickly tweak parameters here.",
            juce::String(juce::CharPointer_UTF8("Parametreleri d" "\xc3" "\xbc" "zenlemek i" "\xc3" "\xa7" "in tuvaldeki bir mod" "\xc3" "\xbc" "le t" "\xc4" "\xb1" "klay" "\xc4" "\xb1" "n.")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9d" "\xd0" "\xb0" "\xd0" "\xb6" "\xd0" "\xbc" "\xd0" "\xb8" "\xd1" "\x82" "\xd0" "\xb5" " " "\xd0" "\xbd" "\xd0" "\xb0" " " "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xb4" "\xd1" "\x83" "\xd0" "\xbb" "\xd1" "\x8c" " " "\xd0" "\xb4" "\xd0" "\xbb" "\xd1" "\x8f" " " "\xd0" "\xbd" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x82" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb9" "\xd0" "\xba" "\xd0" "\xb8" ".")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x91" "\xe3" "\x83" "\xa9" "\xe3" "\x83" "\xa1" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xbf" "\xe3" "\x82" "\x92" "\xe8" "\xaa" "\xbf" "\xe6" "\x95" "\xb4" "\xe3" "\x81" "\x99" "\xe3" "\x82" "\x8b" "\xe3" "\x81" "\xab" "\xe3" "\x81" "\xaf" "\xe3" "\x83" "\x8e" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x89" "\xe3" "\x82" "\x92" "\xe3" "\x82" "\xaf" "\xe3" "\x83" "\xaa" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xaf" "\xe3" "\x81" "\x97" "\xe3" "\x81" "\xa6" "\xe3" "\x81" "\x8f" "\xe3" "\x81" "\xa0" "\xe3" "\x81" "\x95" "\xe3" "\x81" "\x84" "\xe3" "\x80" "\x82")));
        add("PRESET", "Preset",
            juce::String(juce::CharPointer_UTF8("Haz" "\xc4" "\xb1" "r Ayar")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd1" "\x80" "\xd0" "\xb5" "\xd1" "\x81" "\xd0" "\xb5" "\xd1" "\x82")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x97" "\xe3" "\x83" "\xaa" "\xe3" "\x82" "\xbb" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x88")));
        add("LOAD", "Load",
            juce::String(juce::CharPointer_UTF8("Y" "\xc3" "\xbc" "kle")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x97" "\xd0" "\xb0" "\xd0" "\xb3" "\xd1" "\x80" "\xd1" "\x83" "\xd0" "\xb7" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\xaa" "\xad" "\xe8" "\xbe" "\xbc")));
        add("SAVE", "Save",
            "Kaydet",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xbe" "\xd1" "\x85" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xbd" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe4" "\xbf" "\x9d" "\xe5" "\xad" "\x98")));
        add("SAVE_AS", "Save As..",
            juce::String(juce::CharPointer_UTF8("Farkl" "\xc4" "\xb1" " Kaydet..")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xbe" "\xd1" "\x85" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xbd" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xba" "\xd0" "\xb0" "\xd0" "\xba" "..")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x88" "\xa5" "\xe5" "\x90" "\x8d" "\xe4" "\xbf" "\x9d" "\xe5" "\xad" "\x98" "..")));
        add("UNDO", "Undo",
            "Geri Al",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x82" "\xd0" "\xbc" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x85" "\x83" "\xe3" "\x81" "\xab" "\xe6" "\x88" "\xbb" "\xe3" "\x81" "\x99")));
        add("REDO", "Redo",
            "Yinele",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xbe" "\xd0" "\xb2" "\xd1" "\x82" "\xd0" "\xbe" "\xd1" "\x80" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\x84" "\xe3" "\x82" "\x8a" "\xe7" "\x9b" "\xb4" "\xe3" "\x81" "\x97")));
        add("SETTINGS", "Settings",
            "Ayarlar",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9d" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x82" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb9" "\xd0" "\xba" "\xd0" "\xb8")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\xa8" "\xad" "\xe5" "\xae" "\x9a")));
        add("SNAP", "Snap",
            "Hizala",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xb5" "\xd1" "\x82" "\xd0" "\xba" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb9" "\xe3" "\x83" "\x8a" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x97")));
        add("AUDIO_HARDWARE_BTN", "Audio Device & Buffer...",
            juce::String(juce::CharPointer_UTF8("Ses Kart" "\xc4" "\xb1" " ve Tampon (Buffer)...")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x90" "\xd1" "\x83" "\xd0" "\xb4" "\xd0" "\xb8" "\xd0" "\xbe" "\xd1" "\x83" "\xd1" "\x81" "\xd1" "\x82" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb9" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xb2" "\xd0" "\xbe" "...")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xaa" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x87" "\xe3" "\x82" "\xa3" "\xe3" "\x82" "\xaa" "\xe8" "\xa8" "\xad" "\xe5" "\xae" "\x9a" "...")));
        add("MODULES_COUNT", "Modules",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3" "\xbc" "l")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9c" "\xd0" "\xbe" "\xd0" "\xb4" "\xd1" "\x83" "\xd0" "\xbb" "\xd0" "\xb8")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab")));
        add("MASTER_OUT", "Master Out",
            juce::String(juce::CharPointer_UTF8("Ana " "\xc3" "\x87" "\xc4" "\xb1" "k" "\xc4" "\xb1" "\xc5" "\x9f")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9c" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xb5" "\xd1" "\x80" " " "\xd0" "\xb2" "\xd1" "\x8b" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xb4")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x9e" "\xe3" "\x82" "\xb9" "\xe3" "\x82" "\xbf" "\xe3" "\x83" "\xbc" "\xe5" "\x87" "\xba" "\xe5" "\x8a" "\x9b")));
        add("ACTIVE", "Active",
            "Aktif",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x90" "\xd0" "\xba" "\xd1" "\x82" "\xd0" "\xb8" "\xd0" "\xb2" "\xd0" "\xbd" "\xd0" "\xbe")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\x9c" "\x89" "\xe5" "\x8a" "\xb9")));
        add("BYPASS", "Bypass",
            juce::String(juce::CharPointer_UTF8("Devre D" "\xc4" "\xb1" "\xc5" "\x9f" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x91" "\xd0" "\xb0" "\xd0" "\xb9" "\xd0" "\xbf" "\xd0" "\xb0" "\xd1" "\x81")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x90" "\xe3" "\x82" "\xa4" "\xe3" "\x83" "\x91" "\xe3" "\x82" "\xb9")));
        add("SR_LABEL", "SR",
            juce::String(juce::CharPointer_UTF8("\xc3" "\x96" "RN")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa7" "\xd0" "\x90" "\xd0" "\xa1")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb5" "\xe3" "\x83" "\xb3" "\xe3" "\x83" "\x97" "\xe3" "\x83" "\xab")));
        add("BUF_LABEL", "BUF",
            "TMP",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x91" "\xd0" "\xa3" "\xd0" "\xa4")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x90" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x95" "\xe3" "\x82" "\xa1")));
        add("RESP_LABEL", "RESP",
            "GEC",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd0" "\xa2" "\xd0" "\x9a")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\xbf" "\x9c" "\xe7" "\xad" "\x94")));
        add("CPU_LABEL", "CPU",
            juce::String(juce::CharPointer_UTF8("\xc4" "\xb0" "\xc5" "\x9e" "L")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa6" "\xd0" "\x9f")),
            "CPU");
        add("CAT_UTILITY", "Utility",
            juce::String(juce::CharPointer_UTF8("Ara" "\xc3" "\xa7" "lar")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa3" "\xd1" "\x82" "\xd0" "\xb8" "\xd0" "\xbb" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8b")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa6" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x86" "\xe3" "\x82" "\xa3" "\xe3" "\x83" "\xaa" "\xe3" "\x83" "\x86" "\xe3" "\x82" "\xa3")));
        add("CAT_DYNAMICS", "Dynamics",
            "Dinamikler",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\xb8" "\xd0" "\xbd" "\xd0" "\xb0" "\xd0" "\xbc" "\xd0" "\xb8" "\xd0" "\xba" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x80" "\xe3" "\x82" "\xa4" "\xe3" "\x83" "\x8a" "\xe3" "\x83" "\x9f" "\xe3" "\x82" "\xaf" "\xe3" "\x82" "\xb9")));
        add("CAT_FREQUENCY", "Frequency",
            "Frekans",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa7" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xbe" "\xd1" "\x82" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x91" "\xa8" "\xe6" "\xb3" "\xa2" "\xe6" "\x95" "\xb0")));
        add("CAT_CLEANUP", "Cleanup",
            "Temizleme",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x87" "\xd0" "\xb8" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xba" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xaf" "\xe3" "\x83" "\xaa" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xb3" "\xe3" "\x82" "\xa2" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x97")));
        add("CAT_VOCAL_TONE", "Vocal Tone",
            "Vokal Tonu",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd0" "\xbe" "\xd0" "\xba" "\xd0" "\xb0" "\xd0" "\xbb" "\xd1" "\x8c" "\xd0" "\xbd" "\xd1" "\x8b" "\xd0" "\xb9" " " "\xd1" "\x82" "\xd0" "\xbe" "\xd0" "\xbd")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x9c" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xab" "\xe3" "\x83" "\xab" "\xe3" "\x83" "\x88" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xb3")));
        add("AUDIO_IN", "Audio In",
            juce::String(juce::CharPointer_UTF8("Ses Giri" "\xc5" "\x9f" "i")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x90" "\xd1" "\x83" "\xd0" "\xb4" "\xd0" "\xb8" "\xd0" "\xbe" " " "\xd0" "\x92" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xb4")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xaa" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x87" "\xe3" "\x82" "\xa3" "\xe3" "\x82" "\xaa" "\xe5" "\x85" "\xa5" "\xe5" "\x8a" "\x9b")));
        add("AUDIO_OUT", "Audio Out",
            juce::String(juce::CharPointer_UTF8("Ses " "\xc3" "\x87" "\xc4" "\xb1" "k" "\xc4" "\xb1" "\xc5" "\x9f" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x90" "\xd1" "\x83" "\xd0" "\xb4" "\xd0" "\xb8" "\xd0" "\xbe" " " "\xd0" "\x92" "\xd1" "\x8b" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xb4")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xaa" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x87" "\xe3" "\x82" "\xa3" "\xe3" "\x82" "\xaa" "\xe5" "\x87" "\xba" "\xe5" "\x8a" "\x9b")));
        add("POWER", "Power",
            juce::String(juce::CharPointer_UTF8("G" "\xc3" "\xbc" "\xc3" "\xa7")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xb8" "\xd1" "\x82" "\xd0" "\xb0" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe9" "\x9b" "\xbb" "\xe6" "\xba" "\x90")));
        add("OPEN_UI", "Open UI",
            juce::String(juce::CharPointer_UTF8("Aray" "\xc3" "\xbc" "z" "\xc3" "\xbc" " A" "\xc3" "\xa7")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x82" "\xd0" "\xba" "\xd1" "\x80" "\xd1" "\x8b" "\xd1" "\x82" "\xd1" "\x8c" " UI")),
            juce::String(juce::CharPointer_UTF8("UI" "\xe3" "\x82" "\x92" "\xe9" "\x96" "\x8b" "\xe3" "\x81" "\x8f")));
        add("DELETE", "Delete",
            "Sil",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa3" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xbb" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x89" "\x8a" "\xe9" "\x99" "\xa4")));
        add("DUPLICATE", "Duplicate Module (Ctrl+D)",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3" "\xbc" "l" "\xc3" "\xbc" " " "\xc3" "\x87" "o" "\xc4" "\x9f" "alt (Ctrl+D)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd1" "\x83" "\xd0" "\xb1" "\xd0" "\xbb" "\xd0" "\xb8" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb2" "\xd0" "\xb0" "\xd1" "\x82" "\xd1" "\x8c" " (Ctrl+D)")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\xa4" "\x87" "\xe8" "\xa3" "\xbd" " (Ctrl+D)")));
        add("DUPLICATE_MODULE", "Duplicate Module (Ctrl+D)",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3" "\xbc" "l" "\xc3" "\xbc" " " "\xc3" "\x87" "o" "\xc4" "\x9f" "alt (Ctrl+D)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd1" "\x83" "\xd0" "\xb1" "\xd0" "\xbb" "\xd0" "\xb8" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb2" "\xd0" "\xb0" "\xd1" "\x82" "\xd1" "\x8c" " (Ctrl+D)")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\xa4" "\x87" "\xe8" "\xa3" "\xbd" " (Ctrl+D)")));
        add("DISSOLVE", "Dissolve & Reconnect (Shift+Del)",
            juce::String(juce::CharPointer_UTF8("\xc3" "\x87" "\xc3" "\xb6" "z ve Yeniden Ba" "\xc4" "\x9f" "la (Shift+Del)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa0" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xb2" "\xd0" "\xbe" "\xd1" "\x80" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xb8" " " "\xd1" "\x81" "\xd0" "\xbe" "\xd0" "\xb5" "\xd0" "\xb4" "\xd0" "\xb8" "\xd0" "\xbd" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " (Shift+Del)")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\xba" "\xb6" "\xe8" "\xa7" "\xa3" "\xe3" "\x81" "\x97" "\xe3" "\x81" "\xa6" "\xe5" "\x86" "\x8d" "\xe6" "\x8e" "\xa5" "\xe7" "\xb6" "\x9a" " (Shift+Del)")));
        add("DISSOLVE_RECONNECT", "Dissolve & Reconnect (Shift+Del)",
            juce::String(juce::CharPointer_UTF8("\xc3" "\x87" "\xc3" "\xb6" "z ve Yeniden Ba" "\xc4" "\x9f" "la (Shift+Del)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa0" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xb2" "\xd0" "\xbe" "\xd1" "\x80" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xb8" " " "\xd1" "\x81" "\xd0" "\xbe" "\xd0" "\xb5" "\xd0" "\xb4" "\xd0" "\xb8" "\xd0" "\xbd" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " (Shift+Del)")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\xba" "\xb6" "\xe8" "\xa7" "\xa3" "\xe3" "\x81" "\x97" "\xe3" "\x81" "\xa6" "\xe5" "\x86" "\x8d" "\xe6" "\x8e" "\xa5" "\xe7" "\xb6" "\x9a" " (Shift+Del)")));
        add("ENABLE_MODULE", "Enable Module",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3" "\xbc" "l" "\xc3" "\xbc" " Etkinle" "\xc5" "\x9f" "tir")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd0" "\xba" "\xd0" "\xbb" "\xd1" "\x8e" "\xd1" "\x87" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xb4" "\xd1" "\x83" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe6" "\x9c" "\x89" "\xe5" "\x8a" "\xb9" "\xe5" "\x8c" "\x96")));
        add("BYPASS_MODULE", "Bypass Module",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3" "\xbc" "l" "\xc3" "\xbc" " Devre D" "\xc4" "\xb1" "\xc5" "\x9f" "\xc4" "\xb1" " B" "\xc4" "\xb1" "rak")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x82" "\xd0" "\xba" "\xd0" "\xbb" "\xd1" "\x8e" "\xd1" "\x87" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xb4" "\xd1" "\x83" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe3" "\x83" "\x90" "\xe3" "\x82" "\xa4" "\xe3" "\x83" "\x91" "\xe3" "\x82" "\xb9")));
        add("SET_COLOR", "Set Color",
            juce::String(juce::CharPointer_UTF8("Renk Se" "\xc3" "\xa7")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd1" "\x8b" "\xd0" "\xb1" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd1" "\x86" "\xd0" "\xb2" "\xd0" "\xb5" "\xd1" "\x82")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\x89" "\xb2" "\xe3" "\x82" "\x92" "\xe8" "\xa8" "\xad" "\xe5" "\xae" "\x9a")));
        add("DISCONNECT_CABLE", "Disconnect Cable",
            juce::String(juce::CharPointer_UTF8("Kabloyu S" "\xc3" "\xb6" "k")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x82" "\xd0" "\xba" "\xd0" "\xbb" "\xd1" "\x8e" "\xd1" "\x87" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xba" "\xd0" "\xb0" "\xd0" "\xb1" "\xd0" "\xb5" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb1" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x96" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe5" "\x88" "\x87" "\xe6" "\x96" "\xad")));
        add("STRAIGHTEN_CABLE", "Straighten Cable (Reset Route)",
            juce::String(juce::CharPointer_UTF8("Kabloyu D" "\xc3" "\xbc" "zelt")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd1" "\x8b" "\xd0" "\xbf" "\xd1" "\x80" "\xd1" "\x8f" "mi" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xba" "\xd0" "\xb0" "\xd0" "\xb1" "\xd0" "\xb5" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb1" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x96" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe6" "\x95" "\xb4" "\xe5" "\x88" "\x97")));
        add("RESET_ROUTE", "Straighten Cable (Reset Route)",
            juce::String(juce::CharPointer_UTF8("Kabloyu D" "\xc3" "\xbc" "zelt")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd1" "\x8b" "\xd0" "\xbf" "\xd1" "\x80" "\xd1" "\x8f" "\xd0" "\xbc" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xba" "\xd0" "\xb0" "\xd0" "\xb1" "\xd0" "\xb5" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb1" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x96" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe6" "\x95" "\xb4" "\xe5" "\x88" "\x97")));
        add("INSERT_MODULE", "Insert Module",
            juce::String(juce::CharPointer_UTF8("Araya Mod" "\xc3" "\xbc" "l Ekle")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xb4" "\xd1" "\x83" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe6" "\x8c" "\xbf" "\xe5" "\x85" "\xa5")));
        add("ADD_MODULE", "Add Module",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3" "\xbc" "l Ekle")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\xbe" "\xd0" "\xb1" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xb4" "\xd1" "\x83" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe8" "\xbf" "\xbd" "\xe5" "\x8a" "\xa0")));
        add("RESET_VIEW", "Reset View (100%)",
            juce::String(juce::CharPointer_UTF8("G" "\xc3" "\xb6" "r" "\xc3" "\xbc" "n" "\xc3" "\xbc" "m" "\xc3" "\xbc" " S" "\xc4" "\xb1" "f" "\xc4" "\xb1" "rla (%100)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xb1" "\xd1" "\x80" "\xd0" "\xbe" "\xd1" "\x81" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbc" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x88" "\xd1" "\x82" "\xd0" "\xb0" "\xd0" "\xb1" " (100%)")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\xa1" "\xa8" "\xe7" "\xa4" "\xba" "\xe3" "\x82" "\x92" "\xe3" "\x83" "\xaa" "\xe3" "\x82" "\xbb" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x88" " (100%)")));
        add("ZOOM_IN", "Zoom In",
            juce::String(juce::CharPointer_UTF8("Yak" "\xc4" "\xb1" "nla" "\xc5" "\x9f")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd1" "\x80" "\xd0" "\xb8" "\xd0" "\xb1" "\xd0" "\xbb" "\xd0" "\xb8" "\xd0" "\xb7" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\x8b" "\xa1" "\xe5" "\xa4" "\xa7")));
        add("ZOOM_OUT", "Zoom Out",
            juce::String(juce::CharPointer_UTF8("Uzakla" "\xc5" "\x9f")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x82" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xbb" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe7" "\xb8" "\xae" "\xe5" "\xb0" "\x8f")));
        add("ADD_STICKY_NOTE", "Add Sticky Note",
            juce::String(juce::CharPointer_UTF8("Yap" "\xc4" "\xb1" "\xc5" "\x9f" "kan Not Ekle")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\xbe" "\xd0" "\xb1" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xb7" "\xd0" "\xb0" "\xd0" "\xbc" "\xd0" "\xb5" "\xd1" "\x82" "\xd0" "\xba" "\xd1" "\x83")),
            juce::String(juce::CharPointer_UTF8("\xe4" "\xbb" "\x98" "\xe7" "\xae" "\x8b" "\xe3" "\x82" "\x92" "\xe8" "\xbf" "\xbd" "\xe5" "\x8a" "\xa0")));
        add("REMOVE_NODE", "Remove (Delete)",
            juce::String(juce::CharPointer_UTF8("Kald" "\xc4" "\xb1" "r (Sil)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa3" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xbb" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x89" "\x8a" "\xe9" "\x99" "\xa4")));
        add("COLOR_GREY", "Grey (Default)",
            juce::String(juce::CharPointer_UTF8("Gri (Varsay" "\xc4" "\xb1" "lan)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xb5" "\xd1" "\x80" "\xd1" "\x8b" "\xd0" "\xb9" " (" "\xd0" "\x9f" "\xd0" "\xbe" " " "\xd1" "\x83" "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xbb" "\xd1" "\x87" ".)")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb0" "\xe3" "\x83" "\xac" "\xe3" "\x83" "\xbc" " (" "\xe6" "\xa8" "\x99" "\xe6" "\xba" "\x96" ")")));
        add("COLOR_RED", "Red",
            juce::String(juce::CharPointer_UTF8("K" "\xc4" "\xb1" "rm" "\xc4" "\xb1" "z" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9a" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x81" "\xd0" "\xbd" "\xd1" "\x8b" "\xd0" "\xb9")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xac" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x89")));
        add("COLOR_BLUE", "Blue",
            "Mavi",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xb8" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb9")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x96" "\xe3" "\x83" "\xab" "\xe3" "\x83" "\xbc")));
        add("COLOR_YELLOW", "Yellow",
            juce::String(juce::CharPointer_UTF8("Sar" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x96" "\xd0" "\xb5" "\xd0" "\xbb" "\xd1" "\x82" "\xd1" "\x8b" "\xd0" "\xb9")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xa4" "\xe3" "\x82" "\xa8" "\xe3" "\x83" "\xad" "\xe3" "\x83" "\xbc")));
        add("COLOR_GREEN", "Green",
            juce::String(juce::CharPointer_UTF8("Ye" "\xc5" "\x9f" "il")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x97" "\xd0" "\xb5" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd1" "\x8b" "\xd0" "\xb9")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb0" "\xe3" "\x83" "\xaa" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xb3")));
        add("COLOR_PURPLE", "Purple",
            "Mor",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa4" "\xd0" "\xb8" "\xd0" "\xbe" "\xd0" "\xbb" "\xd0" "\xb5" "\xd1" "\x82" "\xd0" "\xbe" "\xd0" "\xb2" "\xd1" "\x8b" "\xd0" "\xb9")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x91" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x97" "\xe3" "\x83" "\xab")));
        add("COLOR_ORANGE", "Orange",
            "Turuncu",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xbd" "\xd0" "\xb6" "\xd0" "\xb5" "\xd0" "\xb2" "\xd1" "\x8b" "\xd0" "\xb9")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xaa" "\xe3" "\x83" "\xac" "\xe3" "\x83" "\xb3" "\xe3" "\x82" "\xb8")));
        add("TAB_SETTINGS", "Settings",
            "Ayarlar",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9d" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x82" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb9" "\xd0" "\xba" "\xd0" "\xb8")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\xa8" "\xad" "\xe5" "\xae" "\x9a")));
        add("TAB_SHORTCUTS", "Shortcuts",
            juce::String(juce::CharPointer_UTF8("K" "\xc4" "\xb1" "sayollar")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x93" "\xd0" "\xbe" "\xd1" "\x80" "\xd1" "\x8f" "\xd1" "\x87" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd0" "\xba" "\xd0" "\xbb" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xb8" "\xd1" "\x88" "\xd0" "\xb8")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb7" "\xe3" "\x83" "\xa7" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x88" "\xe3" "\x82" "\xab" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x88")));
        add("TAB_ABOUT", "About",
            juce::String(juce::CharPointer_UTF8("Hakk" "\xc4" "\xb1" "nda")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" " " "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb3" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xbc" "\xd0" "\xbc" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\x83" "\x85" "\xe5" "\xa0" "\xb1")));
        add("LANGUAGE", "Language",
            "Dil",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xaf" "\xd0" "\xb7" "\xd1" "\x8b" "\xd0" "\xba")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\xa8" "\x80" "\xe8" "\xaa" "\x9e")));
        add("CLOSE", "Close",
            "Kapat",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x97" "\xd0" "\xb0" "\xd0" "\xba" "\xd1" "\x80" "\xd1" "\x8b" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe9" "\x96" "\x89" "\xe3" "\x81" "\x98" "\xe3" "\x82" "\x8b")));
        add("PRESET_FOLDER", "Preset Library Folder",
            juce::String(juce::CharPointer_UTF8("Haz" "\xc4" "\xb1" "r Ayar Klas" "\xc3" "\xb6" "r" "\xc3" "\xbc")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xb0" "\xd0" "\xbf" "\xd0" "\xba" "\xd0" "\xb0" " " "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xb5" "\xd1" "\x81" "\xd0" "\xb5" "\xd1" "\x82" "\xd0" "\xbe" "\xd0" "\xb2")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x97" "\xe3" "\x83" "\xaa" "\xe3" "\x82" "\xbb" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x88" "\xe3" "\x83" "\x95" "\xe3" "\x82" "\xa9" "\xe3" "\x83" "\xab" "\xe3" "\x83" "\x80")));
        add("OPEN_EXPLORER", "Open in Explorer",
            juce::String(juce::CharPointer_UTF8("Klas" "\xc3" "\xb6" "r" "\xc3" "\xbc" " A" "\xc3" "\xa7")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x82" "\xd0" "\xba" "\xd1" "\x80" "\xd1" "\x8b" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xb2" " " "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb2" "\xd0" "\xbe" "\xd0" "\xb4" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xba" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xa8" "\xe3" "\x82" "\xaf" "\xe3" "\x82" "\xb9" "\xe3" "\x83" "\x97" "\xe3" "\x83" "\xad" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xa9" "\xe3" "\x83" "\xbc" "\xe3" "\x81" "\xa7" "\xe9" "\x96" "\x8b" "\xe3" "\x81" "\x8f")));
        add("CHANGE_FOLDER", "Change Folder...",
            juce::String(juce::CharPointer_UTF8("Klas" "\xc3" "\xb6" "r" "\xc3" "\xbc" " De" "\xc4" "\x9f" "i" "\xc5" "\x9f" "tir...")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x98" "\xd0" "\xb7" "\xd0" "\xbc" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbf" "\xd0" "\xb0" "\xd0" "\xbf" "\xd0" "\xba" "\xd1" "\x83" "...")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x95" "\xe3" "\x82" "\xa9" "\xe3" "\x83" "\xab" "\xe3" "\x83" "\x80" "\xe3" "\x82" "\x92" "\xe5" "\xa4" "\x89" "\xe6" "\x9b" "\xb4" "...")));
        add("SNAP_TOGGLE", "Enable Canvas Grid Snapping",
            juce::String(juce::CharPointer_UTF8("Izgara Hizalamay" "\xc4" "\xb1" " Etkinle" "\xc5" "\x9f" "tir")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd0" "\xba" "\xd0" "\xbb" "\xd1" "\x8e" "\xd1" "\x87" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xb8" "\xd0" "\xb2" "\xd1" "\x8f" "\xd0" "\xb7" "\xd0" "\xba" "\xd1" "\x83" " " "\xd0" "\xba" " " "\xd1" "\x81" "\xd0" "\xb5" "\xd1" "\x82" "\xd0" "\xba" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb0" "\xe3" "\x83" "\xaa" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x89" "\xe3" "\x82" "\xb9" "\xe3" "\x83" "\x8a" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x97" "\xe3" "\x82" "\x92" "\xe6" "\x9c" "\x89" "\xe5" "\x8a" "\xb9" "\xe5" "\x8c" "\x96")));
        add("MINIMAP_TOGGLE", "Show Canvas Minimap HUD (Navigator)",
            juce::String(juce::CharPointer_UTF8("Navigator Haritas" "\xc4" "\xb1" "n" "\xc4" "\xb1" " G" "\xc3" "\xb6" "ster")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xbe" "\xd0" "\xba" "\xd0" "\xb0" "\xd0" "\xb7" "\xd1" "\x8b" "\xd0" "\xb2" "\xd0" "\xb0" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbd" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xb8" "\xd0" "\xb3" "\xd0" "\xb0" "\xd1" "\x82" "\xd0" "\xbe" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x9f" "\xe3" "\x83" "\x8b" "\xe3" "\x83" "\x9e" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x97" "\xe3" "\x82" "\x92" "\xe8" "\xa1" "\xa8" "\xe7" "\xa4" "\xba")));
        add("GLOW_TOGGLE", "Enable Cable Specular Glow Effect",
            juce::String(juce::CharPointer_UTF8("Kablo Parlama Efektini Etkinle" "\xc5" "\x9f" "tir")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd0" "\xba" "\xd0" "\xbb" "\xd1" "\x8e" "\xd1" "\x87" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd1" "\x81" "\xd0" "\xb2" "\xd0" "\xb5" "\xd1" "\x87" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd0" "\xba" "\xd0" "\xb0" "\xd0" "\xb1" "\xd0" "\xb5" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xb9")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb1" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x96" "\xe3" "\x83" "\xab" "\xe3" "\x81" "\xae" "\xe7" "\x99" "\xba" "\xe5" "\x85" "\x89" "\xe3" "\x82" "\x92" "\xe6" "\x9c" "\x89" "\xe5" "\x8a" "\xb9" "\xe5" "\x8c" "\x96")));
        add("RESET_VIEW_BTN", "Reset View & Zoom to 100%",
            juce::String(juce::CharPointer_UTF8("G" "\xc3" "\xb6" "r" "\xc3" "\xbc" "n" "\xc3" "\xbc" "m" "\xc3" "\xbc" " S" "\xc4" "\xb1" "f" "\xc4" "\xb1" "rla (%100)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xb1" "\xd1" "\x80" "\xd0" "\xbe" "\xd1" "\x81" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xbc" "\xd0" "\xb0" "\xd1" "\x81" "\xd1" "\x88" "\xd1" "\x82" "\xd0" "\xb0" "\xd0" "\xb1" " (100%)")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\xa1" "\xa8" "\xe7" "\xa4" "\xba" "\xe3" "\x82" "\x92" "\xe3" "\x83" "\xaa" "\xe3" "\x82" "\xbb" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x88" " (100%)")));
        add("CLEAR_GRAPH_BTN", "Clear All Canvas Modules",
            juce::String(juce::CharPointer_UTF8("T" "\xc3" "\xbc" "m Mod" "\xc3" "\xbc" "lleri Temizle")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd1" "\x87" "\xd0" "\xb8" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c" " " "\xd0" "\xb2" "\xd0" "\xb5" "\xd1" "\x81" "\xd1" "\x8c" " " "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xbb" "\xd1" "\x81" "\xd1" "\x82")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x81" "\x99" "\xe3" "\x81" "\xb9" "\xe3" "\x81" "\xa6" "\xe3" "\x81" "\xae" "\xe3" "\x83" "\xa2" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xab" "\xe3" "\x82" "\x92" "\xe3" "\x82" "\xaf" "\xe3" "\x83" "\xaa" "\xe3" "\x82" "\xa2")));
        add("CANVAS_WORKFLOW", "Canvas Workflow & Aesthetics",
            juce::String(juce::CharPointer_UTF8("Tuval Ak" "\xc4" "\xb1" "\xc5" "\x9f" "\xc4" "\xb1" " & G" "\xc3" "\xb6" "r" "\xc3" "\xbc" "n" "\xc3" "\xbc" "m")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa0" "\xd0" "\xb0" "\xd0" "\xb1" "\xd0" "\xbe" "\xd1" "\x87" "\xd0" "\xb8" "\xd0" "\xb9" " " "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xbe" "\xd1" "\x86" "\xd0" "\xb5" "\xd1" "\x81" "\xd1" "\x81" " " "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xbb" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xad" "\xe3" "\x83" "\xa3" "\xe3" "\x83" "\xb3" "\xe3" "\x83" "\x90" "\xe3" "\x82" "\xb9" "\xe6" "\x93" "\x8d" "\xe4" "\xbd" "\x9c" "\xe3" "\x81" "\xa8" "\xe5" "\xa4" "\x96" "\xe8" "\xa6" "\xb3")));
        add("DSP_TELEMETRY", "DSP Engine Telemetry",
            "DSP Motor Telemetrisi",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa2" "\xd0" "\xb5" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbc" "\xd0" "\xb5" "\xd1" "\x82" "\xd1" "\x80" "\xd0" "\xb8" "\xd1" "\x8f" " DSP " "\xd0" "\xb4" "\xd0" "\xb2" "\xd0" "\xb8" "\xd0" "\xb6" "\xd0" "\xba" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("DSP" "\xe3" "\x82" "\xa8" "\xe3" "\x83" "\xb3" "\xe3" "\x82" "\xb8" "\xe3" "\x83" "\xb3" "\xe6" "\x83" "\x85" "\xe5" "\xa0" "\xb1")));
        add("SHORTCUTS_TITLE", "Keyboard Shortcuts & Canvas Controls",
            juce::String(juce::CharPointer_UTF8("Klavye K" "\xc4" "\xb1" "sayollar" "\xc4" "\xb1" " & Tuval Kontrolleri")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x93" "\xd0" "\xbe" "\xd1" "\x80" "\xd1" "\x8f" "\xd1" "\x87" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd0" "\xba" "\xd0" "\xbb" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xb8" "\xd1" "\x88" "\xd0" "\xb8" " " "\xd0" "\xb8" " " "\xd1" "\x83" "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xad" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x9c" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x89" "\xe3" "\x82" "\xb7" "\xe3" "\x83" "\xa7" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x88" "\xe3" "\x82" "\xab" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x88")));
        add("DUAL_COMPARE", "Dual Compare",
            juce::String(juce::CharPointer_UTF8("\xc3" "\x87" "ift Kar" "\xc5" "\x9f" "\xc4" "\xb1" "la" "\xc5" "\x9f" "t" "\xc4" "\xb1" "rma")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xbd" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x87" "\xe3" "\x83" "\xa5" "\xe3" "\x82" "\xa2" "\xe3" "\x83" "\xab" "\xe6" "\xaf" "\x94" "\xe8" "\xbc" "\x83")));
        add("INPUT_ONLY", "Input Only",
            juce::String(juce::CharPointer_UTF8("Yaln" "\xc4" "\xb1" "zca Giri" "\xc5" "\x9f")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa2" "\xd0" "\xbe" "\xd0" "\xbb" "\xd1" "\x8c" "\xd0" "\xba" "\xd0" "\xbe" " " "\xd0" "\xb2" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xb4")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x85" "\xa5" "\xe5" "\x8a" "\x9b" "\xe3" "\x81" "\xae" "\xe3" "\x81" "\xbf")));
        add("OUTPUT_ONLY", "Output Only",
            juce::String(juce::CharPointer_UTF8("Yaln" "\xc4" "\xb1" "zca " "\xc3" "\x87" "\xc4" "\xb1" "k" "\xc4" "\xb1" "\xc5" "\x9f")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa2" "\xd0" "\xbe" "\xd0" "\xbb" "\xd1" "\x8c" "\xd0" "\xba" "\xd0" "\xbe" " " "\xd0" "\xb2" "\xd1" "\x8b" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xb4")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x87" "\xba" "\xe5" "\x8a" "\x9b" "\xe3" "\x81" "\xae" "\xe3" "\x81" "\xbf")));
        add("DELTA_IMPACT", "Delta / Impact",
            "Dinamik Fark",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa0" "\xd0" "\xb0" "\xd0" "\xb7" "\xd0" "\xbd" "\xd0" "\xb8" "\xd1" "\x86" "\xd0" "\xb0" " / " "\xd0" "\xad" "\xd1" "\x84" "\xd1" "\x84" "\xd0" "\xb5" "\xd0" "\xba" "\xd1" "\x82")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\xb7" "\xae" "\xe5" "\x88" "\x86" " / " "\xe5" "\xbd" "\xb1" "\xe9" "\x9f" "\xbf")));
        add("SPECTRUM_FFT", "FFT Spectrum",
            "FFT Spektrum",
            juce::String(juce::CharPointer_UTF8("FFT " "\xd0" "\xa1" "\xd0" "\xbf" "\xd0" "\xb5" "\xd0" "\xba" "\xd1" "\x82" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("FFT" "\xe3" "\x82" "\xb9" "\xe3" "\x83" "\x9a" "\xe3" "\x82" "\xaf" "\xe3" "\x83" "\x88" "\xe3" "\x83" "\xa9" "\xe3" "\x83" "\xa0")));
        add("FREEZE", "Freeze",
            "Dondur",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x97" "\xd0" "\xb0" "\xd0" "\xbc" "\xd0" "\xbe" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb7" "\xd0" "\xb8" "\xd1" "\x82" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x95" "\xe3" "\x83" "\xaa" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xba")));
        add("FROZEN", "FROZEN (Hold)",
            "DONDURULDU",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x97" "\xd0" "\x90" "\xd0" "\x9c" "\xd0" "\x9e" "\xd0" "\xa0" "\xd0" "\x9e" "\xd0" "\x96" "\xd0" "\x95" "\xd0" "\x9d" "\xd0" "\x9e")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x9b" "\xba" "\xe5" "\xae" "\x9a" "\xe4" "\xb8" "\xad")));
        add("IN_LABEL", "IN",
            juce::String(juce::CharPointer_UTF8("G" "\xc4" "\xb0" "R" "\xc4" "\xb0" "\xc5" "\x9e")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd0" "\xa5" "\xd0" "\x9e" "\xd0" "\x94")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x85" "\xa5" "\xe5" "\x8a" "\x9b")));
        add("OUT_LABEL", "OUT",
            juce::String(juce::CharPointer_UTF8("\xc3" "\x87" "IK")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x92" "\xd0" "\xab" "\xd0" "\xa5")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x87" "\xba" "\xe5" "\x8a" "\x9b")));
        add("RMS_LABEL", "RMS",
            "RMS",
            "RMS",
            "RMS");
        add("CREST_LABEL", "CREST",
            "TEPE/ORT",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\x98" "\xd0" "\x9a" "/" "\xd0" "\xa1" "\xd0" "\xa0")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\xb3" "\xa2" "\xe9" "\xab" "\x98" "\xe6" "\xaf" "\x94")));
        add("DELTA_LABEL", "DELTA",
            "FARK",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\x95" "\xd0" "\x9b" "\xd0" "\xac" "\xd0" "\xa2" "\xd0" "\x90")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\xb7" "\xae" "\xe5" "\x88" "\x86")));
        add("COMPRESSION_DESC", "Dynamic Gain Compression",
            juce::String(juce::CharPointer_UTF8("Dinamik S" "\xc4" "\xb1" "k" "\xc4" "\xb1" "\xc5" "\x9f" "t" "\xc4" "\xb1" "rma")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\xb8" "\xd0" "\xbd" "\xd0" "\xb0" "\xd0" "\xbc" "\xd0" "\xb8" "\xd1" "\x87" "\xd0" "\xb5" "\xd1" "\x81" "\xd0" "\xba" "\xd0" "\xb0" "\xd1" "\x8f" " " "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xbc" "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xb5" "\xd1" "\x81" "\xd1" "\x81" "\xd0" "\xb8" "\xd1" "\x8f")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x80" "\xe3" "\x82" "\xa4" "\xe3" "\x83" "\x8a" "\xe3" "\x83" "\x9f" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xaf" "\xe5" "\x9c" "\xa7" "\xe7" "\xb8" "\xae")));
        add("BOOST_DESC", "Harmonic Boost & Saturation",
            "Harmonik Takviye & Doygunluk",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x93" "\xd0" "\xb0" "\xd1" "\x80" "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xbd" "\xd0" "\xb8" "\xd1" "\x87" "\xd0" "\xb5" "\xd1" "\x81" "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xb5" " " "\xd1" "\x83" "\xd1" "\x81" "\xd0" "\xb8" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x8f" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\xa2" "\xe3" "\x83" "\x8b" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xaf" "\xe5" "\xa2" "\x97" "\xe5" "\xb9" "\x85")));
        add("TRANSPARENT_DESC", "Linear Passthrough",
            juce::String(juce::CharPointer_UTF8("Do" "\xc4" "\x9f" "rusal Ge" "\xc3" "\xa7" "i" "\xc5" "\x9f")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9b" "\xd0" "\xb8" "\xd0" "\xbd" "\xd0" "\xb5" "\xd0" "\xb9" "\xd0" "\xbd" "\xd1" "\x8b" "\xd0" "\xb9" " " "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xbe" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xb4")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xaa" "\xe3" "\x83" "\x8b" "\xe3" "\x82" "\xa2" "\xe9" "\x80" "\x9a" "\xe9" "\x81" "\x8e")));
        add("Gain", "Gain",
            juce::String(juce::CharPointer_UTF8("Kazan" "\xc3" "\xa7")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa3" "\xd1" "\x81" "\xd0" "\xb8" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb2" "\xe3" "\x82" "\xa4" "\xe3" "\x83" "\xb3")));
        add("Gain_Sub", "Transparent Output Leveling",
            juce::String(juce::CharPointer_UTF8("\xc5" "\x9e" "effaf " "\xc3" "\x87" "\xc4" "\xb1" "k" "\xc4" "\xb1" "\xc5" "\x9f" " Seviyeleme")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd1" "\x80" "\xd0" "\xbe" "\xd0" "\xb7" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x87" "\xd0" "\xbd" "\xd1" "\x8b" "\xd0" "\xb9" " " "\xd0" "\xb2" "\xd1" "\x8b" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xb4")),
            juce::String(juce::CharPointer_UTF8("\xe9" "\x80" "\x8f" "\xe6" "\x98" "\x8e" "\xe3" "\x81" "\xaa" "\xe5" "\x87" "\xba" "\xe5" "\x8a" "\x9b" "\xe8" "\xaa" "\xbf" "\xe6" "\x95" "\xb4")));
        add("Compressor", "Compressor",
            juce::String(juce::CharPointer_UTF8("Kompres" "\xc3" "\xb6" "r")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9a" "\xd0" "\xbe" "\xd0" "\xbc" "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xb5" "\xd1" "\x81" "\xd1" "\x81" "\xd0" "\xbe" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb3" "\xe3" "\x83" "\xb3" "\xe3" "\x83" "\x97" "\xe3" "\x83" "\xac" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xb5" "\xe3" "\x83" "\xbc")));
        add("Compressor_Sub", "Vocal Opto-Style Compression",
            juce::String(juce::CharPointer_UTF8("Vokal Opto Tipi S" "\xc4" "\xb1" "k" "\xc4" "\xb1" "\xc5" "\x9f" "t" "\xc4" "\xb1" "rma")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd0" "\xbf" "\xd1" "\x82" "\xd0" "\xb8" "\xd1" "\x87" "\xd0" "\xb5" "\xd1" "\x81" "\xd0" "\xba" "\xd0" "\xb0" "\xd1" "\x8f" " " "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xbc" "\xd0" "\xbf" "\xd1" "\x80" "\xd0" "\xb5" "\xd1" "\x81" "\xd1" "\x81" "\xd0" "\xb8" "\xd1" "\x8f")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xaa" "\xe3" "\x83" "\x97" "\xe3" "\x83" "\x88" "\xe9" "\xa2" "\xa8" "\xe5" "\x9c" "\xa7" "\xe7" "\xb8" "\xae")));
        add("Limiter", "Limiter",
            "Limitleyici",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9b" "\xd0" "\xb8" "\xd0" "\xbc" "\xd0" "\xb8" "\xd1" "\x82" "\xd0" "\xb5" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\xaa" "\xe3" "\x83" "\x9f" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xbf" "\xe3" "\x83" "\xbc")));
        add("Limiter_Sub", "Brickwall Peak Limiter",
            juce::String(juce::CharPointer_UTF8("Kesin Tepe Noktas" "\xc4" "\xb1" " Limitleyici")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xb8" "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xb2" "\xd1" "\x8b" "\xd0" "\xb9" " " "\xd0" "\xbb" "\xd0" "\xb8" "\xd0" "\xbc" "\xd0" "\xb8" "\xd1" "\x82" "\xd0" "\xb5" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x94" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xaf" "\xe3" "\x83" "\xaa" "\xe3" "\x83" "\x9f" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xbf" "\xe3" "\x83" "\xbc")));
        add("Gate", "Gate",
            juce::String(juce::CharPointer_UTF8("G" "\xc3" "\xbc" "r" "\xc3" "\xbc" "lt" "\xc3" "\xbc" " Kap" "\xc4" "\xb1" "s" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x93" "\xd0" "\xb5" "\xd0" "\xb9" "\xd1" "\x82")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb2" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x88")));
        add("Gate_Sub", "Expander & Downward Gate",
            juce::String(juce::CharPointer_UTF8("Geni" "\xc5" "\x9f" "letici & Alt E" "\xc5" "\x9f" "ik Kap" "\xc4" "\xb1" "s" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xad" "\xd0" "\xba" "\xd1" "\x81" "\xd0" "\xbf" "\xd0" "\xb0" "\xd0" "\xbd" "\xd0" "\xb4" "\xd0" "\xb5" "\xd1" "\x80" " / " "\xd0" "\xb3" "\xd0" "\xb5" "\xd0" "\xb9" "\xd1" "\x82")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xa8" "\xe3" "\x82" "\xaf" "\xe3" "\x82" "\xb9" "\xe3" "\x83" "\x91" "\xe3" "\x83" "\xb3" "\xe3" "\x83" "\x80" "\xe3" "\x83" "\xbc")));
        add("AGC", "AGC",
            juce::String(juce::CharPointer_UTF8("Otomatik Kazan" "\xc3" "\xa7" " (AGC)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x90" "\xd0" "\xa0" "\xd0" "\xa3" " (AGC)")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\x87" "\xaa" "\xe5" "\x8b" "\x95" "\xe3" "\x82" "\xb2" "\xe3" "\x82" "\xa4" "\xe3" "\x83" "\xb3" "\xe5" "\x88" "\xb6" "\xe5" "\xbe" "\xa1")));
        add("AGC_Sub", "Automatic Broadcast Gain",
            juce::String(juce::CharPointer_UTF8("Otomatik Yay" "\xc4" "\xb1" "n Seviyesi")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x90" "\xd0" "\xb2" "\xd1" "\x82" "\xd0" "\xbe" "\xd0" "\xbc" "\xd0" "\xb0" "\xd1" "\x82" "\xd0" "\xb8" "\xd1" "\x87" "\xd0" "\xb5" "\xd1" "\x81" "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xb5" " " "\xd1" "\x83" "\xd1" "\x81" "\xd0" "\xb8" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe8" "\x87" "\xaa" "\xe5" "\x8b" "\x95" "\xe3" "\x83" "\xac" "\xe3" "\x83" "\x99" "\xe3" "\x83" "\xab" "\xe8" "\xaa" "\xbf" "\xe6" "\x95" "\xb4")));
        add("Dynamic EQ", "Dynamic EQ",
            "Dinamik Ekolayzer",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\xb8" "\xd0" "\xbd" "\xd0" "\xb0" "\xd0" "\xbc" "\xd0" "\xb8" "\xd1" "\x87" "\xd0" "\xb5" "\xd1" "\x81" "\xd0" "\xba" "\xd0" "\xb8" "\xd0" "\xb9" " EQ")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x80" "\xe3" "\x82" "\xa4" "\xe3" "\x83" "\x8a" "\xe3" "\x83" "\x9f" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xaf" "EQ")));
        add("Dynamic EQ_Sub", "Targeted Frequency Control",
            juce::String(juce::CharPointer_UTF8("Hedef Frekans Kontrol" "\xc3" "\xbc")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa2" "\xd0" "\xbe" "\xd1" "\x87" "\xd0" "\xb5" "\xd1" "\x87" "\xd0" "\xbd" "\xd0" "\xb0" "\xd1" "\x8f" " " "\xd0" "\xba" "\xd0" "\xbe" "\xd1" "\x80" "\xd1" "\x80" "\xd0" "\xb5" "\xd0" "\xba" "\xd1" "\x86" "\xd0" "\xb8" "\xd1" "\x8f")),
            juce::String(juce::CharPointer_UTF8("\xe7" "\x89" "\xb9" "\xe5" "\xae" "\x9a" "\xe5" "\x91" "\xa8" "\xe6" "\xb3" "\xa2" "\xe6" "\x95" "\xb0" "\xe8" "\xa3" "\x9c" "\xe6" "\xad" "\xa3")));
        add("De-Esser", "De-Esser",
            juce::String(juce::CharPointer_UTF8("Isl" "\xc4" "\xb1" "k Giderici")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\xb5" "\xd1" "\x8d" "\xd1" "\x81" "\xd1" "\x81" "\xd0" "\xb5" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x87" "\xe3" "\x82" "\xa3" "\xe3" "\x82" "\xa8" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xb5" "\xe3" "\x83" "\xbc")));
        add("De-Esser_Sub", "Sibilance & Harshness Reduction",
            juce::String(juce::CharPointer_UTF8("Sert I" "\xc5" "\x9f" "\xc4" "\xb1" "lt" "\xc4" "\xb1" " ve T" "\xc4" "\xb1" "slama Giderici")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa3" "\xd1" "\x81" "\xd1" "\x82" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xbd" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd1" "\x81" "\xd0" "\xb8" "\xd0" "\xb1" "\xd0" "\xb8" "\xd0" "\xbb" "\xd1" "\x8f" "\xd0" "\xbd" "\xd1" "\x82" "\xd0" "\xbe" "\xd0" "\xb2")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\xad" "\xaf" "\xe6" "\x93" "\xa6" "\xe9" "\x9f" "\xb3" "\xe4" "\xbd" "\x8e" "\xe6" "\xb8" "\x9b")));
        add("Noise Suppression", "Noise Suppression",
            juce::String(juce::CharPointer_UTF8("G" "\xc3" "\xbc" "r" "\xc3" "\xbc" "lt" "\xc3" "\xbc" " Engelleme")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xbe" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd1" "\x88" "\xd1" "\x83" "\xd0" "\xbc" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x8e" "\xe3" "\x82" "\xa4" "\xe3" "\x82" "\xba" "\xe6" "\x8a" "\x91" "\xe5" "\x88" "\xb6")));
        add("Noise Suppression_Sub", "Real-Time Spectral Denoise",
            juce::String(juce::CharPointer_UTF8("Ger" "\xc3" "\xa7" "ek Zamanl" "\xc4" "\xb1" " Spektral Filtre")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xbf" "\xd0" "\xb5" "\xd0" "\xba" "\xd1" "\x82" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xbb" "\xd1" "\x8c" "\xd0" "\xbd" "\xd1" "\x8b" "\xd0" "\xb9" " " "\xd1" "\x88" "\xd1" "\x83" "\xd0" "\xbc" "\xd0" "\xbe" "\xd0" "\xbf" "\xd0" "\xbe" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xb8" "\xd1" "\x82" "\xd0" "\xb5" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb9" "\xe3" "\x83" "\x9a" "\xe3" "\x82" "\xaf" "\xe3" "\x83" "\x88" "\xe3" "\x83" "\xab" "\xe3" "\x83" "\x8e" "\xe3" "\x82" "\xa4" "\xe3" "\x82" "\xba" "\xe9" "\x99" "\xa4" "\xe5" "\x8e" "\xbb")));
        add("De-Plosive", "De-Plosive",
            "Patlama Giderici",
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xbe" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd0" "\xb2" "\xd0" "\xb7" "\xd1" "\x80" "\xd1" "\x8b" "\xd0" "\xb2" "\xd0" "\xbd" "\xd1" "\x8b" "\xd1" "\x85")),
            juce::String(juce::CharPointer_UTF8("\xe7" "\xa0" "\xb4" "\xe8" "\xa3" "\x82" "\xe9" "\x9f" "\xb3" "\xe9" "\x99" "\xa4" "\xe5" "\x8e" "\xbb")));
        add("De-Plosive_Sub", "Microphone Pop & Thump Filter",
            "Mikrofon Patlama ve Darbe Filtresi",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa4" "\xd0" "\xb8" "\xd0" "\xbb" "\xd1" "\x8c" "\xd1" "\x82" "\xd1" "\x80" " " "\xd0" "\xb2" "\xd0" "\xb7" "\xd1" "\x80" "\xd1" "\x8b" "\xd0" "\xb2" "\xd0" "\xbd" "\xd1" "\x8b" "\xd1" "\x85" " " "\xd0" "\xb7" "\xd0" "\xb2" "\xd1" "\x83" "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xb2")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x9d" "\xe3" "\x83" "\x83" "\xe3" "\x83" "\x97" "\xe3" "\x83" "\x8e" "\xe3" "\x82" "\xa4" "\xe3" "\x82" "\xba" "\xe9" "\x99" "\xa4" "\xe5" "\x8e" "\xbb")));
        add("De-Click", "De-Click",
            juce::String(juce::CharPointer_UTF8("\xc3" "\x87" "\xc4" "\xb1" "t" "\xc4" "\xb1" "rt" "\xc4" "\xb1" " Giderici")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9f" "\xd0" "\xbe" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd1" "\x89" "\xd0" "\xb5" "\xd0" "\xbb" "\xd1" "\x87" "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xb2")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xaf" "\xe3" "\x83" "\xaa" "\xe3" "\x83" "\x83" "\xe3" "\x82" "\xaf" "\xe9" "\x99" "\xa4" "\xe5" "\x8e" "\xbb")));
        add("De-Click_Sub", "Mouth Click & Transient Repair",
            juce::String(juce::CharPointer_UTF8("A" "\xc4" "\x9f" "\xc4" "\xb1" "z " "\xc3" "\x87" "\xc4" "\xb1" "t" "\xc4" "\xb1" "rt" "\xc4" "\xb1" "s" "\xc4" "\xb1" " Onar" "\xc4" "\xb1" "m" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa3" "\xd1" "\x81" "\xd1" "\x82" "\xd1" "\x80" "\xd0" "\xb0" "\xd0" "\xbd" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd1" "\x89" "\xd0" "\xb5" "\xd0" "\xbb" "\xd1" "\x87" "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xb2")),
            juce::String(juce::CharPointer_UTF8("\xe5" "\x8f" "\xa3" "\xe8" "\x85" "\x94" "\xe3" "\x83" "\x8e" "\xe3" "\x82" "\xa4" "\xe3" "\x82" "\xba" "\xe9" "\x99" "\xa4" "\xe5" "\x8e" "\xbb")));
        add("AEC", "AEC",
            juce::String(juce::CharPointer_UTF8("Yank" "\xc4" "\xb1" " Engelleme (AEC)")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xad" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xbf" "\xd0" "\xbe" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " (AEC)")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xa8" "\xe3" "\x82" "\xb3" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xad" "\xe3" "\x83" "\xa3" "\xe3" "\x83" "\xb3" "\xe3" "\x82" "\xbb" "\xe3" "\x83" "\xa9" "\xe3" "\x83" "\xbc")));
        add("AEC_Sub", "Acoustic Echo Cancellation",
            juce::String(juce::CharPointer_UTF8("Akustik Yank" "\xc4" "\xb1" " Bast" "\xc4" "\xb1" "rma")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x90" "\xd0" "\xba" "\xd1" "\x83" "\xd1" "\x81" "\xd1" "\x82" "\xd0" "\xb8" "\xd1" "\x87" "\xd0" "\xb5" "\xd1" "\x81" "\xd0" "\xba" "\xd0" "\xbe" "\xd0" "\xb5" " " "\xd1" "\x8d" "\xd1" "\x85" "\xd0" "\xbe" "\xd0" "\xbf" "\xd0" "\xbe" "\xd0" "\xb4" "\xd0" "\xb0" "\xd0" "\xb2" "\xd0" "\xbb" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe9" "\x9f" "\xb3" "\xe9" "\x9f" "\xbf" "\xe3" "\x82" "\xa8" "\xe3" "\x82" "\xb3" "\xe3" "\x83" "\xbc" "\xe6" "\x8a" "\x91" "\xe5" "\x88" "\xb6")));
        add("Dereverb", "De-reverb",
            juce::String(juce::CharPointer_UTF8("Yank" "\xc4" "\xb1" " Azalt" "\xc4" "\xb1" "c" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb5" "\xd0" "\xb2" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb1" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x86" "\xd0" "\xb8" "\xd1" "\x8f")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\xae" "\x8b" "\xe9" "\x9f" "\xbf" "\xe9" "\x99" "\xa4" "\xe5" "\x8e" "\xbb")));
        add("De-reverb", "De-reverb",
            juce::String(juce::CharPointer_UTF8("Yank" "\xc4" "\xb1" " Azalt" "\xc4" "\xb1" "c" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x94" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb5" "\xd0" "\xb2" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb1" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x86" "\xd0" "\xb8" "\xd1" "\x8f")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\xae" "\x8b" "\xe9" "\x9f" "\xbf" "\xe9" "\x99" "\xa4" "\xe5" "\x8e" "\xbb")));
        add("Dereverb_Sub", "Room Ambience Attenuation",
            juce::String(juce::CharPointer_UTF8("Oda Yans" "\xc4" "\xb1" "mas" "\xc4" "\xb1" "n" "\xc4" "\xb1" " S" "\xc3" "\xb6" "n" "\xc3" "\xbc" "mleme")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa3" "\xd0" "\xbc" "\xd0" "\xb5" "\xd0" "\xbd" "\xd1" "\x8c" "\xd1" "\x88" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd1" "\x80" "\xd0" "\xb5" "\xd0" "\xb2" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb1" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x86" "\xd0" "\xb8" "\xd0" "\xb8")),
            juce::String(juce::CharPointer_UTF8("\xe9" "\x83" "\xa8" "\xe5" "\xb1" "\x8b" "\xe3" "\x81" "\xae" "\xe6" "\xae" "\x8b" "\xe9" "\x9f" "\xbf" "\xe6" "\x8a" "\x91" "\xe5" "\x88" "\xb6")));
        add("De-reverb_Sub", "Room Ambience Attenuation",
            juce::String(juce::CharPointer_UTF8("Oda Yans" "\xc4" "\xb1" "mas" "\xc4" "\xb1" "n" "\xc4" "\xb1" " S" "\xc3" "\xb6" "n" "\xc3" "\xbc" "mleme")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa3" "\xd0" "\xbc" "\xd0" "\xb5" "\xd0" "\xbd" "\xd1" "\x8c" "\xd1" "\x88" "\xd0" "\xb5" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5" " " "\xd1" "\x80" "\xd0" "\xb5" "\xd0" "\xb2" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb1" "\xd0" "\xb5" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x86" "\xd0" "\xb8" "\xd0" "\xb8")),
            juce::String(juce::CharPointer_UTF8("\xe9" "\x83" "\xa8" "\xe5" "\xb1" "\x8b" "\xe3" "\x81" "\xae" "\xe6" "\xae" "\x8b" "\xe9" "\x9f" "\xbf" "\xe6" "\x8a" "\x91" "\xe5" "\x88" "\xb6")));
        add("Phase Rotator", "Phase Rotator",
            juce::String(juce::CharPointer_UTF8("Faz D" "\xc3" "\xb6" "nd" "\xc3" "\xbc" "r" "\xc3" "\xbc" "c" "\xc3" "\xbc")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa4" "\xd0" "\xb0" "\xd0" "\xb7" "\xd0" "\xbe" "\xd0" "\xb2" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x89" "\xd0" "\xb0" "\xd1" "\x82" "\xd0" "\xb5" "\xd0" "\xbb" "\xd1" "\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x83" "\x95" "\xe3" "\x82" "\xa7" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xba" "\xe3" "\x83" "\xad" "\xe3" "\x83" "\xbc" "\xe3" "\x83" "\x86" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xbf" "\xe3" "\x83" "\xbc")));
        add("Phase Rotator_Sub", "Waveform Symmetry Optimizer",
            juce::String(juce::CharPointer_UTF8("Dalga Bi" "\xc3" "\xa7" "imi Simetri " "\xc4" "\xb0" "yile" "\xc5" "\x9f" "tirici")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\x9e" "\xd0" "\xbf" "\xd1" "\x82" "\xd0" "\xb8" "\xd0" "\xbc" "\xd0" "\xb8" "\xd0" "\xb7" "\xd0" "\xb0" "\xd1" "\x86" "\xd0" "\xb8" "\xd1" "\x8f" " " "\xd1" "\x84" "\xd0" "\xb0" "\xd0" "\xb7" "\xd1" "\x8b" " " "\xd0" "\xb2" "\xd0" "\xbe" "\xd0" "\xba" "\xd0" "\xb0" "\xd0" "\xbb" "\xd0" "\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe4" "\xbd" "\x8d" "\xe7" "\x9b" "\xb8" "\xe5" "\xaf" "\xbe" "\xe7" "\xa7" "\xb0" "\xe6" "\x9c" "\x80" "\xe9" "\x81" "\xa9" "\xe5" "\x8c" "\x96")));
        add("Saturation", "Saturation",
            "Doygunluk",
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa1" "\xd0" "\xb0" "\xd1" "\x82" "\xd1" "\x83" "\xd1" "\x80" "\xd0" "\xb0" "\xd1" "\x86" "\xd0" "\xb8" "\xd1" "\x8f")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xb5" "\xe3" "\x83" "\x81" "\xe3" "\x83" "\xa5" "\xe3" "\x83" "\xac" "\xe3" "\x83" "\xbc" "\xe3" "\x82" "\xb7" "\xe3" "\x83" "\xa7" "\xe3" "\x83" "\xb3")));
        add("Saturation_Sub", "Warm Tube & Tape Character",
            juce::String(juce::CharPointer_UTF8("S" "\xc4" "\xb1" "cak T" "\xc3" "\xbc" "p ve Kaset Karakteri")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xa2" "\xd0" "\xb5" "\xd0" "\xbf" "\xd0" "\xbb" "\xd0" "\xbe" "\xd0" "\xb5" " " "\xd0" "\xbb" "\xd0" "\xb0" "\xd0" "\xbc" "\xd0" "\xbf" "\xd0" "\xbe" "\xd0" "\xb2" "\xd0" "\xbe" "\xd0" "\xb5" " " "\xd0" "\xb7" "\xd0" "\xb2" "\xd1" "\x83" "\xd1" "\x87" "\xd0" "\xb0" "\xd0" "\xbd" "\xd0" "\xb8" "\xd0" "\xb5")),
            juce::String(juce::CharPointer_UTF8("\xe6" "\xb8" "\xa9" "\xe3" "\x81" "\x8b" "\xe3" "\x81" "\xbf" "\xe3" "\x81" "\xae" "\xe3" "\x81" "\x82" "\xe3" "\x82" "\x8b" "\xe7" "\x9c" "\x9f" "\xe7" "\xa9" "\xba" "\xe7" "\xae" "\xa1" "\xe3" "\x82" "\xb5" "\xe3" "\x82" "\xa6" "\xe3" "\x83" "\xb3" "\xe3" "\x83" "\x89")));
        add("Aural Exciter", "Aural Exciter",
            juce::String(juce::CharPointer_UTF8("Harmonik Canland" "\xc4" "\xb1" "r" "\xc4" "\xb1" "c" "\xc4" "\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0" "\xad" "\xd0" "\xba" "\xd1" "\x81" "\xd0" "\xb0" "\xd0" "\xb9" "\xd1" "\x82" "\xd0" "\xb5" "\xd1" "\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3" "\x82" "\xa8" "\xe3" "\x82" "\xad" "\xe3" "\x82" "\xb5" "\xe3" "\x82" "\xa4" "\xe3" "\x82" "\xbf" "\xe3" "\x83" "\xbc")));
        add("Aural Exciter_Sub", "High-End Air & Presence",
            juce::String(juce::CharPointer_UTF8("\xc3\x9c" "st Frekans Parlakl" "\xc4\xb1\xc4\x9f\xc4\xb1" " ve Hava")),
            juce::String(juce::CharPointer_UTF8("\xd0\x92\xd0\xbe\xd0\xb7\xd0\xb4\xd1\x83\xd1\x88\xd0\xbd\xd0\xbe\xd1\x81\xd1\x82\xd1\x8c" " " "\xd0\xb8" " " "\xd1\x8f\xd1\x80\xd0\xba\xd0\xbe\xd1\x81\xd1\x82\xd1\x8c")),
            juce::String(juce::CharPointer_UTF8("\xe7\xa9\xba\xe6\xb0\x97\xe6\x84\x9f\xe3\x81\xa8\xe8\xbc\x9d\xe3\x81\x8d\xe3\x81\xae\xe4\xbb\x98\xe5\x8a\xa0")));
        add("PACK_SELECTED_CONTAINER", "Pack Selected Modules into Container",
            juce::String(juce::CharPointer_UTF8("Se" "\xc3\xa7" "ili Mod" "\xc3\xbc" "lleri Paketle (Container)")),
            juce::String(juce::CharPointer_UTF8("\xd0\xa3\xd0\xbf\xd0\xb0\xd0\xba\xd0\xbe\xd0\xb2\xd0\xb0\xd1\x82\xd1\x8c" " " "\xd0\xb2" " " "\xd0\xba\xd0\xbe\xd0\xbd\xd1\x82\xd0\xb5\xd0\xb9\xd0\xbd\xd0\xb5\xd1\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3\x82\xb3\xe3\x83\xb3\xe3\x83\x86\xe3\x83\x8a\xe3\x81\xab\xe3\x83\x91\xe3\x83\x83\xe3\x82\xaf")));
        add("UNPACK_CONTAINER", "Unpack Container",
            juce::String(juce::CharPointer_UTF8("Container'" "\xc4\xb1" " " "\xc3\x87" "\xc3\xb6" "z (Unpack)")),
            juce::String(juce::CharPointer_UTF8("\xd0\xa0\xd0\xb0\xd1\x81\xd0\xbf\xd0\xb0\xd0\xba\xd0\xbe\xd0\xb2\xd0\xb0\xd1\x82\xd1\x8c" " " "\xd0\xba\xd0\xbe\xd0\xbd\xd1\x82\xd0\xb5\xd0\xb9\xd0\xbd\xd0\xb5\xd1\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3\x82\xb3\xe3\x83\xb3\xe3\x83\x86\xe3\x83\x8a\xe3\x82\x92\xe5\xb1\x95\xe9\x96\x8b")));
        add("Container", "Container",
            juce::String(juce::CharPointer_UTF8("Mod" "\xc3\xbc" "l Paketi (Container)")),
            juce::String(juce::CharPointer_UTF8("\xd0\x9a\xd0\xbe\xd0\xbd\xd1\x82\xd0\xb5\xd0\xb9\xd0\xbd\xd0\xb5\xd1\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3\x82\xb3\xe3\x83\xb3\xe3\x83\x86\xe3\x83\x8a")));
        add("Spectral Clarity", "Spectral Clarity",
            juce::String(juce::CharPointer_UTF8("Ak" "\xc4\xb1" "ll" "\xc4\xb1" " Rezonans Temizleyici")),
            juce::String(juce::CharPointer_UTF8("\xd0\xa1\xd0\xbf\xd0\xb5\xd0\xba\xd1\x82\xd1\x80\xd0\xb0\xd0\xbb\xd1\x8c\xd0\xbd\xd0\xb0\xd1\x8f" " " "\xd1\x87\xd0\xb8\xd1\x81\xd1\x82\xd0\xbe\xd1\x82\xd0\xb0")),
            juce::String(juce::CharPointer_UTF8("\xe3\x82\xb9\xe3\x83\x9a\xe3\x82\xaf\xe3\x83\x88\xe3\x83\xa9\xe3\x83\xab\xe3\x82\xaf\xe3\x83\xa9\xe3\x83\xaa\xe3\x83\x86\xe3\x82\xa3")));
        add("Upward Compressor", "Upward Compressor",
            juce::String(juce::CharPointer_UTF8("Vokal Ayr" "\xc4\xb1" "nt" "\xc4\xb1" " Y" "\xc3\xbc" "kseltici (OTT)")),
            juce::String(juce::CharPointer_UTF8("\xd0\x92\xd0\xbe\xd1\x81\xd1\x85\xd0\xbe\xd0\xb4\xd1\x8f\xd1\x89\xd0\xb8\xd0\xb9" " " "\xd0\xba\xd0\xbe\xd0\xbc\xd0\xbf\xd1\x80\xd0\xb5\xd1\x81\xd1\x81\xd0\xbe\xd1\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3\x82\xa2\xe3\x83\x83\xe3\x83\x97\xe3\x83\xaf\xe3\x83\xbc\xe3\x83\x89\xe3\x82\xb3\xe3\x83\xb3\xe3\x83\x97\xe3\x83\xac\xe3\x83\x83\xe3\x82\xb5\xe3\x83\xbc")));
        add("Crossover Splitter", "Crossover Splitter",
            juce::String(juce::CharPointer_UTF8("Frekans Bant B" "\xc3\xb6" "l" "\xc3\xbc" "c" "\xc3\xbc")),
            juce::String(juce::CharPointer_UTF8("\xd0\x9a\xd1\x80\xd0\xbe\xd1\x81\xd1\x81\xd0\xbe\xd0\xb2\xd0\xb5\xd1\x80" " " "\xd1\x81\xd0\xbf\xd0\xbb\xd0\xb8\xd1\x82\xd1\x82\xd0\xb5\xd1\x80")),
            juce::String(juce::CharPointer_UTF8("\xe3\x82\xaf\xe3\x83\xad\xe3\x82\xb9\xe3\x82\xaa\xe3\x83\xbc\xe3\x83\x90\xe3\x83\xbc\xe3\x82\xb9\xe3\x83\x97\xe3\x83\xaa\xe3\x83\x83\xe3\x82\xbf\xe3\x83\xbc")));
        add("Frequency Splitter", "Frequency Splitter",
            juce::String(juce::CharPointer_UTF8("Frekans Ay" "\xc4\xb1" "r" "\xc4\xb1" "c" "\xc4\xb1")),
            juce::String(juce::CharPointer_UTF8("\xd0\xa0\xd0\xb0\xd0\xb7\xd0\xb4\xd0\xb5\xd0\xbb\xd0\xb8\xd1\x82\xd0\xb5\xd0\xbb\xd1\x8c" " " "\xd1\x87\xd0\xb0\xd1\x81\xd1\x82\xd0\xbe\xd1\x82")),
            juce::String(juce::CharPointer_UTF8("\xe5\x91\xa8\xe6\xb3\xa2\xe6\x95\xb0\xe3\x82\xb9\xe3\x83\x97\xe3\x83\xaa\xe3\x83\x83\xe3\x82\xbf\xe3\x83\xbc")));
        add("Frequency Splitter_Sub", "3-Band LR4 Modular Crossover",
            juce::String(juce::CharPointer_UTF8("3 Bantl" "\xc4\xb1" " LR4 Mod" "\xc3\xbc" "ler Frekans B" "\xc3\xb6" "l" "\xc3\xbc" "c" "\xc3\xbc")),
            juce::String(juce::CharPointer_UTF8("3-\xd0\xbf\xd0\xbe\xd0\xbb\xd0\xbe\xd1\x81\xd0\xbd\xd1\x8b\xd0\xb9" " " "\xd0\xba\xd1\x80\xd0\xbe\xd1\x81\xd1\x81\xd0\xbe\xd0\xb2\xd0\xb5\xd1\x80" " LR4")),
            juce::String(juce::CharPointer_UTF8("3\xe3\x83\x90\xe3\x83\xb3\xe3\x83\x89" " LR4 " "\xe3\x82\xaf\xe3\x83\xad\xe3\x82\xb9\xe3\x82\xaa\xe3\x83\xbc\xe3\x83\x90\xe3\x83\xbc")));
        add("Frequency Joiner", "Frequency Joiner",
            juce::String(juce::CharPointer_UTF8("Frekans Birle" "\xc5\x9f" "tirici")),
            juce::String(juce::CharPointer_UTF8("\xd0\x9e\xd0\xb1\xd1\x8a\xd0\xb5\xd0\xb4\xd0\xb8\xd0\xbd\xd0\xb8\xd1\x82\xd0\xb5\xd0\xbb\xd1\x8c" " " "\xd1\x87\xd0\xb0\xd1\x81\xd1\x82\xd0\xbe\xd1\x82")),
            juce::String(juce::CharPointer_UTF8("\xe5\x91\xa8\xe6\xb3\xa2\xe6\x95\xb0\xe3\x82\xb8\xe3\x83\xa7\xe3\x82\xa4\xe3\x83\x8a\xe3\x83\xbc")));
        add("Frequency Joiner_Sub", "3-Band LR4 Modular Combiner",
            juce::String(juce::CharPointer_UTF8("3 Bantl" "\xc4\xb1" " LR4 Mod" "\xc3\xbc" "ler Frekans Birle" "\xc5\x9f" "tirici")),
            juce::String(juce::CharPointer_UTF8("3-\xd0\xbf\xd0\xbe\xd0\xbb\xd0\xbe\xd1\x81\xd0\xbd\xd1\x8b\xd0\xb9" " " "\xd1\x81\xd1\x83\xd0\xbc\xd0\xbc\xd0\xb0\xd1\x82\xd0\xbe\xd1\x80" " LR4")),
            juce::String(juce::CharPointer_UTF8("3\xe3\x83\x90\xe3\x83\xb3\xe3\x83\x89" " LR4 " "\xe3\x82\xb3\xe3\x83\xb3\xe3\x83\x90\xe3\x82\xa4\xe3\x83\x8a\xe3\x83\xbc")));
        add("De-Breath", "De-Breath",
            juce::String(juce::CharPointer_UTF8("Nefes Sesi Giderici")),
            juce::String(juce::CharPointer_UTF8("\xd0\x9f\xd0\xbe\xd0\xb4\xd0\xb0\xd0\xb2\xd0\xbb\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb5" " " "\xd0\xb4\xd1\x8b\xd1\x85\xd0\xb0\xd0\xbd\xd0\xb8\xd1\x8f")),
            juce::String(juce::CharPointer_UTF8("\xe3\x83\x96\xe3\x83\xac\xe3\x82\xb9\xe9\x99\xa4\xe5\x8e\xbb")));
        add("3-Band EQ", "3-Band EQ",
            juce::String(juce::CharPointer_UTF8("3-Bantl\xc4\xb1 EQ")),
            juce::String(juce::CharPointer_UTF8("3-\xd0\xbf\xd0\xbe\xd0\xbb\xd0\xbe\xd1\x81\xd0\xbd\xd1\x8b\xd0\xb9 EQ")),
            juce::String(juce::CharPointer_UTF8("3\xe3\x83\x90\xe3\x83\xb3\xe3\x83\x89 EQ")));
        add("3-Band EQ_Sub", "Low/Mid/High Tone Equalizer",
            juce::String(juce::CharPointer_UTF8("Bas/Mid/Tiz Ton Ekolayzer" "\xc4\xb1")),
            juce::String(juce::CharPointer_UTF8("3-\xd0\xbf\xd0\xbe\xd0\xbb\xd0\xbe\xd1\x81\xd0\xbd\xd1\x8b\xd0\xb9 \xd1\x82\xd0\xbe\xd0\xbd-\xd1\x8d\xd0\xba\xd0\xb2\xd0\xb0\xd0\xbb\xd0\xb0\xd0\xb9\xd0\xb7\xd0\xb5\xd1\x80")),
            juce::String(juce::CharPointer_UTF8("3\xe3\x83\x90\xe3\x83\xb3\xe3\x83\x89\xe3\x82\xa4\xe3\x82\xaf\xe3\x82\xa2\xe3\x83\xa9\xe3\x82\xa4\xe3\x82\xb6\xe3\x83\xbc")));
        add("Spatial 3D", "Spatial 3D",
            juce::String(juce::CharPointer_UTF8("3D Ses Uzay" "\xc4\xb1")),
            juce::String(juce::CharPointer_UTF8("3D " "\xd0\x9f\xd1\x80\xd0\xbe\xd1\x81\xd1\x82\xd1\x80\xd0\xb0\xd0\xbd\xd1\x81\xd1\x82\xd0\xb2\xd0\xbe")),
            juce::String(juce::CharPointer_UTF8("3D " "\xe7\xa9\xba\xe9\x96\x93\xe3\x82\xaa\xe3\x83\xbc\xe3\x83\x87\xe3\x82\xa3\xe3\x82\xaa")));
        add("Spatial 3D_Sub", "3D Binaural Acoustic Realm",
            juce::String(juce::CharPointer_UTF8("3 Boyutlu Akustik Ses Alan" "\xc4\xb1")),
            juce::String(juce::CharPointer_UTF8("3D " "\xd0\xb1\xd0\xb8\xd0\xbd\xd0\xb0\xd1\x83\xd1\x80\xd0\xb0\xd0\xbb\xd1\x8c\xd0\xbd\xd0\xbe\xd0\xb5" " " "\xd0\xbf\xd1\x80\xd0\xbe\xd1\x81\xd1\x82\xd1\x80\xd0\xb0\xd0\xbd\xd1\x81\xd1\x82\xd0\xb2\xd0\xbe")),
            juce::String(juce::CharPointer_UTF8("3D " "\xe3\x83\x90\xe3\x82\xa4\xe3\x83\x8e\xe3\x83\xbc\xe3\x83\xa9\xe3\x83\xab\xe7\xa9\xba\xe9\x96\x93")));
    }


};

inline juce::String tr (const juce::String& key)
{
    return LocalizationManager::instance().get (key);
}
