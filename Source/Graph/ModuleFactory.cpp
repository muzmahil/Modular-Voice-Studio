#include "ModuleFactory.h"
#include "StubModule.h"
#include "../Modules/GainModule.h"
#include "../Modules/GateModule.h"
#include "../Modules/DePlosiveModule.h"
#include "../Modules/SaturationModule.h"
#include "../Modules/CompressorModule.h"
#include "../Modules/DeEsserModule.h"
#include "../Modules/PhaseRotatorModule.h"
#include "../Modules/LimiterModule.h"
#include "../Modules/ProximityModule.h"
#include "../Modules/BroadcastMorphModule.h"
#include "../Modules/PhantomSubModule.h"
#include "../Modules/AuralExciterModule.h"
#include "../Modules/DeClickModule.h"
#include "../Modules/DynamicEQModule.h"
#include "../Modules/AGCModule.h"
#include "../Modules/VocalDoublerModule.h"
#include "../Modules/ParametricEQModule.h"
#include "../Modules/NoiseSuppressionModule.h"
#include "../Modules/AECModule.h"
#include "../Modules/DereverbModule.h"
#include "../Modules/SpectralClarityModule.h"
#include "../Modules/DeBreathModule.h"
#include "../Modules/UpwardCompressorModule.h"
#include "../Modules/CrossoverSplitterModule.h"
#include "../Modules/CrossoverJoinerModule.h"
#include "../Modules/ThreeBandEQModule.h"
#include "../Modules/Spatial3DModule.h"
#include "../Modules/ContainerModule.h"

ModuleFactory &ModuleFactory::instance()
{
    static ModuleFactory factory;
    return factory;
}

ModuleFactory::ModuleFactory()
{
    // Utility
    registerType("Container", "Utility", "Multi-Module Sub-Rack & Chain Container", []
                 { return std::make_unique<ContainerModule>(); });
    registerType("Gain", "Utility", "Signal Level Adjust", []
                 { return std::make_unique<GainModule>(); });

    // Dynamics
    registerType("Compressor", "Dynamics", "Vocal Dynamics Control", []
                 { return std::make_unique<CompressorModule>(); });
    registerType("Upward Compressor", "Dynamics", "Vocal OTT Detail Maximizer", []
                 { return std::make_unique<UpwardCompressorModule>(); });
    registerType("Limiter", "Dynamics", "Output Peak Protection", []
                 { return std::make_unique<LimiterModule>(); });
    registerType("Gate", "Dynamics", "Mute Noise & Bleed", []
                 { return std::make_unique<GateModule>(); });
    registerType("AGC", "Dynamics", "Speech Level Normalizer", []
                 { return std::make_unique<AGCModule>(); });

    // Frequency
    registerType("Frequency Splitter", "Frequency", "3-Band LR4 Modular Splitter (1 IN -> 3 OUT)", []
                 { return std::make_unique<CrossoverSplitterModule>(); });
    registerType("Frequency Joiner", "Frequency", "3-Band LR4 Modular Combiner (3 IN -> 1 OUT)", []
                 { return std::make_unique<CrossoverJoinerModule>(); });
    registerType("3-Band EQ", "Frequency", "3-Band Musical Equalizer with Low/High Shelves & Mid Bell", []
                 { return std::make_unique<ThreeBandEQModule>(); });
    registerType("Parametric EQ", "Frequency", "7-Band Pro Parametric RTA", []
                 { return std::make_unique<ParametricEQModule>(); });
    registerType("Spectral Clarity", "Frequency", "Dynamic Resonance & Harshness Tamer", []
                 { return std::make_unique<SpectralClarityModule>(); });
    registerType("Dynamic EQ", "Frequency", "Resonance Suppressor", []
                 { return std::make_unique<DynamicEQModule>(); });
    registerType("De-Esser", "Frequency", "Sibilance & Harshness Tamer", []
                 { return std::make_unique<DeEsserModule>(); });

    // Cleanup
    registerType("Noise Suppression", "Cleanup", "RNNoise Neural Reducer", []
                 { return std::make_unique<NoiseSuppressionModule>(); });
    registerType("De-Breath", "Cleanup", "Inhalation & Breath Suppressor", []
                 { return std::make_unique<DeBreathModule>(); });
    registerType("De-Plosive", "Cleanup", "Mic Pop & Thump Filter", []
                 { return std::make_unique<DePlosiveModule>(); });
    registerType("De-Click", "Cleanup", "Mouth Click & Saliva Filter", []
                 { return std::make_unique<DeClickModule>(); });
    registerType("AEC", "Cleanup", "Echo Cancellation", []
                 { return std::make_unique<AECModule>(); });
    registerType("De-reverb", "Cleanup", "Room Ambience Removal", []
                 { return std::make_unique<DereverbModule>(); });

    // Vocal Tone (Radio Tone)
    registerType("Vocal Doubler", "Vocal Tone", "Stereo Spread & Double Track", []
                 { return std::make_unique<VocalDoublerModule>(); });
    registerType("Aural Exciter", "Vocal Tone", "Air & High Presence", []
                 { return std::make_unique<AuralExciterModule>(); });
    registerType("Phantom Sub", "Vocal Tone", "Sub-Harmonic Fundamental Reconstructor", []
                 { return std::make_unique<PhantomSubModule>(); });
    registerType("Broadcast Morph", "Vocal Tone", "Microphone & Character Profile Cloner", []
                 { return std::make_unique<BroadcastMorphModule>(); });
    registerType("Proximity", "Vocal Tone", "Acoustic Distance & Dynamic Body", []
                 { return std::make_unique<ProximityModule>(); });
    registerType("Phase Rotator", "Vocal Tone", "Phase Symmetry Enhancer", []
                 { return std::make_unique<PhaseRotatorModule>(); });
    registerType("Saturation", "Vocal Tone", "Tube & Tape Harmonics", []
                 { return std::make_unique<SaturationModule>(); });
    registerType("Spatial 3D", "Vocal Tone", "3D Binaural Acoustic Realm & Multi-Band Panner", []
                 { return std::make_unique<Spatial3DModule>(); });
}

void ModuleFactory::registerType(const juce::String &typeId, const juce::String &category, const juce::String &description, Creator creator)
{
    entries.push_back({typeId, category, description, std::move(creator)});
}

std::unique_ptr<ModuleProcessor> ModuleFactory::create(const juce::String &typeId) const
{
    for (auto &e : entries)
        if (e.typeId.equalsIgnoreCase (typeId))
            return e.creator();

    // Fallback alias
    if (typeId.equalsIgnoreCase ("Dereverb") || typeId.equalsIgnoreCase ("De-reverb"))
        return std::make_unique<DereverbModule>();
    if (typeId.equalsIgnoreCase ("Crossover Splitter") || typeId.equalsIgnoreCase ("Frequency Splitter") || typeId.equalsIgnoreCase ("Crossover"))
        return std::make_unique<CrossoverSplitterModule>();
    if (typeId.equalsIgnoreCase ("Crossover Joiner") || typeId.equalsIgnoreCase ("Frequency Joiner") || typeId.equalsIgnoreCase ("Joiner") || typeId.equalsIgnoreCase ("Combiner"))
        return std::make_unique<CrossoverJoinerModule>();
    if (typeId.equalsIgnoreCase ("3-Band EQ") || typeId.equalsIgnoreCase ("3 Band EQ") || typeId.equalsIgnoreCase ("ThreeBandEQ") || typeId.equalsIgnoreCase ("Three-Band EQ"))
        return std::make_unique<ThreeBandEQModule>();
    if (typeId.equalsIgnoreCase ("Spatial 3D") || typeId.equalsIgnoreCase ("3D Spatializer") || typeId.equalsIgnoreCase ("Spatial Realm") || typeId.equalsIgnoreCase ("Spatial3D") || typeId.equalsIgnoreCase ("Spatial"))
        return std::make_unique<Spatial3DModule>();

    return nullptr;
}

juce::StringArray ModuleFactory::getCategories() const
{
    juce::StringArray categories;
    for (auto &e : entries)
        categories.addIfNotAlreadyThere(e.category);
    return categories;
}

juce::StringArray ModuleFactory::getTypesInCategory(const juce::String &category) const
{
    juce::StringArray types;
    for (auto &e : entries)
        if (e.category == category)
            types.add(e.typeId);
    return types;
}

juce::String ModuleFactory::getDescription(const juce::String &typeId) const
{
    for (auto &e : entries)
        if (e.typeId == typeId)
            return e.description;
    return {};
}

juce::String ModuleFactory::getCategory(const juce::String &typeId) const
{
    for (auto &e : entries)
        if (e.typeId == typeId)
            return e.category;
    return {};
}
