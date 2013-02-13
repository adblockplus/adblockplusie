; AdblockPlus installer for 32 & 64

[Setup]
AppVerName=avast! Ad Blocker IE
AppName=avast! Adblocker IE
DefaultDirName={pf}\AVAST Software\avast! Adblocker IE
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\avast! Adblocker IE
DisableDirPage=yes


[Files]
Source: "Adblocker32.dll"; DestDir: "{app}"
Source: "Adblocker64.dll"; DestDir: "{app}"
Source: "..\html\static\css\*"; DestDir: "{userappdata}\..\LocalLow\avast! Ad Blocker\html\static\css"
Source: "..\html\static\img\*"; DestDir: "{userappdata}\..\LocalLow\avast! Ad Blocker\html\static\img"
Source: "..\html\static\js\*"; DestDir: "{userappdata}\..\LocalLow\avast! Ad Blocker\html\static\js"
Source: "..\html\templates\*"; DestDir: "{userappdata}\..\LocalLow\avast! Ad Blocker\html\templates"
Source: "..\AdBlocker\files\settings.ini"; DestDir: "{userappdata}\..\LocalLow\avast! Ad Blocker"
Source: "..\AdBlocker\files\dictionary_w.ini"; DestDir: "{userappdata}\..\LocalLow\avast! Ad Blocker"
Source: "..\AdBlocker\files\settings_page_w.ini"; DestDir: "{userappdata}\..\LocalLow\avast! Ad Blocker"

[Code]

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    Exec(ExpandConstant('{sys}') + '\regsvr32', '/s ' + '"' + ExpandConstant('{app}') + '\Adblocker32.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
    if IsWin64 then
      Exec(ExpandConstant('{sys}') + '\regsvr32', '/s ' + '"' + ExpandConstant('{app}') + '\Adblocker64.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
  end
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec(ExpandConstant('{sys}') + '\regsvr32', '/u /s ' + '"' + ExpandConstant('{app}') + '\Adblocker32.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
    if IsWin64 then
      Exec(ExpandConstant('{sys}') + '\regsvr32', '/u /s ' + '"' + ExpandConstant('{app}') + '\Adblocker64.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
  end
end;