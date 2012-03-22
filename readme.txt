UMODEL (UE Viewer)
(c) Konstantin Nosov (Gildor), 2007-2012


Please support the development by making a donation here:
http://www.gildor.org/en/donate


System requirements
~~~~~~~~~~~~~~~~~~~
Windows or Linux operating system
x86-compatible CPU with SSE support
OpenGL 1.1 videocard (OpenGL 2.0 is recommended)
SDL 1.2 (for Linux only, windows distribution has included sdl.dll)


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

Other documentation:
http://www.gildor.org/smf/index.php/board,9.0.html


Some usage information
~~~~~~~~~~~~~~~~~~~~~~
This is a console application, there is no GUI.
For the list of command line options run 'umodel' without arguments.
Note: if you will launch program from Windows explorer etc, you will get a console window
with a help message, but this window will immediately disappear.

Keyboard:
H                show/hide full keyboard help
Esc              exit from the umodel
PgUp/PgDn        browse through loaded objects
Ctrl+S           take a screenshot into the file Screenshots/ObjectName.tga
Alt+S            take screenshot with transparent background
Ctrl+X           export all objects from the current scene
Ctrl+PgUp/PgDn   scroll onscreen texts
Ctrl+L           switch lighting modes
Shift+Up/Down    change scene FOV
Ctrl+G           toggle OpenGL 2.0 / OpenGL 1.1 renderer

To see full shortcut list press 'H' key.


Notes about psk/psa export
~~~~~~~~~~~~~~~~~~~~~~~~~~
To load psk or psa into 3ds Max you'll need this script (ActorX Importer) created by me:
http://www.gildor.org/projects/unactorx
Its announcements thread is here:
http://www.gildor.org/smf/index.php/topic,228.0.html

Some meshes contains information which cannot fit into psk standard. In this case I've
extended the format to hold advanced information. Files in this format has extension pskx
and cannot be loaded into UnrealEd or other application with ActorX support. There only
one tool with pskx support at the moment - ActorX Importer for 3ds Max which was mentioned
above.


Notes about md5mesh/md5anim export
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To use these exporters you should add "-md5" option to the command line options.
MeshAnimation and AnimSet objects are exported as multiple md5anim files (one file for
each animation track). "bounds" section in md5anim is filled with dummy data. "hierarchy"
section also does not contain real skeleton hierarchy, because Unreal engine uses
hierarchy from the mesh, not from animations. Some md5 viewers/importers does require
md5anim hierarchy, some - does not.

There is a 3ds Max md5mesh/md5anim importer script available on umodel forum:
http://www.gildor.org/smf/index.php?topic=87.0
or here
http://www.gildor.org/downloads
This script was originally created by der_ton, but was updated by me.


Notes about StaticMesh support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StaticMesh export is performed into psk format. This format was originally designed for
SkeletalMesh export, but umodel uses it for StaticMesh too. Exported mesh will not have
skeleton and vertex influences. Resulting psk files cannot be imported directly into the
UnrealEd, so I've decided to save ot with pskx extension to avoid silly user errors.


Notes about material export
~~~~~~~~~~~~~~~~~~~~~~~~~~~
Materials are exported in a custom format. File extension is ".mat". At the current moment,
this format is supported by my ActorX Importer plugin only. Unreal engine materials are
very complex, it's hard to separate a few channels (diffuse, specular, bump etc) from it.
Umodel tries to do this by using some heuristics. Umodel will never export full materials
(GLSL script etc). Do not expect too much from this.


Used third-party libraries
~~~~~~~~~~~~~~~~~~~~~~~~~~
zlib
  (c) Jean-loup Gailly and Mark Adler
  http://zlib.net/

lzo
  (c) Markus F.X.J. Oberhumer
  http://www.oberhumer.com/opensource/lzo/

libmspack
  (c) Stuart Caie
  http://www.cabextract.org.uk/libmspack/

NVIDIA Texture Tools
  (c) NVIDIA
  http://code.google.com/p/nvidia-texture-tools/

PVRTexLib Library
  (c) Imagination Technologies Limited
  http://www.imgtec.com/powervr/insider/


Changes
~~~~~~~
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
  SkeletalMesh

07.02.2012
- eliminated -pskx option requirement - extra UV sets are stored in standard ActorX 2010 format

31.01.2012
- displaying real (cooked) texture size in material viewer

30.01.2012
- umodel will try to find cooked resources in startup packages with non-standard name
  (not "startup_int.xxx" etc) which are specified using "-pkg=..." option

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
  go before the package name; example: "umodel <package_name> -meshes" (did not worked
  before)

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
  - no more crashes when umodel is unable to create output file due to invalid characters etc
  - added option "-nooverwrite" to prevent existing files from being overwritten; this may
    be useful to speedup export process when the same object could be exported from different
    packages
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
- highlighting current mesh in a viewer in multipart mesh rendering mode
- taking into account bounds of all meshes of multipart mesh when positioning camera
- Ctrl+X in a viewer will export all objects which are currently shown on the scene

08.11.2011
- animation system were rewritten
  - implemented support for UE3 rotation-only tracks
  - removed export into psax format - now everything is saved into psa format, additional
    atttributes are stored in the text configuration file near the psa file
- improved positioning of the mesh in viewer

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
- software skinning code were remade using SSE instructions, works 4 times faster now

07.05.2011
- implemented Medal of Honor: Airborne StaticMesh support

27.04.2011
- implemented Blade & Soul beta support
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
- implemented support for Turok animations and StaticMesh

13.12.2010
- optimized PVRTC decompression code - works 3.5 times faster
- implemented UE3/iOS material specularity

12.12.2010
- implemented iOS (iPhone/iPad) texture support, activated with "-ios" switch

09.12.2010
- updated support for recent versions of the XBox360 UE3
- updated TERA: The Exiled Realm of Arborea support
- "-noxbox" has been replaced with "-ps3" switch

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
  - disabled Havok parsing for XBox360 version of Bioshock (prevent umodel from crash)
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
- implemented SWAT 4 support (use "-game=swat4")


23.07.2010
- implemented pskx/psax mesh and animation export - activated by "-pskx" command
  line switch; pskx and psax formats are supported by ActorX Importer 1.10 and
  higher

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
  - correct exporting skeletal meshes with unregistered materials (both psk and md5mesh)
  - correct handling of duplicate skeletal mesh bone names (md5mesh only)
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
