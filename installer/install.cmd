@echo off
setlocal
set PACKAGE=%1
if "%PACKAGE%"=="-h" goto Help
if "%PACKAGE%"=="--help" goto Help
if "%PACKAGE%"=="help" goto Help

:ArgArch
shift
set ARCH=%1
if "%ARCH%"=="ia32" goto ArgArchEnd
if "%ARCH%"=="x64" goto ArgArchEnd
if NOT "%ARCH%"=="" goto ArgArchError
set ARCH=x64
goto ArgArchEnd
:ArgArchError
echo Unrecognized architecture argument '%ARCH%' 1>&2
exit /b 1
:ArgArchEnd

:ArgPackage
if NOT "%PACKAGE%"=="abp" goto ArgPackage1
set MSI=adblockplusie-FINAL-%ARCH%.msi
set LOG=install-adblockplusie.log
goto GoMsiexec
:ArgPackage1
if NOT "%PACKAGE%"=="test" goto ArgPackage2
set MSI=test-installer-lib.msi
set LOG=install-test.log
goto GoMsiexec
:ArgPackage2
if NOT "%PACKAGE%"=="setup" goto ArgPackageError
goto goSetup
:ArgPackageError
echo Unrecognized package argument '%PACKAGE%' 1>&2
exit /b 1  

:GoMsiexec
pushd %~dp0%
cd build\%ARCH%
echo on
msiexec /i %MSI% /l*v %LOG%
@echo off
popd
exit /b

:GoSetup
pushd %~dp0%
cd build\common
echo on
echo .\setup-abp-ie
@echo off
popd
exit /b

:Help
echo install - install ABP-IE from the build directory
echo.
echo usage: install [-h] ^<package^> ^<architecture^>
echo   package - either 'abp' or 'test'
echo   architecture - either 'ia32' or 'x64'
echo   ^-h ^| --help - show this help message only
echo.
exit /b

