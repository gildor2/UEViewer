- [General](#General)
- [Help with various problems](#Help-with-various-problems)
- [Starting UModel](#Starting-UModel)
- [Viewer questions](#Viewer-Questions)
- [Texturing questions](#Texturing-questions)
- [Unreal engine 4 specific questions](#Unreal-Engine-4)
- [Exporter and PSK/PSA questions](#Exporter)

## General {#General}

---

### How can I check for umodel updates?
RSS Feeds:
- There is RSS feed available on the main site - check icon on the upper right corner. However the main page is rarely updated, only very major news are posted there.
- Forum RSS feed is also available, you'll get a notification about every new message there.
- The most detailed (and the most technical) feed is at [Github page of UModel](https://github.com/gildor2/UModel/commits/master.atom). You'll have information about every commit made to UModel's repository.

Other options are to track web pages.
- If you have a facebook account, you may subscribe to the [facebook UModel news](http://www.facebook.com/ue.viewer). I am trying to keep it up to date. You can also find a link to the facebook page on the left column of this page, below the site menu.
- Check [online version of readme file](https://www.gildor.org/projects/umodel/readme). It contains non-technical changelog.

### Which game titles are supported?
There are more than 300 games made with all Unreal engine versions, for all platform supported by Epic Games. Check this table: https://www.gildor.org/projects/umodel/compat

I'm trying to keep it up-to-date, mostly with reports posted on my forum. Note presence of the filter on the top of the table which lets you to find a game by the part of its name or by the company name quickly.

### Why don't you add support for non-Unreal engine games?
Other game engines has different principles of data storage. Umodel is focused on Unreal engine games and most of it's code works with Unreal engine data. Another reasons (that's why I did not created separate application for other games): other game engines are usually has more simple data formats, there are a lot of extractors for them exists already, even made with non-professional programmers. So I don't think there is any reason to support other engines.

### What's the reason of starting your development of UE Viewer?
I had a task on my job to write a skeletal animation system for upcoming game. I checked a lot of open-source systems, read some articles. But I was not satisfied with these. So I started research animation system from Unreal Tournament 2004 to learn how to do that.

_If you're really interested, more information is available on my "About me" page, check the link on site's menu._

### How can I help you?
The best help is [a donation](https://www.gildor.org/en/donate).
Also you may vote for the umodel (press "LIKE") on [its Facebook page](https://www.facebook.com/ue.viewer).

### I have made some project/video with a help of umodel. Can I add you/your program to the credits?
Yes of course

### Can I post your tool on my site/forum?
I see no reason to do so, my site is alive. You should not copy texts from this site, post a link instead. Do not post direct links to files - these links may be (and _will be_) changed in a future.

### Where could I find the source code?
The source code is available [at GitHub](https://github.com/gildor2/UModel). The discussion is open [on the forum](https://www.gildor.org/smf/index.php/topic,2392.0.html).

### Where could I get older version of UModel?
In general, it is not needed to get an older version of UModel. Newer versions should have support for all games which were supported before, and if something is wrong in newer UModel - this is rather a bug and should be fixed. However, with some frequently updated UE4 games, it might be useful to revert to older UModel, for example if game uses some "intermediate" UE4 version. This is mostly happens with "early access" or online games. So, all UModel executables [could be found on GitHub here](https://github.com/gildor2/UModel/commits/master/umodel.exe).

A brief instruction how to download the file from history - as Github changed user interface in non-obvious way (it was easier before). So. Open the provided link. You'll see a list of different file versions. Select one, press at [<>] button at that line. Github will open the whole repository for _that version_. Now, select the file umodel.exe again. Here you'll see "Download" button.

### What are you going to do next?
I've put my ["TODO"/roadmap](https://trello.com/b/lp1XjYaz/ue-viewer) on Trello service, it's publicly available.

### What should I provide to you for support for some new game?
First of all, I should mention that **my current job keeps me extremely busy**, and sometimes I can't work on UModel at all, with exceptions for small stability issue fixes. Also, supporting new UE2 or UE3 games has lowest priority for me than extending UE4 support and making a better UI. Other things which I'll most likely not consider doing:
- Supporting Asian games. I have absolutely nothing against "Asians" at all, however China and Korea making hundreds of games which are looking very close to each other, doing updates to those games very often (breaking UModel's compatibility), and almost always protecting content and executable with encryption, what makes my work impossible, or in the best case - very hard.
- Online games (not just "Asian" ones) - these have frequent updates, often have protected executable files (not possible to analyze), often have encryption on packages.
- Unreal engine 4 games with customization. Unless the game is "absolutely must to support" (like Gears of Wars 4), I prefer to not add custom code into UModel for UE4 games. This is just because of difficulty to maintain that code in a case of game will get engine update.

Ok. The following info is a little bit ourdated - I wrote it for UE2 and UE3 games, and it is not suitable for UE4. For making a new game supported, I'll need a few files:

1. The game executable.
   - Unreal engine 3. The game is usually placed in the single executable file. Note: I need non-encrypted file! For PC it should contain a few "key" strings in ANSI and Unicode - one of them is a "USkeletalMesh". If this string is not present in the file - either it is encrypted, or it's a wrong file, and useless for me. Typical Unreal engine 3 executable is about 15Mb of size, it could be larger or a bit smaller.
   - XBox360. The executable is usually (or always?) named "default.xex". It is always encrypted, but that's not a problem.
   - Unreal engine 1 or 2. The game executable itself is relatively small (a few hundreds of kilobytes) and has no "key strings" inside.  Most of the engine code is located in core.dll and engine.dll, so I'll need these files as well.
2. core.u and engine.u (for some UE3 games file extension may be "xxx") - these files contains compiled scripts. Placing all .u files into archive is welcome unless they're too big.
3. startup_int package (usually present for cooked games).
4. file version [report of the pkgtool](https://www.gildor.org/smf/index.php/topic,308.msg2979.html#msg2979) - to let me make autodetection of this game with umodel.

_These files should be compressed to reduce traffic_ - use Rar or 7zip. It is good to send archive which is nearly 50Mb of size.

`NOTE`: I can not guarantee that I will support the game, but in most cases it is not possible to do anything without having the files.

### What is "cooked package"?
Cooking were introduced in Unreal engine 3. Cooking is a process in development cycle which produces package files for target platform. Cooked packages are smaller than non-cooked and could be loaded faster. Usually these packages resides in "Cooked<PlatformName>" game folder. This folder contains compiled script packages, map packages and seek-free packages. For more information please read UDN article ["Content cooking"](http://udn.epicgames.com/Three/ContentCooking.html).

### What's the difference between "export", "save" and "extract" terms?
Export is probably self-explaining, but I'll add a few words. This word in computer software usually means - get something stored in internal data format, and save that information in a format suitable for other software. For Maya you can export a "mb" scene into fbx file format - it will lose some information, but you can open fbx somewhere else. In UModel e.g. for a skeletal mesh - mesh is stored in some internal engine's format. UModel is loading it into UModel's data structures (can't say "format" because it's in-memory, not a disk representation), then save it to psk or gltf disk format.

"Extract" depends on what you're extracting. This term means no data conversion. Extracting from a pak file means - get some file and save it to disk, as is. UModel has "save selected packages" in UI for that, I just decided to use "save" term for it. In a case of game which is not packaged with a pak file, this will simply copy data - with no extraction. So "save" word is more suitable here.

Extracting an upk or uasset file - this is done with a "package extractor" tool. It loads an uasset ("package") and saves every internal data as a separate file. I use it from time to time to inspect what's stored inside a package, the data is not useful _for me_ in any other way. Some people does something more with extracted data, sometimes they even able to replace some structures by recombining package back from extracted data.

"Saved package" won't be useful for Unreal editor - editor is not designed to work with such kind of data. There are 2 kinds of uassets in UE4 (same for upk in UE3): "source assets" and "cooked assets". Cooked assets contains data for packaged game, source assets doesn't contain "cooked data" but has data useful for editor. A simplest example - texture asset. For "source asset" it contains PNG image, for cooked asset - DDS texture, in a **different** place. When package is marked internally as "source asset", it often uses different file layout. For a static mesh, uasset contains "raw mesh" - something what was imported from fbx and stored directly into uasset. It doesn't contain optimized data, LODs etc.


## Help with various problems {#Help-with-various-problems}

---

### Umodel is crashed!
- When you've got a crash, first of all check the [compatibility table](https://www.gildor.org/projects/umodel/compat). Most probably the game you are trying to open is not fully supported and you must restrict umodel to not load some kinds of resources: use -nomesh, -nostat, -noanim or -notex switches.
- If the game has associated forum thread, please review it - it is possible that the game requires some special options to run.
- If this is an Unreal engine 4 game, try selecting a different engine version, even if engine version is specified on the forum or somewhere else. UE4 games often gets updates, so the game might use a newer engine.

Also verify that you _did not mixed files from different games in the same directory_. For example if you'll replace textures.tfc file with the file from other game you'll definitely get a crash when umodel will try to access textures. Another example - if you'll place package from Batman: Arkham Asylum into the directory with Batman: Arkham City files - these games has a lot of files with the same names, but these files are not compatible with each other.

### Umodel is crashed with error "zlib uncompress() returned -3"
This might happen for UE3 games. Try adding option "-lzo" to the command line (also can select "LZO" from startup UI).

### Umodel is crashed with "BindFramebuffer error ..."
Your videocard has buggy drivers. Umodel is crashed when trying to render scene to texture. Update your drivers.

### What does error "specified package has no supported objects" means?
This is not an error. The package file you're trying to open has nothing to display in umodel. Unreal engine has thousands of various object types, but umodel supports only a dozen of them. Verify other packages. The easiest way to find something in UE3 is to check largest packages first. Also UI has "Scan content" tool which reveal which packages has something openable with UModel.

If you want to know what's inside the particular package, you may try opening it with UModel. In this case you'll see a list of object types stored there - in viewer window and in log.

### What does error "too many unknown objects" means?
The answer [is here](https://www.gildor.org/smf/index.php/topic,314.0.html).

### What does error "wrong tag in package" means?
- You're trying to open non-package file.
- Or you're trying to open file from some unsupported game which uses modified package format.
- Sometimes this error happens when people trying to open incorrectly extracted package files from XBox360 ISO images.
- For UE4 games with encryption, this could mean that the provided decryption key is wrong.

With older UModel versions this error crashed UModel. Newer UModel will show a warning message and do nothing with such packages.

### UModel displays error message "Unversioned UE4 packages are not supported"
This could happen when you're opening UE4 packages which were cooked with stripped out version information. This is a very strange feature of UE4, but we should accept it and live with it. To bypass this error, you should select UE4 version manually. You may do that either from startup UI or by passing "-game=ue4.nn" option to the command line. If the game was already found as supported, most likely you will find exact UE4 version in the compatibility table on this site. Also you could check forums. If not - just try every UE4 version, and if UModel will crash - try another version next run.

If you didn't provide an engine version, when UModel tries to open unversioned package for the first time, it will display a window which will ask for engine version. This list will have only those engine versions which are suitable according some (minimal) information stored in package file.

Please keep in mind that you could have the following situation: you will set UE4.5 and get skeletal meshes and textures loaded. However static mesh will crash UModel. In this case you should try 4.6 - in this version SkeletalMesh format is not changed, but StaticMesh - was.

Versions not worth trying: 4.1, 4.5, 4.9 - these versions has no changes important for UModel, so you may simply bypass them.

### Why umodel error messages are so strange? Text1 <- Text2 <- Text3 ...
This error message is in Unreal engine 1 and 2 style. It displays callstack of the function calls, plus some additional information which allows me to determine the reason of error with just looking at the log.

### What is the message "WARNING: BoolProperty "SomeClass::SomeProp" was not found" in the log?
This message means: object of SomeClass has property SomeProp which is not known to the umodel. Simply ignore these messages, it could be useful for some diagnostic only.

### Why the same mesh is duplicated in multiple packages?
This is a side effect (or feature) of the UE3 cooking process. Read more about cooking above.

## Starting UModel {#Starting-UModel}

---

### Is there a GUI for umodel?
Yes, starting from September of 2014 Windows version of UModel has a GUI. Just run UModel executable without arguments, and startup options window will appear. Command line options still works, so if you need some option which is missing in settings UI but exists in a command line, just add these options to the command line. An example: you want to use GUI for export, and need to use DDS texture format. Just run "umodel.exe -dds" - you will see GUI, and everything will work as usual, but exported textures will be saved in .dds format whenever possible.

### Program is not working - when I started umodel some black window appears and immediately disappears
This could happen if you're starting UModel in command line mode with some parameters wrong. Please review your command line parameters. If you're using .bat file (Windows batch), add "pause" command at the end to be able to see possible error message.

Most likely this problem won't happen with GUI version of UModel, unless you're running it from some shortcut with wrong command line parameters.

### Which command line options are available?
Start umodel with option "-help" from the command line, it will display a full list of command line options.

## Viewer questions {#Viewer-Questions}

---

### How to use umodel in a viewer mode? What umodel can do?
Viewer will start automatically when you'll not specify "-export", "-list" etc options which affects umodel mode. In a viewer mode press <H> key to display full information about keyboard shortcuts. Note: this list depends on the object beeng displayed - texture and mesh viewer has a different set of commands.
Also check my brief introduction into umodel:
https://www.gildor.org/en/projects/umodel/tutorials
"Unreal Model Viewer basics"

### How can I take a screenshot?
Press <Ctrl+S>. Screenshot will be placed in the current directory, in subdirectory "Screenshots". They are in TGA format. If you want to produce screenshot with transparent background you should use key <Alt+S>. In this case umodel will replace background color with translucent black color. Also key <Ctrl+Q> could be useful - it will disable onscreen debug output such as texts and coordinate system axis.

### How can I view player models combined from a few meshes?
Start umodel in a viewer mode. Browse through the meshes. Find a mesh which is a part of "combined mesh". Press <Ctrl+T> ("tag mesh"). Mesh name will appear in a list on the screen. Now, when you will browse to another mesh, tagged mesh will remains on the screen. Use the same method to tag all required meshes (head, legs, torso, arms etc). Note: the combined mesh can be animated in a usual way.

Also I suggest you to watch [second tutorial video](https://www.gildor.org/en/projects/umodel/tutorials).

### How to view animations for UE3 games? When I'm opening a model it has no available animations
Press <Ctrl+A> key. This key will cycle through all loaded AnimSets (MeshAnimation object for UE2).

### Animated mesh has gone outside from viewport. How can I focus camera on it?
Probably animation uses "Root Motion". Press <F> ("focus") key to focus camera on the mesh center. You may hold this key pressed during the animation to keep mesh on the screen center.
Of course, you may also use traditional navigation with mouse.

### How can I view animation when it is stored separately from the mesh (in a different package)?
When starting umodel add extra switch "-pkg=<package_name>" providing the name of that "different package". Umodel will load the priomary package (which name is provided in an "ordinary way") and all secondary packages (there may be any number of "-pkg=..." options).

`NOTE`: you may use the same approach to combine meshes which parts are placed in a different packages too. Example - UT3 Bonus Pack - it contains additional head, torso, legs meshes for human races - you may combine them with the meshes from retail game and even animate it with a standard animations.

### There are a lot of materials in object browser. How can I quickly view meshes?
Use additional command line option "-meshes". In UI you may enable "Navigate | Enable Meshes" option from menu.

## Texturing questions {#Texturing-questions}

---

### Meshes in a viewer are not textured. What's wrong?
There are a few variants of answer:

1. umodel is unable to find some package containing materials - check umodel logs
2. mesh is not textured, materials are assigned by the game in runtime or in MeshComponent object (UE3)
3. material is too complex for umodel, so it is replaced with "default texture"
4. this is a variant of (1) - you're trying to start umodel with a wrong game path. For example: UE2 game usually has "Textures", "Animations" and some other directories. If you'll start umodel from the "Amimations" directory, umodel will not file textures. You should start it from the parent directory - common for the "Animations" and "Textures".

More info [here](https://www.gildor.org/smf/index.php/topic,407.0.html) and [here](https://www.gildor.org/smf/index.php/topic,36.0.html).

### Why UE3 game textures are in very low resolution - 64x64 instead of 2048x2048?
That's another "side effect" (really - feature) of cooking. Check the log. Most probably umodel is unable to find tfc file (usually "textures.tfc"). If this file really exists - verify your "-path=..." option.
[More info](https://www.gildor.org/smf/index.php/topic,207.0.html). Also check [this thread](https://www.gildor.org/smf/index.php/topic,445.0.html).

Another note: this error often exists when user (following some third-party tutorials) is copying game files to some directory and trying to extract files from that place. This is wrong - umodel is written to open files directly from the game directory, no file copying is required.

Newer versions of Unreal engine 3 has capabilities to completely remove texture data from packages so you whould get completely black textures instead of low-res ones when tfc file is missing.

### I'm using -dds option for export, but textures are still in tga format
This could happen if you're exporting textures which are not in DXT format. For example, ETC or PVRTC (mobole texture formats) can not be saved as dds because they are encoded in format which is not supported by dds container. Uncompressed textures are also saved in tga format.

### How to extract data from the textures.tfc?
Textures.tfc cannot be extracted directly. This file is used by Unreal Engine 3 (and by umodel) automatically when package file has streamed texture. This file contains texture data, but texture metadatas (texture name, size, format) are located inside upk files.

The same applies to the Bioshock's .blk and Unreal Championship 2 .xpr files.

## Unreal engine 4 specific questions {#Unreal-Engine-4}

---

### What is "unversioned package"?
Unreal engine (ALL versions) packages has version information to be able to load older package into a newer engine editor. Older Unreal engine versions (1 to 3) were possible to load older cooked packages into an updated game made with newer engine. For some reason, UE4 has stripped that information, so you MUST say to umodel which engine is used for the game. Not sure if I should explain why version data is stripped - there are reasons for that, and explanation would be too technical. I'll just say that it is called "unversioned package". All version information are filled with zeros in a cooked UE4 game. The game, when it sees unversioned package, just assumes that the package was saved with EXACTLY the same engine version as game has, assuming they were packaged exactly for it. Unreal editor silently rejects unversioned packages, this is why you can't see "saved" packages in asset tree.

### What are "cooked" and "source" packages in UE4?
As for UE3, cooked packages has editor information stripped. However unlike UE3, editor in UE4 _can't_ read cooked packages, it expects somewhat different data for loading - which are stored in "source" packages. To be able to load cooked package into editor, it should have editor-only data there. However, for Unreal Tournament 4, Epic tried to adapt editor for loading and using cooked packages - to allow game modding (extending) without providing source packages for everything. With special build for UT4, users were able to use cooked textures (may be something else). Unfortunately these changes weren't merged into base UE4, and the game itself were officially abandoned (or frozen) in 2018.

### What are ubulk and uexp files?
Unreal engine, starting with UE3, has "bulk" system. It allows to store part of data internally or externally of object, and that data will not be loaded unless engine requests that. For UE4 bulk data is stored either at the end of uasset file, or in separate ubulk file. When starting a level, engine skips bulk data from loading process, and loads it after the game process started (using streaming).

.uexp is a different thing. Engine strips package header (which is in .uasset) from data - which is in .uexp file. This allows to store package's header and data in different order inside a pak file. So, when engine started, it loads all package's headers, and then loads data only when it is needed. If properly optimized, this speeds up loading, because data may be stored in pak in order of access.

### I can't export animation from UE4 games
Animation workflow differs from UE2/UE3 games. This is mostly because there's no AnimSet object in UE4 which holds references to all relevant animations, each animation sequence is a separate object now. The only thing which links all animations and meshes together is a Skeleton object. It's easier to show how to export animations in video than trying to explain that using words. Please see [my video here](https://www.youtube.com/watch?v=KSZaX0YQ2qg).

### How could I know which UE4 version to use when opening a particular game?
Check the compatibility table. Check the forum. There you may find the recent information about the game. If the game is not in table, just try to entering different engine versions by yourself. When you're starting the UModel, it allows to select engine version. The list is very long and grows with every UE4 release. However you may avoid selecting the engine right on startup, and leave this decision to later time - when you'll try to open any uasset. In this case, engine selection dialog will appear, with much shorter list. See [the video](https://www.youtube.com/watch?v=gTeINcso8ks) explaining this feature here.

Please note that UE1-UE3 games didn't have to select which engine version should be used. This is because packages had required version information in there. However, in UE4, right from UE4.0, all cooked packages are "unversioned" by default - this means that any version information is erased from all packages, and this is why you should select the engine with UModel.

### Materials appears colored with pure red/green/blue colors, what's wrong?
Most likely this game uses "layered materials". This means that the same mesh uses for example wood, stone, skin, metal materials, which are masted with particular color in some mask texture (which you're seeing in UModel's window). To learn more about layered materials, please refer to these links:

https://www.youtube.com/watch?v=nVes6OUyzdw

https://forums.unrealengine.com/development-discussion/rendering/1414193-material-layering-feedback-for-4-19

### Encrypted pak files
UE4 has built-in possibility to encrypt pak files. At the moment UModel doesn't support AES decryption, it will crash when you'll try to open any uasset from encrypted pak file with a message "unable to open file". To see if pak files are encrypted or not, you may check UModel's log right after selecting a difectory with game files (at startup) - you'll see a list of loaded pak files in log, with messages telling how many encrypted files are there. Usually people are extracting such pak files with third-party software like QuickBMS (you may find scripts are on the forum in relevant threads) and then opening extracted files with UModel.

## Exporter and PSK/PSA questions {#Exporter}

---

### Where I can find extracted files?
Files should be located by default in a current directory.

Windows Vista and 7 has disabled writing files into "Program Files" directory for non-admin users. When you're trying to export context from some game located in Program Files, operating system will redirect all output to the "C:\Users\<UserName>\AppData\Local\VirtualStore\Program Files" directory.

If you having trouble locating exported files, use "-out=SomeOtherDirectory" option. Alternatively you may specify export directory in program's settings.

### Umodel extracts nothing from package SomePackageName.upk
If umodel is not crashed - it's ok. Most probably this package has neither textures nor meshes inside, relax. Some time ago I've added a helper which allows to easily determine if package has something useful:

1. In package selection GUI, select "Tools | Scan package content". After that you will see number of textures, skeletal, static meshes, and animations near every package name.

2. If you will open a package which has nothing to display, UModel will display a simple on-screen text showing number of objects of every type stored in this package.

### How to recompile extracted data back to the packages?
You may recreate package if target game has UnrealEd (of course, if all required data were exported - most data types are not supported, it is impossible to support everything). If the game has no publically available UnrealEd, this task is impossible.

### How can I load PSK file into 3ds Max?
Use my [ActorX Importer plugin](https://www.gildor.org/projects/unactorx).

### How can I save (export) PSK/PSA files from 3ds Max or Maya?
Use [Epic Games ActorX plugin](http://udn.epicgames.com/Three/ActorX.html) for 3ds Max or Maya. These plugins were discontinued by Epic Games in favour of FBX file format, and you will not find versions for recent Max or Maya software. Updated plugins could be found on this site, see [this location](https://www.gildor.org/smf/index.php/topic,1221.0.html).

### Is there PSK/PSA importer for Maya?
Check [this forum thread](https://www.gildor.org/smf/index.php/topic,2273.0.html).

### Is there importer for Blender?
Yes. There are number of solutions available (none were made by me). It seems that the best link for Blender psk and psa importer is this one (check "master" and "latest" branches, which one has newer plugin). This script has possibility to import PSKX files.

For more information please refer to these threads: [first](https://www.gildor.org/smf/index.php/topic,718.0.html), [second](https://www.gildor.org/smf/index.php/topic,1745.0.html).

### What is PSKX format?
This format is a modified PSK (ActorX mesh) format. Modifications were made by me (Gildor) to allow placing some additional information into the file. You should not try to import PKSX into UnrealEd, it may crash. I have changed file extension specially for people who trying to load such files into programs with PSK support (and asking after that "why that program is crashed when I've loaded psk?"). There is no PSKX documentation available, but I can reveal my changes if there will be any interest.

Currently PSKX has following additional features in comparison to PSK:
- can store mesh without a skeleton (i.e. StaticMesh)
- mesh can be made with more than 64k vertices
-
Earlier versions of the umodel were produced PSAX files too (in some conditions), but later it was overridden with text .config files stored aside with PSA file.

### What are .mat files exported by UModel?
UModel generates .mat files to allow ActorX Importer to load materials into 3ds Max. The format of these .mat files are completely custom, don't try to load it into any other application (except perhaps any text editor). The format is fully human readable and you may read it to see which textures are used for material's diffuse, normal, etc.

### Mesh is loaded into 3ds Max untextured, while it is appeared in umodel's viewer correctly
You should correctly set up ActorX Importer options "Path to materials" and "look in subfolders".

Check this thread for more details. Also you could check [2nd tutorial video](https://www.gildor.org/en/projects/umodel/tutorials), I'm describing how to load textured mesh into 3ds max there.

### How to export sounds?
Use "-sounds" command line option, or enable sounds from UI. This options is not used by default because I cannot guarantee that umodel will not crash on exporting sounds for particular game.

### How to export FaceFX and ScaleForm assets? (UE3)
Use "-3rdparty" option, or enable assets from UI.

### How can I export data from all game packages?
1. GUI approach. Start UModel with GUI, enter all required options on startup window. You will see a window containing list of found game packages. Just select all packages there and press "Export". If packages are located in multiple directories, check "flat view" checkbox, and you will see all packages in a single list.
2. Old command line approach. Use [this batch extractor](https://www.gildor.org/smf/index.php/topic,1099.0.html). Note that this page has a lot of information about tuning this batch.
3. New command line approach. Use command `umodel -export -path=... *.uasset`
