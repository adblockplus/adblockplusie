; AdblockPlus installer for 32 & 64

[Setup]
AppVerName=Adblock Plus IE
AppName=Adblock Plus IE
DefaultDirName={pf}\Adblock Plus IE
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\Adblock Plus IE
DisableDirPage=yes


[Files]
Source: "..\AdBlocker\Release Test\Adblock.dll"; DestDir: "{app}"
Source: "..\AdBlocker\x64\Release Test\Adblocker.dll"; DestDir: "{app}"
Source: "..\html\static\css\*"; DestDir: "{app}\html\static\css"
Source: "..\html\static\img\*"; DestDir: "{app}\html\static\img"
Source: "..\html\static\img\social\*"; DestDir: "{app}\html\static\img\social"
Source: "..\html\static\img\button-background\*"; DestDir: "{app}\html\static\img\button-background"
Source: "..\html\static\js\*"; DestDir: "{app}\html\static\js"
Source: "..\html\templates\*"; DestDir: "{app}\html\templates"
Source: "..\AdBlocker\files\settings.ini"; DestDir: "{app}"
Source: "..\AdBlocker\files\dictionary_w.ini"; DestDir: "{app}"
Source: "..\AdBlocker\files\settings_page_w.ini"; DestDir: "{app}"

[Code]

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    Exec(ExpandConstant('{sys}') + '\regsvr32', '/s ' + '"' + ExpandConstant('{app}') + '\Adblock.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
    if IsWin64 then
      Exec(ExpandConstant('{sys}') + '\regsvr32', '/s ' + '"' + ExpandConstant('{app}') + '\Adblocker.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
  end
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec(ExpandConstant('{sys}') + '\regsvr32', '/u /s ' + '"' + ExpandConstant('{app}') + '\Adblock.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
    if IsWin64 then
      Exec(ExpandConstant('{sys}') + '\regsvr32', '/u /s ' + '"' + ExpandConstant('{app}') + '\Adblocker.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
  end
end;