#define MyAppName "KICKSTART"
#define MyAppVersion "0.0.1"
#define MyAppPublisher "JAFFCO"
#define MyAppURL "https://github.com/joeljaffesd/JUCE-kickstart"
#define MyAppExeName "KICKSTART.exe"

[Setup]
AppId={{JFCO-Kstr}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=Output
OutputBaseFilename=KICKSTART-0.0.1-Windows
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\KICKSTART_artefacts\Release\VST3\KICKSTART.vst3"; DestDir: "{code:GetVST3Dir}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\KICKSTART_artefacts\Release\Standalone\KICKSTART.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Code]
function GetVST3Dir(Default: string): string;
begin
  if RegQueryStringValue(HKLM, 'SOFTWARE\VST\VST3', '', Result) then
  begin
    Result := Result + '\KICKSTART.vst3';
  end
  else
  begin
    Result := ExpandConstant('{pf}\Common Files\VST3\KICKSTART.vst3');
  end;
end;