UE Viewer (UModel)
==================

UE Viewer, formerly known as "Unreal model viewer", is a viewer for visual resources of games made with
[Unreal engine](http://www.unrealengine.com/). Currently all engine versions (from 1 to 4) are supported.

There's a place where you may discuss about the source code:
[gildor.org forums](http://www.gildor.org/smf/index.php?board=37.0).


Obtaining the source code
-------------------------

The source code is [available on GitHub](https://github.com/gildor2/UModel). You may either checkout it
using any Git client, or download it as a [Zip file](https://github.com/gildor2/UModel/archive/master.zip).


Building the source code
------------------------

We are using own build system to compile UModel. You may find a Perl script in Tools/genmake. This script
generates makefiles from some human-friendly project format. After that you may build generated makefile
using 'nmake' for Visual Studio or 'make' for gcc.

### Windows

UModel is compiled using Visual Studio. Required VisualStudio 2013 or newer. Older Visual Studio compilers are
not suitable because UModel's code using some C++11 stuff.

Currently build is performed with Visual C++ 2013.

Build system utilizes GNU Tools for building, in particular - Bash and Perl. I've packaged Windows versions
of these tools which was a part of [MinGW/MSYS project](http://www.mingw.org/). You can get everything what you need
for a build [here](https://github.com/gildor2/BuildTools). This page contains **BuildTools**. You should
download it and extract into some directory (press the green button "Clone or download", then "Download ZIP"). Let's say you extracted them to *C:\BuildTools*. After that, add *C:\BuildTools\bin*
to the system's *PATH* environment variable. As an alternative it is possible to create a batch file which will temporarily
modify *PATH* and then execute build script. Here's an example of such file:

    @echo off
    set PATH=%PATH%;C:\BuildTools\bin
    bash build.sh

To launch a build process without a batch, simply execute

    bash build.sh

### Windows 64-bit
Despite we're providing only 32-but builds of UModel, it is possible to compile it for 64-bit platform. To do that, you
should change a variable in *build.sh*: *PLATFORM* should be changed from `vc-win32` to `vc-win64`. Please note that
64-bit SDL2.dll is not present in this git repository, you should download this library by yourself.

### Linux

This system has everything what is required for build by default. You'll only need to install SDL2 development package
(and of course gcc). To build UModel, simply execute the following command from terminal

    ./build.sh

### Visual Studio Code
UModel contains project files needed for opening and running it from [Visual Studio Code](https://code.visualstudio.com/). Just open umodel's folder in VSCode, and you'll get everything. Project already has a build task and launch actions set up. Of course you'll need a [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) installed.


C runtime library for MSVC
--------------------------

UModel is dynamically linked with CRT library, so it requires CRT DLL files to be installed onto your system. It is possible
to statically link with you compiler's CRT by changing a line in *common.project* (with cost of growing executable file size):
```
LIBC = shared
```

to

```
LIBC = static
```

UModel uses custom CRT library for being able to link against MSVCRT.DLL. MSVCRT.DLL is choosen because it allows to
reduce size of UModel distribution without needs to install compiler runtime libraries on system - MSVCRT.DLL present on
any Windows system. You may disable MSVCRT.DLL linking by commenting out line
```
OLDCRT = 1
```

Please note that custom CRT library will not be compatible with Visual Studio 2015, so it must be disabled in order to
build with this or newer Visual Studio version. There's no needs to disable OLDCRT manually if you're correctly setting
*vc_ver* variable in *build.sh* - it will be disabled automatically.

You might also want to disable OLDCRT if you didn't install MSVCRT library as described below.

If you want to use MSVCRT.DLL, you should extract **MSVCRT.zip** archive available
[here](https://github.com/gildor2/UModel/releases) to the directory LIBS one level above of UModel directory.
So, the directory structure should look like this

    /Libs
      /MSVCRT
        /include
        /lib
        msvcrt.project
    /UModel
      /Core
      /Unreal
      ...
      build.sh
      ...

Also you may change MSVCRT library path by changing **WDKCRT** variable in *common.project*.


Debugging in Visual Studio
--------------------------

UModel was released without a Visual Studio solution. By the way it is still possible to debug it within an IDE. You
can build a Debug version of UModel by uncommenting ```#define MAX_DEBUG 1``` in *UmodelTool/Build.h* and rebuilding the
project. After that you'll get executable with optimizations disabled, and with some extra features. For example,
if umodel.exe crashes, and it is started with *-debug* option, standard Windows window appears with prompt to close
program or debug it. You may choose "Debug with Visual Studio" there.

If you want to debug umodel.exe in VIsual Studio without having a crash, you may load it either from IDE (```File |
Open | Project/Solution```, then select *umodel.exe*), or you may type

    devenv umodel.exe

from console.

It is recommended to use **Visual Studio 2013** IDE or newer because it has more advanced debugging features than previous studio
versions. You may copy **Tools/umodel.natvis** file to *C:\Users\Your_user_folder\My Documents\Visual Studio 20NN\Visualizers*,
and after that you'll be able to view *TArray* and *FString* structures during debug session.


Directory structure
-------------------

    /.vscode - Visual Studio Code project files
    /Core
      /GL - OpenGL wrapper builder
    /Docs
    /Exporters
    /Libs - third-party libraries used for building
    /MeshInstance
    /obj - all compiled object files goes there
    /Tools
      /CompatTable
      /MaxActorXImport
      /PackageExtract
      /PackageUnpack
    /UI - library used to show UI on Windows
    /UmodelTool - source code of umodel itself
    /Unreal - source code of Unreal Engine framework
      /Shaders - shaders used in UModel
    /Viewers
