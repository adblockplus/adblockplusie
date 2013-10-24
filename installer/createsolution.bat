@echo off
pushd %~dp0
python ..\libadblockplus\msvs_gyp_wrapper.py --depth=build\ia32 -f msvs --generator-output=build\ia32 -G msvs_version=2012 -Dtarget_arch=ia32 installer.gyp
python ..\libadblockplus\msvs_gyp_wrapper.py --depth=build\x64  -f msvs --generator-output=build\x64  -G msvs_version=2012 -Dtarget_arch=x64  installer.gyp
popd
