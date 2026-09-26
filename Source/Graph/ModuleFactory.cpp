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
#include "../Modules/PitchShiftModule.h"
#include "../Modules/ReverbModule.h"
#include "../Modules/DelayModule.h"
#include "../Modules/RobotVoiceModule.h"
#include "../Modules/VSTPluginModule.h"

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

    // Vocal Tone & Creative Effects
    registerType("Pitch Shifter", "Vocal Tone", "Real-Time Pitch & Formant Voice Shifter (±12 Semitones)", []
                 { return std::make_unique<PitchShiftModule>(); });
    registerType("Reverb", "Vocal Tone", "Pro Studio Acoustic Vocal Reverb (Room / Plate / Hall)", []
                 { return std::make_unique<ReverbModule>(); });
    registerType("Delay", "Vocal Tone", "Stereo Ping-Pong & Vocal Echo Delay", []
                 { return std::make_unique<DelayModule>(); });
    registerType("Robot Voice", "Vocal Tone", "Sci-Fi / Robotic / Ring Modulation / Digital Crush", []
                 { return std::make_unique<RobotVoiceModule>(); });
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

    // External Plugins
    registerType("VST3 Host", "External FX", "Load External VST3 / VST Audio Effect Plugins", []
                 { return std::make_unique<VSTPluginModule>(); });
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

    juce::String lower = typeId.toLowerCase().trim();

    // Creative & Tone
    if (lower.contains ("pitch") || lower.contains ("octav") || lower.contains ("harmoniz")) return std::make_unique<PitchShiftModule>();
    if (lower.contains ("reverb") && !lower.contains ("de")) return std::make_unique<ReverbModule>();
    if (lower.contains ("delay") || lower.contains ("echo")) return std::make_unique<DelayModule>();
    if (lower.contains ("robot") || lower.contains ("ring mod") || lower.contains ("crush")) return std::make_unique<RobotVoiceModule>();
    if (lower.contains ("double") || lower.contains ("chorus")) return std::make_unique<VocalDoublerModule>();
    if (lower.contains ("saturat") || lower.contains ("tape") || lower.contains ("tube") || lower.contains ("drive") || lower.contains ("warmth")) return std::make_unique<SaturationModule>();
    if (lower.contains ("spatial") || lower.contains ("3d") || lower.contains ("binaural")) return std::make_unique<Spatial3DModule>();
    if (lower.contains ("excit") || lower.contains ("aural")) return std::make_unique<AuralExciterModule>();
    if (lower.contains ("sub") || lower.contains ("phantom")) return std::make_unique<PhantomSubModule>();
    if (lower.contains ("morph") || lower.contains ("broadcast")) return std::make_unique<BroadcastMorphModule>();
    if (lower.contains ("proxim")) return std::make_unique<ProximityModule>();
    if (lower.contains ("rotator") || lower.contains ("phase")) return std::make_unique<PhaseRotatorModule>();

    // Dynamics
    if (lower.contains ("upward") || lower.contains ("ott")) return std::make_unique<UpwardCompressorModule>();
    if (lower.contains ("compress")) return std::make_unique<CompressorModule>();
    if (lower.contains ("limit")) return std::make_unique<LimiterModule>();
    if (lower.contains ("gate")) return std::make_unique<GateModule>();
    if (lower.contains ("agc") || lower.contains ("auto gain")) return std::make_unique<AGCModule>();

    // Frequency
    if (lower.contains ("3-band") || lower.contains ("3 band") || lower.contains ("three band")) return std::make_unique<ThreeBandEQModule>();
    if (lower.contains ("parametric") || lower.contains ("eq") || lower.contains ("equaliz")) return std::make_unique<ParametricEQModule>();
    if (lower.contains ("clarity") || lower.contains ("spectral")) return std::make_unique<SpectralClarityModule>();
    if (lower.contains ("dynamic eq")) return std::make_unique<DynamicEQModule>();
    if (lower.contains ("esser") || lower.contains ("de-ess") || lower.contains ("deess")) return std::make_unique<DeEsserModule>();
    if (lower.contains ("splitter") || lower.contains ("crossover splitter")) return std::make_unique<CrossoverSplitterModule>();
    if (lower.contains ("joiner") || lower.contains ("crossover joiner")) return std::make_unique<CrossoverJoinerModule>();

    // Cleanup
    if (lower.contains ("noise") || lower.contains ("denois") || lower.contains ("rnnoise")) return std::make_unique<NoiseSuppressionModule>();
    if (lower.contains ("breath")) return std::make_unique<DeBreathModule>();
    if (lower.contains ("plosive") || lower.contains ("pop")) return std::make_unique<DePlosiveModule>();
    if (lower.contains ("click")) return std::make_unique<DeClickModule>();
    if (lower.contains ("aec") || lower.contains ("echo cancel")) return std::make_unique<AECModule>();
    if (lower.contains ("dereverb") || lower.contains ("de-reverb")) return std::make_unique<DereverbModule>();

    // Utility & VST
    if (lower.contains ("gain") || lower.contains ("volume") || lower.contains ("trim")) return std::make_unique<GainModule>();
    if (lower.contains ("container")) return std::make_unique<ContainerModule>();
    if (lower.contains ("vst") || lower.contains ("host")) return std::make_unique<VSTPluginModule>();

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
