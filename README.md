Adblock Plus for Internet Explorer
==================================

This combines a Browser Helper Object with a singleton engine process to block
ads in Internet Explorer. The engine process embeds libadblockplus.

Building
--------

You need Microsoft Visual C++ (Express is sufficient) 2012 and Python 2.6. Make
sure that `python.exe` is on your `PATH`. When building with Express Edition
you also need Windows 7.1 Device Kit (*not* version 8) to satisfy the ATL
dependency, set `WINDDKDIR` environment variable to the installation directory
of the Device Kit.

* Execute `createsolution.bat` to generate project files, this will create
`build\ia32\adblockplus.sln` (solution for the 32 bit build) and
`build\x64\adblockplus.sln` (solution for the 64 bit build). Unfortunately,
V8 (which is used by libadblockplus) doesn't support creating both from the
same project files.
* Open `build\ia32\adblockplus.sln` or `build\x64\adblockplus.sln` in
Visual Studio and build the solution there. Alternatively you can use the
`msbuild` command line tool, e.g. run `msbuild /m build\ia32\adblockplus.sln`
from the Visual Studio Developer Command Prompt to create a 32 bit debug build.

Development environment
-----------------------

TODO: Describe how to test your build
