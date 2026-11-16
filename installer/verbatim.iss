; Verbatim installer (Inno Setup 6 or 7). Installs the VST3 bundle into the shared VST3 folder and checks for the
; Microsoft WebView2 runtime the editor needs. Build: ISCC.exe installer\verbatim.iss (after a Release build).

#define AppName "Verbatim"
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define AppPublisher "Verbatim Audio"
#define Bundle "..\build\Verbatim_artefacts\Release\VST3\Verbatim.vst3"

[Setup]
AppId={{6C1E3A9B-5D2F-4E8A-9B47-3F0D2A8C1E55}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/Bilohit/verbatim
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
; Keep the uninstaller out of the shared VST3 folder hosts scan.
UninstallFilesDir={commonpf64}\Verbatim
UninstallDisplayName={#AppName} {#AppVersion}
LicenseFile=..\LICENSE
OutputDir=..\build\installer
OutputBaseFilename=Verbatim-{#AppVersion}-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "{#Bundle}\*"; DestDir: "{commoncf64}\VST3\Verbatim.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Verbatim.vst3"

[Code]
// The WebView2 Evergreen runtime registers its version under this client GUID (per machine or per user).
const WebView2Key = '\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';

function HasWebView2(): Boolean;
var
  Version: String;
begin
  Result :=
    (RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node' + WebView2Key, 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0')) or
    (RegQueryStringValue(HKLM, 'SOFTWARE' + WebView2Key, 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0')) or
    (RegQueryStringValue(HKCU, 'Software' + WebView2Key, 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ErrorCode: Integer;
begin
  if (CurStep = ssPostInstall) and not HasWebView2() then
    if MsgBox('Verbatim''s editor needs the Microsoft WebView2 runtime, which is not installed. Audio still works without it.' + #13#10#13#10 +
              'Open the download page now?', mbConfirmation, MB_YESNO) = IDYES then
      ShellExec('open', 'https://developer.microsoft.com/microsoft-edge/webview2/', '', '', SW_SHOWNORMAL, ewNoWait, ErrorCode);
end;
