UE Viewer (UModel)
==================

UE Viewer, formerly known as "Unreal model viewer", is viewer for visual resources of games made with
[Unreal engine](http://www.unrealengine.com/). Currently supported engine versions from 1 to 3, support
for Unreal engine 4 is under development.

There's a place where you may discuss source code:
[gildor.org forums](http://www.gildor.org/smf/index.php?board=37.0).


Obtaining the source code
-------------------------

The source code is [available on GitHub](https://github.com/gildor2/UModel). You may either checkout it
using any Git client, or download it as [Zip file](https://github.com/gildor2/UModel/archive/master.zip).


Building the source code
------------------------

We are using own build system to compile UModel. You may find a Perl script in Tools/genmake. This script
generates makefiles from some human-friendly project format. After that you may build generated makefile
using 'nmake' for Visual Studio or 'make' for gcc.

### Windows

UModel is compiled using Visual Studio. Currently build is performed with Visual C++ 2010, but in theory
almost all Visual Studio versions should be supported (perhaps except Visual C++ 6.0 and Visual C++ 2001).

Build system utilizes GNU Tools for building, in particular - Bash and Perl. I've packaged WIndows versions
of such tools which was a part of [MinGW project](http://www.mingw.org/). You may everything required for
build [here](https://github.com/gildor2/UModel/releases). This page contains "BuildTools.zip". You should
download it and extract into some directory. After that, put "GNU/bin" and "Tools" directories to system
*PATH* variable. Also it is possible to create batch file which will temporarily tune *PATH* and then execute
build script.

To launch build process, execute

```
bash build.sh
```

### Linux

This system has everything what is required for build. You'll only need to install SDL development package
(and of course gcc). To build UModel, execute from console

```
./build.sh
```


C runtime library for MSVC
--------------------------

UModel uses custom CRT library for being able to link with MSVCRT.DLL. It is possible to statically link with
you compiler's CRT by changing in *common.project*

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

If you want to use MSVCRT.DLL, you should extract MSVCRT.zip archive available
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


Directory structure
-------------------
