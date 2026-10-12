# ==============================================================================
# Modular Voice Studio - Professional Inno Setup Package Builder
# ==============================================================================

param(
    [string]$Configuration = "Release",
    [switch]$SkipPublish = $false
)

$ErrorActionPreference = "Stop"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Modular Voice Studio - Installer Packaging (Inno Setup 6)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# 1. Locate Inno Setup Compiler (ISCC.exe)
$IsccCandidates = @(
    "C:\Users\isu\AppData\Local\Programs\Inno Setup 6\ISCC.exe",
    (Join-Path ${env:LOCALAPPDATA} "Programs\Inno Setup 6\ISCC.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
    (Join-Path ${env:ProgramFiles} "Inno Setup 6\ISCC.exe")
)

$IsccExe = $null
foreach ($path in $IsccCandidates) {
    if ($path -and (Test-Path $path)) {
        $IsccExe = $path
        break
    }
}

if (-not $IsccExe) {
    $cmd = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
    if ($cmd) {
        $IsccExe = $cmd.Source
    }
}

if (-not $IsccExe) {
    Write-Error "Inno Setup 6 (ISCC.exe) was not found. Please install Inno Setup 6 to compile the installer."
    exit 1
}

Write-Host "[+] Using Inno Setup Compiler: $IsccExe" -ForegroundColor Gray

# 2. Standalone Application (Excluded from Plugin-only installer)
$StandaloneDir = Join-Path $ScriptRoot "dist/Standalone"
if (-not $SkipPublish -and (Test-Path (Join-Path $ScriptRoot "Standalone/ModularVoiceStudio.App.csproj"))) {
    Write-Host "[*] Skipping Standalone publish (Installer is configured for VST3/VST2 plugins only)." -ForegroundColor Gray
}

# 3. Synchronize Core DSP Engine DLL
$CoreDllSourceCandidates = @(
    (Join-Path $ScriptRoot "build/$Configuration/ModularVoiceStudio_Core.dll"),
    (Join-Path $ScriptRoot "ModularVoiceStudio_Core.dll"),
    (Join-Path $ScriptRoot "Standalone/ModularVoiceStudio_Core.dll")
)

$SyncedCoreDll = $false
foreach ($cand in $CoreDllSourceCandidates) {
    if (Test-Path $cand) {
        Copy-Item $cand (Join-Path $StandaloneDir "ModularVoiceStudio_Core.dll") -Force
        Write-Host "[+] Synchronized Core DSP DLL from: $cand" -ForegroundColor Gray
        $SyncedCoreDll = $true
        break
    }
}

if (-not $SyncedCoreDll) {
    Write-Warning "Could not find ModularVoiceStudio_Core.dll to synchronize into dist/Standalone."
}

# 4. Verify VST3 and VST2 Artefacts
$ArtefactsDir = Join-Path $ScriptRoot "build/ModularVoiceStudio_artefacts/$Configuration"
$Vst3Bundle = Join-Path $ArtefactsDir "VST3/Modular Voice Studio.vst3"
$Vst2Dll = Join-Path $ArtefactsDir "VST/Modular Voice Studio.dll"

if (Test-Path $Vst3Bundle) {
    Write-Host "[+] Found VST3 plugin bundle." -ForegroundColor Green
} else {
    Write-Warning "VST3 plugin bundle not found at $Vst3Bundle."
}

if (Test-Path $Vst2Dll) {
    Write-Host "[+] Found VST2 plugin DLL." -ForegroundColor Green
} else {
    Write-Warning "VST2 plugin DLL not found at $Vst2Dll."
}

# 5. Compile Inno Setup Script
$IssFile = Join-Path $ScriptRoot "Installer/ModularVoiceStudio.iss"
Write-Host "[+] Compiling Inno Setup package ($IssFile)..." -ForegroundColor Green

& "$IsccExe" "$IssFile"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Inno Setup compilation failed."
    exit 1
}

# 6. Verify Output
$TargetExe = Join-Path $ScriptRoot "dist/ModularVoiceStudio_Setup.exe"
if (Test-Path $TargetExe) {
    $item = Get-Item $TargetExe
    $sizeMb = [math]::Round($item.Length / 1MB, 2)
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "  INSTALLER GENERATED SUCCESSFULLY!" -ForegroundColor Green
    Write-Host "  Output: $TargetExe ($sizeMb MB)" -ForegroundColor Yellow
    Write-Host "============================================================" -ForegroundColor Cyan
} else {
    Write-Error "Target executable not found at $TargetExe."
    exit 1
}
