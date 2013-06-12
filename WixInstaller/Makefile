#----------------------------------
# Makefile for Microsoft NMAKE
#
# Call with only the version parameter to set the default target to all installers.
#   nmake VERSION=1.2.3
# Call with a definition of the NMAKE variable ARCH on the command line
#   nmake VERSION=1.2.3 ARCH=ia32
#   nmake VERSION=1.2.3 ARCH=x64
# If an architecture is specified, the default target is constrained to it.
#----------------------------------

.SUFFIXES: .msi .wixobj .wxs

Build_Dir_common = ..\build\ #
Build_Dir_ia32 = ..\build\ia32\ # comment prevents newline
Build_Dir_x64  = ..\build\x64\ #

!ifndef Configuration
Configuration = Release Test
!endif

!ifndef VERSION
VERSION = 99.9
!endif

#---------------------
# Default Targets
#
# We change the default rule depending upon the ARCH (architecture) definition.
#---------------------

Installer_ia32 = $(Build_Dir_ia32)adblockplusie-$(VERSION)-en-us-ia32.msi
Installer_x64  = $(Build_Dir_x64)adblockplusie-$(VERSION)-en-us-x64.msi
Setup = $(Build_Dir_common)setup-$(VERSION).exe

!ifndef ARCH
default: $(Installer_ia32) $(Installer_x64)
!elseif "$(ARCH)"=="ia32"
default: $(Installer_ia32)
!elseif "$(ARCH)"=="x64"
default: $(Installer_x64)
!else
!error Unknown variable ARCH=$(ARCH)
!endif

all: ia32 x64 setup

ia32: $(Installer_ia32)

x64: $(Installer_x64)

setup: $(Setup)

#---------------------
# candle .wxs --> .wixobj
#---------------------

Candle = candle -nologo -dNoDefault -dVersion=$(VERSION) "-dConfiguration=$(Configuration)" $(CANDLE_FLAGS) $(*F).wxs -out $@
.wxs{$(Build_Dir_common)}.wixobj:
    $(Candle)
.wxs{$(Build_Dir_ia32)}.wixobj:
    $(Candle) -arch x86
.wxs{$(Build_Dir_x64)}.wixobj:
    $(Candle) -arch x64

objects_common = $(Build_Dir_common)custom_WixUI_InstallDir.wixobj
objects_ia32 = $(Build_Dir_ia32)adblockplusie.wixobj $(objects_common)
objects_x64 = $(Build_Dir_x64)adblockplusie.wixobj $(objects_common)

object_setup = $(Build_Dir_common)setup.wixobj
$(object_setup): setup.wxs
    $(Candle) -ext WixBalExtension

#---------------------
# light .wixobj --> .msi
#---------------------

Light = light -notidy -nologo -cultures:en-us -loc en-us.wxl -ext WixUIExtension -out $@

$(Installer_ia32): $(objects_ia32) "..\build\ia32\$(Configuration)\AdblockPlus.dll"
    $(Light) $(objects_ia32)

$(Installer_x64): $(objects_x64) "..\build\x64\$(Configuration)\AdblockPlusx64.dll"
    $(Light) $(objects_x64)

#---------------------
# light .wixobj --> .exe
#---------------------

$(Setup): $(object_setup)
    $(Light) $(object_setup) -ext WixBalExtension

#---------------------
# msiexec .msi --> installed --> uninstalled
#---------------------

install-ia32: $(Installer_ia32)
    call <<nmake_temporary.bat
pushd $(Build_Dir_ia32)
msiexec /i $(**F) /l*v install.log
popd
<<

install-x64: $(Installer_x64)
    call <<nmake_temporary.bat
pushd $(Build_Dir_x64)
msiexec /i $(**F) /l*v install.log
popd
<<

uninstall:
    msiexec /x {4f27c814-5ee0-4b25-b3ab-3ad565551918}

install-setup: $(Setup)
    $(Setup)

#---------------------
# Miscellaneous
#---------------------

clean:
    del $(objects_ia32)
