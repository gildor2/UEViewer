UE Viewer (UModel)
==================

UE Viewer is a viewer for visual resources of games made with [Unreal engine](https://www.unrealengine.com/).
Currently all engine versions (from 1 to 4) are supported.

Previously project was called "Unreal model viewer", however the name
[has been changed](https://www.gildor.org/smf/index.php/topic,731.0.html) in 2011 to meet demand from Epic Games.

There's a place where you may discuss the source code:
[gildor.org forums](https://www.gildor.org/smf/index.php?board=37.0).


Obtaining the source code
-------------------------
The source code is [available on GitHub](https://github.com/gildor2/UModel). You may either checkout it
using any Git client, or download it as a [Zip file](https://github.com/gildor2/UModel/archive/master.zip).


Building the source code
------------------------

We are using own build system to compile UModel. You may find a Perl script in *Tools/genmake*. This script
generates makefiles from some human-friendly project format. After that you may build generated makefile
using 'nmake' for Visual Studio or 'make' for gcc. Build process is controlled with *build.sh* script.

### build.sh options
To list all options, run `build.sh --help`. Current options are:
- `--64` compile for Windows 64bit
- `--debug` make a debug version of executable
- `--vc <version>` specify which Visual Studio version should be used for compilation, default is latest compiler
  installed on your system

### Windows 32-bit

UModel is compiled using Visual Studio. Required VisualStudio 2013 or newer. Older Visual Studio compilers are
not suitable because UModel's code using some C++11 stuff.

Currently build is performed with Visual C++ 2019.

Build system utilizes GNU Tools for building, in particular - Bash and Perl. I've packaged Windows versions
of these tools which was a part of [MinGW/MSYS project](http://www.mingw.org/). You can get everything what you need
for a build [here](https://github.com/gildor2/BuildTools). This page contains **BuildTools**. You should
download it and extract into some directory (press the green button "Clone or download", then "Download ZIP"). Let's say you
extracted them to *C:\BuildTools*. After that, add *C:\BuildTools\bin* to the system's *PATH* environment variable. As an
alternative it is possible to create a batch file which will temporarily modify *PATH* and then execute build script.
Here's an example of such file:

    @echo off
    set PATH=%PATH%;C:\BuildTools\bin
    bash build.sh

To launch a build process without a batch, simply execute

    bash build.sh

### Windows 64-bit
Despite we're providing only 32-but builds of UModel, it is possible to compile it for 64-bit platform. To do that, you
should change a variable in *build.sh*: *PLATFORM* should be changed from `vc-win32` to `vc-win64`. Also 64-bit build could
be initiated with launching *build.sh --64*.

### Linux
This system has everything what is required for build by default. You'll only need to install SDL2 development package
(and of course gcc). To build UModel, simply execute the following command from terminal

    ./build.sh

### Visual Studio Code
UModel contains project files needed for opening and running it from [Visual Studio Code](https://code.visualstudio.com/).
Just open UModel's folder in VSCode, and you'll get everything. Project already has a build task and launch actions set up.
Of course you'll need a [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) installed.

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


C runtime library for MSVC
--------------------------
UModel is dynamically linked with CRT library, so it requires CRT DLL files to be installed onto your system. It is possible
to statically link with you compiler's CRT by changing a line in *common.project* (with cost of growing executable file size):

    LIBC = shared

to

    LIBC = static

UModel uses custom CRT library for being able to link against MSVCRT.DLL. MSVCRT.DLL is chosen because it allows to
reduce size of UModel distribution without needs to install compiler runtime libraries on system - MSVCRT.DLL present on
any Windows system. You may disable MSVCRT.DLL linking by commenting out line

    OLDCRT = 1

Please note that custom CRT library will not be compatible with Visual Studio 2015, so it must be disabled in order to
build with this or newer Visual Studio version. There's no needs to disable OLDCRT manually if you're correctly setting
*vc_ver* variable in *build.sh* - it will be disabled automatically.

You might also want to disable OLDCRT if you didn't install MSVCRT library as described below.

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


Debugging in Visual Studio
--------------------------
UModel was released without a Visual Studio solution. By the way it is still possible to debug it within an IDE. You
can build a Debug version of UModel by uncommenting ```#define MAX_DEBUG 1``` in *UmodelTool/Build.h* and rebuilding the
project. After that you'll get executable with optimizations disabled, and with some extra features. For example,
if umodel.exe crashes, and it is started with *-debug* option, standard Windows window appears with prompt to close
program or debug it. You may choose "Debug with Visual Studio" there.

Also you may use `--debug` parameter for build.sh script. This will generate separate set of object files and link into
debug version of the executable (with the same executable file's name). You may quickly switch between "debug" and "release"
builds without having to fully recompile the program.

If you want to debug umodel.exe in Visual Studio without having a crash, you may load it either from IDE (```File |
Open | Project/Solution```, then select *umodel.exe*), or you may type

    devenv umodel.exe

from console.

It is recommended to use **Visual Studio 2013** IDE or newer because it has more advanced debugging features than previous studio
versions. You may copy **Tools/umodel.natvis** file to *C:\Users\Your_user_folder\My Documents\Visual Studio 20NN\Visualizers*,
and after that you'll be able to view *TArray* and *FString* structures during debug session.

### Visual Studio Code
As was mentioned earlier, UModel source code comes with Visual Studio Code project. You may easily edit, launch and debug UModel
with it. For debugging, there are 2 configurations: "No arguments" runs UModel with default startup UI, and for command line use
you may launch 2nd "Volatile" configuration, which reads command line string from file *docs/cmdline.cfg* - please refer to
[Response files documentation](https://github.com/gildor2/UModel/wiki/Response-file) for details on its format.

Directory structure
-------------------
Below is the list of major folders which exists in this repository or which are generated during build process.
```
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
├── common.project        # main project file, reused vetween different sub-projects
├── t.bat                 # Windows CMD caller for test.sh
└── test.sh               # internal script used for testing
```

License
-------
The code is not covered with any existing license yet, however I'm thinking about adding BSD 3-clause license. I just probably
need help from some people who knows about that more than I.
