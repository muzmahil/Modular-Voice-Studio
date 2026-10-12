; ==============================================================================
; Modular Voice Studio - Audio Plugins Installer (VST3 & VST2)
; ==============================================================================

#define MyAppName "Modular Voice Studio"
#define MyAppVersion "1.5.0"
#define MyAppPublisher "Modular Voice Studio Team"
#define MyAppURL "https://github.com/isudev/ModularAudioOS"

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

; For plugin-only installer, we disable standard application dir page
DefaultDirName={commoncf}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes

; Modern Styling & Layout
WizardStyle=modern
WizardSizePercent=120,120
WizardImageFile=Assets\WizardImage.bmp
WizardSmallImageFile=Assets\WizardSmallImage.bmp
SetupIconFile=..\Standalone\Assets\icon.ico
UninstallFilesDir={commoncf}\VST3\Modular Voice Studio.vst3

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
Name: "full"; Description: "Full Installation"
Name: "custom"; Description: "Custom Installation"; Flags: iscustom
Name: "vst3only"; Description: "VST3 Plugin Only"
Name: "vst2only"; Description: "VST2 Plugin Only"

[Components]
Name: "VST3"; Description: "Modular Voice Studio VST3"; Types: full vst3only
Name: "VST2"; Description: "Modular Voice Studio VST2"; Types: full vst2only


[Files]
; --- VST3 Plugin Bundle ---
Source: "..\build\ModularVoiceStudio_artefacts\Release\VST3\Modular Voice Studio.vst3\*"; DestDir: "{code:GetVST3Dir}\Modular Voice Studio.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: VST3

; --- VST2 Plugin DLL ---
Source: "..\build\ModularVoiceStudio_artefacts\Release\VST\Modular Voice Studio.dll"; DestDir: "{code:GetVST2Dir}"; Flags: ignoreversion; Components: VST2

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
  // Page 1: VST3 Directory Selection (appears right after Components selection if VST3 is selected)
  VST3DirPage := CreateInputDirPage(
    wpSelectComponents,
    'Select VST3 Plugin Destination',
    'Where should Modular Voice Studio VST3 be installed?',
    'Setup will install Modular Voice Studio VST3 into the following directory.',
    False, '');
  VST3DirPage.Add('');
  VST3DirPage.Values[0] := GetDefaultVST3Dir();

  // Page 2: VST2 Directory Selection (appears after VST3 page if VST2 is selected)
  VST2DirPage := CreateInputDirPage(
    VST3DirPage.ID,
    'Select VST2 Plugin Destination',
    'Where should Modular Voice Studio VST2 be installed?',
    'Setup will install Modular Voice Studio VST2 into the following directory.',
    False, '');
  VST2DirPage.Add('');
  VST2DirPage.Values[0] := GetDefaultVST2Dir();

end;

// Dynamically skip directory selection pages if corresponding component is not selected
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (PageID = VST3DirPage.ID) and (not WizardIsComponentSelected('VST3')) then
    Result := True
  else if (PageID = VST2DirPage.ID) and (not WizardIsComponentSelected('VST2')) then
    Result := True;
end;

// Ensure at least one component is selected before proceeding from wpSelectComponents
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectComponents then
  begin
    if (not WizardIsComponentSelected('VST3')) and (not WizardIsComponentSelected('VST2')) then
    begin
      MsgBox('Please select at least one plugin format (VST3 or VST2) to install.', mbError, MB_OK);
      Result := False;
    end;
  end;
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
