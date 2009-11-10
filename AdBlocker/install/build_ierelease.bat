::input parameters
::prod, test, dev

@echo off
cls

:: SET BUILD PARAMETERS!!!!!
set version=0.8.1
set release=858
set comment=Release 0.8.1

set pathVisualStudio=C:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools
set pathAdvancedInstaller=C:\Program Files\Caphyon\Advanced Installer 7.2.1


:: Write version to config files
echo #define IEPLUGIN_VERSION "%version%" > ..\source\Shared\Version.h

echo BUILD %version%

if %1 == setvar  (
  echo SET path to visual studio
  echo ******************
  "%pathVisualStudio%\vsvars32.bat"
  goto end
)

if %1 == prod  (
  echo CREATE PRODUCTION RELEASE
  echo ******************
  goto prod_build
)

if %1 == test  (
  echo CREATE TEST RELEASE
  echo ******************
  goto test_build
)

if %1 == dev  (
  echo CREATE DEV RELEASE
  echo ******************
  goto dev_build
)

:invalid
echo ******************
echo Please enter valid input
echo Input parameters are: prod, test, dev or setvar
goto end
        

:prod_build

@echo on
devenv ..\source\AdPlugin.sln /rebuild "Release Production"
"%pathAdvancedInstaller%\AdvancedInstaller.com" /edit adblock.aip /SetVersion %version%
"%pathAdvancedInstaller%\AdvancedInstaller.com" /rebuild adblock.aip
copy AdvInstallers\simpleadblock.msi download\simpleadblock.msi
copy AdvInstallers\simpleadblock.msi installers\simpleadblock%version%.msi

"%pathAdvancedInstaller%\AdvancedInstaller.com" /edit adblockupdate.aip /SetVersion %version%
"%pathAdvancedInstaller%\AdvancedInstaller.com" /rebuild adblockupdate.aip
copy AdvInstallers\simpleadblockupdate.msi download\simpleadblockupdate.msi
copy AdvInstallers\simpleadblockupdate.msi installers\simpleadblockupdate%version%.msi

echo %version%;%release%;%date%;%1;%comment% >> installers\simpleadblock_buildlog.dat
@echo off
goto end

:test_build

@echo on
REM devenv ..\source\AdPlugin.sln /rebuild "Release Test"
"%pathAdvancedInstaller%\AdvancedInstaller.com" /edit adblock.aip /SetVersion %version%
"%pathAdvancedInstaller%\AdvancedInstaller.com" /rebuild adblock.aip
copy AdvInstallers\simpleadblock.msi download\simpleadblocktest.msi

"%pathAdvancedInstaller%\AdvancedInstaller.com" /edit adblockupdate.aip /SetVersion %version%
"%pathAdvancedInstaller%\AdvancedInstaller.com" /rebuild adblockupdate.aip
copy AdvInstallers\simpleadblockupdate.msi download\simpleadblocktestupdate.msi

echo %version%;%release%;%date%;%1;%comment% >> installers\simpleadblock_buildlog.dat
@echo off
goto end

:dev_build
@echo on
devenv ..\source\AdPlugin.sln /rebuild "Release Development"
"%pathAdvancedInstaller%\AdvancedInstaller.com" /edit adblock.aip /SetVersion %version%
"%pathAdvancedInstaller%\AdvancedInstaller.com" /rebuild adblock.aip
copy AdvInstallers\simpleadblock.msi download\simpleadblockdevelopment.msi

"%pathAdvancedInstaller%\AdvancedInstaller.com" /edit adblockupdate.aip /SetVersion %version%
"%pathAdvancedInstaller%\AdvancedInstaller.com" /rebuild adblockupdate.aip
copy AdvInstallers\simpleadblockupdate.msi download\simpleadblockdevelopmentupdate.msi

echo %version%;%release%;%date%;%1;%comment% >> installers\simpleadblock_buildlog.dat
@echo off
goto end

:end
@echo on