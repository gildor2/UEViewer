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

UModel is compiled using Visual Studio. Currently build is performed with Visual C++ 2010, but in theory
almost all Visual Studio versions should be supported (perhaps except Visual C++ 6.0 and Visual C++ 2001).

Build system utilizes GNU Tools for building, in particular - Bash and Perl. I've packaged Windows versions
of these tools which was a part of [MinGW/MSYS project](http://www.mingw.org/). You can get everything what you need
for a build [here](https://github.com/gildor2/UModel/releases). This page contains **BuildTools.zip**. You should
download it and extract into some directory, let's say to *C:\BuildTools*. After that, put *C:\BuildTools\bin*
to the system's *PATH* variable. As an alternative it is possible to create a batch file which will temporarily
modify *PATH* and then execute build script. Here's an example of such file:

    @echo off
    set PATH=%PATH%;C:\BuildTools\bin
    bash build.sh

To launch a build process without a batch, simply execute

    bash build.sh

### Linux

This system has everything what is required for build by default. You'll only need to install SDL2 development package
(and of course gcc). To build UModel, execute from console

    ./build.sh


C runtime library for MSVC
--------------------------

UModel uses custom CRT library for being able to link with MSVCRT.DLL. It is possible to statically link with
you compiler's CRT by changing in *common.project*. Please note that custom CRT library will not be compatible
with Visual Studio 2015, you should always disable it.

```
LIBC = shared
```

to

```
LIBC = static
```

MSVCRT.DLL is choosen because it allows to reduce size of UModel distribution without needs to install compiler
runtime libraries on system. You may disable MSVCRT.DLL linking by commenting out line

```
OLDCRT = 1
```

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

**TODO**

    /Core
      /GL
    /Docs
    /Exporters
    /Libs
    /MeshInstance
    /obj
    /Tools
      /CompatTable
      /MaxActorXImport
      /PackageExtract
      /PackageUnpack
    /UI
    /UmodelTool
    /Unreal
      /Shaders
    /Viewers
