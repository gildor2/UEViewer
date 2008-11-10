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
  Harry Potter and the Prisoner of Azkaban
Modified Unreal Engine 2:
  Splinter Cell 1,2
  Tribes: Vengeance
  Lineage 2

List of games with limited support:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Using "-noanim" option (unsupported animation format):
  Harry Potter (UE1)
  Devastation

Project home page and forum:
http://www.gildor.org/projects/umodel


For command line information run 'umodel' without arguments.
Keyboard: press <H> for help, <ESC> for exit.


Changes:
~~~~~~~~
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
