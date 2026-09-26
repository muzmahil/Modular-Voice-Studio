#pragma once

#ifdef _WIN32
  #define MVS_API extern "C" __declspec(dllexport)
#else
  #define MVS_API extern "C"
#endif

// =============================================================================
// Modular Voice Studio - C-ABI Native Core API
// =============================================================================

MVS_API int  MVS_Init (double sampleRate, int blockSize);
MVS_API void MVS_Shutdown();

MVS_API int  MVS_GetInputDevices (char* outBuffer, int maxLen);
MVS_API int  MVS_GetOutputDevices (char* outBuffer, int maxLen);

MVS_API int  MVS_SetInputDevice (const char* deviceName);
MVS_API int  MVS_SetVBCableOutputDevice (const char* deviceName);
MVS_API int  MVS_SetMonitorOutputDevice (const char* deviceName);

MVS_API void MVS_SetMicFaderGain (float gainLinear);
MVS_API void MVS_SetMicMute (int isMuted);
MVS_API void MVS_SetMicBypass (int isBypassed);

MVS_API void MVS_SetVBCableFaderGain (float gainLinear);
MVS_API void MVS_SetVBCableMute (int isMuted);
MVS_API void MVS_SetVBCableActive (int isActive);

MVS_API void MVS_SetMonitorFaderGain (float gainLinear);
MVS_API void MVS_SetMonitorActive (int isActive);

MVS_API void MVS_GetLiveLevels (float* inLvl, float* outLvl, float* vbLvl, float* monLvl);

MVS_API void MVS_ResetDSPChain();
MVS_API int  MVS_GetActiveDSPNodes (char* outBuffer, int maxLen);
MVS_API void MVS_OpenModularCanvasWindow();
MVS_API void MVS_CloseModularCanvasWindow();
MVS_API void MVS_OpenAudioSettingsWindow();
MVS_API void MVS_CloseAudioSettingsWindow();
MVS_API void MVS_SaveAudioDeviceState (const char* filePath);
MVS_API void MVS_LoadAudioDeviceState (const char* filePath);
MVS_API int  MVS_GetCurrentAudioDeviceType (char* outBuffer, int maxLen);
MVS_API void MVS_SaveStateToFile (const char* filePath);
MVS_API void MVS_LoadStateFromFile (const char* filePath);

// Quick FX & VST Hosting API
MVS_API void MVS_QuickFx_SetPitch (float semitones, float mixPct, int active);
MVS_API void MVS_QuickFx_SetReverb (float roomSize, float damping, float mixPct, int active);
MVS_API int  MVS_QuickFx_LoadVST (const char* filePath, char* errorBuffer, int maxErrorLen);
MVS_API void MVS_QuickFx_OpenVSTEditor();
MVS_API int  MVS_QuickFx_GetLoadedVSTName (char* outBuffer, int maxLen);
MVS_API void MVS_QuickFx_SetVSTActive (int active);
MVS_API void MVS_QuickFx_SetVSTMix (float mixPct);
MVS_API void MVS_QuickFx_RemoveVST();
MVS_API int  MVS_VST_ScanInstalledPlugins (char* outJsonBuffer, int maxBufferLen);

// Dynamic Effect Chain & Mixer Strip API
MVS_API int  MVS_EffectChain_AddStrip (const char* typeName, const char* optionalVstPath, char* outStripId, int maxIdLen);
MVS_API int  MVS_EffectChain_RemoveStrip (int index);
MVS_API void MVS_EffectChain_Clear();
MVS_API int  MVS_EffectChain_GetCount();
MVS_API int  MVS_EffectChain_GetStripInfo (int index, char* outType, int maxTypeLen, char* outName, int maxNameLen, float* outGainLinear, float* outPan, int* outBypassed, int* outMuted, int* outSolo, float* outMeterL, float* outMeterR);
MVS_API void MVS_EffectChain_SetStripGain (int index, float gainLinear);
MVS_API void MVS_EffectChain_SetStripPan (int index, float pan);
MVS_API void MVS_EffectChain_SetStripBypass (int index, int bypassed);
MVS_API void MVS_EffectChain_SetStripMute (int index, int muted);
MVS_API void MVS_EffectChain_SetStripSolo (int index, int solo);
MVS_API void MVS_EffectChain_OpenStripEditor (int index);
MVS_API int  MVS_EffectChain_GetStripParamCount (int index);
MVS_API int  MVS_EffectChain_GetStripParamInfo (int stripIndex, int paramIndex, char* outParamId, int maxIdLen, char* outName, int maxNameLen, float* outVal, float* outMin, float* outMax, float* outDefault, char* outLabel, int maxLabelLen);
MVS_API void MVS_EffectChain_SetStripParamValue (int stripIndex, const char* paramId, float value);
MVS_API void MVS_EffectChain_SetStripParamValueByIndex (int stripIndex, int paramIndex, float value);

