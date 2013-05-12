.SUFFIXES: .wxs .wixobj

.wxs.wixobj:
    candle -nologo $*.wxs -out $*.wixobj
    
default: adblockplusie-en-us.msi

adblockplusie-en-us.msi: adblockplusie.wixobj
    light $** -notidy -nologo -cultures:en-us -loc en-us.wxl -ext WixUIExtension -out $*.msi 

install:
    msiexec /i adblockplusie-en-us.msi /l*v install.log

uninstall:
    msiexec /x adblockplusie-en-us.msi

        