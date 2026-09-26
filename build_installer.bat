@echo off
setlocal enabledelayedexpansion
title Modular Voice Studio - Build & Package Installer

echo ============================================================
echo   Modular Voice Studio - Full Build ^& Installer Generator
echo ============================================================
echo.

:: 1. Locate CMake
set "CMAKE_EXE="
where cmake >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "CMAKE_EXE=cmake"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

if "%CMAKE_EXE%"=="" (
    echo [!] ERROR: CMake was not found. Please install CMake or Visual Studio C++ tools.
    pause
    exit /b 1
)

:: 2. Locate MSBuild
set "MSBUILD_EXE="
where msbuild >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "MSBUILD_EXE=msbuild"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_EXE=C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_EXE=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
)

if "%MSBUILD_EXE%"=="" (
    echo [!] ERROR: MSBuild was not found. Please install Visual Studio.
    pause
    exit /b 1
)

:: 3. Configure CMake if needed
if not exist "build\ModularVoiceStudio_All.vcxproj" (
    echo [+] Configuring CMake project (Release x64)...
    "%CMAKE_EXE%" -B build -S .
    if %ERRORLEVEL% neq 0 (
        echo [!] CMake configuration failed.
        pause
        exit /b 1
    )
)

:: 4. Build C++ Targets (VST3, VST2, Standalone)
echo.
echo [+] Compiling DSP Engine and Audio Plugins (Release x64)...
"%MSBUILD_EXE%" "build\ModularVoiceStudio_All.vcxproj" -p:Configuration=Release -p:Platform=x64 -m
if %ERRORLEVEL% neq 0 (
    echo [!] MSBuild failed.
    pause
    exit /b 1
)

:: 5. Package Installer with PowerShell Script
echo.
echo [+] Packaging Installer Executable...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-Installer.ps1" -Configuration "Release"
if %ERRORLEVEL% neq 0 (
    echo [!] Installer packaging failed.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo   SUCCESS! All targets built and Installer packaged.
echo   Installer: build\ModularVoiceStudio_artefacts\Release\ModularVoiceStudio_Setup.exe
echo ============================================================
echo.
pause
