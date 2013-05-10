@echo off

if %1.==. goto NoKey

pushd %~dp0
call ..\libadblockplus\createsolution.bat
msbuild ..\libadblockplus\build\ia32\libadblockplus.sln /p:Configuration=Release
msbuild ..\libadblockplus\build\x64\libadblockplus.sln /p:Configuration=Release
msbuild AdPlugin.sln "/p:Configuration=Release Test" /p:Platform=Win32
msbuild AdPlugin.sln "/p:Configuration=Release Test" /p:Platform=x64
signtool.exe sign /v /d "Adblock Plus" /du "http://adblockplus.org/" /f %1 /tr "http://www.startssl.com/timestamp" "AdBlocker\Release Test\AdblockPlus.dll"
signtool.exe sign /v /d "Adblock Plus" /du "http://adblockplus.org/" /f %1 /tr "http://www.startssl.com/timestamp" "AdBlocker\x64\Release Test\AdblockPlusx64.dll"
popd
goto End

:NoKey
  echo Please add a command line parameter with the path of the signing key file
  goto End

:End
