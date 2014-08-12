@echo off
set version=%1
IF %1.==. set version=99.9
pushd %~dp0
python ..\libadblockplus\msvs_gyp_wrapper.py --depth=build\ia32 -f msvs --generator-output=build\ia32 -G msvs_version=2012 -Dtarget_arch=ia32 -Dversion=%version% installer.gyp
python ..\libadblockplus\msvs_gyp_wrapper.py --depth=build\x64  -f msvs --generator-output=build\x64  -G msvs_version=2012 -Dtarget_arch=x64 -Dversion=%version% installer.gyp
popd