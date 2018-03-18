UMODEL (UE Viewer)
(c) Konstantin Nosov (Gildor), 2007-2018


Please support the development by making a donation here:
http://www.gildor.org/en/donate


System requirements
~~~~~~~~~~~~~~~~~~~
Windows or Linux operating system
x86-compatible CPU with SSE2 support
OpenGL 1.1 videocard (OpenGL 2.0 is recommended)
SDL 2.0 (for Linux only, windows distribution has SDL2.dll included)


List of supported games
~~~~~~~~~~~~~~~~~~~~~~~
Supported all Unreal engine versions (1-3). The list of supported games consists of
more than 100 game titles, it is not reasonable to include it here. Some game titles
has limited support or not supported at all. Detailed information can be found here:
http://www.gildor.org/projects/umodel/compat


Web resources
~~~~~~~~~~~~~
Umodel home page and forum:
http://www.gildor.org/en/projects/umodel
or Russian page:
http://www.gildor.org/projects/umodel

Umodel FAQ:
http://www.gildor.org/projects/umodel/faq

Some tutorials available here:
http://www.gildor.org/projects/umodel/tutorials

Youtube page with tutorials and news:
https://www.youtube.com/playlist?list=PLJROJrENPVvK-V8PCTR9qBmY0Q7v4wCym

Other documentation:
http://www.gildor.org/smf/index.php/board,9.0.html


Quick start
~~~~~~~~~~~
WARNING: it's highly recommended to read the FAQ and to watch video tutorials (see the
links above) before starting the umodel for the first time.

UModel primarily is a console application, with rich command line capabilities. Easiest
run is 'umodel <package_file>', it will start umodel in a viewer mode. To see the full
list of available command line options run 'umodel -help'.

You could also drag a package file (.upk, .xxx, .ukx etc) to umodel's icon to launch
the application. However default settings will be used in this case, so if game requires
some compatibility options, this will not work.


GUI
~~~
Some time ago simple GUI has been added for Windows version of UModel. It appears when
you start UModel without arguments (for example, clicking on UModel icon from Windows
Explorer). Please note that all command line option still works even in GUI mode.
A startup window appears only when you have neither game path nor package name specified.
It will allow you to choose where UModel will look for files ('-path' option) as well as
compatibility options. If you will set '-path' from the command line, startup GUI will
not appear. In a case you want to specify path and show startup GUI, add option '-gui'
to the command line.

Viewer window has user menu on the top of the window. Please review and try options
provided there. Most of them could be duplicated with keystrokes, and these key shortcuts
are listed in menu.


Viewer mode
~~~~~~~~~~~
The application is controlled with keyboard and mouse. You may see the full list of
keyboard shortcuts by pressing 'H' (Help) key. Here's the list of some shortcuts:

  PgUp/PgDn        browse through loaded objects
  Ctrl+S           take a screenshot into the file Screenshots/ObjectName.tga
  Alt+S            take screenshot with transparent background
  Ctrl+X           export all objects from the current scene
  Ctrl+PgUp/PgDn   scroll onscreen texts
  Shift+Up/Down    change scene FOV
  Ctrl+L           switch lighting modes
  Ctrl+Q           toggle visualization of debug information (text, 3D axis etc)
  Ctrl+G           toggle OpenGL 2.0 / OpenGL 1.1 renderer
  Esc              exit from the umodel

You may attach the AnimSet to the SkeletalMesh object using Ctrl+A key. Animation
sequences are listed by '[' and ']' keys, playback is started with a Space key.


Psk/psa export
~~~~~~~~~~~~~~
To load psk or psa into the 3ds Max you'll need ActorX Importer script created by me:
http://www.gildor.org/projects/unactorx
It has own announcements thread here:
http://www.gildor.org/smf/index.php/topic,228.0.html

Some meshes contains information which cannot fit into psk standard. For this reason I've
extended the format to hold advanced information. Files in this format has extension pskx
and cannot be loaded into UnrealEd or any other application with ActorX support. There's
only one tool with pskx support at the moment - ActorX Importer mentioned above.


Md5mesh/md5anim export
~~~~~~~~~~~~~~~~~~~~~~
Umodel has possibility to export skeletal meshes and animations in idSoftware md5 format.
To use this exporter you should use command line option "-md5". MeshAnimation and AnimSet
objects are exported as multiple md5anim files (one file for each animation track). "bounds"
section in md5anim is filled with dummy data. "hierarchy" section also does not contain real
skeleton hierarchy because Unreal engine uses hierarchy from the mesh, not from the
animation. Some md5 viewers/importers does require md5anim hierarchy, some - does not.

There is a 3ds Max md5mesh/md5anim importer script available on umodel forum:
http://www.gildor.org/smf/index.php?topic=87.0
or here
http://www.gildor.org/downloads
This script was originally created by der_ton, but was updated by me.

Please note that psk/psa format is more powerful, and ActorX Importer script has much more
capabilities than md5 Importer.


StaticMesh export
~~~~~~~~~~~~~~~~~
StaticMesh export is performed into psk format. This format was originally designed to hold
SkeletalMesh, but umodel uses it for StaticMesh too. Exported mesh will not have a skeleton
and vertex influences. Resulting psk files cannot be imported directly into the UnrealEd,
so I've decided to save ot with pskx extension to avoid silly user errors. Such mesh could
be imported into 3ds Max using ActorX Importer plugin as well as ordinary psk file.


Material export
~~~~~~~~~~~~~~~
Materials are exported in a custom format. File extension is ".mat". At the current moment,
this format is supported by ActorX Importer plugin only. Unreal engine materials are very
complex, so it's very hard to separate a few channels (diffuse, specular, bump etc) from it.
Umodel tries to accomplish this with use of some heuristics - sometimes with good results,
sometimes with bad. Umodel will never export full materials (GLSL script etc). Do not expect
too much from this feature.


Used third-party libraries
~~~~~~~~~~~~~~~~~~~~~~~~~~
SDL - Simple DirectMedia Layer
  (c) Sam Lantinga
  http://www.libsdl.org/

zlib data compression library
  (c) Jean-loup Gailly and Mark Adler
  http://zlib.net/

LZO data compression library
  (c) Markus F.X.J. Oberhumer
  http://www.oberhumer.com/opensource/lzo/

libmspack - a library for Microsoft compression formats
  (c) Stuart Caie
  http://www.cabextract.org.uk/libmspack/

NVIDIA Texture Tools
  (c) NVIDIA
  https://github.com/castano/nvidia-texture-tools

PVRTexLib Library
  (c) Imagination Technologies Limited
  http://www.imgtec.com/powervr/insider/

ASTC encoder
  (c) ARM Limited and Contributors
  https://github.com/ARM-software/astc-encoder

detex
  (c) Harm Hanemaaijer
  https://github.com/hglm/detex

LZ4
  (c) Yann Collet
  http://www.lz4.org


Changes
~~~~~~~
18.03.2018
- implemented Gears of War 4 pak file and package support; requires game override -game=gears4 (or use UI)

24.02.2018
- AES encryption key could be now in hex format: 0x123456789ABCDEF (C-like format)

18.02.2018
- implemented Heroes of Incredible Tales (HIT) static mesh support; requires -game=hit override

08.02.2018
- added Paragon support (requires game override: -game=paragon)

07.02.2018
- implemented UE4.19 support

31.01.2018
- implemented loader for encrypted compressed UE4 pak files

23.01.2018
- added support for loading AES-encrypted UE4 pak files; AES key may be specified with command line option
  -aes=key, or it will be requested by UI when needed

20.01.2018
- improved laptop keyboard support

04.01.2018
- updated UE4.18 support
- displaying a warning message in UE4 SkeletalMesh viewer when Skeleton object is not loaded, and therefore
  animation will not work

06.12.2017
- an attempt to make smoothing groups working: always exporting 1st smoothing group for all mesh faces

19.11.2017
- exporting "source art" (png) textures whenever possible - for UE3 and UE4 editor packages

02.10.2017
- added Fortnite support (currently requres game override)

29.09.2017
- added Unreal engine 4.17 support and initial UE4.18 support

29.07.2017
- implemented Lawbreakers support

28.07.2017
- added Gigantic auto detection

27.07.2017
- showing object's group name in viewer for UE1-UE3 games

12.06.2017
- added Tekken 7 support, game requires override -game=tekken7

09.06.2017
- added Friday the 13th: The Game support, game requires override -game=friday13

16.05.2017
- fixed loading of UE4 source animation assets

09.05.2017
- fixed bug in Win32 SDL2 caused incorrect handling of -path="some path" command line option

26.04.2017
- added Android ETC2 texture format support

25.04.2017
- fixes with mesh rotation (only affects mesh display in viewer)
- exporting skeletal mesh with "-uc" parameter will not also dump mesh socket information

09.04.2017
- updated UE4.16 support

08.04.2017
- Heavily optimized package scanner. Results: PARAGON scanned 30 times faster (reduced scan time from 2.5 min
  to 4 sec) and requires 35% less memory after package scan.

03.04.2017
- implemented loading of Blade & Soul specific animations

18.03.2017
- improved handling of unversioned UE4 packages: displaying a dialog box prompting for entering engine version

17.03.2017
- changed support for UE4 game tags, cleaned up list of UE4 engine versions from -help and -taglist options

08.03.2017
- added ASTC texture format support

01.03.2017
- updated SDL2 to 2.0.5

16.02.2017
- final UE4.15 support, initial 4.16 support
- added some fix to avoid crash when loading corrupted PARAGON animations

06.02.2017
- improved support for UE4 versioned packages

01.02.2017
- updated UE4.15 support

31.01.2017
- UE4 pak files: umodel skips encrypted files instead of throwing an error

30.01.2017
- added support for UE4 packages which were cooked for Event Driven Loader (UE4.14+): such packages
  has separated data into .uexp file with the same name as .uasset

10.01.2017
- initial UE4.15 support

09.01.2017
- fixed incorrect decoding of UE2 skeletal mesh when it has soft and rigid parts

27.12.2016
- Bioshock Remastered (1&2) partial support

02.12.2016
- added UE4.14 support

20.11.2016
- exporting all referenced textures from materials, not just recognized ones
- improved layout of UmodelExport directory when exporting UE4 assets

07.11.2016
- added simple wildcard capabilities to command line: now package names could contain "*" character

30.10.2016
- UI: added possibility to append selected packages to loaded package set

28.10.2016
- added UE4 animation support
- removed limit to 32k packages in a game, now number of packages is unlimited

19.10.2016
- added UE4.13 support

30.08.2016
- UI: added sorting of packages in package dialog

28.08.2016
- improved package dialog:
  - "filter" box now accepts multiple strings delimited with spaces
  - 100x times faster "flat view" (noticeable for UE4 games, with 20k+ packages, especially when typing
    text in "filter" box)

18.08.2016
- added UE4 sound export

28.06.2016
- improved "too many unknown files" error logic - this error will not appear for correct game paths anymore

05.06.2016
- implemented Devil's Third support

22.05.2016
- fixed loading of UE4 skeletal meshes with more than 4 bones per vertex: extra weights are dropped, and
  weights are re-normalized

15.05.2016
- added UE4.12 support
- added advanced option "-pkgver=..." to specify exact numeric version for package; useful when UE4 game
  has mid-release engine files
- game overriding for UE4 will now work only for unversioned packages; explicitly versioned packages will
  ignore this option

08.05.2016
- implemented loading of UE4 StaticMesh from editor packages

02.05.2016
- added Dungeon Defenders support (-game=dundef is required)

15.02.2016
- fixed crash with PS3 BulletStorm packages
- improved support for PS3 audio extraction

11.02.2016
- added SMITE encryption support; -game=smite is required

06.02.2016
- added XCOM 2 StaticMesh support (animation is not supported)

26.01.2016
- added UE4.11 support

21.12.2015
- reading all texture mipmaps from UE3 and UE4 packages - this speeds up content browsing a lot, especially
  for games which have huge textures

29.11.2015
- improved support for UE4 editor packages, added support for editor SkeletalMesh

29.09.2015
- implemented partial support for UE4 source textures (8-bit uncompressed textures)

26.09.2015
- implemented support for Blacklight: Retribution textures; note: for the moment game should be overrided to
  "Tribes: Ascend" (-game=t4)

18.08.2015
- improved Unreal engine 4 texture support

16.07.2015
- updated Lineage 2 animation support

11.07.2015
- added new command line syntax: "umodel <options> <directory>", shortcut to
  "umodel <options> -path=<directory>"

07.07.2015
- optimizations of memory use, especially for export operation

06.07.2015
- implemented full support for Batman: Arkham Knight

26.06.2015
- implemented Metro Conflict support; -game=metroconf option is required

20.06.2015
- implemented Mortal Kombat X SkeletalMesh and StaticMesh support

14.06.2015
- added support for up to 8 UV sets, required for UE4 static meshes

13.06.2015
- added "Save selected packages" option for "Tools" button in package dialog - this allows user
  to extract packages from pak and obb files

12.06.2015
- added support for compressed UE4 pak files
- implemented UE4.8 support

11.06.2015
- improved UE3 Android support

04.06.2015
- added Might & Magic Heroes 7 support

14.05.2015
- added full BC7 texture format support using "detex" library
- added support for Android ETC texture packages (DXT5 format didn't work)

24.04.2015
- implemented Mortal Kombat X textre support; note: all textures are in BC7 format, no extraction
  possible

20.04.2015
- added Mortal Kombat X package support

19.04.2015
- added UV display mode for skeletal and static meshes, activated with Ctrl+U
- improved UE4 SkeletalMesh LOD support

08.04.2015
- implemented support for destructible meshes: UE3 FracturedStaticMesh and UE4 DestructibleMesh

07.03.2015
- added Gigantic (alpha) support (-game=gigantic is required)

02.02.2015
- implemented Life is Strange support

20.01.2015
- fixed loading of bulk data from compressed UE4 packages

18.01.2015
- implemented UE4 SkeletalMesh support

12.01.2015
- implemented quick support for UE3 materials

11.01.2015
- fixed visual appearance of materials using BC5 textures for normal maps

10.01.2015
- added support for UE4 StaticMesh materials
- UE4 package imports are working now

09.01.2015
- implemented UE4 StaticMesh support

05.12.2014
- implemented full support for Guilty Gear Xrd (PS3 version)

23.11.2014
- implemented Unreal engine 4 PAK file support

22.11.2014
- improved Passion Leads Army SkeletalMesh compatibility

20.11.2014
- added new menu items duplicating functionality previously available through keyboard shortcuts

07.11.2014
- replaced "Scan ..." buttons in package dialog with single menu button "Tools"

03.11.2014
- added "Scan content" button in package selection dialog which will perform analysis of all game
  packages and display additional information in package list showing number of objects which are
  supported by umodel

31.10.2014
- added support for PVRTC and DXT Android textures

30.10.2014
- added support for loading Android OBB files; to open them, just specify game path containing
  the .obb file, and umodel will automatically scan its contents and allow working with embedded
  files

23.10.2014
- migrated to SDL2

13.10.2014
- added error message dialog which would appear if umodel crashes and any of umodel windows
  appeared before (so, it won't appear only in pure command line mode)

12.10.2014
- implemented support for UE4 textures

09.10.2014
- fixed bug: during batch export from GUI many objects were occasionally skipped from export
- Ctrl+X didn't use export directory option ("-out=...")
- file performance optimizations (reading packages and export)

08.10.2014
- implemented WWE All Stars support - everything except the animation (use -noanim option)
- package scan utility supports UE4 package format

06.10.2014
- significantly reduced memory footprint of loaded package
- closing package files when they're not needed; this allows umodel to perform batch export
  on large number of packages (previously there was an error "unable to open file")

30.09.2014
- initial implementation of menu for the main window

26.09.2014
- implemented The Vanishing of Ethan Carter support; requires overriding of game either with
  -game=vec command line option or with UI

23.09.2014
- added package version scanner to package selection dialog (UI analogue of "pkgtool")

13.09.2014
- updated Fable: Anniversary detection code

10.09.2014
- filling some DDS header fields when exporting DXT textures

08.09.2014
- UI: pressing Ctrl+A on package list will select all packages
- UI: displaying progress window while loading or exporting objects
- by default, umodel now exports all files to the directory {current_path}/UmodelExport

06.09.2014
- UI: added possibility to select multiple packages; these packages could be either loaded
  for viewing or exported in batch mode
- passing a package name in command line without -path=... option will not pop up a startup
  UI anymore

05.09.2014
- UI: added "flat" mode for package selection, with no directory tree, all packages are in
  single list
- UI: added package name filter

01.09.2014
- first public release of umodel with UI; to show the UI, launch umodel without arguments;
  to show package selection dialog at any time, press "O" key

31.08.2014
- improved -pkginfo output: displaying class statistics for loaded package(s)
- displaying -pkginfo when trying to load a package with no supported objects

28.07.2014
- implemented Seal of Vajara skeletal mesh support (-game=sov is required)

15.06.2014
- implemented Murdered: Soul Suspect support
- changed default texture appearance

01.06.2014
- added initial Unreal engine 4 package support

02.05.2014
- updated material support for recent Lineage 2 version
- implemented Tao Yuan texture support; "-game=taoyuan" option is required

02.04.2014
- fixed incompatibility with some Thief packages

24.03.2014
- implemented Thief static mesh support
- added support for viewing BC7 textures, when hardware supports it
- improved stability to OpenGL errors

07.02.2014
- improved UE2 SkeletalMesh compatibility
- much faster generation on UE2 mesh normals

05.02.2014
- updated Fable autodetection - supports Fable Anniversary

02.02.2014
- support for DXT3 and DXT5 textures from recent Unreal 1 patch

29.11.2013
- improved compatibility with UE2 animations

25.11.2013
- fixed issue with incompatibility of Bioshock compressed textures with some videocard drivers

25.10.2013
- implemented Batman: Arkham Origins support

12.10.2013
- implemented The Bureau: XCOM Declassified support

29.09.2013
- added "-nolightmap" option to prevent lightmap textures from being loaded and exported

31.08.2013
- implemented Lost Planet 3 support

11.08.2013
- added experimental compression method detection, so perhaps -lzo option is not needed anymore

14.06.2013
- implemented Remember Me support

30.05.2013
- implemented Injustice: Gods Among Us support (except animation)

04.05.2013
- implemented Bioshock Infinite support (all but animations)

21.02.2013
- added "-version" option to display brief umodel build information

20.02.2013
- implemented Gears of War: Judgment support; "-game=gowj" option is required

12.02.2013
- Aliens: Colonial Marines is fully supported

07.02.2013
- umodel will detect platform by the name of "Cooked<PlatformName>" directory when possible
- implemented support for Android's ETC1 compressed textures

06.02.2013
- implemented full support for America's Army 2 ("-game=aa2" option is required)

16.01.2013
- improved Splinter Cell 4 (Double Agent) support

15.01.2013
- umodel should no longer crash with error "Too much unknown files" when game path includes
  files extracted by umodel

29.12.2012
- implemented Passion Leads Army full support

25.12.2012
- fixed bug in UE2 SkeletalMesh verification code caused "bad LOD/base mesh" errors
- reduced amount of log output in a case of missing packages

18.12.2012
- implemented Storm Warriors Online support

14.12.2012
- Hawken (beta) is fully supported

21.11.2012
- implemented DmC: Devil May Cry (XBox360 demo) SkeletalMesh support

23.10.2012
- implemented Fable: The Journey support

22.10.2012
- implemented Vanguard: Saga of Heroes support; "-game=vang" option is required

16.10.2012
- fixed bug in LZX decompression code causing crashes in some cases

10.10.2012
- implemented support for Dishonored (except animations)

26.09.2012
- implemented Transformers sounds support
- FSB sound format is now recognized

20.09.2012
- implemented Borderlands 2 support

18.09.2012
- implemented Transformers: Fall of Cybertron support

06.09.2012
- fixed incompatibility of exported DDS with UE2's UnrealEd

17.08.2012
- improved compatibility with UE2 SkeletalMesh

03.07.2012
- implemented Tribes: Ascend texture support

25.06.2012
- improved compatibility with some UE2 games

04.06.2012
- improved Blade & Soul rendering

28.05.2012
- updated Special Force 2 (Tornado Force) support

27.05.2012
- implemented Tao Yuan (beta) support - everything but textures is supported

26.05.2012
- preventing umodel from opening non-package files from the command line (tfc, blk etc)

23.05.2012
- improved support for Mass Effect 3 animation (more animations are available now)

22.05.2012
- implemented Special Force 2 support

11.05.2012
- implemented Gunslayer Legend Texture2D support; "-game=gunsl" is required
- fixed some problems with non-English keyboards

27.04.2012
- updated Blade & Soul support for CBT3

16.04.2012
- fixed incompatibility with some UE2 games

22.03.2012
- updated support for the February 2012 UDK

21.03.2012
- fixed crash in StaticMesh with recent Lineage 2 update

20.03.2012
- Shift+Up/Down key could be used to change scene FOV

19.03.2012
- Ctrl+Q key will toggle visualization of debug information (text, 3D axis etc)
- Alt+S key will produce screenshot with transparent background

07.03.2012
- implemented Mass Effect 3 support

05.03.2012
- sharing vertices with the same position and normal when exporting psk file

02.03.2012
- implemented support for The Bourne Conspiracy animations

28.02.2012
- implemented support for Transformers: War for Cybertron and Transformers: Dark of the Moon
  animation

20.02.2012
- 'F' key will focus camera on SkeletalMesh (useful for animations with root motion)

17.02.2012
- implemented Batman: Arkham City animation support

13.02.2012
- added option "-obj=<object>" to specify any number of objects to load
- added option "-anim=<object>" to specify AnimSet which will be automatically attached to
  SkeletalMesh objects

07.02.2012
- eliminated -pskx option requirement - extra UV sets are stored in standard ActorX 2010 format

31.01.2012
- displaying real (cooked) texture size in the material viewer

30.01.2012
- umodel will try to find cooked resources in startup packages with non-standard name
  (not just "startup_int.xxx" etc) which are specified using "-pkg=..." option

25.01.2012
- implemented Brothers in Arms: Hell's Highway animation support

24.01.2012
- implemented Rise of the Argonauts and Thor: God of Thunder animation support

19.01.2012
- implemented Batman: Arkham City support (everything but animations)

11.01.2012
- updated support for recent Battle Territory Online version

10.01.2012
- command line arguments now may be specified in any order; before that, all options had to
  go before the package name; example: "umodel <package_name> -meshes" (did not work before)

14.12.2011
- implemented XBox360 XMA audio export

05.12.2011
- implemented XBox360 DDS texture export

01.12.2011
- implemented DDS texture export, activated with -dds option (used for DXT textures only)

29.11.2011
- fixed incompatibility with SkeletalMesh from recent UE3
- improved on-screen layout of the animation information

26.11.2011
- implemented support for Batman: Arkham City packages

25.11.2011
- exporter improvements:
  - no more crashes when umodel is unable to create output file due to invalid character etc
  - added option "-nooverwrite" to prevent existing files from being overwritten; this may
    be useful to speedup export process when the same object could be exported multiple times
    from different packages
  - umodel will save mesh with .psk extension when "-pskx" option is specified but there is
    no format extension required, and it will save as .pskx when it is not possible to store
    mesh as .psk, even when "-pskx" is not specified
  - tga: no more zero-length tga files, at least 1x1 correct tga image will be created in a
    case of error

23.11.2011
- implemented support for SkeletalMesh with more than 64k vertices

21.11.2011
- major rewritting of SkeletalMesh subsystem; implemented support for multiple UV sets,
  which can be switched in viewer by 'U' key and exported to psk when "-pskx" option is
  supplied

17.11.2011
- added option "-log=filename" to write whole umodel output to the specified file

13.11.2011
- fixed AnimRotationOnly for Mass Effect
- displaying animation translation mode on the screen
- controlling translation mode with 'Ctrl+R' key

09.11.2011
- multipart mesh support improvements:
  - highlighting current mesh in a viewer in multipart mesh rendering mode
  - taking into account bounds of all meshes of multipart mesh when positioning camera
  - Ctrl+X in a viewer will export all objects which are currently shown on the scene

08.11.2011
- animation system were rewritten
  - implemented support for UE3 rotation-only tracks
  - removed export into psax format - now everything is saved into psa format, additional
    attributes are stored in the text configuration file near the psa file
- improved positioning of the mesh in a viewer

06.11.2011
- major rewritting of the StaticMesh subsystem
  - implemented UE3 LOD support:
    - LODs are exported when "-lods" switch is passed to the command line
    - LODs can be switched in the viewer with 'L' key
  - using tangents from UE3 mesh instead of calculating them
  - implemented loading of all UV sets instead of only the first one, can switch them in the
    viewer with 'U' key
  - implemented export of all UV sets

21.10.2011
- implemented support for more than 64k indices (more than 22k triangles) for UE3 StaticMesh

25.09.2011
- added new exporter options "-uncook" and "-groups"

24.09.2011
- implemented APB: Reloaded skeletal mesh support

19.09.2011
- updated support for the August 2011 UDK

15.09.2011
- no more errors with old GLSL systems

13.08.2011
- implemented Battle Territory Online sound support

08.08.2011
- implemented APB: Reloaded package support

21.07.2011
- updated support for the June 2011 UDK

27.06.2011
- implemented Transformers: Dark of the Moon support

24.06.2011
- implemented Shadows of the Damned support

17.06.2011
- fixed Alice: Madness Returns compatibility issue

01.06.2011
- implemented Dungeons & Dragons: Daggerdale support; "-game=dnd" is required

31.05.2011
- implemented Hunted: The Demon's Forge support; "-game=hunt" is required

19.05.2011
- updated Nurien support

13.05.2011
- software skinning code were remade with use of SSE instructions, now works 4 times faster

07.05.2011
- implemented Medal of Honor: Airborne StaticMesh support

27.04.2011
- implemented Blade & Soul CBT1 support
- limited renderer FPS to reduce CPU usage

24.04.2011
- fixed bugs in recent UE3 SkeletalMesh code (GOW3 beta support fixed)

21.04.2011
- implemented Mortal Kombat (2011) support; animations are not supported, PS3 SkeletalMesh
  is not supported
- implemented Borderlands animation support

12.04.2011
- fixed crash in exporter when exported object has unicode name

08.04.2011
- win32 version has upgraded to use SDL 1.3 and got some improvements:
  - Alt+Enter will toggle fullscreen mode
  - minimized umodel will no more waste CPU time

06.04.2011
- added ScaleForm SwfMovie export (gfx files), activated with "-3rdparty"
- added FaceFXAsset and FaceFXAnimSet export (fxa files), activated with "-3rdparty"
- added "-notgacomp" option to disable exported TGA image compression (required for
  playback of extracted gfx files using ScaleForm FxMediaPlayer)
- implemented XIII SkeletalMesh and texture support

02.04.2011
- fixed problems with ATI OpenGL drivers

27.03.2011
- implemented looking for resources in "startup_xxx" package

26.03.2011
- added "-materials" switch to exclude textures from viewing

25.03.2011
- improved UE material emissive rendering
- implemented UE3 cubemap (TextureCube) support
- improved mesh positioning in a viewer
- changed viewport background color

17.03.2011
- implemented DC Universe Online TFC texture support
- implemented Homefront support (StaticMesh is not supported)

12.03.2011
- implemented Enslaved PS3 support

11.03.2011
- updated support for the March 2011 UDK

24.02.2011
- fixed compatibility with some Bulletstorm animations
- animation exporter will warn user about requirement of the psax format when needed

09.02.2011
- implemented UE1 and UE2 sound support

06.02.2011
- implemented Singularity StaticMesh support
- implemented UE3 audio (SoundNodeWave) export, activated with "-sounds" option

05.02.2011
- added switches to override compression method of the fully compressed packages:
  -lzo, -zlib, -lzx
- implemented Undertow support; requires "-game=undertow -lzo" switches

02.02.2011
- fixed Mirror's Edge compatibility issue

29.01.2011
- added U8V8 and BC5 texture format support
- improved rendering of some UE3 materials

28.01.2011
- updated support for the latest UE3 SkeletalMesh

26.01.2011
- implemented Bulletstorm support

10.01.2011
- implemented support for The Bourne Conspiracy (everything but animations)

09.01.2011
- updated Battle Territory Online support

08.01.2011
- implemented EndWar package, texture, skeletal and static mesh support (requires
  "-game=endwar")

30.12.2010
- updated TERA: The Exiled Realm of Arborea autodetection

29.12.2010
- implemented Fury support

27.12.2010
- implemented DC Universe Online support

21.12.2010
- Splinter Cell 4 (Double Agent) SkeletalMesh support is implemented (requires "-game=scell")
- implemented support for Undying textures (requires "-game=undying")

16.12.2010
- implemented support for LightMapTexture2D objects
- command line "umodel <package> <object>" will now find all objects with the name
  <object> (not only the first one)
- implemented support for Splinter Cell 3 and 4 packages (requires "-game=scell")

14.12.2010
- implemented support for Turok StaticMesh and animation

13.12.2010
- optimized PVRTC decompression code - works 3.5 times faster
- implemented UE3/iOS material specularity

12.12.2010
- implemented iOS (iPhone/iPad) texture support, activated with "-ios" option

09.12.2010
- updated support for recent versions of the XBox360 UE3
- updated TERA: The Exiled Realm of Arborea support
- "-noxbox" option has been replaced with "-ps3" switch

29.11.2010
- implemented support for the recent UE3 animations (September 2010+ UDK)
- implemented support for the Destroy All Humans! Path of the Furon

26.11.2010
- updated UDK support for November 2010 version

16.11.2010
- implemented Berkanix support

07.11.2010
- improved multipart mesh display: Ctrl+T will tag/untag mesh, supports animations for
  all parts
- added "-pkg=<package>" option to load extra package (may be useful when animation is
  placed separately from mesh)

24.10.2010
- updated Land of Chaos Online (LOCO) support

22.10.2010
- implemented support for the recent UE3 materials

15.10.2010
- fixed bug reading UE3 SkeletalMesh with multiple UV sets

11.10.2010
- implemented Medal of Honor 2010 support

06.10.2010
- added "-out=directory" option to specify place where to export data (otherwise export
  will be made into the current directory)

04.10.2010
- implemented Enslaved: Odyssey to the West StaticMesh support

03.10.2010
- improved XBox360 Bioshock support:
  - disabled parsing of Havok data for XBox360 version of Bioshock (prevents umodel
    from crash)
  - implemented support for XBox360 version of Bioshock textures

01.10.2010
- implemented Enslaved: Odyssey to the West SkeletalMesh support

30.09.2010
- updated Mortal Online support

16.08.2010
- suppressed useless message "WARNING: Export object ...: unsupported type ..."

30.07.2010
- updated UDK support for July 2010 version

27.07.2010
- implemented game autodetection override with "-game=tag" switch, list of possible
  game tags can be obtained with "-taglist" option
- implemented SWAT 4 support (requires "-game=swat4" option)


23.07.2010
- implemented pskx/psax mesh and animation export - activated by "-pskx" command
  line switch; pskx and psax formats are supported by ActorX Importer 1.10 and newer

14.07.2010
- skeletal mesh LODs will have reduced skeleton (removing unused bones)

02.07.2010
- implemented Transformers: War for Cybertron SkeletalMesh support

30.06.2010
- implemented Transformers: War for Cybertron StaticMesh support

25.06.2010
- implemented Transformers: War for Cybertron package and texture support

24.06.2010
- implemented Unreal Championship 1 package and animation support

19.06.2010
- implemented APB SkeletalMesh and StaticMesh support

29.05.2010
- implemented APB package and Texture2D support

28.05.2010
- implemented Alpha Protocol support

05.05.2010
- implemented support for TERA: The Exiled Realm of Arborea

03.05.2010
- updated UDK support for April 2010 version

24.04.2010
- updated Army of Two: the 40th Day detection code

17.04.2010
- updated UDK support up to March 2010 version

14.04.2010
- added "-noxbox" switch which can be used to disable XBox 360 texture decryption
  (may be useful to load ps3 packages)
- implemented support for UE3 animation compression "method #5"

12.04.2010
- implemented AVA Online StaticMesh support

03.04.2010
- implemented Battle Territory Online support

23.03.2010
- implemented Star Wars: Republic Commando animation support

11.03.2010
- implemented attachment socket support for UE3 SkeletalMesh
- umodel usage page: changed appearance of list of supported game titles
- fixed annoying "WARNING: Unknown class 'Package' for object ..."

10.03.2010
- added new SkeletalMesh exporter option "-lods" - allows to export lower mesh LODS
  as well as basic mesh

09.03.2010
- implemented support for Rainbow 6: Vegas 2 packages
- implemented support for oldest UE3 Texture2D, SkeletalMesh and StaticMesh formats

08.03.2010
- implemented Star Wars: Republic Commando SkeletalMesh and StaticMesh support

04.03.2010
- some fixes in Bioshock-specific code

23.02.2010
- implemented Legendary: Pandora's Box SkeletalMesh support

21.02.2010
- implemented support for loading external Unreal Championship 2 animations

18.02.2010
- implemented Bioshock 2 SkeletalMesh and StaticMesh support

15.02.2010
- implemented Bioshock 2 package support

08.02.2010
- added Mass Effect (1) for XBox 360 support (updated autodetection code)

07.02.2010
- implemented support for Unreal Championship 2 animations

29.01.2010
- implemented Huxley StaticMesh support

24.01.2010
- implemented TNA iMPACT! package support

23.01.2010
- implemented Mass Effect 2 support

20.01.2010
- implemented Dark Void support (except StaticMesh)

11.01.2010
- implemented Army of Two: the 40th Day package and SkeletalMesh support

05.01.2010
- remade lighting for StaticMesh objects with normalmap support
- using better normals for UE3 SkeletalMesh

30.12.2009
- implemented StaticMesh support for the following games:
  - Mortal Kombat vs. DC Universe
  - BlackSite: Area 51
  - Borderlands

29.12.2009
- implemented StaticMesh support for the following games:
  - Batman: Arkham Asylum
  - Mass Effect
  - The Last Remnant

27.12.2009
- implemented support for UE3 StaticMesh (versions from GoW1_XBox360 to UDK)

15.12.2009
- implemented Land of Chaos Online (LOCO) support

11.12.2009
- updated Borderlands support for DLC

06.12.2009
- implemented bloom rendering effect

23.11.2009
- implemented Frontlines: Fuel of War support

21.11.2009
- implemented Wheelman package support

20.11.2009
- implemented Star Wars: Republic Commando package support
- implemented AVA Online package and SkeletalMesh support

17.11.2009
- implemented 50 Cent: Blood on the Sand support

12.11.2009
- implemented Magna Carta 2 support

06.11.2009
- implemented UDK support

03.11.2009
- implemented Borderlands Texture2D and SkeletalMesh support

28.10.2009
- filtering output spam from ATI GLSL compiler
- UE2 renderer: improved complex material support (Shader, Combiner)

27.10.2009
- completely rewritten UE2 renderer:
  - support normalmaps for Tribes: Vengeance and Bioshock meshes
  - support specular mask
  - export UE2 materials

23.10.2009
- implemented Tribes: Vengeance, Bioshock and Mass Effect compressed normalmap support
  (DXT5n and 3Dc/ATI2 texture compressions)
- Ctrl+S will take screenshot into file Screenshots/ObjectName.tga

21.10.2009
- fixed translucent surface drawing (not erased by opaque surfaces)
- skeletal mesh LOD model drawing now have full features (show normals, influences
  etc)

17.10.2009
- renderer: implemented support for material light emission
- implemented UE3 material export (*.mat file, custom format)

16.10.2009
- renderer: implemented specular and opacity map support

15.10.2009
- integrated GLSL Validator to check compatibility with GLSL standard
- improved compatibility with 3DLabs (and ATI) GLSL

13.10.2009
- renderer: implemented normalmap (bumpmap) support for UE3 materials

11.10.2009
- remade skeletal mesh drawing; now LODs supports lighting

07.10.2009
- implemented OpenGL 2.0 shader support; can be disabled/enabled on-fly with
  'Ctrl+G' key (switch to fixed pipeline and back)

29.09.2009
- implemented StaticMesh export (psk format)

17.09.2009
- implemented Stranglehold SkeletalMesh support

15.09.2009
- implemented Stranglehold package support

14.09.2009
- implemented Crime Craft SkeletalMesh support

11.09.2009
- implemented Batman: Arkham Asylum animation support

10.09.2009
- implemented Batman: Arkham Asylum SkeletalMesh support
- added code to avoid "... unread bytes" when loading Texture2D from unknown UE3 games
  (now Damnation is fully supported)

03.09.2009
- implemented full support for UE3 forced exports (loading from another packages)

01.09.2009
- implemented new UE3 SkeletalMesh and animation support (tested with Mortal Online)

27.08.2009
- implemented Mass Effect animation support

26.08.2009
- implemented Mortal Online package support
- implemented Mirror's Edge animation support

03.08.2009
- implemented X-Men Origins: Wolverine animations support

27.07.2009
- implemented Nurien support

30.06.2009
- Bioshock: implemented support for FacingShader class and hi-res textures

29.06.2009
- Bioshock: implemented SkeletalMesh support

26.06.2009
- Bioshock: implemented parsing of Havok data structures

10.06.2009
- Bioshock: implemented quick support for materials

09.06.2009
- Bioshock: implemented static mesh support

08.06.2009
- Tribes: Vengeance: implemented static mesh support

06.06.2009
- Bioshock: implemented package support

29.05.2009
- improved UE3 material rendering (better diffuse texture detection)
- implemented Army of Two SkeletalMesh support

28.05.2009
- creating subdirectories when exporting data from packages

26.05.2009
- implemented support for loading UE3 fully compressed packages (mostly used for *.u)

25.05.2009
- exporter improvements
  - correct exporting of skeletal meshes with unregistered materials (both psk and md5mesh)
  - correct handling of duplicates of skeletal mesh bone names (md5mesh only)
  - when exporting 2 objects with the same name index suffix (_2, _3 etc) will be appended
    to the filename

22.05.2009
- implemented support for Mortal Kombat vs. DC Universe (package, texture, SkeletalMesh)

20.05.2009
- implemented support for old UE3 property formats

18.05.2009
- implemented support for BlackSite: Area 51 SkeletalMesh

15.05.2009
- implemented support for Zlib-compressed UE3 packages

14.05.2009
- implemented support for Blacksite: Area 51 packages

08.05.2009
- implemented support for Huxley

07.05.2009
- better game root autodetection when "-path" is not specified

06.05.2009
- implemented StaticMesh support for UT2003/2004 and UE2Runtime
- added "-nostat" command line switch to disable StaticMesh loading

29.04.2009
- implemented support for some kind of unknown UE2 SkeletalMesh-es
  (supported Rainbow 6: Raven Shield)

27.04.2009
- implemented support for Mass Effect SkeletalMesh

17.04.2009
- implemented support for The Last Remnant animations

09.04.2009
- improved stability of loading bad UE3 animation tracks (GOW1 Geist_Reaver)

08.04.2009
- implemented bone influence visualization for SkeletalMesh (activated with key 'I')

07.04.2009
- fixed texture mapping for UE3 SkeletalMesh LODs

24.03.2009
- implemented export of skeletal mesh and animation to md5mesh/md5anim format
  (activated with "-md5" command line switch)
- psk export: when possible, real material names are used instead of "material_N"

19.03.2009
- faster (almost 2x) UE3 mesh loading
- added simple path autodetection from full package filename

18.03.2009
- completely rewritten "-path" handling - now works with UE3 games too
- significantly faster (10 times) UE2 package loading

17.03.2009
- added "-meshes" command line switch to disable material viewer (object browser
  will iterate meshes only)
- added "-uc" command line switch to allow creating scripts for exported meshes
  (was created unconditionally before)
- improved initial positioning of UE3 mesh in viewer

16.03.2009
- implemented support for UE3 texture cache (textures.tfc) - for XBox360 games
- improved GOW2 animation support

13.03.2009
- implemented support for GOW2 animations

12.03.2009
- implemented support for animations, compressed with "remove every second key"
  algorithm

11.03.2009
- implemented support for loading compressed XBox360 packages

05.03.2009
- restoring UE3 mesh from GPU skin when needed
- added "-notex" command line option to prevent Texture/Texture2D class loading in a
  case of unsupported data format

26.02.2009
- 'Ctrl+A' key will switch animations sets for current skeletal mesh
- implemented support for loading XBox360 packages

25.02.2009
- implemented support for UE3 AnimSet (all compresion algorithms are supported except
  ACF_Float32NoW)

23.02.2009
- implemented support for UE3 SkeletalMesh lods
- fixed duplicate weights for some UE3 SkeletalMesh vertices
- fixed zooming with right mouse button

19.02.2009
- implemented quick support for UE3 Material and MaterialInstanceConstant texturing
  (diffuse only)
- significantly improved mesh rendering speed

17.02.2009
- writting uc-script when exporting SkeletalMesh

16.02.2009
- implemented Mirror's Edge SkeletalMesh support

11.02.2009
- implemented UE3 SkeletalMesh support
- releasing OpenGL texture memory when closing material viewer (smaller memory usage)

09.02.2009
- fixed bug with Splinter Cell animation loading (code was conflicted with
  UE2Runtime animations)
- fixed loading UE3 textures with missing first mipmap levels
- implemented support for TEXF_L8 (UE2) and PF_G8 (UE3) greyscale texture formats

03.02.2009
- added "-nomesh" command line option to prevent SkeletalMesh class loading in
  a case of unsupported data format
- implemented UE3 UTexture2D class support
- fixed bug in UE3 package decompression code

23.01.2009
- implemented loading of texture mips from xpr files (for Unreal Championship 2)

18.01.2009
- implemented support for Unreal Championship 2 skeletal models

17.01.2009
- implemented support for loading Unreal Championship 2 packages

12.01.2009
- implemented support for loading UE3 packages (supported UT3 and Gears of War)

25.12.2008
- implemented Exteel support

24.12.2008
- updated Lineage2 material support for version 123/37

16.12.2008
- added "-all" command line option - modifier for exporting logic

09.12.2008
- workaround for strange ATI bug with recent Catalyst drivers: texturing were
  disappeared, background color becomes black and everything colorized red; bug
  description is here: http://bugzilla.icculus.org/show_bug.cgi?id=3526

01.12.2008
- implemented (temporary) support for different lighting modes: specular, diffuse
  and ambient only; switched by Ctrl+L

26.11.2008
- implemented attachment socket visualization for skeletal mesh (key 'A')

25.11.2008
- additional fix for Lineage LOD models

23.11.2008
- fixed skeletal LOD model visualisation
- implemented skinning for LOD models
- Lineage2: implemented support for Lineage-specific LOD models
- Lineage2: restoring base skeletal mesh from 1st LOD when needed

21.11.2008
- fixed RGBA8 texture format loading (swapping red and blue channels)
- fixed PSK format export: material assignment was lost when importing mesh into
  UnrealEd
- using bottom-top orientation when exporting TGA file, because UnrealEd prior to
  UE3 have no top-bottom TGA support (texture is flipped vertically when imported)

14.11.2008
- implemented support for UE2Runtime mesh animation format

13.11.2008
- detecting UT2 (UT2003/2004) packages, separated UT2 support and generic UE2 support

10.11.2008
- updated Lineage 2 support (for latest Lineage 2 Gracia)

09.11.2008
- implemented support for unicode strings (sometimes used in Lineage 2 packages)

07.11.2008
- implemented serializers for most UE2 materials
- long texts on screen may be scrolled now with Ctrl+PgUp/Ctrl+PgDn keys

06.11.2008
- implemented material outline viewer (key 'M')

05.11.2008
- implemented quick support for Shader material (does works in most cases)

02.11.2008
- implemented Lineage 2 texture and MeshAnimation support

01.11.2008
- implemented Lineage 2 package and SkeletalMesh support

30.10.2008
- added "-noanim" command line option to prevent MeshAnimation (UE2) / Animation
  (UE1) class loading in a case of unsupported data format

23.10.2008
- implemented Harry Potter and the Prisoner of Azkaban support

22.10.2008
- implemented Tribes: Vengeance support

09.10.2008
- implemented support for Unreal engine 1 UMesh class
- improved UE1 LodMesh support

08.10.2008
- linux: workaround for SDL bug caused undesired mesh rotation when pressing left
  mouse button

06.10.2008
- implemented Rune mesh support

01.10.2008
- implemented VertMesh export into Unreal's .3d format
- fixed wrong orientation of VertMesh in viewer
- "dump" command now displays object properties

30.09.2008
- displaying of texture information in material viewer

28.09.2008
- implemented DeusEx VertMesh support

23.06.2008
- fixed error in compressed TGA export, caused format incompatibility with some
  image viewer/editor software

15.06.2008
- skeleton dump has been moved to 'Ctrl+B' key (previously was automatically
  made on mesh selection)
- fixed crash when handling broken object imports (import object from package,
  which does not hold it)
- implemented UT1 SkeletalMesh support

11.06.2008
- added Unreal1 and UT VertMesh support

06.06.2008
- added Splinter Cell 2 support
- fixed "default" material loss when window is resized

03.06.2008
- added Splinter Cell support

14.05.2008
- added texture export

26.03.2008
- exporting whole package with a single command

20.03.2008
- advanced usage information

18.03.2008
- added support for Unreal1 paletted textures
