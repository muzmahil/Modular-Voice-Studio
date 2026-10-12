; ==============================================================================
; Modular Voice Studio - Professional Inno Setup Script
; ==============================================================================

#define MyAppName "Modular Voice Studio"
#define MyAppVersion "1.5.0"
#define MyAppPublisher "Modular Voice Studio Team"
#define MyAppURL "https://github.com/isudev/ModularAudioOS"
#define MyAppExeName "ModularVoiceStudio.exe"

[Setup]
AppId={{5C0A7463-718D-4F3D-949B-C57E868784E2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; Native 64-Bit Architecture
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes

; Modern Styling & Layout
WizardStyle=modern
WizardSizePercent=120,120
WizardImageFile=Assets\WizardImage.bmp
WizardSmallImageFile=Assets\WizardSmallImage.bmp
SetupIconFile=..\Standalone\Assets\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

; Compression
Compression=lzma2/ultra64
SolidCompression=yes

; Output Configuration
OutputDir=..\dist
OutputBaseFilename=ModularVoiceStudio_Setup
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes
RestartIfNeededByRun=no

AlwaysShowComponentsList=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]

Name: "full"; Description: "Full Installation (Standalone + VST3 + VST2)"
Name: "custom"; Description: "Custom Installation (Choose individual components)"; Flags: iscustom
Name: "standaloneonly"; Description: "Standalone Application Only"
Name: "pluginsonly"; Description: "Audio Plugins Only (VST3 + VST2 for DAWs)"

[Components]
Name: "Standalone"; Description: "Modular Voice Studio Standalone Application (Desktop GUI & Audio Engine)"; Types: full standaloneonly
Name: "VST3"; Description: "Modular Voice Studio VST3 Audio Effect (Recommended for modern DAWs: FL Studio, Ableton, Reaper, Cubase, etc.)"; Types: full pluginsonly
Name: "VST2"; Description: "Modular Voice Studio VST2 Audio Effect (64-bit DLL for legacy DAW compatibility)"; Types: full pluginsonly



[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: Standalone

[Files]
; --- Standalone Application ---
Source: "..\dist\Standalone\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: Standalone

; --- VST3 Plugin Bundle ---
; VST3 is a bundle folder structure: "Modular Voice Studio.vst3\Contents\..."
Source: "..\build\ModularVoiceStudio_artefacts\Release\VST3\Modular Voice Studio.vst3\*"; DestDir: "{code:GetVST3Dir}\Modular Voice Studio.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: VST3

; --- VST2 Plugin DLL ---
Source: "..\build\ModularVoiceStudio_artefacts\Release\VST\Modular Voice Studio.dll"; DestDir: "{code:GetVST2Dir}"; Flags: ignoreversion; Components: VST2

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"; Components: Standalone
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Components: Standalone

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Components: Standalone

[Code]
var
  VST3DirPage: TInputDirWizardPage;
  VST2DirPage: TInputDirWizardPage;

// Helper: Query VST2 directory from Windows Registry or fallback to standard path
function GetDefaultVST2Dir(): String;
var
  RegDir: String;
begin
  RegDir := '';
  // Check 64-bit registry keys first
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\VST', 'VSTPluginsPath', RegDir) and (RegDir <> '') then
  begin
    Result := RegDir;
    Exit;
  end;
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\Steinberg\VSTPlugins', 'Path', RegDir) and (RegDir <> '') then
  begin
    Result := RegDir;
    Exit;
  end;
  // Standard 64-bit fallback
  Result := ExpandConstant('{autopf}\VSTPlugins');
end;

// Helper: Query standard VST3 directory (Common Files\VST3)
function GetDefaultVST3Dir(): String;
begin
  Result := ExpandConstant('{commoncf}\VST3');
end;

// Inno Setup Wizard Initialization
procedure InitializeWizard();
begin
  // Page 1: VST3 Directory Selection
  VST3DirPage := CreateInputDirPage(
    wpSelectDir,
    'Select VST3 Plugin Destination',
    'Where should the 64-bit VST3 plugin be installed?',
    'Setup will install the Modular Voice Studio VST3 plugin bundle into the following directory.'#13#10 +
    'The standard directory below is automatically detected and recognized by all modern DAWs.',
    False, '');
  VST3DirPage.Add('');
  VST3DirPage.Values[0] := GetDefaultVST3Dir();

  // Page 2: VST2 Directory Selection
  VST2DirPage := CreateInputDirPage(
    VST3DirPage.ID,
    'Select VST2 Plugin Destination',
    'Where should the 64-bit VST2 plugin (.dll) be installed?',
    'Setup will install the Modular Voice Studio VST2 plugin into the following directory.'#13#10 +
    'Choose your DAW''s 64-bit VST plugins directory (e.g., C:\Program Files\VSTPlugins).',
    False, '');
  VST2DirPage.Add('');
  VST2DirPage.Values[0] := GetDefaultVST2Dir();
end;

// Dynamically skip directory selection pages if corresponding component is not selected
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (PageID = wpSelectDir) and (not WizardIsComponentSelected('Standalone')) then
    Result := True
  else if (PageID = VST3DirPage.ID) and (not WizardIsComponentSelected('VST3')) then
    Result := True
  else if (PageID = VST2DirPage.ID) and (not WizardIsComponentSelected('VST2')) then
    Result := True;
end;

// Getter functions for [Files] section destinations
function GetVST3Dir(Param: String): String;
begin
  Result := VST3DirPage.Values[0];
end;

function GetVST2Dir(Param: String): String;
begin
  Result := VST2DirPage.Values[0];
end;
