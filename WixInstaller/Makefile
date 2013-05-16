.SUFFIXES: .wxs .wixobj

.wxs.wixobj:
    candle -nologo $*.wxs -out $*.wixobj

default: ..\build\adblockplusie-en-us.msi

OBJ = adblockplusie.wixobj custom_WixUI_InstallDir.wixobj 

..\build\adblockplusie-en-us.msi: $(OBJ) "..\build\ia32\Release Test\AdblockPlus.dll"
    light -notidy -nologo -cultures:en-us -loc en-us.wxl -ext WixUIExtension -out $*.msi $(OBJ)

install:
    # Doesn't work. MSI does not accept relative paths.
    msiexec /i ..\build\adblockplusie-en-us.msi /l*v install.log

uninstall:
    msiexec /x ..\build\adblockplusie-en-us.msi


