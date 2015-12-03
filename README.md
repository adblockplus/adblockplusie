Adblock Plus for Internet Explorer
==================================

This combines a Browser Helper Object with a singleton engine process to block
ads in Internet Explorer. The engine process embeds libadblockplus.

Requirements to work with the repository and the code
-----------------------------------------------------

### Python
You need to have installed python 2.7. It should be the version available by
default from Visual Studio as well as in your command line environment.
Simply put, make sure that path to `python.exe` is in your `PATH` environment
variable.

### Visual C++ toolset
There should be available v110 and v110_xp toolsets. For instance, they are
contained in freely available Visual Studio 2012 Express for Windows Desktop
and in any paid edition of Visual Studio 2012. Take into account that you might
need to get the recent updates of mentioned editions, more details about
v110_xp https://msdn.microsoft.com/en-us/library/jj851139.aspx.

### ATL versions
It works with ATL shipped with any paid edition of Visual Studio 2012 as well
as with ATL shipped with Visual Studio 2013 Community edition.
If you use Visual Studio 2013 Community Edition as the source of ATL then set
`ADBLOCKPLUS_ATL` environment variable to the directory of the corresponding
ATL (e.g, `C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\atlmfc`)
Attention:
- ATL is not shipped with Express edition of any Visual Studio.
- ATL from Visual Studio 2015 is not supported.
More information about libraies and headers in Visual Studios:
2015 - https://msdn.microsoft.com/en-us/library/hs24szh9(v=vs.140).aspx
2013 - https://msdn.microsoft.com/en-us/library/hs24szh9(v=vs.120).aspx
2012 - https://msdn.microsoft.com/en-us/library/hs24szh9(v=vs.110).aspx

### Visual Studio as an IDE
Currently the project configured to work with MS Visual Studio 2012 as an IDE
because the main development and release builds are in Visual Studio 2012.
However one can also use any higher version of Visual Studio as and IDE, the
caveat is to disable "Upgrade C++ Compilers and Libraries".

Getting/updating the dependencies
---------------------------------

adblockplusie has dependencies that aren't part of this repository. They are
retrieved and updated when you're generating the VS solution for the build, but
you can also manually update them by running the following:

    ./ensure_dependencies.py

Building
--------

Building is tested on the following configurations
- [free for everybody] "Microsoft Visual Studio Express 2012 for Windows
Desktop" as the source of toolset with "Microsoft Visual Studio Community 2013"
as the source of ATL. Pay attention to the configuring of the environment
described in "ATL versions" section.
- "Microsoft Visual Studio Professional 2012"
- "Microsoft Visual Studio Ultimate 2012"

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
