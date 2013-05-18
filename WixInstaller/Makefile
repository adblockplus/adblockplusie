#----------------------------------
# Makefile for Microsoft NMAKE
#
# Call with no arguments to set the default target to all installers.
#   nmake 
# Call with a definition of the NMAKE variable ARCH on the command line
#   nmake ARCH=ia32
#   nmake ARCH=x64
# If an architecture is specified, the default target is constrained to it.
#----------------------------------

.SUFFIXES: .msi .wixobj .wxs

Build_Dir_common = ..\build\ # 
Build_Dir_ia32 = ..\build\ia32\ # comment prevents newline 
Build_Dir_x64  = ..\build\x64\ #

#---------------------
# Default Targets
# 
# We change the default rule depending upon the ARCH (architecture) definition.
#---------------------

Installer_ia32 = $(Build_Dir_ia32)adblockplusie-en-us-ia32.msi 
Installer_x64  = $(Build_Dir_x64)adblockplusie-en-us-x64.msi 

!ifndef ARCH
default: $(Installer_ia32) $(Installer_x64)
!elseif "$(ARCH)"=="ia32"
default: $(Installer_ia32)
!elseif "$(ARCH)"=="x64"
default: $(Installer_x64)
!else
!error Unknown variable ARCH=$(ARCH)
!endif

#---------------------
# candle .wxs --> .wixobj 
#---------------------

Candle = candle -nologo $(CANDLE_FLAGS) $< -out $@  
.wxs{$(Build_Dir_common)}.wixobj:
    $(Candle)
.wxs{$(Build_Dir_ia32)}.wixobj:
    $(Candle) -arch x86
.wxs{$(Build_Dir_x64)}.wixobj:
    $(Candle) -arch x64

objects_common = $(Build_Dir_common)custom_WixUI_InstallDir.wixobj 
objects_ia32 = $(Build_Dir_ia32)adblockplusie.wixobj $(objects_common)  
objects_x64 = $(Build_Dir_x64)adblockplusie.wixobj $(objects_common)

#---------------------
# light .wixobj --> .msi 
#---------------------

Light = light -notidy -nologo -cultures:en-us -loc en-us.wxl -ext WixUIExtension -out $@

$(Installer_ia32): $(objects_ia32) "..\build\ia32\Release Test\AdblockPlus.dll"
    $(Light) $(objects_ia32)

$(Installer_x64): $(objects_x64) "..\build\x64\Release Test\AdblockPlusx64.dll"
    $(Light) $(objects_x64) 

#---------------------
# msiexec .msi --> installed --> uninstalled
#---------------------

install-ia32: $(Installer_ia32)
    # Doesn't work. MSI does not accept relative paths.
    msiexec /i $(**R).msi /l*v install.log

uninstall:
    msiexec /x ..\build\adblockplusie-en-us.msi

#---------------------
# Miscellaneous
#---------------------

clean:
    del $(objects_ia32) 