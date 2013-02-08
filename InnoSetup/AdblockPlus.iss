; AdblockPlus installer for 32 & 64

[Setup]
AppVerName=Adblock Plus
AppName=Adblock Plus
DefaultDirName={pf}\AdblockPlus
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\AdblockPlus
DisableDirPage=yes
LicenseFile={#file AddBackslash(SourcePath) + "License.txt"}


[Files]
Source: "AdblockPlus32.dll"; DestDir: "{app}"
Source: "AdblockPlus64.dll"; DestDir: "{app}"

[Code]

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    Exec(ExpandConstant('{sys}') + '\regsvr32', '/s ' + '"' + ExpandConstant('{app}') + '\AdblockPlus32.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
    if IsWin64 then
      Exec(ExpandConstant('{sys}') + '\regsvr32', '/s ' + '"' + ExpandConstant('{app}') + '\AdblockPlus64.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
  end
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec(ExpandConstant('{sys}') + '\regsvr32', '/u /s ' + '"' + ExpandConstant('{app}') + '\AdblockPlus32.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
    if IsWin64 then
      Exec(ExpandConstant('{sys}') + '\regsvr32', '/u /s ' + '"' + ExpandConstant('{app}') + '\AdblockPlus64.dll' + '"', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
  end
end;