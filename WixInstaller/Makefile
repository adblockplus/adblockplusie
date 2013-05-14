.SUFFIXES: .wxs .wixobj

.wxs.wixobj:
    candle -nologo $*.wxs -out $*.wixobj

default: ..\build\adblockplusie-en-us.msi

..\build\adblockplusie-en-us.msi: adblockplusie.wixobj "..\build\ia32\Release Test\AdblockPlus.dll"
    light adblockplusie.wixobj -notidy -nologo -cultures:en-us -loc en-us.wxl -ext WixUIExtension -out $*.msi

install:
    msiexec /i ..\build\adblockplusie-en-us.msi /l*v install.log

uninstall:
    msiexec /x ..\build\adblockplusie-en-us.msi


