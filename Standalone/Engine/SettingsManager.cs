using System;
using System.IO;
using System.Text.Json;

namespace ModularVoiceStudio.App.Engine;

public class AppSettings
{
    public string SelectedInputDevice { get; set; } = "";
    public string SelectedVBCableDevice { get; set; } = "";
    public string SelectedMonitorDevice { get; set; } = "";
    public double MicGainDb { get; set; } = 0.0;
    public bool MicMuted { get; set; } = false;
    public double VBCableGainDb { get; set; } = 0.0;
    public bool VBCableMuted { get; set; } = false;
    public double MonitorGainDb { get; set; } = 0.0;
    public bool MonitorActive { get; set; } = false;
    public bool DspBypassed { get; set; } = false;
}

public static class SettingsManager
{
    private static readonly string SettingsFolder = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "ModularVoiceStudio");

    private static readonly string SettingsFilePath = Path.Combine(SettingsFolder, "settings.json");
    private static readonly string DspStateFilePath = Path.Combine(SettingsFolder, "session_state.mvs");
    private static readonly string AudioDevicesXmlFilePath = Path.Combine(SettingsFolder, "audio_devices.xml");

    public static AppSettings Current { get; private set; } = new();

    public static string GetDspStateFilePath() => DspStateFilePath;
    public static string GetAudioDevicesXmlFilePath() => AudioDevicesXmlFilePath;

    public static void Load()
    {
        try
        {
            if (File.Exists(SettingsFilePath))
            {
                string json = File.ReadAllText(SettingsFilePath);
                var loaded = JsonSerializer.Deserialize<AppSettings>(json);
                if (loaded != null)
                {
                    Current = loaded;
                    return;
                }
            }
        }
        catch { }

        Current = new AppSettings();
    }

    public static void Save()
    {
        try
        {
            if (!Directory.Exists(SettingsFolder))
                Directory.CreateDirectory(SettingsFolder);

            var options = new JsonSerializerOptions { WriteIndented = true };
            string json = JsonSerializer.Serialize(Current, options);
            File.WriteAllText(SettingsFilePath, json);
        }
        catch { }
    }
}
