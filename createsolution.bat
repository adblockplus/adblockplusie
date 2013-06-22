@echo off

set version=%1
set type=%2
if "%type%"=="" set type=devbuild

pushd %~dp0
if NOT EXIST build\ia32\libadblockplus\shell\nul mkdir build\ia32\libadblockplus\shell
if NOT EXIST build\x64\libadblockplus\shell\nul mkdir build\x64\libadblockplus\shell

python libadblockplus\msvs_gyp_wrapper.py --depth=build\ia32 -f msvs -I libadblockplus\common.gypi --generator-output=build\ia32 -G msvs_version=2012 -Dtarget_arch=ia32 -Dbuild_type=%type% -Dbuild_version=%version% adblockplus.gyp
python libadblockplus\msvs_gyp_wrapper.py --depth=build\x64 -f msvs -I libadblockplus\common.gypi --generator-output=build\x64 -G msvs_version=2012 -Dtarget_arch=x64 -Dbuild_type=%type% -Dbuild_version=%version% adblockplus.gyp
popd
