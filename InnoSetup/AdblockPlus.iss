; AdblockPlus installer for 32 & 64

[Setup]
AppVerName=Adblock Plus IE
AppName=Adblock Plus IE
DefaultDirName={pf}\Adblock Plus IE
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\Adblock Plus IE
DisableDirPage=yes


[Files]
Source: "..\build\ia32\Release Test\AdblockPlus.dll"; DestDir: "{app}"
Source: "..\build\x64\Release Test\AdblockPlusX64.dll"; DestDir: "{app}"
Source: "..\html\static\css\*"; DestDir: "{app}\html\static\css"
Source: "..\html\static\img\*"; DestDir: "{app}\html\static\img"
Source: "..\html\static\img\social\*"; DestDir: "{app}\html\static\img\social"
Source: "..\html\static\img\button-background\*"; DestDir: "{app}\html\static\img\button-background"
Source: "..\html\static\js\*"; DestDir: "{app}\html\static\js"
Source: "..\html\templates\*"; DestDir: "{app}\html\templates"
Source: "..\files\settings.ini"; DestDir: "{app}"
Source: "..\files\dictionary_w.ini"; DestDir: "{app}"
Source: "..\files\settings_page_w.ini"; DestDir: "{app}"

[Code]

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    Exec(ExpandConstant('{sys}') + '\regsvr32', '/s ' + '"' + ExpandConstant('{app}') + '\AdblockPlus.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
    if IsWin64 then
      Exec(ExpandConstant('{sys}') + '\regsvr32', '/s ' + '"' + ExpandConstant('{app}') + '\AdblockPlusX64.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
  end
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec(ExpandConstant('{sys}') + '\regsvr32', '/u /s ' + '"' + ExpandConstant('{app}') + '\AdblockPlus.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
    if IsWin64 then
      Exec(ExpandConstant('{sys}') + '\regsvr32', '/u /s ' + '"' + ExpandConstant('{app}') + '\AdblockerPlusX64.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
  end
end;
