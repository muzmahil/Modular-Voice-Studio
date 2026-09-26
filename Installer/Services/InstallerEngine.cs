using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Threading.Tasks;

namespace ModularVoiceStudio.Installer.Services;

public class InstallerEngine
{
    public async Task InstallAsync(
        bool installVst3,
        string vst3Dir,
        bool installVst2,
        string vst2Dir,
        IProgress<(double progress, string status)> progress)
    {
        await Task.Run(() =>
        {
            progress.Report((0.05, LocalizationManager.Get("StatusPreparing")));

            string appDataProgramDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Programs", "Modular Voice Studio");

            Directory.CreateDirectory(appDataProgramDir);

            string installedVst3 = "";
            string installedVst2 = "";

            var assembly = Assembly.GetExecutingAssembly();
            using Stream? payloadStream = assembly.GetManifestResourceStream("ModularVoiceStudio.Installer.Payload.payload.zip");

            if (payloadStream != null)
            {
                progress.Report((0.15, LocalizationManager.Get("StatusExtracting")));
                using var archive = new ZipArchive(payloadStream, ZipArchiveMode.Read);

                // Temporary extraction directory
                string tempExtract = Path.Combine(Path.GetTempPath(), "MVS_Install_" + Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(tempExtract);

                try
                {
                    archive.ExtractToDirectory(tempExtract, overwriteFiles: true);

                    if (installVst3 && !string.IsNullOrWhiteSpace(vst3Dir))
                    {
                        progress.Report((0.40, LocalizationManager.Get("StatusVst3")));
                        Directory.CreateDirectory(vst3Dir);

                        // Look for .vst3 folder or file in tempExtract
                        string[] vst3Matches = Directory.GetDirectories(tempExtract, "*.vst3", SearchOption.AllDirectories);
                        if (vst3Matches.Length > 0)
                        {
                            string targetDir = Path.Combine(vst3Dir, Path.GetFileName(vst3Matches[0]));
                            CopyDirectory(vst3Matches[0], targetDir);
                            installedVst3 = targetDir;
                        }
                        else
                        {
                            string[] vst3Files = Directory.GetFiles(tempExtract, "*.vst3", SearchOption.AllDirectories);
                            if (vst3Files.Length > 0)
                            {
                                string targetFile = Path.Combine(vst3Dir, Path.GetFileName(vst3Files[0]));
                                File.Copy(vst3Files[0], targetFile, true);
                                installedVst3 = targetFile;
                            }
                        }
                    }

                    if (installVst2 && !string.IsNullOrWhiteSpace(vst2Dir))
                    {
                        progress.Report((0.70, LocalizationManager.Get("StatusVst2")));
                        Directory.CreateDirectory(vst2Dir);

                        string[] dllMatches = Directory.GetFiles(tempExtract, "*.dll", SearchOption.AllDirectories);
                        foreach (var dll in dllMatches)
                        {
                            if (Path.GetFileName(dll).Contains("Modular Voice Studio", StringComparison.OrdinalIgnoreCase))
                            {
                                string targetDll = Path.Combine(vst2Dir, Path.GetFileName(dll));
                                File.Copy(dll, targetDll, true);
                                installedVst2 = targetDll;
                                break;
                            }
                        }
                    }
                }
                finally
                {
                    try { Directory.Delete(tempExtract, true); } catch { }
                }
            }
            else
            {
                // Fallback: search local build artefacts folder
                string baseDir = AppDomain.CurrentDomain.BaseDirectory;
                string[] searchRoots = new[]
                {
                    Path.Combine(baseDir, "..", "build", "ModularVoiceStudio_artefacts", "Release"),
                    Path.Combine(baseDir, "..", "..", "..", "..", "build", "ModularVoiceStudio_artefacts", "Release"),
                    Path.Combine(baseDir, "build", "ModularVoiceStudio_artefacts", "Release")
                };

                string? releaseDir = null;
                foreach (var root in searchRoots)
                {
                    if (Directory.Exists(root))
                    {
                        releaseDir = root;
                        break;
                    }
                }

                if (releaseDir != null)
                {
                    if (installVst3 && !string.IsNullOrWhiteSpace(vst3Dir))
                    {
                        progress.Report((0.40, LocalizationManager.Get("StatusVst3")));
                        Directory.CreateDirectory(vst3Dir);

                        string vst3Source = Path.Combine(releaseDir, "VST3", "Modular Voice Studio.vst3");
                        if (Directory.Exists(vst3Source))
                        {
                            string targetDir = Path.Combine(vst3Dir, "Modular Voice Studio.vst3");
                            CopyDirectory(vst3Source, targetDir);
                            installedVst3 = targetDir;
                        }
                        else if (File.Exists(vst3Source))
                        {
                            string targetFile = Path.Combine(vst3Dir, "Modular Voice Studio.vst3");
                            File.Copy(vst3Source, targetFile, true);
                            installedVst3 = targetFile;
                        }
                    }

                    if (installVst2 && !string.IsNullOrWhiteSpace(vst2Dir))
                    {
                        progress.Report((0.70, LocalizationManager.Get("StatusVst2")));
                        Directory.CreateDirectory(vst2Dir);

                        string vst2Source = Path.Combine(releaseDir, "VST", "Modular Voice Studio.dll");
                        if (File.Exists(vst2Source))
                        {
                            string targetDll = Path.Combine(vst2Dir, "Modular Voice Studio.dll");
                            File.Copy(vst2Source, targetDll, true);
                            installedVst2 = targetDll;
                        }
                    }
                }
            }

            // Register Uninstaller
            progress.Report((0.90, LocalizationManager.Get("StatusRegistering")));
            string uninstallerExe = Path.Combine(appDataProgramDir, "uninstall.exe");
            try
            {
                string currentExe = Process.GetCurrentProcess().MainModule?.FileName ?? "";
                if (File.Exists(currentExe))
                {
                    File.Copy(currentExe, uninstallerExe, true);
                }
            }
            catch { }

            WindowsRegistryHelper.RegisterUninstall(
                string.IsNullOrEmpty(installedVst3) ? null : installedVst3,
                string.IsNullOrEmpty(installedVst2) ? null : installedVst2,
                uninstallerExe,
                "1.5.0");

            progress.Report((1.0, LocalizationManager.Get("FinishTitle")));
        });
    }

    public static async Task UninstallAsync(IProgress<(double progress, string status)>? progress = null)
    {
        await Task.Run(() =>
        {
            progress?.Report((0.15, LocalizationManager.Get("UninstallProgressDesc")));

            var (vst3Path, vst2Path) = WindowsRegistryHelper.GetInstalledPaths();

            if (!string.IsNullOrWhiteSpace(vst3Path))
            {
                try
                {
                    if (Directory.Exists(vst3Path)) Directory.Delete(vst3Path, true);
                    else if (File.Exists(vst3Path)) File.Delete(vst3Path);
                }
                catch { }
            }

            if (!string.IsNullOrWhiteSpace(vst2Path) && File.Exists(vst2Path))
            {
                try { File.Delete(vst2Path); } catch { }
            }

            progress?.Report((0.60, LocalizationManager.Get("UninstallProgressDesc")));
            WindowsRegistryHelper.RemoveUninstallRegistry();

            string appDataProgramDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Programs", "Modular Voice Studio");

            // Delayed self removal of uninstall directory
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = $"/c \"timeout /t 1 /nobreak >nul & rmdir /s /q \\\"{appDataProgramDir}\\\"\"",
                    WindowStyle = ProcessWindowStyle.Hidden,
                    CreateNoWindow = true,
                    UseShellExecute = true
                };
                Process.Start(psi);
            }
            catch { }

            progress?.Report((1.0, LocalizationManager.Get("UninstallSuccessTitle")));
        });
    }

    private static void CopyDirectory(string sourceDir, string targetDir)
    {
        Directory.CreateDirectory(targetDir);

        foreach (var file in Directory.GetFiles(sourceDir))
        {
            string dest = Path.Combine(targetDir, Path.GetFileName(file));
            File.Copy(file, dest, true);
        }

        foreach (var dir in Directory.GetDirectories(sourceDir))
        {
            string dest = Path.Combine(targetDir, Path.GetFileName(dir));
            CopyDirectory(dir, dest);
        }
    }
}
