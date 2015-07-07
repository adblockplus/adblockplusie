Adblock Plus for Internet Explorer
==================================

This combines a Browser Helper Object with a singleton engine process to block
ads in Internet Explorer. The engine process embeds libadblockplus.

Getting/updating the dependencies
---------------------------------

adblockplusie has dependencies that aren't part of this repository. They are
retrieved and updated when you're generating the VS solution for the build, but
you can also manually update them by running the following:

    ./ensure_dependencies.py

Building
--------

You need Microsoft Visual C++ 2012 and Python 2.7. Make sure that `python.exe` 
is on your `PATH`. Unfortunately until we move to Visual Studio 2013  you'll
need to use Visual Studio 2012 version that comes with ATL (Proffessional etc). 

* Execute `createsolution.bat` to retrieve dependencies and generate project
files, this will create `build\ia32\adblockplus.sln` (solution for the 32 bit
build) and `build\x64\adblockplus.sln` (solution for the 64 bit build). 
Unfortunately, V8 (which is used by libadblockplus) doesn't support creating 
both from the same project files.
* Open `build\ia32\adblockplus.sln` or `build\x64\adblockplus.sln` in
Visual Studio and build the solution there. Alternatively you can use the
`msbuild` command line tool, e.g. run `msbuild /m build\ia32\adblockplus.sln`
from the Visual Studio Developer Command Prompt to create a 32 bit debug build.

Running
-------

In order to test the extension, you need to register the Browser
Helper Object with IE. You can do this by locating _AdblockPlus.dll_
(e.g. in _build\ia32\Debug_) and running (with elevated privileges):

    regsvr32 AdblockPlus.dll

For the UI to work, you also need to copy the _html_ and _locale_
directories to the same directory _AdblockPlus.dll_ is in.

Building the installer
----------------------

You need [WiX 3.8](http://wixtoolset.org) (make sure `%WIX%\bin` is in `%PATH%`)
and [InnoSetup 5.5](http://www.jrsoftware.org/isinfo.php).

* Execute `installer\createsolutions.bat` to generate the installer project
files, this will create a bunch of project files in the _installer\build\ia32_
and _installer\build\x64_ directories.
* Open and build `installer\build\ia32\installer.sln` and
`installer\build\x64\installer.sln` in Visual Studio. Alternatively you can use
the `msbuild` command line tool, i.e. run
`msibuild /m installer\build\ia32\adblockplus.sln` and
`msibuild /m installer\build\x64\adblockplus.sln` from the Visual Studio
Developer Command Prompt.
* Either open and compile `installer\src\innosetup-exe\64BitTwoArch.iss` in
InnoSetup or run `iscc.exe installer\src\innosetup-exe\64bitTwoArch.iss`.
