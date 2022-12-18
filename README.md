UE Viewer
=========

UE Viewer is a viewer for visual resources of games made with [Unreal engine](https://www.unrealengine.com/).
Currently all engine versions (from 1 to 4) are supported.

The project was originally named the "Unreal model viewer", however the name
[was changed](https://www.gildor.org/smf/index.php/topic,731.0.html) in 2011 to meet the request from Epic Games.
Please note that "official" project's name is "UE Viewer", and a short unofficial name of the project is "umodel"
(it was left from the older name "**U**nreal **MODEL** viewer").

There's a place where you may discuss the source code:
[gildor.org forums](https://www.gildor.org/smf/index.php?board=37.0).


Getting the source code
-----------------------
The source code is [available at GitHub](https://github.com/gildor2/UEViewer). You may either checkout it
with use of any Git client, or download it as a [Zip file](https://github.com/gildor2/UEViewer/archive/master.zip).


Building the source code
------------------------
We are using own build system to compile UE Viewer. You may find a Perl script in *Tools/genmake*. This script
generates makefiles from some human-friendly project format. After that you may build generated makefile
using 'nmake' for Visual Studio or 'make' for gcc. Build process is controlled with *build.sh* script.

### build.sh options
To list all options, run `build.sh --help`. Current options are:
- `--64` compile for Windows 64bit
- `--debug` make a debug version of executable
- `--profile` make a special build which is intended to use with external profiler
- `--vc <version>` specify which Visual Studio version should be used for compilation, default is latest compiler
  installed on your system

Please note that `build.sh` is not just a shortcut for calling `make -f <makefile>`, it performs more actions.
It does:
- Generating a makefile for current platform.
- Making `UModelTool/Version.h` file which contains current build number based on number of Git commits.
- Preprocessing shaders (with executing `Unreal/Shaders/make.pl`).
- It has the possibility to compile just a single cpp file from the project (used with Visual Studio Code Ctrl+F7 key).

### Windows 32-bit

UE Viewer is compiled using Visual Studio. Required VisualStudio 2013 or newer. Older Visual Studio compilers are
not suitable because viewer's code using some C++11 stuff.

Currently build is performed with Visual C++ 2019.

Build system utilizes GNU Tools for building, in particular - Bash and Perl. I've packaged Windows versions
of these tools which was a part of [MinGW/MSYS project](http://www.mingw.org/). You can get everything what you need
for a build [here](https://github.com/gildor2/BuildTools). You should download it and extract into some directory (press
the green button "Clone or download", then "Download ZIP"). Let's say you
extracted everything to *C:\BuildTools*. After that, add *C:\BuildTools\bin* to the system's *PATH* environment variable. As an
alternative it is possible to create a batch file which will temporarily modify *PATH* and then execute build script.
Here's an example of such file:

    @echo off
    set PATH=C:\BuildTools\bin;%PATH%
    bash build.sh

To launch a build process without a batch, simply execute

    bash build.sh

### Windows 64-bit
Despite only 32-bit builds of UE Viewer being provided, it is possible to compile it for 64-bit platform. To do that, you
should change a variable in *build.sh*: *PLATFORM* should be changed from `vc-win32` to `vc-win64`. Also 64-bit build could
be initiated with launching

	build.sh --64

### Linux
Linux system has the most of dependencies by default. You'll need to install the following development packages if they're
not available on your system: SDL2, zlib, libpng. Of course, you'll also need gcc for compiling the project.
To build UE Viewer, simply execute the following command from terminal

    ./build.sh

When compiling for Linux, project will use system's zlib and libpng libraries. If you want to bundle (statically link) them
into umodel executable, you may find and comment the following line in *common.project*

	USE_SYSTEM_LIBS = 1

In this case, Linux build will be performed in the same way as Windows build, with compiling and bundling mentioned libraries.

### macOS
UE Viewer is provided with initial support for macOS platform. I'm using VMWare macOS image to build it, so I can't do the
full testing. Therefore, some features are disabled:
- no OpenGL support (no visualization) - it is explicitly disabled in *UmodelTool/Build.h*
- no multithreading support - it's disabled in the same place

In other words, UE Viewer on macOS works just like a simple command-line exporter utility.


Using IDE
---------

### Visual Studio
As UE Viewer is using custom cross-platform build system, there's no MSBuild support. However we have a simple Visual Studio
project which allows to use this IDE to edit, compile, run and debug the project. Project files are located in `.vs` directory.
In order to open the project, you should start Visual Studio, use "Open a local folder" command, and then choose root project's
directory. Please note: there's .sln file somewhere in *Tools* folder, don't use it - it is intended for UI framework testing.

Please note that you should use Visual Studio 2019 or newer, otherwise [some features will not work](https://www.gildor.org/smf/index.php/topic,7419.0.html).

### Visual Studio Code
UE Viewer contains project files needed for opening and running it from [Visual Studio Code](https://code.visualstudio.com/).
Just open viewer's folder in VSCode, and you'll get everything. Project already has a build task and launch actions set up.
Of course you'll need a [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) installed.
You may build, launch and debug UE Viewer using this IDE.

VSCode project comes with additional build command which could be bound to a key. Just use
```
	{
		"key": "ctrl+f7",
		"command": "workbench.action.tasks.runTask",
		"args": "Compile"
	}
```
and then Ctrl+F7 key will compile a file currently opened in editor. Of course, it won't work for headers and other non-cpp
files.

By default, Visual Studio Code project performs Debug build. If you want something else, change `.vscode/tasks.json` file,
and remove `--debug` option from `build.sh` command.

There are 2 configurations For debugging: "No arguments" runs UE Viewer with default startup UI, and for use of command line
you may launch 2nd "Volatile" configuration, which reads command line arguments from file *docs/cmdline.cfg* - please refer to
[Response files documentation](https://github.com/gildor2/UModel/wiki/Response-file) for details on its format. You may also
edit *.vscode/tasks.json* file to add your own debug configurations.


Advanced debugging using Visual Studio
--------------------------------------
Initially UE Viewer was released without a Visual Studio solution. However it was possible to debug it within an IDE. The information
below describes how to build and debug UE Viewer for debugging without use of VS project (e.g. when you're using older IDE version).

You can build a Debug version of viewer by uncommenting ```#define MAX_DEBUG 1``` in *UmodelTool/Build.h* and rebuilding the
project. After that you'll get executable with optimizations disabled, and with some extra features. For example,
if umodel.exe crashes, and it is started with *-debug* option, standard Windows window appears with prompt to close
program or debug it. You may choose "Debug with Visual Studio" there.

Also you may use `--debug` parameter for build.sh script. This will generate separate set of object files and link into
debug version of the executable (with the same executable file's name). You may quickly switch between "debug" and "release"
builds without having to fully recompile the program.

If you want to debug umodel.exe in Visual Studio without having a crash, you may load it either from IDE (```File |
Open | Project/Solution```, then select *umodel.exe*), or you may type in console

    devenv umodel.exe

It is recommended to use **Visual Studio 2013** IDE or newer because it has more advanced debugging features than previous studio
versions. You may copy **Tools/umodel.natvis** file to *C:\Users\Your_user_folder\My Documents\Visual Studio 20NN\Visualizers*,
and after that you'll be able to view *TArray* and *FString* structures during debug session.


C runtime library for MSVC
--------------------------
UE Viewer is dynamically linked with CRT library, so it requires CRT DLL files to be installed onto your system. It is possible
to *statically* link with you compiler's CRT by changing a line in *common.project* (with cost of growing executable file size):

    LIBC = shared

to

    LIBC = static

UE Viewer uses custom CRT library for being able to link against MSVCRT.DLL. MSVCRT.DLL is chosen because it allows to
reduce size of UE Viewer distribution without needs to install compiler runtime libraries onto a Windows system - MSVCRT.DLL present on
_any_ Windows installation. You may disable MSVCRT.DLL linking by commenting out the line

    OLDCRT = 1

Previously there were some problems with using msvcrt.dll with Visual Studio compiler 2015 and newer. However all issues has been
solved. For those who interested in details, I've [prepared an article](https://github.com/gildor2/UModel/wiki/Using-MSVCRT.DLL-with-Visual-Studio-compiler).

If you want to use MSVCRT.DLL, you should extract **MSVCRT.zip** archive available
[here](https://github.com/gildor2/UModel/releases) to the directory LIBS one level above of UModel directory.
So, the directory structure should look like this
```
├── Libs
│   └── MSVCRT
│       ├── include
│       ├── lib
│       └── msvcrt.project
├── UModel
│   ├── Core
│   ├── Unreal
│   ...
│   ├── build.sh
│   ...
```
Also you may change MSVCRT library path by changing **WDKCRT** variable in *common.project*.


Directory structure
-------------------
Below is the list of major folders which exists in this repository or which are generated during build process.
```
├── .vs                   # Visual Studio 2019 project files
├── .vscode               # Visual Studio Code project files
├── Core                  # corelibraries not related to Unreal engine
│   └── GL                # OpenGL wrapper builder
├── Docs                  # miscellaneous text files
├── Exporters             # exporters for different object types
├── Libs                  # third-party libraries used for building
├── MeshInstance          # mesh renderers
├── obj                   # all compiled object files goes there
├── Tools
│   ├── CompatTable       # source of compatibility table
│   ├── MaxActorXImport   # ActorX Importer script for 3ds Max
│   ├── PackageExtract    # Unreal package extractor source
│   └── PackageUnpack     # unreal package decompressor source
├── UI                    # library used to show UI on Windows
├── UmodelTool            # source code of umodel itself
├── Unreal                # source code of Unreal Engine framework
│   └── Shaders           # shaders used in UModel's renderer
├── Viewers               # viewers for different object types
├── build.sh              # main build script
├── common.project        # main project file, reused between different sub-projects
├── t.bat                 # Windows CMD caller for test.sh
└── test.sh               # internal script used for testing
```

License
-------
UE Viewer is licensed under the MIT License, see [LICENSE.txt](https://github.com/gildor2/UEViewer/blob/master/LICENSE.txt) for more information.
