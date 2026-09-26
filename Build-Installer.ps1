# ==============================================================================
# Modular Voice Studio - Automated Installer Package Builder
# ==============================================================================

param(
    [string]$Configuration = "Release",
    [string]$OutputDir = "build/ModularVoiceStudio_artefacts/Release"
)

$ErrorActionPreference = "Stop"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Modular Voice Studio - Installer Packaging" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$ReleaseDir = Join-Path $ScriptRoot "build/ModularVoiceStudio_artefacts/$Configuration"
$PayloadDir = Join-Path $ScriptRoot "Installer/Payload"
$TempStaging = Join-Path $ScriptRoot "Installer/PayloadStaging"
$ZipTarget = Join-Path $PayloadDir "payload.zip"

# 1. Clean and prepare payload directories
if (Test-Path $TempStaging) { Remove-Item $TempStaging -Recurse -Force }
if (Test-Path $ZipTarget) { Remove-Item $ZipTarget -Force }
New-Item -ItemType Directory -Path $TempStaging -Force | Out-Null
New-Item -ItemType Directory -Path $PayloadDir -Force | Out-Null

# 2. Copy VST3 bundle if present
$Vst3Source = Join-Path $ReleaseDir "VST3/Modular Voice Studio.vst3"
if (Test-Path $Vst3Source) {
    Write-Host "[+] Packaging VST3 plugin..." -ForegroundColor Green
    Copy-Item $Vst3Source (Join-Path $TempStaging "Modular Voice Studio.vst3") -Recurse -Force
} else {
    Write-Warning "VST3 plugin not found at $Vst3Source"
}

# 3. Copy VST2 dll if present
$Vst2Source = Join-Path $ReleaseDir "VST/Modular Voice Studio.dll"
if (Test-Path $Vst2Source) {
    Write-Host "[+] Packaging VST2 plugin..." -ForegroundColor Green
    Copy-Item $Vst2Source (Join-Path $TempStaging "Modular Voice Studio.dll") -Force
} else {
    Write-Warning "VST2 plugin not found at $Vst2Source"
}

# 4. Create ZIP payload
Write-Host "[+] Compressing payload.zip..." -ForegroundColor Green
Compress-Archive -Path "$TempStaging/*" -DestinationPath $ZipTarget -Force
Remove-Item $TempStaging -Recurse -Force

# 5. Publish Installer as a single-file executable
Write-Host "[+] Publishing standalone installer executable..." -ForegroundColor Green
$InstallerProj = Join-Path $ScriptRoot "Installer/ModularVoiceStudio.Installer.csproj"
$DistDir = Join-Path $ScriptRoot "dist/installer"
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

dotnet publish $InstallerProj -c $Configuration -r win-x64 --self-contained -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o $DistDir

$BuiltExe = Join-Path $DistDir "ModularVoiceStudio_Setup.exe"
if (Test-Path $BuiltExe) {
    $TargetReleaseExe = Join-Path $ReleaseDir "ModularVoiceStudio_Setup.exe"
    Copy-Item $BuiltExe $TargetReleaseExe -Force
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "  INSTALLER READY!" -ForegroundColor Green
    Write-Host "  Location: $TargetReleaseExe" -ForegroundColor Yellow
    Write-Host "============================================================" -ForegroundColor Cyan
} else {
    Write-Error "Installer build failed. Target executable not generated."
}
