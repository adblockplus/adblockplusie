[Setup]
AppName=Adblock Plus IE
AppVersion=1.2
DefaultDirName=Adblock Plus IE
DisableStartupPrompt=Yes
DisableDirPage=Yes
DisableProgramGroupPage=Yes
DisableReadyPage=Yes
DisableFinishedPage=Yes
DisableWelcomePage=Yes
Uninstallable=No
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\..\build
OutputBaseFilename=adblockplusie-{#version}
SignTool=signtool

[Files]
; Install adblockplusie-FINAL-x64.msi if running in 64-bit mode,
; adblockplusie-FINAL-ia32.msi otherwise.
Source: "..\..\build\x64\adblockplusie-{#version}-multilanguage-x64.msi"; DestDir: "{tmp}"; Check: Is64BitInstallMode
Source: "..\..\build\ia32\adblockplusie-{#version}-multilanguage-ia32.msi"; DestDir: "{tmp}"; Check: not Is64BitInstallMode

[Run]
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\adblockplusie-{#version}-multilanguage-x64.msi"""; Check: Is64BitInstallMode
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\adblockplusie-{#version}-multilanguage-ia32.msi"""; Check: not Is64BitInstallMode

[Code]
// Make sure InnoSetup always runs in silent mode, the UI is provided by the
// MSI. Origin of the code is https://stackoverflow.com/a/21577388/785541.
#ifdef UNICODE
  #define AW "W"
#else
  #define AW "A"
#endif
type
  HINSTANCE = THandle;

function ShellExecute(hwnd: HWND; lpOperation: string; lpFile: string;
  lpParameters: string; lpDirectory: string; nShowCmd: Integer): HINSTANCE;
  external 'ShellExecute{#AW}@shell32.dll stdcall';

function InitializeSetup: Boolean;
begin
  // if this instance of the setup is not silent which is by running
  // setup binary without /SILENT parameter, stop the initialization
  Result := WizardSilent;
  // if this instance is not silent, then...
  if not Result then
  begin
    // re-run the setup with /SILENT parameter; because executing of
    // the setup loader is not possible with ShellExec function, we
    // need to use a WinAPI workaround
    if ShellExecute(0, '', ExpandConstant('{srcexe}'), '/SILENT', '',
      SW_SHOW) <= 32
    then
      // if re-running this setup to silent mode failed, let's allow
      // this non-silent setup to be run
      Result := True;
  end;
end;
