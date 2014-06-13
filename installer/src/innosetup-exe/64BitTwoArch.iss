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

[Files]
; Install adblockplusie-FINAL-x64.msi if running in 64-bit mode,
; adblockplusie-FINAL-ia32.msi otherwise.
Source: "C:\projects\ABP\installer\build\x64\adblockplusie-FINAL-x64.msi"; DestDir: "{tmp}"; Check: Is64BitInstallMode
Source: "C:\projects\ABP\installer\build\ia32\adblockplusie-FINAL-ia32.msi"; DestDir: "{tmp}"; Check: not Is64BitInstallMode

[Run]
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\adblockplusie-FINAL-x64.msi"""; Check: Is64BitInstallMode
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\adblockplusie-FINAL-ia32.msi"""; Check: not Is64BitInstallMode 