using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace ModularVoiceStudio.App.Engine;

public static class NativeBridge
{
    private const string DllName = "ModularVoiceStudio_Core.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_Init(double sampleRate, int blockSize);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_Shutdown();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_GetInputDevices(byte[] outBuffer, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_GetOutputDevices(byte[] outBuffer, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_SetInputDevice([MarshalAs(UnmanagedType.LPUTF8Str)] string deviceName);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_SetVBCableOutputDevice([MarshalAs(UnmanagedType.LPUTF8Str)] string deviceName);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_SetMonitorOutputDevice([MarshalAs(UnmanagedType.LPUTF8Str)] string deviceName);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SetMicFaderGain(float gainLinear);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SetMicMute(int isMuted);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SetMicBypass(int isBypassed);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SetVBCableFaderGain(float gainLinear);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SetVBCableMute(int isMuted);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SetVBCableActive(int isActive);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SetMonitorFaderGain(float gainLinear);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SetMonitorActive(int isActive);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_GetLiveLevels(out float inLvl, out float outLvl, out float vbLvl, out float monLvl);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_ResetDSPChain();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_GetActiveDSPNodes(byte[] outBuffer, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_OpenModularCanvasWindow();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_CloseModularCanvasWindow();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_OpenAudioSettingsWindow();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_CloseAudioSettingsWindow();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SaveAudioDeviceState([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_LoadAudioDeviceState([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_GetCurrentAudioDeviceType(byte[] outBuffer, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_SaveStateToFile([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_LoadStateFromFile([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

    public static string GetCurrentAudioDeviceType()
    {
        try
        {
            var buf = new byte[256];
            int res = MVS_GetCurrentAudioDeviceType(buf, buf.Length);
            if (res <= 0) return "Windows Audio";
            return Encoding.UTF8.GetString(buf).TrimEnd('\0');
        }
        catch
        {
            return "Windows Audio";
        }
    }

    public static string[] GetInputDeviceList()
    {
        try
        {
            var buf = new byte[8192];
            int count = MVS_GetInputDevices(buf, buf.Length);
            if (count <= 0) return Array.Empty<string>();
            string raw = Encoding.UTF8.GetString(buf).TrimEnd('\0');
            return string.IsNullOrWhiteSpace(raw) ? Array.Empty<string>() : raw.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        }
        catch
        {
            return Array.Empty<string>();
        }
    }

    public static string[] GetOutputDeviceList()
    {
        try
        {
            var buf = new byte[8192];
            int count = MVS_GetOutputDevices(buf, buf.Length);
            if (count <= 0) return Array.Empty<string>();
            string raw = Encoding.UTF8.GetString(buf).TrimEnd('\0');
            return string.IsNullOrWhiteSpace(raw) ? Array.Empty<string>() : raw.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        }
        catch
        {
            return Array.Empty<string>();
        }
    }

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_QuickFx_SetPitch(float semitones, float mixPct, int active);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_QuickFx_SetReverb(float roomSize, float damping, float mixPct, int active);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_QuickFx_LoadVST([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath, byte[] errorBuffer, int maxErrorLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_QuickFx_OpenVSTEditor();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_QuickFx_GetLoadedVSTName(byte[] outBuffer, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_QuickFx_SetVSTActive(int active);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_QuickFx_SetVSTMix(float mixPct);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_QuickFx_RemoveVST();

    public static string GetLoadedVSTName()
    {
        try
        {
            var buf = new byte[512];
            int len = MVS_QuickFx_GetLoadedVSTName(buf, buf.Length);
            if (len <= 0) return string.Empty;
            return Encoding.UTF8.GetString(buf).TrimEnd('\0');
        }
        catch
        {
            return string.Empty;
        }
    }

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_VST_ScanInstalledPlugins(byte[] outJsonBuffer, int maxBufferLen);

    public static List<VstPluginInfo> ScanInstalledVST3Plugins()
    {
        var list = new List<VstPluginInfo>();
        try
        {
            var buf = new byte[131072]; // 128KB buffer for JSON plugin descriptions
            int len = MVS_VST_ScanInstalledPlugins(buf, buf.Length);
            if (len > 0)
            {
                string json = Encoding.UTF8.GetString(buf, 0, len).TrimEnd('\0');
                if (!string.IsNullOrWhiteSpace(json))
                {
                    var items = System.Text.Json.JsonSerializer.Deserialize<List<VstPluginInfo>>(json, new System.Text.Json.JsonSerializerOptions
                    {
                        PropertyNameCaseInsensitive = true
                    });
                    if (items != null) list.AddRange(items);
                }
            }
        }
        catch { }
        return list;
    }

    public static string LoadVSTPlugin(string filePath)
    {
        try
        {
            var errBuf = new byte[1024];
            int res = MVS_QuickFx_LoadVST(filePath, errBuf, errBuf.Length);
            if (res == 1) return string.Empty;
            string err = Encoding.UTF8.GetString(errBuf).TrimEnd('\0');
            return string.IsNullOrWhiteSpace(err) ? "Failed to load VST3 plugin" : err;
        }
        catch (Exception ex)
        {
            return ex.Message;
        }
    }

    public static string GetActiveDSPNodesString()
    {
        try
        {
            var buf = new byte[2048];
            int count = MVS_GetActiveDSPNodes(buf, buf.Length);
            if (count <= 0) return "Direct Input -> Output (Clean)";
            string raw = Encoding.UTF8.GetString(buf).TrimEnd('\0');
            return string.IsNullOrWhiteSpace(raw) ? "Direct Input -> Output (Clean)" : raw;
        }
        catch
        {
            return "Direct Input -> Output (Clean)";
        }
    }

    // =========================================================================
    // Dynamic Effect Chain API Bindings
    // =========================================================================
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_EffectChain_AddStrip([MarshalAs(UnmanagedType.LPUTF8Str)] string typeName, [MarshalAs(UnmanagedType.LPUTF8Str)] string? optionalVstPath, byte[] outStripId, int maxIdLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_EffectChain_RemoveStrip(int index);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_Clear();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_EffectChain_GetCount();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_EffectChain_GetStripInfo(int index, byte[] outType, int maxTypeLen, byte[] outName, int maxNameLen, out float outGainLinear, out float outPan, out int outBypassed, out int outMuted, out int outSolo, out float outMeterL, out float outMeterR);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_SetStripGain(int index, float gainLinear);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_SetStripPan(int index, float pan);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_SetStripBypass(int index, int bypassed);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_SetStripMute(int index, int muted);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_SetStripSolo(int index, int solo);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_OpenStripEditor(int index);

    public static int AddEffectStrip(string typeName, string? optionalVstPath = null)
    {
        try
        {
            var idBuf = new byte[128];
            return MVS_EffectChain_AddStrip(typeName, optionalVstPath, idBuf, idBuf.Length);
        }
        catch
        {
            return -1;
        }
    }

    public static bool RemoveEffectStrip(int index)
    {
        try
        {
            return MVS_EffectChain_RemoveStrip(index) == 1;
        }
        catch
        {
            return false;
        }
    }

    public static void ClearEffectChain()
    {
        try { MVS_EffectChain_Clear(); } catch { }
    }

    public static int GetEffectChainCount()
    {
        try { return MVS_EffectChain_GetCount(); } catch { return 0; }
    }

    public static EffectStripData? GetEffectStripData(int index)
    {
        try
        {
            var typeBuf = new byte[256];
            var nameBuf = new byte[256];
            int res = MVS_EffectChain_GetStripInfo(index, typeBuf, typeBuf.Length, nameBuf, nameBuf.Length,
                out float gain, out float pan, out int bypassed, out int muted, out int solo, out float meterL, out float meterR);
            if (res != 1) return null;

            return new EffectStripData
            {
                Index = index,
                TypeName = Encoding.UTF8.GetString(typeBuf).TrimEnd('\0'),
                DisplayName = Encoding.UTF8.GetString(nameBuf).TrimEnd('\0'),
                GainLinear = gain,
                Pan = pan,
                IsBypassed = bypassed != 0,
                IsMuted = muted != 0,
                IsSolo = solo != 0,
                MeterPeakL = meterL,
                MeterPeakR = meterR
            };
        }
        catch
        {
            return null;
        }
    }

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_EffectChain_GetStripParamCount(int index);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int MVS_EffectChain_GetStripParamInfo(int stripIndex, int paramIndex,
        byte[] outParamId, int maxIdLen,
        byte[] outName, int maxNameLen,
        out float outVal, out float outMin, out float outMax, out float outDefault,
        byte[] outLabel, int maxLabelLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_SetStripParamValue(int stripIndex, [MarshalAs(UnmanagedType.LPUTF8Str)] string paramId, float value);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void MVS_EffectChain_SetStripParamValueByIndex(int stripIndex, int paramIndex, float value);

    public static List<EffectParamData> GetStripParameters(int stripIndex)
    {
        var list = new List<EffectParamData>();
        try
        {
            int count = MVS_EffectChain_GetStripParamCount(stripIndex);
            for (int i = 0; i < count; i++)
            {
                var idBuf = new byte[128];
                var nameBuf = new byte[256];
                var labelBuf = new byte[64];
                int ok = MVS_EffectChain_GetStripParamInfo(stripIndex, i, idBuf, idBuf.Length, nameBuf, nameBuf.Length,
                    out float val, out float minV, out float maxV, out float defV, labelBuf, labelBuf.Length);

                if (ok == 1)
                {
                    list.Add(new EffectParamData
                    {
                        Index = i,
                        StripIndex = stripIndex,
                        ParamId = Encoding.UTF8.GetString(idBuf).TrimEnd('\0'),
                        Name = Encoding.UTF8.GetString(nameBuf).TrimEnd('\0'),
                        Value = val,
                        MinValue = minV,
                        MaxValue = maxV,
                        DefaultValue = defV,
                        Label = Encoding.UTF8.GetString(labelBuf).TrimEnd('\0')
                    });
                }
            }
        }
        catch { }
        return list;
    }

    public static void SetStripParameter(int stripIndex, string paramId, float value)
    {
        try { MVS_EffectChain_SetStripParamValue(stripIndex, paramId, value); } catch { }
    }

    public static void SetStripParameterByIndex(int stripIndex, int paramIndex, float value)
    {
        try { MVS_EffectChain_SetStripParamValueByIndex(stripIndex, paramIndex, value); } catch { }
    }
}

public class EffectStripData
{
    public int Index { get; set; }
    public string TypeName { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;
    public float GainLinear { get; set; } = 1.0f;
    public float Pan { get; set; } = 0.0f;
    public bool IsBypassed { get; set; }
    public bool IsMuted { get; set; }
    public bool IsSolo { get; set; }
    public float MeterPeakL { get; set; }
    public float MeterPeakR { get; set; }
}

public class EffectParamData
{
    public int Index { get; set; }
    public int StripIndex { get; set; }
    public string ParamId { get; set; } = string.Empty;
    public string Name { get; set; } = string.Empty;
    public float Value { get; set; }
    public float MinValue { get; set; }
    public float MaxValue { get; set; }
    public float DefaultValue { get; set; }
    public string Label { get; set; } = string.Empty;
}

public class VstPluginInfo
{
    public string Name { get; set; } = string.Empty;
    public string Vendor { get; set; } = string.Empty;
    public string Category { get; set; } = string.Empty;
    public string Path { get; set; } = string.Empty;
    public bool IsInstrument { get; set; }
}


