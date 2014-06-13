#----------------------------------
# Makefile for Microsoft NMAKE
#
# Argument: VERSION
#   Version number to incorporate into the file names of targets
#   Defaults to 99.9 if not specified.
#
# Target: ia32
#   Release version of the multilanguage, 32-bit MSI
#   File name = adblockplusie-VERSION-multilanguage-ia32.msi
# Target: x64
#   Release version of the multilanguage, 64-bit MSI
#   File name = adblockplusie-VERSION-multilanguage-x64.msi
# Target: setup
#   Release version of the executable bundle of both of the above MSI
#   File name = adblockplusie-VERSION.exe   
#----------------------------------

Build_Dir_ia32 = build\ia32\ # comment prevents newline
Build_Dir_x64  = build\x64\ #
Build_Dir_common = build\common\ #

!ifndef VERSION
VERSION = 99.9
!endif

Installer_ia32_name = adblockplusie-$(VERSION)-multilanguage-ia32
Installer_ia32_base = $(Build_Dir_ia32)$(Installer_ia32_name)
Installer_ia32_msi = $(Installer_ia32_base).msi
Installer_ia32_archive = $(Installer_ia32_base).7z
Installer_ia32_exe = $(Installer_ia32_base).exe

Installer_x64_name = adblockplusie-$(VERSION)-multilanguage-x64 
Installer_x64_base = $(Build_Dir_x64)$(Installer_x64_name)
Installer_x64_msi = $(Installer_x64_base).msi 
Installer_x64_archive = $(Installer_x64_base).7z 
Installer_x64_exe = $(Installer_x64_base).exe 

Setup = $(Build_Dir_common)Release\adblockplusie-$(VERSION).exe

#---------------------
# Top-Level Targets
#---------------------

default:
    @echo Available targets
    @echo - ia32
    @echo - x64
    @echo - setup

ia32: $(Installer_ia32_msi)

x64: $(Installer_x64_msi)

setup: $(Setup)


#---------------------
# MSI
#   The real work happens in MSBuild.
#   The output of MSBuild has a fixed name; we need a parametric one.
#---------------------

MSBuild_Output_ia32 = $(Build_Dir_ia32)adblockplusie-FINAL-ia32.msi 
MSBuild_Output_x64 = $(Build_Dir_x64)adblockplusie-FINAL-x64.msi 

#
# NMAKE has no way of specifying that these MSBuild rules have their own 
# dependencies and thus should be considered the moral equivalent of part of the
# Makefile. For -a (all) builds, this isn't a problem. For incremental builds,
# use the 'clean-*' targets below to delete the MSBuild outputs. This forces an
# invocation of MSBuild because its targets are absent. The last step of the
# MSBuild process is a file copy, so it's not expensive to clean and build.  
#
$(Build_Dir_ia32)installer.sln $(Build_Dir_x64)installer.sln:
    createsolutions.bat

$(MSBuild_Output_ia32): $(Build_Dir_ia32)installer.sln
    msbuild /p:Configuration=Release $(Build_Dir_ia32)installer.sln

$(MSBuild_Output_x64): $(Build_Dir_x64)installer.sln
    msbuild /p:Configuration=Release $(Build_Dir_x64)installer.sln

$(Installer_ia32_msi): $(MSBuild_Output_ia32)
    copy $(MSBuild_Output_ia32) $(Installer_ia32_msi)

$(Installer_x64_msi): $(MSBuild_Output_x64)
    copy $(MSBuild_Output_x64) $(Installer_x64_msi)


clean-msi: clean-ia32 clean-x64

clean-ia32:
    del $(MSBuild_Output_ia32) $(Installer_ia32_msi) 

clean-x64:
    del $(MSBuild_Output_x64) $(Installer_x64_msi) 
 
#---------------------
# Bundle
#---------------------
object_setup = $(Build_Dir_common)setup.wixobj
Source_Dir_setup = src\setup-exe\ #
setup_compile_sources = $(Source_Dir_setup)setup.wxs 
setup_link_sources = $(Source_Dir_setup)bootstrap-theme.xml $(Source_Dir_setup)bootstrap-theme.wxl  

$(object_setup): $(setup_compile_sources)
    candle -nologo -dVersion=$(VERSION) $(setup_compile_sources) -out $(object_setup) -ext WixBalExtension

$(Setup): $(Installer_ia32_msi) $(Installer_x64_msi)
$(Setup): $(object_setup) $(setup_link_sources)
    light -nologo -dVersion=$(VERSION) $(object_setup) -ext WixBalExtension -loc $(Source_Dir_setup)bootstrap-theme.wxl -out $(Setup) 

#---------------------
# msiexec .msi --> installed --> uninstalled
#---------------------

uninstall:
    msiexec /x {4f27c814-5ee0-4b25-b3ab-3ad565551918}

install-setup: $(Setup)
    $(Setup) -log ..\build\setup.log

#---------------------
# Self-extracting versions of the MSI
#---------------------

#
# 7-Zip can't help but store directories (no command line switch to do otherwise).
#
$(Installer_ia32_exe): $(Installer_ia32_msi) $(Source_Dir_setup)self-extract-config-ia32.txt
    cd $(Build_Dir_ia32) & 7z a -t7z $(Installer_ia32_name).7z ..\..\$(MSBuild_Output_ia32) 
    copy /b $(Source_Dir_setup)7zS.sfx + $(Source_Dir_setup)self-extract-config-ia32.txt + $(Installer_ia32_archive) $(Installer_ia32_exe)

$(Installer_x64_exe): $(Installer_x64_msi) $(Source_Dir_setup)self-extract-config-x64.txt
    cd $(Build_Dir_x64) & 7z a -t7z $(Installer_x64_name).7z ..\..\$(MSBuild_Output_x64) 
    copy /b $(Source_Dir_setup)7zS.sfx + $(Source_Dir_setup)self-extract-config-x64.txt + $(Installer_x64_archive) $(Installer_x64_exe)

