UMODEL
Unreal Model Viewer

List of supported games:
~~~~~~~~~~~~~~~~~~~~~~~~
Unreal Engine 1:
  Unreal 1, Unreal Tournament 1
  The Wheel of Time
Modified Unreal Engine 1:
  DeusEx, Rune
Unreal Engine 2:
  Unreal Tournament 2003/2004
  Postal 2
Modified Unreal Engine 2:
  Splinter Cell 1,2
  Rainbow 6: Raven Shield
  Lineage 2 Gracia
  Land of Chaos Online (LOCO)
Unreal Engine 2.5:
  UE2Runtime
  Harry Potter and the Prisoner of Azkaban
Modified Unreal Engine 2.5:
  Tribes: Vengeance
  Bioshock, Bioshock 2
  Exteel
Unreal Engine 2X:
  Unreal Championship 2: The Liandri Conflict
Unreal Engine 3:
  Unreal Tournament 3
  Gears of War
  Gears of War 2 (XBox360)
  UDK
  Mortal Online
  Section 8
Modified Unreal Engine 3:
  The Last Remnant
  Mass Effect
  BlackSite: Area 51
  Mortal Kombat vs. DC Universe (XBox360)
  TNA iMPACT! (XBox360)
  Army of Two (XBox360)
  Mirror's Edge
  Huxley
  Rise of the Argonauts
  X-Men Origins: Wolverine
  Magna Carta 2 (XBox360)
  Damnation
  Batman: Arkham Asylum
  Crime Craft
  AVA Online
  Frontlines: Fuel of War
  50 Cent: Blood on the Sand (XBox360)
  Nurien
  Stranglehold
  Borderlands
  Dark Void

List of games with limited support:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Using "-noanim" option (unsupported animation format):
  Harry Potter (UE1)
  Devastation
  BlackSite: Area 51 (uses Havok animation system)
  Mortal Kombat vs. DC Universe
  Army of Two
  Stranglehold
  Borderlands
Unsupported animations (without umodel crash):
  Bioshock, Bioshock 2 (uses Havok animation system)
  Rise of the Argonauts


System requirements:
~~~~~~~~~~~~~~~~~~~~
Windows or Linux operating system
OpenGL 1.1 videocard (OpenGL 2.0 is recommended)
SDL 1.2 (for Linux only, windows distribution included sdl.dll)


Additional information:
~~~~~~~~~~~~~~~~~~~~~~~
Project author:
Konstantin Nosov (aka Gildor)

Project home page, usage documentation and forum:
http://www.gildor.org/en/projects/umodel
or Russian page:
http://www.gildor.org/projects/umodel

Detailed title compatibility list (by Esr911) can be located here:
http://www.gildor.org/projects/umodel/compat

Please support project by making a donation here:
http://www.gildor.org/en/donate


Some usage information:
~~~~~~~~~~~~~~~~~~~~~~~
This is a console application, there is no GUI.
For the list of command line options run 'umodel' without arguments.
Note: if you will launch program from Windows explorer etc, you will get a console
window with a help message, but this windoww will immediately disappear.

Keyboard:
H                full keyboard help
ESC              exit umodel
PgUp/UpDn        browse loaded objects
Ctrl+S           take screenshot
Ctrl+PgUp/PgDn   scroll onscreen texts
Ctrl+L           switch lighting modes
Ctrl+G           toggle OpenGL 2.0 / OpenGL 1.1 renderer


Notes about psk/psa export.
~~~~~~~~~~~~~~~~~~~~~~~~~~~
There is a 3ds Max importer script (Actor X Importer) created by me:
http://www.gildor.org/projects/unactorx
Its announcements thread is here:
http://www.gildor.org/smf/index.php/topic,228.0.html


Notes about md5mesh/md5anim export.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To use these exporters you should add "-md5" key to a command line options.
MeshAnimation/AnimSet objects are exported as multiple md5anim files (one
file per animation track). "bounds" section in md5anim is filled with dummy
data. Also, "hierarchy" section does not contain real skeleton hierarchy,
because Unreal Engine uses hierarchy from mesh, not from animations. Some md5
viewers/importers does require md5anim hierarchy, some - does not.

There is a 3ds Max md5mesh/md5anim importer script available on umodel forum:
http://www.gildor.org/smf/index.php?topic=87.0
This script was originally created by der_ton, but was updated by me.


Notes about StaticMesh support.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StaticMesh export made into psk format. This format is designed for SkeletalMesh
export, but umodel uses it for StaticMesh too. Exported mesh will not have
skeleton and contains only one UV set.
Resulting psk files are cannot be imported directly into UnrealEd.


Notes about material export.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Materials are exported in a custom format. File extension is ".mat". At the
current moment, this format is supported by my Actor X Importer plugin only.
Unreal Engine materials are very complex, it's hard to separate a few channels
(diffuse, specular, bump etc) from it. Umodel tries to do this using some
heuristics. Umodel will never export full materials (GLSL script etc). Do not
expect much from this.


Changes:
~~~~~~~~
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
  Ctrl+G key (switch to fixed pipeline and back)

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
- implemented bone influence visualization for SkeletalMesh (activated with key I)

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
- Ctrl+A key will switch animations sets for current skeletal mesh
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
- implemented attachment socket visualization for skeletal mesh (key A)

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
- implemented support for Unreal Engine 1 UMesh class
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
- skeleton dump has been moved to Ctrl+B key (previously was automatically
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
