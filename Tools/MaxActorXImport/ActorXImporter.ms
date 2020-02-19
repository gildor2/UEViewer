/*

 ActorX mesh (psk) and animation (psa) importer for 3ds Max

 Created:	September 18 2009

 Author:	Konstantin Nosov (aka Gildor)

 Web page:	http://www.gildor.org/projects/unactorx

 Revision History:

    13.10.2019 v1.38
    - implemented loading of vertex colors

	28.08.2019 v1.37
	- implemented loading of PNG textures

	19.05.2018 v1.36
	- using case-insensitive comparison when finding bones in scene for animation

	06.12.2017 v1.35
	- an attempt to make smoothing groups working
	- renamed "recurse" option to "look in subfolders" to be less confising to new users

    16.10.2017 v1.34
    - added possibility to select and open multiple psk files at time

	21.07.2015 v1.33
	- saving bind pose information inside bone objects, this information will survive saving scene
	  to a max file

	18.07.2015 v1.32
	- allowing psa import to work with any mesh, i.e. without previously imported psk file

	14.07.2015 v1.31
	- trying to detect and repair bad vertex weights for imported psk file

	01.02.2015 v1.30
	- added animation import option "import at slider position"
	- integrated patch by Ayrshi (http://www.gildor.org/smf/index.php/topic,1925.0.html) intended to
	  improve mesh normals

	02.12.2014 v1.29
	- ActorX Imported now could be bound to toolbar, keyboard or menu - use "Customize user interface",
	  category "Gildor Tools", and then use "ActorX Importer" as you like
	- reordered controls, separated some options to own rollouts for easy reordering etc
	- preserving dialog position, scroll position and rollout "open" state during 3ds Max session (until
	  Max closed)

	28.11.2014 v1.28
	- added "mesh translation" options to settings
	- "advanced settings" are not stored to the ini file anymore

	16.12.2013 v1.27
	- added option "Don't conjugate root bone"

	10.06.2012 v1.26
	- stability improvements
	  more info: MaxScript documentation, "Do not use return, break, exit or continue"

	02.06.2012 v1.25
	- fixed Max 2013 support; fix made by sunnydavis, check
	  http://www.gildor.org/smf/index.php/topic,1408.0.html for details

	18.02.2012 v1.24
	- fixed parsing psa config file with spaces in track names

	07.02.2012 v1.23
	- support for extra UV sets stored in standard psk format (ActorX 2010)

	23.01.2012 v1.22
	- fixed automatic loading of DDS textures for materials

	06.12.2011 v1.21
	- fixed "translation mode" checkbox to work with psa without config file

	01.12.2011 v1.20
	- implemented loading of DDS textures

	26.11.2011 v1.19
	- implemented support for loading pskx files with more than 64k vertices
	- added option to control behaviour of animation with rotation-only tracks: you can let AnimSet
	  to decide which bones will use animated translation, you can force to use translation from the
	  animation (old, pre-1.18 behaviour) or force to not use animated translation at all; the option
	  is located in "Animation import" group

	09.11.2011 v1.18
	- implemented support for animation tracks without translation keys
	- reading extended psa information from the .config file, removed psax ANIMFLAGS section support

	06.11.2011 v1.17
	- eliminated error messages when loading psk or psa file with unknown section name (SCALEKEYS etc)
	- implemented support for pskx with 2 or more UV channels

	03.05.2011 v1.16
	- improved animation cleanup

	01.01.2011 v1.15
	- workaround for loading animation with the root bone name different than mesh root bone
	- removed "Load confirmation" setting (not needed anymore because of functional "batch export")

	29.12.2010 v1.14
	- added "Batch export" tool

	22.12.2010 v1.13
	- mesh rotation formula is now identical to used in UnrealEd
	- added "Clear scene" tool

	15.12.2010 v1.12
	- added mesh rotation settings
	- added protection from errors appeared when updating this script while 3ds Max is running

	09.09.2010 v1.11
	- added "reorient bones" option

	23.07.2010 v1.10
	- implemented extended ActorX format (pskx and psax) support
	- "tools" rollout with options to restore mesh bindpose and remove animations

	24.04.2010 v1.09
	- applying normalmap using correct technique (previously was a bumpmap)

	14.04.2010 v1.08
	- fixed loading of psk files with root bone parent set to -1 (usually it is 0)

	20.02.2010 v1.07
	- added "Load confirmation" setting to display message box after completion of operation
	- added "Reposition existing bones" option
	- fixed error when loading .mat files with missing textures

	12.12.2009 v1.06
	- fixed merging meshes on a single skeleton when previously loaded mesh is not in bind
	  pose
	- improved compatibility with Epic's ActorX Exporter (dropping trailing spaces from
	  bone names)

	18.09.2009 v1.05
	- implemented materal loading
	- fixing duplicate bone names

	29.09.2009 v1.04
	- implemented support for loading non-skeletal (static) meshes

	26.09.2009 v1.03
	- fixed bug with interpolation between first two animation keyframes
	- option to fix animation looping (duplicate first animation frame after last frame)
	- added button to load all animations from psa file
	- progress bar for loading animation with "cancel" capabilities
	- option to not load mesh skin (load skeleton only)
	- storing last used directory separately for psk and psa

	25.09.2009 v1.02
	- added option to scale mesh and animations when loading
	- added options for texture search (path, recursive search)
	- added option to ask for missing texture files when mesh is loading

	24.09.2009 v1.01
	- fixed bug in a vertex weighting code
	- saving settings to ActorXImporter.ini (Max 9 and higher)
	- saving last used psk/psa directory
	- settings to change bone size for a new mesh

	22.09.2009 v1.00
	- first public release

*/


/*
TODO:
- option to create separate materials, not submaterials
- do not create material when it is already exists - but how to find whether I need to get loaded material
  or create a new one?
*/

/*
NOTES:
- setBoneEnable false 0:
  This call is required. Without it, we will have numerous problems with imported skeleton. Note that FBX
  importer will enable "bones" mode, however we can't use it in Max. Why "bone" mode could be useful: hiding
  bones could not hide the skeleton when bones are off, and works well when they are on. Why "bone" mode should
  be disabled - otherwise we've got problems with rotation/moving of particular bones, they behave like
  connected objects. Also saw a bug with imported animation with "force AnimSet translation" - mesh could became
  bronen in parts, but began to behave well when unlinked child bone of bad bone and relinked it back. So it's
  easier to disable bone mode than to fight against all the bugs.
*/

-- constant used to detect ActorX Importer updates during single 3ds Max session
global AX_IMPORTER_VERSION = 138

g_axProfile = false

-------------------------------------------------------------------------------
--	Global variables
-------------------------------------------------------------------------------

global g_seeThru
global g_skelOnly
global g_updateTime
global g_playAnim
global g_animAtSlider
global g_animTransMode			-- 1 = from AnimSet, 2 = force mesh translation, 3 = force AnimSet translation
global g_fixLooping
global g_lastDir1
global g_lastDir2
global g_texDir
global g_texRecurse
global g_texMissAction
global g_boneSize
global g_reposBones
global g_rotY
global g_rotP
global g_rotR
global g_transX
global g_transY
global g_transZ
global g_meshScale
global g_reorientBones
global g_dontConjugateRoot
global Anims					-- array of AnimInfoBinary


-------------------------------------------------------------------------------
--	Default settings
-------------------------------------------------------------------------------

fn axDefaultSettings =
(
	-- defaults settings
	g_seeThru    = false
	g_skelOnly   = false
	g_updateTime = true
	g_playAnim   = false
	g_animAtSlider = false
	g_animTransMode = 1
	g_fixLooping = false
	g_lastDir1   = ""
	g_lastDir2   = ""
	g_texDir     = ""
	g_texRecurse = true
	g_texMissAction = 1
	g_boneSize   = 0.5
	g_reposBones = true
	g_rotY       = 0
	g_rotP       = 0
	g_rotR       = 0
	g_transX     = 0
	g_transY     = 0
	g_transZ     = 0
	g_meshScale  = 1.0
	g_reorientBones = false
	g_dontConjugateRoot = false
)


-------------------------------------------------------------------------------
--	Configuration
-------------------------------------------------------------------------------

configFile = undefined
if getSourceFileName != undefined then	-- checking Max version (Max9+) ...
(
	local s = getSourceFileName()
	configFile = (getFilenamePath s) + (getFilenameFile s) + ".ini"
)
else
(
	-- workaround for Max 8 and older
	configFile = (getDir #scripts) + "\ActorXImporter.ini"
)


tmp_v = undefined		-- global variable, helper for axDoSetting() (required for execute() ...)
g_isLoading = true		-- axDoSetting() mode

fn axDoSetting name var =
(
	local default = execute var							-- value has the same type as var
	if g_isLoading then
	(
		try
		(
			-- loading value
			tmp_v = getINISetting configFile "Main" name	-- get from ini as string
			if (tmp_v != "") and (tmp_v != "undefined") then
			(
				local type = classOf default
--				format "reading % (%) = %\n" var type tmp_v
				if (not isKindOf default String) then
					execute (var + "=tmp_v as " + (type as string))
				else
					execute (var + "=tmp_v")				-- no conversion
			)
		)
		catch
		(
			format "Reading %: %\n" name (getCurrentException())
		)
	)
	else
	(
		-- saving value
		setINISetting configFile "Main" name (default as string)
	)
)


fn axSerializeSettings isLoading =
(
	if configFile == undefined then return undefined			-- could happen with old 3ds Max, where getSourceFileName() doesn't exist
	if isLoading then
	(
		if not doesFileExist configFile then return undefined	-- no config file
	)
	g_isLoading = isLoading
	-- read/write settings
	axDoSetting "LastUsedDir"   "g_lastDir1"
	axDoSetting "LastUsedDir2"  "g_lastDir2"
	axDoSetting "TexturesDir"   "g_texDir"
	axDoSetting "TexRecurse"    "g_texRecurse"
	axDoSetting "TexMissAction" "g_texMissAction"
	axDoSetting "AutoPlayAnim"  "g_playAnim"
	axDoSetting "AnimAtSlider"  "g_animAtSlider"
	axDoSetting "AnimTransMode" "g_animTransMode"
	axDoSetting "UpdateTime"    "g_updateTime"
	axDoSetting "FixLoopAnim"   "g_fixLooping"
	axDoSetting "SeeThru"       "g_seeThru"
	axDoSetting "SkelOnly"      "g_skelOnly"
	axDoSetting "BoneSize"      "g_boneSize"
	axDoSetting "ReposBones"    "g_reposBones"
	axDoSetting "MeshYaw"       "g_rotY"
	axDoSetting "MeshPitch"     "g_rotP"
	axDoSetting "MeshRoll"      "g_rotR"
	axDoSetting "MeshX"         "g_transX"
	axDoSetting "MeshY"         "g_transY"
	axDoSetting "MeshZ"         "g_transZ"
	axDoSetting "MeshScale"     "g_meshScale"
--	axDoSetting "ReorientBones" "g_reorientBones"
--	axDoSetting "DontConjRoot"  "g_dontConjugateRoot"
)


-------------------------------------------------------------------------------
--	Service functions
-------------------------------------------------------------------------------

fn ErrorMessage text =
(
	local msg = ("ERROR: " + text + "\n")
	format "%\n" msg
	messageBox msg
	throw msg
)


fn TrimSpaces text =
(
	trimLeft(trimRight(text))
)


fn IsEndOfFile bstream =
(
	local savePos = ftell bstream
	fseek bstream 0 #seek_end			-- compute file size
	local fileSize = ftell bstream
	fseek bstream savePos #seek_set
	(savePos >= fileSize)
)


fn ReadFixedString bstream fixedLen =
(
	local str = ""
	local length = 0
	local finished = false
	for i = 1 to fixedLen do
	(
		local c = ReadByte bstream #unsigned
		if c == 0 then finished = true	-- end of line char
		if not finished then 			-- has end of line before - skip remaining chars
		(
			-- not "finished" string
			str += bit.intAsChar(c)		-- append a character
			if c != 32 then length = i	-- position of last non-space char
		)
	)
	substring str 1 length				-- return first "length" chars
)

fn ReadVector2 bstream =
(
	local v = point2 0 0
	v.x = ReadFloat bstream
	v.y = ReadFloat bstream
	v
)

fn ReadFVector bstream =
(
	local v = point3 0 0 0
	v.x = ReadFloat bstream
	v.y = ReadFloat bstream
	v.z = ReadFloat bstream
	v
)

fn ReadFQuat bstream =
(
	local q = quat 0 0 0 0
	q.x = ReadFloat bstream
	q.y = ReadFloat bstream
	q.z = ReadFloat bstream
	q.w = ReadFloat bstream
	q
)

-- Function used to determine bone length
fn axFindFirstChild boneArray boneIndex =
(
	local res = undefined, notfound = true
	for i = 1 to boneArray.count while notfound do
	(
		if (i != boneIndex) then
		(
			bn = boneArray[i]
			if bn.ParentIndex == boneIndex-1 then
			(
				res = bn
				notfound = false
			)
		)
	)
	res
)


fn axFixBoneNames boneArray =
(
	-- Find and correct duplicate names
	for i = 1 to (boneArray.count-1) do
	(
		local n = boneArray[i].Name
		local dupCount = 1
		for j = (i+1) to boneArray.count do
		(
			local n2 = boneArray[j].Name
			if n == n2 then
			(
				dupCount += 1
				n2 = n + "_" + (dupCount as string)
				format "Duplicate bone name \"%\", renamed to \"%\"\n" n n2
				boneArray[j].Name = n2
			)
		)
	)
)


fn axFindFile path filename recurse:false =
(
	local res = undefined
	local check = path + "\\" + filename
	if doesFileExist check then
	(
		res = check
	)
	else if recurse then
	(
		local dirs = getDirectories (path + "/*")
		local notfound = true
		for dir in dirs while notfound do
		(
			res = axFindFile dir filename recurse:true
			if res != undefined then
			(
				notfound = false		-- break the loop
			)
		)
	)
	res
)


fn axGetRootMatrix =
(
	local angles = eulerAngles g_rotR -g_rotP -g_rotY
	local m = angles as matrix3
	m.translation = [g_transX, g_transY, g_transZ]
	m
)

-- Reference: https://forums.autodesk.com/t5/3ds-max-programming/getopenfilename-for-multiple-files/td-p/4097903
fn getMultiOpenFilenames caption: "Open" filename: "" types: "All Files (*.*)|*.*" default: 1 =
(
	local dlg = DotNetObject "System.Windows.Forms.OpenFileDialog"
	dlg.multiSelect = true
	dlg.title = caption

	local p = getFilenamePath filename
	if doesFileExist p then
	dlg.initialDirectory = p

	-- MAXScript getOpenFilename uses trailing |;
	-- OpenFileDialog filter does not.
	if types == "|" then
		dlg.filter = (substring types 1 (types.count - 1))
	else
		dlg.filter = types

	dlg.filterIndex = default

	local result = dlg.ShowDialog()
	if (result.Equals result.OK) then
		dlg.filenames
	else
		undefined
)

g_axProfileBeginTime = 0

fn axBeginProfile =
(
	g_axProfileBeginTime = timeStamp()
)

fn axProfilePoint title debug:false =
(
	if (not debug or g_axProfile) then
	(
		local current = timeStamp()
		local delta = current - g_axProfileBeginTime
		g_axProfileBeginTime = current
		format "% took % s\n" title (delta/1000.0)
	)
)

-------------------------------------------------------------------------------
--	ActorX data structures
-------------------------------------------------------------------------------

struct VChunkHeader
(
	ChunkID,
	TypeFlag,
	DataSize,
	DataCount
)

fn ReadChunkHeader bstream =
(
	local hdr = VChunkHeader ()
	hdr.ChunkID   = ReadFixedString bstream 20
	hdr.TypeFlag  = ReadLong bstream #unsigned
	hdr.DataSize  = ReadLong bstream #unsigned
	hdr.DataCount = ReadLong bstream #unsigned
--	format "Read chunk header: %\n" hdr
	hdr
)

struct VVertex
(
	PointIndex,
	U, V,
	MatIndex,
	Reserved,
	Pad
)

fn ReadVVertex bstream =
(
	local v = VVertex ()
	local pad
	v.PointIndex = ReadShort bstream #unsigned
	pad          = ReadShort bstream
	v.U          = ReadFloat bstream
	v.V          = ReadFloat bstream
	v.MatIndex   = ReadByte  bstream #unsigned
	v.Reserved   = ReadByte  bstream #unsigned
	v.Pad        = ReadShort bstream #unsigned
	v
)

fn ReadVVertex32 bstream =
(
	local v = VVertex ()
	v.PointIndex = ReadLong  bstream #unsigned			-- short -> long, no "pad"
	v.U          = ReadFloat bstream
	v.V          = ReadFloat bstream
	v.MatIndex   = ReadByte  bstream #unsigned
	v.Reserved   = ReadByte  bstream #unsigned
	v.Pad        = ReadShort bstream #unsigned
	v
)

struct VTriangle
(
	Wedge0, Wedge1, Wedge2,
	MatIndex,
	AuxMatIndex,
	SmoothingGroups
)

fn ReadVTriangle bstream =
(
	local v = VTriangle ()
	v.Wedge0          = ReadShort bstream #unsigned
	v.Wedge1          = ReadShort bstream #unsigned
	v.Wedge2          = ReadShort bstream #unsigned
	v.MatIndex        = ReadByte  bstream #unsigned
	v.AuxMatIndex     = ReadByte  bstream #unsigned
	v.SmoothingGroups = ReadLong  bstream #unsigned
	v
)

fn ReadVTriangle32 bstream =
(
	local v = VTriangle ()
	v.Wedge0          = ReadLong  bstream #unsigned		-- short -> long
	v.Wedge1          = ReadLong  bstream #unsigned		-- ...
	v.Wedge2          = ReadLong  bstream #unsigned		-- ...
	v.MatIndex        = ReadByte  bstream #unsigned
	v.AuxMatIndex     = ReadByte  bstream #unsigned
	v.SmoothingGroups = ReadLong  bstream #unsigned
	v
)

struct VMaterial
(
	MaterialName,
	TextureIndex,
	PolyFlags,
	AuxMaterial,
	AuxFlags,
	LodBias,
	LodStyle
)

fn ReadVMaterial bstream =
(
	local m = VMaterial ()
	m.MaterialName = ReadFixedString bstream 64
	m.TextureIndex = ReadLong bstream #unsigned
	m.PolyFlags    = ReadLong bstream #unsigned
	m.AuxMaterial  = ReadLong bstream #unsigned
	m.AuxFlags     = ReadLong bstream #unsigned
	m.LodBias      = ReadLong bstream
	m.LodStyle     = ReadLong bstream
	m
)

struct VColor
(
	R, G, B, A
)

fn ReadVColor bstream =
(
	local c = VColor ()
	c.R = ReadByte bstream #unsigned
	c.G = ReadByte bstream #unsigned
	c.B = ReadByte bstream #unsigned
	c.A = ReadByte bstream #unsigned
	c
)

struct VBone
(
	Name,
	Flags,
	NumChildren,
	ParentIndex,
	-- VJointPos
	Orientation,
	Position,
	Length,
	Size,
	-- Computed data
	Matrix
)

fn ReadVBone bstream =
(
	local b = VBone ()
	b.Name        = ReadFixedString bstream 64
	b.Flags       = ReadLong    bstream #unsigned
	b.NumChildren = ReadLong    bstream
	b.ParentIndex = ReadLong    bstream
	b.Orientation = ReadFQuat   bstream
	b.Position    = ReadFVector bstream
	b.Length      = ReadFloat   bstream
	b.Size        = ReadFVector bstream
	b
)

struct VRawBoneInfluence
(
	Weight,
	PointIndex,
	BoneIndex
)

fn ReadVRawBoneInfluence bstream =
(
	local v = VRawBoneInfluence ()
	v.Weight     = ReadFloat bstream
	v.PointIndex = ReadLong bstream #unsigned
	v.BoneIndex  = ReadLong bstream #unsigned
	v
)

fn InfluenceSort v1 v2 =
(
	-- we just need to get influences sorted by vertex index
	local cmp = v1.PointIndex - v2.PointIndex
	-- add bone index sorting for sort stability
	if (cmp == 0) then cmp = v1.BoneIndex - v2.BoneIndex
	cmp
)

struct AnimInfoBinary
(
	Name,
	Group,
	TotalBones,
	RootInclude,
	KeyCompressionStyle,
	KeyQuotum,
	KeyReduction,
	TrackTime,
	AnimRate,
	StartBone,
	FirstRawFrame,
	NumRawFrames
)

fn ReadAnimInfoBinary bstream =
(
	v = AnimInfoBinary ()
	v.Name                = ReadFixedString bstream 64
	v.Group               = ReadFixedString bstream 64
	v.TotalBones          = ReadLong  bstream
	v.RootInclude         = ReadLong  bstream
	v.KeyCompressionStyle = ReadLong  bstream
	v.KeyQuotum           = ReadLong  bstream
	v.KeyReduction        = ReadFloat bstream
	v.TrackTime           = ReadFloat bstream
	v.AnimRate            = ReadFloat bstream
	v.StartBone           = ReadLong  bstream
	v.FirstRawFrame       = ReadLong  bstream
	v.NumRawFrames        = ReadLong  bstream
	v
)

struct VQuatAnimKey
(
	Position,
	Orientation,
	Time
)

fn ReadVQuatAnimKey bstream =
(
	local k = VQuatAnimKey ()
	k.Position    = ReadFVector bstream
	k.Orientation = ReadFQuat   bstream
	k.Time        = ReadFloat   bstream
	k
)


-------------------------------------------------------------------------------
--	Bone attributes
-------------------------------------------------------------------------------

AXBoneCustomDataDef = attributes AXBoneCustomData
attribID:#(0xF3DD7FCD, 0x4DB58449)
(
	parameters BindPose
	(
		AX_RelMatrix	type: #matrix3		-- matrix relative to parent bone
		AX_WorldMatrix	type: #matrix3		-- world matrix
	)
)

-------------------------------------------------------------------------------
--	Loading materials
-------------------------------------------------------------------------------

fn axFindTexture texDir baseName =
(
	-- DDS
	foundTex = axFindFile texDir (baseName + ".dds") recurse:g_texRecurse
	if foundTex == undefined then
	(
		-- TGA
		foundTex = axFindFile texDir (baseName + ".tga") recurse:g_texRecurse
		if foundTex == undefined then
		(
			-- PNG
			foundTex = axFindFile texDir (baseName + ".png") recurse:g_texRecurse
		)
	)
	foundTex
)

fn axImportMaterial matName texDir =
(
	local subMat = standardMaterial name:matName

	local texFilename
	local foundTex

	-- try to file material file
	texFilename = matName + ".mat"
	foundTex = axFindFile texDir texFilename recurse:g_texRecurse
	if foundTex != undefined then
	(
		texFilename = foundTex
		format "Loading material %\n" texFilename
		local matFile = openFile texFilename
		while eof matFile == false do
		(
			local line = readline matFile
			local tok = filterString line " ="
--			format "[%] = [%]\n" tok[1] tok[2]
			local parm = tok[1]
			local file = tok[2]
			foundTex = axFindTexture texDir file
			if foundTex == undefined then continue
			local bitmap = bitmapTexture name:foundTex fileName:foundTex
			if parm == "Normal" then
			(
				local normalMap = normal_bump name:foundTex normal_map:bitmap
				subMat.bumpMap = normalMap
				subMat.bumpMapAmount = 100		-- amount is set to 30 by default
			)
			else
			(
				if parm == "Diffuse"   then subMat.diffuseMap = bitmap
				if parm == "Specular"  then subMat.specularMap = bitmap
				if parm == "SpecPower" then subMat.specularLevelMap = bitmap
				if parm == "Opacity"   then subMat.opacityMap = bitmap
				if parm == "Emissive"  then subMat.selfIllumMap = bitmap
			)
		)
		close matFile
		return subMat
	)
	-- no material file found, try simple texture
	-- get texture filename
	texFilename = matName
	foundTex = axFindTexture texDir matName
	if foundTex != undefined then
	(
		texFilename = foundTex
	)
	else
	(
		if g_texMissAction == 2 then			-- ask
		(
			local check = getOpenFileName caption:("Get texture for material " + matName) \
				types:"Texture files (*.tga,*.dds,*.png)|*.tga;*.dds;*.png|All (*.*)|*.*|" filename:texFilename
			if check != undefined then texFilename = check
		)
	)
	if not doesFileExist texFilename then format "Unable to find texture %\n" texFilename
	-- continue setup (even in a case of error)
	local bitmap = bitmapTexture name:texFilename fileName:texFilename
	subMat.diffuseMap = bitmap
	-- return
	subMat
)

-------------------------------------------------------------------------------
--	MAX helpers
-------------------------------------------------------------------------------

fn FindAllBones_Recurse bones parent =
(
	for i = 1 to parent.children.count do
	(
		node = parent.children[i]
		if isKindOf node BoneObj then
		(
			append bones node
		)
		FindAllBones_Recurse bones node
	)
)

fn FindAllBones =
(
	local bones = #()

	FindAllBones_Recurse bones rootNode

	bones
)

fn RemoveAnimation =
(
	stopAnimation()
	bones = FindAllBones()
	for i = 1 to bones.count do
	(
		b = bones[i]
		deleteKeys b #allKeys
	)
	animationRange = interval 0 1
)


fn RestoreBindpose =
(
	RemoveAnimation()

	try
	(
		local rotMatrix = axGetRootMatrix()
		-- note: should rotate every bone because we are not applying parent's rotation here
		-- find bones
		bones = FindAllBones()
		for i = 1 to bones.count do
		(
			b = bones[i]
			data = custAttributes.get b AXBoneCustomDataDef
			if data != undefined then
			(
				b.transform = data.AX_WorldMatrix * rotMatrix
			)
--			else
--			(
--				format "no info for %\n" b.name
--			)
		)

		set coordsys world
	)
	catch
	(
		format "ERROR!\n"
	)
)


fn ClearMaxScene =
(
	max select all
	if $ != undefined then delete $
)


-------------------------------------------------------------------------------
--	Loading PSK file
-------------------------------------------------------------------------------

fn ImportPskFile filename skelOnly:false =
(
	set coordsys world

	local Verts     = #()
	local Wedges    = #()
	local Tris      = #()
	local Materials = #()
	local MeshBones = #()
	local Infs      = #()
	local Colors    = #()

	--------- Read the file ---------

	local numVerts      = 0
	local numWedges     = 0
	local numTris       = 0
	local numMaterials  = 0
	local numBones      = 0
	local numInfluences = 0
	local numVertColors = 0
	local numTexCoords  = 1

	local extraUV = #()

	axBeginProfile()

	try
	(
		file = fopen filename "rb"
		if file == undefined then return undefined

		-- First header --
		hdr = ReadChunkHeader file
		if (hdr.ChunkID != "ACTRHEAD") then
		(
			ErrorMessage("Bad chunk header: \"" + hdr.ChunkID + "\"")
		)

		while not IsEndOfFile(file) do
		(
			hdr = ReadChunkHeader file
			local chunkID = hdr.ChunkID
			-- check for extra UV set from latest ActorX exporter
			-- note: data has the same format as pskx extension, so the same loading code is used
			if (chunkID == "EXTRAUVS0") or (chunkID == "EXTRAUVS1") or (chunkID == "EXTRAUVS2") then
				chunkID = "EXTRAUV0";
--			format "Chunk: % (% items, % bytes/item, pos %)\n" hdr.ChunkID hdr.DataCount hdr.DataSize (ftell file)
			case chunkID of
			(
			-- Points --
			"PNTS0000":
				(
					numVerts = hdr.DataCount
					Verts[numVerts] = [ 0, 0, 0 ]		-- preallocate
					for i = 1 to numVerts do Verts[i] = ReadFVector file
				)

			-- Wedges --
			"VTXW0000":
				(
					numWedges = hdr.DataCount
					Wedges[numWedges] = VVertex ()		-- preallocate
					if numWedges <= 65536 then
					(
						for i = 1 to numWedges do Wedges[i] = ReadVVertex file
					)
					else
					(
						for i = 1 to numWedges do Wedges[i] = ReadVVertex32 file
					)
				)

			-- Faces --
			"FACE0000":
				(
					numTris = hdr.DataCount
					Tris[numTris] = VTriangle ()		-- preallocate
					for i = 1 to numTris do Tris[i] = ReadVTriangle file
				)

			-- Faces32 --
			"FACE3200":
				(
					numTris = hdr.DataCount
					Tris[numTris] = VTriangle ()		-- preallocate
					for i = 1 to numTris do Tris[i] = ReadVTriangle32 file
				)

			-- Materials --
			"MATT0000":
				(
					numMaterials = hdr.DataCount
					if numMaterials > 0 then Materials[numMaterials] = VMaterial ()	-- preallocate
					for i = 1 to numMaterials do Materials[i] = ReadVMaterial file
				)

			-- Bones --
			"REFSKELT":
				(
					numBones = hdr.DataCount
					if numBones > 0 then MeshBones[numBones] = VBone () -- preallocate
					for i = 1 to numBones do
					(
						MeshBones[i] = ReadVBone file
--							format "Bone[%] = %\n" (i-1) MeshBones[i].Name
					)
					axFixBoneNames MeshBones
				)

			-- Weights --
			"RAWWEIGHTS":
				(
					numInfluences = hdr.DataCount
					if numInfluences > 0 then Infs[numInfluences] = VRawBoneInfluence () -- preallocate
					for i = 1 to numInfluences do Infs[i] = ReadVRawBoneInfluence file
				)

			-- Vertex colors --
			"VERTEXCOLOR":
				(
					numVertColors = hdr.DataCount
					if numVertColors > 0 then Colors[numVertColors] = VColor () -- preallocate
					for i = 1 to numVertColors do Colors[i] = ReadVColor file
				)

			-- Additional UV set --
			"EXTRAUV0":
				(
					numUVVerts = hdr.DataCount
					if (numUVVerts != numWedges) then ErrorMessage("Bad vertex count for extra UV set")
					local UV = #()
					UV[numUVVerts] = [ 0, 0 ]
					for i = 1 to numUVVerts do UV[i] = ReadVector2 file
					extraUV[numTexCoords] = UV
					numTexCoords = numTexCoords + 1
				)

			default:
				(
					-- skip unknown chunk
					format "Unknown chunk header: \"%\" at %\n" hdr.ChunkID (ftell file)
					fseek file (hdr.DataSize * hdr.DataCount) #seek_cur
				)
			)
		)
	)
	catch
	(
		fclose file
		messageBox("Error loading file " + filename)
		format "FATAL ERROR: %\n" (getCurrentException())
		return undefined
	)

	format "Read mesh: % verts, % wedges, % tris, % materials, % bones, % influences\n" \
		numVerts numWedges numTris numMaterials numBones numInfluences
	fclose file

	axProfilePoint "File loading"

	--------- File is completely read now ---------

	-- generate skeleton
	MaxBones = #()
	local rotMatrix = matrix3 1
	for i = 1 to numBones do
	(
		bn = MeshBones[i]
		-- build bone matrix
		q = bn.Orientation
		if ((i == 1) and not g_dontConjugateRoot) then q = conjugate q
		mat = (normalize q) as matrix3
		mat.row4 = bn.Position * g_meshScale
		-- transform from parent bone coordinate space to world space
		if (i > 1) then
		(
			bn.Matrix = mat * MeshBones[bn.ParentIndex + 1].Matrix
		)
		else
		(
			bn.Matrix = mat
		)

		-- get bone length (just for visual appearance)
		childBone = axFindFirstChild MeshBones i
		if (childBone != undefined) then
		(
			len = (length childBone.Position) * g_meshScale
		)
		else
		(
			len = 4		-- no children, default length; note: when len = 1 has bugs with these bones!
		)
		if len < 4 then len = 4
		-- create Max bone
		newBone = getNodeByName bn.Name exact:true ignoreCase:true
		if (newBone == undefined) then
		(
			if (g_reorientBones == false or childBone == undefined) then
			(
				-- create new bone
				newBone = bonesys.createbone	\
					  bn.Matrix.row4			\
					  (bn.Matrix.row4 + len * (normalize bn.Matrix.row1)) \
					  (normalize bn.Matrix.row3)
			)
			else
			(
				-- reorient bone matrix to point directly to a child
				-- get world position of the child bone
				local childPos = childBone.Position * bn.Matrix * g_meshScale
				newBone = bonesys.createbone	\
					  bn.Matrix.row4			\
					  childPos					\
					  bn.Matrix.row3
			)
			newBone.name   = bn.Name
			newBone.width  = g_boneSize
			newBone.height = g_boneSize
			newBone.setBoneEnable false 0					-- this is a required thing, otherwise a lot of problems would appear
			newBone.pos.controller      = TCB_position ()
			newBone.rotation.controller = TCB_rotation ()	-- required for correct animation
			-- setup parent
			if (i > 1) then
			(
				if (bn.ParentIndex >= i) then
				(
					format "Invalid parent % for bone % (%)" bn.ParentIndex (i-1) bn.Name
					return undefined
				)
				newBone.parent = MaxBones[bn.ParentIndex + 1]
			)
			-- store bind pose in custom data block
			custAttributes.add newBone AXBoneCustomDataDef
			mat = (normalize q) as matrix3		-- rebuild 'mat', but without scale
			mat.row4 = bn.Position
			newBone.AX_RelMatrix   = mat
			newBone.AX_WorldMatrix = bn.Matrix
		)
		else
		(
			-- bone already exists
			if g_reposBones then newBone.transform = bn.Matrix
		)
		MaxBones[i] = newBone
	)

	-- generate mesh
	MaxFaces = #()
	MaxVerts = #()
	MaxFaces[numTris]   = [ 0, 0, 0 ]			-- preallocate
	MaxVerts[numWedges] = [ 0, 0, 0 ]			-- ...
	VertList = #();								-- list of wedges linked for each vertex
	VertList.count = numVerts					-- preallocate
	for i = 1 to numVerts do VertList[i] = #()	-- initialize with empty array
	for i = 1 to numWedges do
	(
		local vertId = Wedges[i].PointIndex + 1
		MaxVerts[i] = Verts[vertId] * g_meshScale
		append VertList[vertId] i
	)
	for i = 1 to numTris do
	(
		tri = Tris[i]
		w0 = tri.Wedge0
		w1 = tri.Wedge1
		w2 = tri.Wedge2
		MaxFaces[i] = [ w1+1, w0+1, w2+1 ]		-- note: reversing vertex order
	)
	newMesh = mesh vertices:MaxVerts faces:MaxFaces name:(getFilenameFile filename)
	-- texturing
	newMesh.xray = g_seeThru
	meshop.setNumMaps newMesh (numTexCoords+1)	-- 0 is vertex color, 1+ are textures
	meshop.setMapSupport newMesh 1 true			-- enable texturemap channel
	meshop.setNumMapVerts newMesh 1 numWedges	-- set number of texture vertices
	for i = 1 to numWedges do
	(
		-- set texture coordinates
		w = Wedges[i]
		meshop.setMapVert newMesh 1 i [ w.U, 1-w.V, 1-w.V ]	-- V coordinate is flipped
	)
	for i = 1 to numTris do
	(
		-- setup face vertices and material
		tri = Tris[i]
		meshop.setMapFace newMesh 1 i [ tri.Wedge1+1, tri.Wedge0+1, tri.Wedge2+1 ]
		setFaceMatId newMesh i (tri.MatIndex+1)
		setFaceSmoothGroup newMesh i tri.SmoothingGroups
	)
	-- extra UV sets (code is similar to above!)
	for j = 2 to numTexCoords do
	(
		format "Loading UV set #% ...\n" j
		uvSet = extraUV[j-1]						-- extraUV does not holds 1st UV set
		meshop.setMapSupport newMesh j true			-- enable texturemap channel
		meshop.setNumMapVerts newMesh j numWedges	-- set number of texture vertices
		for i = 1 to numWedges do
		(
			-- set texture coordinates
			uv = uvSet[i]
			meshop.setMapVert newMesh j i [ uv.x, 1-uv.y, 1-uv.y ]	-- V coordinate is flipped
		)
	)
	-- vertex colors
	if numVertColors > 0 then
	(
		format "Loading vertex colors ...\n"
		setNumCPVVerts newMesh numVertColors true
		defaultVCFaces newMesh
		for i = 1 to numVertColors do
		(
			c = Colors[i]
			setVertColor newMesh i [ c.R, c.G, c.B, c.A ]
		)
	)

	axProfilePoint "Base import" debug:true

	-- import materials
	if g_skelOnly then numMaterials = 0		-- do not load materials for this option
	-- setup path to materials and textures
	local texDir
	if g_texDir != "" then
	(
		texDir = g_texDir
	)
	else
	(
		texDir = getFilenamePath filename
	)
	-- create materials
	newMat = multiMaterial numsubs:numMaterials
	for i = 1 to numMaterials do
	(
		local subMat = axImportMaterial Materials[i].MaterialName texDir
		newMat.materialList[i] = subMat
		showTextureMap subMat true
--		format "Material[%] = %\n" i Materials[i].MaterialName
	)
	newMesh.material = newMat

	axProfilePoint "Material import" debug:true

	update newMesh

	-- smooth vertex normals accross UV seams
	max modify mode
	select newMesh

	normalMod = editNormals ()
	addModifier newMesh normalMod
	normalMod.selectBy = 1

	for i = 1 to VertList.count do
	(
		if VertList[i].count > 1 then
		(
			local seamWedges = VertList[i] as bitArray
			local n = #{}
			normalMod.ConvertVertexSelection &seamWedges &n
			normalMod.Average selection:n
		)
	)
	VertList.count = 0
	collapsestack newMesh

	if numBones <= 0 then
	(
		return undefined
	)

	-- code above is common for SkeletalMesh (psk) and StaticMesh (pskx)
	-- code below is executed only for SkeletalMesh

	-- generate skin modifier
	skinMod = skin()
	boneIDMap = #()

	if numBones > 0 then	--?? checking not needed as we have numBones always > 0 here
	(
		addModifier newMesh skinMod
		for i = 1 to numBones do
		(
			if i != numBones then
				skinOps.addBone skinMod MaxBones[i] 0
			else
				skinOps.addBone skinMod MaxBones[i] 1
		)
		-- In Max 2013 the bone IDs are scrambled, so we look them up
		-- by bone's name and stores them in a table.
		local numSkinBones = skinOps.GetNumberBones skinMod
		-- iterate all bones in the Max (could be more than in a mesh)
		for i = 1 to numSkinBones do
		(
			local boneName = skinOps.GetBoneName skinMod i 0
			-- compare with mesh bones by name
			for j = 1 to numBones do
			(
				if boneName == MeshBones[j].Name then
				(
					boneIDMap[j] = i
--					format "MaxID[%]: %, OriginalID: %\n" i boneName j
					j = numBones + 1 -- break the loop (faster than 'exit')
				)
			)
		)
	)

	axProfilePoint "Preparing skin" debug:true

	if skelOnly then
	(
		delete newMesh		-- non-optimal way, may skip mesh creation
		return undefined
	)

--	redrawViews()

	modPanel.setCurrentObject skinMod 	-- this operation takes a lot of time, and there's no way to move it or optimize
	axProfilePoint "Selecting skin object" debug:true

	-- setup vertex influences (weights)

/*	-- verify if influences are sorted by vertex: sorting takes some time, so try to avoid it
	local sorted = true
	for i = 2 to numInfluences while sorted do
	(
		vert = Infs[i].PointIndex
		if (vert < Infs[i-1].PointIndex) then
		(
			sorted = false
			format "Sorting broken at index % point %\n" i nextPoint
		)
	)
	axProfilePoint "Verify sort" debug:true
	-- sort if required
	if not sorted then */
	(
		qsort Infs InfluenceSort
	)

	axProfilePoint "Sorting influences" debug:true

	-- build vertex to influence map
	vertInfStart = #()
	vertInfNum   = #()
	vertInfStart[numVerts] = 0		-- preallocate
	vertInfNum[numVerts]   = 0		-- ...
	count = 0
	for i = 1 to numInfluences do
	(
		v     = Infs[i]
		vert  = v.PointIndex+1
		count += 1
		if (i == numInfluences) or (Infs[i+1].PointIndex+1 != vert) then
		(
			-- flush
			vertInfStart[vert] = i - count + 1
			vertInfNum[vert]   = count
			count = 0
		)
	)

	axProfilePoint "Prepare influences" debug:true

--	progressStart "Setting weights ..." -- shouldn't call progress functions, causes crash in script
	disableSceneRedraw()
	numRepairedVerts = 0
	numBadVerts = 0
	try
	(

		for wedge = 1 to numWedges do
		(
			vert    = Wedges[wedge].PointIndex+1
			start   = vertInfStart[vert]
			numInfs = vertInfNum[vert]
			if numInfs == undefined then
			(
				numInfs = 0
				format "Vertex % (wedge %) has no weights\n" (vert-1) (wedge-1)
			)

/*
			-- This code uses SetVertexWeights
			oldBone = skinOps.GetVertexWeightBoneID skinMod wedge 1
			numWeights = skinOps.GetVertexWeightCount skinMod wedge
			if numWeights > 1 then
			(
				skinOps.ReplaceVertexWeights skinMod wedge oldBone 1
			)
			for i = 1 to numInfs do
			(
				v = Infs[start + i - 1]
				b = boneIDMap[v.BoneIndex+1]
--				format "Inf %(%) % : %\n" wedge vert MeshBones[b].Name v.Weight
				skinOps.SetVertexWeights skinMod wedge b v.Weight
				if b == oldBone then
				(
					oldBone = -1
				)
			)
			if oldBone > 0 then
			(
				skinOps.SetVertexWeights skinMod wedge oldBone 0
			)
*/
			-- This code uses ReplaceVertexWeights with arrays, a few times slower;
			-- it is still here because of bugs with SetVertexWeights path (SetVertexWeights
			-- sometimes adds influences using its own tricky logic)
			infBones   = #()
			infWeights = #()
			for i = 1 to numInfs do
			(
				v = Infs[start + i - 1]
				append infBones   boneIDMap[v.BoneIndex + 1]
				append infWeights v.Weight
			)
			skinOps.ReplaceVertexWeights skinMod wedge infBones infWeights
			-- NOTE: older Max versions after ReplaceVertexWeights call performed reset of infBones and
			-- infWeights arrays, so we wasn't able to reuse them. At least Max 2015 doesn't do that.

			-- Check is weights were set correctly
			numWeights = skinOps.GetVertexWeightCount skinMod wedge
			if numWeights != numInfs then
			(
				-- We've tried to set weights for this vertex, but MaxScript decided to keep
				-- other bones as dependency (bug in ReplaceVertexWeights). Try to repair:
				-- enumerate all current weights and set unwanted bone weights to 0 explicitly.
				-- Note: it looks like this is not an issue for Max 2014, it appears in 2015:
				-- https://trello.com/c/76npwkAY/115-possible-bug-with-importer-on-max-2015
--				format "Bad vertex: % bones(%) but %\n" wedge numInfs numWeights
				for w = 1 to numWeights do
				(
					bone = skinOps.GetVertexWeightBoneID skinMod wedge w
					found = findItem infBones bone
					if found == 0 then
					(
						append infBones bone
						append infWeights 0
					)
				)
				skinOps.ReplaceVertexWeights skinMod wedge infBones infWeights
				numWeights = skinOps.GetVertexWeightCount skinMod wedge
				if numWeights != numInfs then
				(
--					format "Bad vertex: %: bones(%) weights(%)\n" wedge infBones infWeights
					numBadVerts += 1
				)
				else
				(
					numRepairedVerts += 1
				)
			)
--			progressUpdate (100.0 * wedge / numWedges)
		)
	)
	catch
	(
		enableSceneRedraw()
--		progressEnd()
		throw()
	)
	enableSceneRedraw()

	axProfilePoint "Import influences" debug:true

--	progressEnd()
	if (numRepairedVerts > 0) or (numBadVerts > 0) then
	(
		format "Problems during skinning: % bad vertices, % repaired vertices\n" numBadVerts numRepairedVerts
	)

	-- apply mesh rotation
	if numBones >= 1 then
	(
		MaxBones[1].transform = MaxBones[1].transform * axGetRootMatrix()
	)

	axProfilePoint "Mesh import"

	gc()
)


-------------------------------------------------------------------------------
--	Loading PSA file
-------------------------------------------------------------------------------

fn FindPsaTrackIndex Anims Name =
(
	local notfound = true, res = -1
	for i = 1 to Anims.count while notfound do
	(
		if Anims[i].Name == Name then
		(
			res = i
			notfound = false
		)
	)
	res
)

fn FindPsaBoneIndex Bones Name =
(
	local notfound = true, res = -1
	for i = 1 to Bones.count while notfound do
	(
		if Bones[i].Name == Name then
		(
			res = i
			notfound = false
		)
	)
	res
)

-- UseAnimTranslation[] is array of flags signalling that particular bone should use translation
-- from the animation; when value is set to false, mesh translation will be used
fn LoadPsaConfig filename Anims Bones UseAnimTranslation AnimFlags =
(
	-- allocate and initialize UseAnimTranslation array
	UseAnimTranslation[Bones.count] = true			-- preallocate
	for i = 1 to Bones.count do UseAnimTranslation[i] = true
	-- root bone is always translated, start with index 2 below

	case g_animTransMode of
	(
--	1: - use from AnimSet, do nothing here
	2:	(
			for i = 2 to Bones.count do UseAnimTranslation[i] = false
			return undefined
		)
	3:	(
			for i = 2 to Bones.count do UseAnimTranslation[i] = true	-- old behaviour - everything will be taken from the animation
			return undefined
		)
	)

	-- read configuration file
	local cfgFile = openFile filename
	if cfgFile == undefined then return undefined

	local mode = 0

	while eof cfgFile == false do
	(
		local line = readline cfgFile

		-- process directove
		case line of
		(
		"": continue		-- empty line
		"[AnimSet]": ( mode = 1; continue )
		"[UseTranslationBoneNames]": ( mode = 2; continue )
		"[ForceMeshTranslationBoneNames]": ( mode = 3; continue )
		"[RemoveTracks]":
			(
				mode = 4
				-- allocate AnimFlags array, usually not required (currently used for UC2 animations only)
				local numKeys = Anims.count * Bones.count
				AnimFlags[numKeys] = 0		-- preallocate
				for i = 1 to numKeys do AnimFlags[i] = 0
				continue
			)
		)

		-- process ordinary line
		case mode of
		(
		0:	ErrorMessage("unexpected \"" + line + "\"")

		-- AnimSet
		1:	(
				--!! ugly parsing ... but no other params yet
				if line == "bAnimRotationOnly=1" then
				(
					for i = 2 to Bones.count do UseAnimTranslation[i] = false
				)
				else if line == "bAnimRotationOnly=0" then
				(
					-- already set to true
				)
				else
				(
					ErrorMessage("unexpected AnimSet instruction \"" + line + "\"")
				)
			)

		-- UseTranslationBoneNames - use translation from animation, useful with bAnimRotationOnly=true only
		2:	(
				local BoneIndex = FindPsaBoneIndex Bones line
				if BoneIndex > 0 then
				(
					UseAnimTranslation[BoneIndex] = true
				)
				else
				(
					format "WARNING: UseTranslationBoneNames has specified unknown bone \"%\"\n" line
				)
			)

		-- ForceMeshTranslationBoneNames - use translation from mesh
		3:	(
				local BoneIndex = FindPsaBoneIndex Bones line
				if BoneIndex > 0 then
				(
					UseAnimTranslation[BoneIndex] = false
				)
				else
				(
					format "WARNING: ForceMeshTranslationBoneNames has specified unknown bone \"%\"\n" line
				)
			)

		-- RemoveTracks
		4:	(
				-- line is in format "SequenceName.BoneIndex=[trans|rot|all]"
				local tok1 = filterString line "="			-- [1] = SequenceName.BoneIndex, [2] = Flags
				local tok2 = filterString tok1[1] "."		-- [1] = SequenceName, [2] = BoneIndex
				local SeqName    = TrimSpaces(tok2[1])
				local BoneIdxStr = TrimSpaces(tok2[2])
				local Flag       = TrimSpaces(tok1[2])
				local SeqIdx = FindPsaTrackIndex Anims SeqName		--?? can cache this value
				if SeqIdx <= 0 then ErrorMessage("Animation \"" + SeqName + "\" does not exists" + "\nline:" + line)
				FlagIndex = (SeqIdx - 1) * Bones.count + (BoneIdxStr as integer) + 1
				if Flag == "trans" then
				(
					AnimFlags[FlagIndex] = 1	-- NO_TRANSLATION
				)
				else if Flag == "rot" then
				(
					AnimFlags[FlagIndex] = 2	-- NO_ROTATION
				)
				else if Flag == "all" then
				(
					AnimFlags[FlagIndex] = 3	-- NO_TRANSLATION | NO_ROTATION
				)
				else
				(
					ErrorMessage("unknown RemoveTracks flag \"" + Flag + "\"")
				)
			)

		default:
			ErrorMessage("unexpected config error")
		)
	)
	close cfgFile
)


fn ImportPsaFile filename trackNum all:false =
(
	local Bones     = #()
	      Anims     = #()

	local UseAnimTranslation = #()
	local AnimFlags          = #()

	local numBones  = 0
	local numAnims  = 0

	local keyPos = 0

	--------- Read the file ---------

	try
	(
		file = fopen filename "rb"
		if file == undefined then return undefined

		-- First header --
		hdr = ReadChunkHeader file
		if (hdr.ChunkID != "ANIMHEAD") then
		(
			ErrorMessage("Bad chunk header: \"" + hdr.ChunkID + "\"")
		)

		while not IsEndOfFile(file) do
		(
			hdr = ReadChunkHeader file
--			format "Chunk: % (% items, % bytes/item, pos %)\n" hdr.ChunkID hdr.DataCount hdr.DataSize (ftell file)
			case hdr.ChunkID of
			(
			-- Bone links --
			"BONENAMES":
				(
					numBones = hdr.DataCount
					if numBones > 0 then Bones[numBones] = VBone ()		-- preallocate
					for i = 1 to numBones do Bones[i] = ReadVBone file
				)

			-- Animation sequence info --
			"ANIMINFO":
				(
					numAnims = hdr.DataCount
					if numAnims > 0 then Anims[numAnims] = AnimInfoBinary ()	-- preallocate
					for i = 1 to numAnims do Anims[i] = ReadAnimInfoBinary file

					if trackNum < 0 then
					(
						-- information only
						fclose file
						return undefined
					)
				)

			-- Key data --
			"ANIMKEYS":
				(
					-- determine chunk of the file to load later
					if all then trackNum = 1
					keyPos = ftell file
					for i = 1 to trackNum - 1 do
						keyPos += Anims[i].NumRawFrames * numBones * 32
					if all then
						numFrames = hdr.DataCount / Bones.count
					else
						numFrames = Anims[trackNum].NumRawFrames
					-- skip this chunk
					fseek file (hdr.DataSize * hdr.DataCount) #seek_cur
				)

			default:
				(
					-- skip unknown chunk
					format "Unknown chunk header: \"%\" at %\n" hdr.ChunkID (ftell file)
					fseek file (hdr.DataSize * hdr.DataCount) #seek_cur
				)
			)
		)
	)
	catch
	(
		fclose file
		messageBox ("Error loading file " + filename)
		format "FATAL ERROR: %\n" (getCurrentException())
		throw()
		return undefined
	)

	if numBones < 1 then
	(
		format "Animations has no bones\n"
		return undefined
	)

	if keyPos == 0 then
	(
		format "No ANIMKEYS chunk was found\n"
		return undefined
	)

	-- find existing scene bones
	MaxBones     = #()
	BindPoseInfo = #()
	SceneBones = FindAllBones()
	for i = 1 to numBones do
	(
		boneName = Bones[i].Name
		local notfound = true
		for j = 1 to SceneBones.count while notfound do
		(
			b = SceneBones[j]
			if (stricmp b.name boneName) == 0 then
			(
				MaxBones[i]     = b
				BindPoseInfo[i] = custAttributes.get b AXBoneCustomDataDef	-- could be 'undefined'
				notfound        = false
			)
		)

		if notfound then
		(
			format "WARNING: cannot find bone %\n" boneName
		)
		else if BindPoseInfo[i] == undefined then
		(
			format "WARNING: cannot get bind pose information for bone %\n" boneName
		)
	)

	-- verify for found root bone
	if MaxBones[1] == undefined then
	(
		messageBox ("WARNING: Unable to find root bone \"" + Bones[1].Name + "\"\nAnimation may appear incorrectly!")
	)

	set coordsys world
	startframe = 0	-- can modify layer ...
	if g_animAtSlider then
	(
		startframe = sliderTime
	)
	else
	(
		RemoveAnimation()
	)

	LoadPsaConfig ( (getFilenamePath filename) + (getFilenameFile filename) + ".config" ) Anims Bones UseAnimTranslation AnimFlags

/*
	format "[% trans % flags]\n" UseAnimTranslation.count AnimFlags.count
	for i = 1 to UseAnimTranslation.count do
	(
		if UseAnimTranslation[i] then format "trans: % %\n" i Bones[i].Name
	)
*/

	format "Loading track % (%), % keys\n" trackNum Anims[trackNum].Name (numFrames * Bones.count)
	firstFrame = #()
	firstFlag  = (trackNum - 1) * numBones + 1
	flagCount  = AnimFlags.count
	fseek file keyPos #seek_set							-- seek to animation keys

	animate on
	(
		progressStart "Loading animation ..."
		for i = 1 to numFrames do
		(
			at time (startframe + i - 1)
			(
				flagIndex = firstFlag
				for b = 1 to Bones.count do
				(
					-- get key
					k = ReadVQuatAnimKey file				-- read key from file
					-- get bones
					bone     = MaxBones[b]					-- scene bone to transform
					BindPose = BindPoseInfo[b]				-- for BindPose transform
					-- get animation flags
					flag = 0
					if flagIndex < flagCount then flag = AnimFlags[flagIndex]
					flagIndex = flagIndex + 1
					-- when either scene or mesh bone is missing, skip everything (key was already read)
					if bone == undefined then continue

					local mat
					if BindPose != undefined then
					(
						-- rotation
						if (bit.and flag 2) != 0 then		-- NO_ROTATION
						(
							-- rotation from mesh
							mat = BindPose.AX_RelMatrix
						)
						else
						(
							-- rotation from animation
							q = k.Orientation
							if ((b == 1) and not g_dontConjugateRoot) then q = conjugate q
							mat = (q as matrix3)
						)
						-- translation
						if (bit.and flag 1) != 0 then		-- NO_TRANSLATION
						(
							-- translation from the mesh
							mat.row4 = BindPose.AX_RelMatrix.row4 * g_meshScale
						)
						else if not UseAnimTranslation[b] then
						(
							-- translation from the mesh
							mat.row4 = BindPose.AX_RelMatrix.row4 * g_meshScale
						)
						else
						(
							-- translation from animation
							mat.row4 = k.Position * g_meshScale
						)
					)
					else
					(
						-- the BindPose object doesn't exists, use all data from the animation
						q = k.Orientation					-- rotation from animation
						p = k.Position * g_meshScale		-- translation from animation
						-- build matrix
						if ((b == 1) and not g_dontConjugateRoot) then q = conjugate q
						-- build matrix
						mat = (q as matrix3)
						mat.row4 = p
					)

					-- modify bone
					if bone.parent != undefined then
					(
						bone.transform = mat * bone.parent.transform
					)
					else
					(
						bone.transform = mat
					)
					-- remember 1st frame
					if (i == 1) then firstFrame[b] = bone.transform
				)
				-- rotate animation
				if MaxBones[1] != undefined then
				(
					MaxBones[1].transform = MaxBones[1].transform * axGetRootMatrix()
				)
			)
			-- progress bar
			progressUpdate (100.0 * i / numFrames)
			if getProgressCancel() then exit
		)
		if g_fixLooping then
		(
			-- Add extra 2 frames for correct TCB controller work.
			-- The second frame is not necessary if there is no keys after last frame
			-- (may purge all keys before animation loading instead of adding 2nd key)
			for i = 0 to 1 do
			(
				at time (startframe + numFrames + i)
				for b = 1 to Bones.count do
				(
					bone = MaxBones[b]
					if bone != undefined then
					(
						bone.transform = firstFrame[b]
					)
				)
			)
		)
		progressEnd()
	)

	-- finish loading
	fclose file

	sliderTime = 1
	extraFrame = 0
	if g_fixLooping then extraFrame = 1

	if g_updateTime then
	(
		ar_start = startframe
		ar_end   = startframe + numFrames - 1 + extraFrame
	)
	else
	(
		ar_start = animationRange.start.frame
		ar_end   = animationRange.end.frame
		if animationRange.start.frame > startframe then
			ar_start = startframe
		if animationRange.end.frame < startframe + numFrames + extraFrame then
			ar_end   = startframe + numFrames - 1 + extraFrame
	)
	if (ar_end == ar_start) then ar_end = ar_end + 1 -- avoid zero-length intervals

	animationRange = interval ar_start ar_end
	sliderTime     = startframe
--	frameRate      = track.AnimRate

	if g_playAnim then playAnimation immediateReturn:true

	gc()
)


-------------------------------------------------------------------------------
--	User interface
-------------------------------------------------------------------------------

-- layout
global axRolloutList
global axRolloutStates
global g_axScrollPos

fn axStoreLayout roll =
(
	if axRolloutStates == undefined then axRolloutStates = #()
	for i = 1 to axRolloutList.count do
	(
		axRolloutStates[i] = axRolloutList[i].open
	)
	-- sometimes 'roll' is non-null, but it's property 'scrollPos' is inaccessible
	if roll.scrollPos != undefined then g_axScrollPos = roll.scrollPos
)


fn axRestoreLayout roll =
(
	if axRolloutStates != undefined then
	(
		for i = 1 to axRolloutList.count do
		(
			axRolloutList[i].open = axRolloutStates[i]
		)
	)
	-- when execing first time, layout will not be stored, and g_axScrollPos will be undefined
	if g_axScrollPos != undefined then roll.scrollPos = g_axScrollPos
)


global MeshFileName
global AnimFileName

fn axLoadAnimation index =
(
	if (index > 0) and (index <= Anims.count) then ImportPsaFile AnimFileName index
)


rollout axInfoRollout "ActorX Importer"
(
	-- copyright label
	label     Lbl1 "Version 1.38"
	label     Lbl2 "\xA9 2009-2020 Konstantin Nosov (Gildor)"
	hyperlink Lbl3 "http://www.gildor.org/" \
					address:"http://www.gildor.org/projects/unactorx" align:#center \
					color:black hovercolor:blue visitedcolor:black

	on axInfoRollout close do
	(
		format "Saving settings ...\n"
		axSerializeSettings false
		axStoreLayout axInfoRollout
	)
)


rollout axMeshImportRollout "Mesh Import"
(
	checkbox ChkSeeThru    "See-Thru Mesh" checked:g_seeThru
	checkbox ChkSkelOnly   "Load skeleton only" checked:g_skelOnly
	button   BtnImportPsk  "Import PSK ..."

	-- event handlers

	on ChkSeeThru    changed state do g_seeThru    = state
	on ChkSkelOnly   changed state do g_skelOnly   = state

	on BtnImportPsk pressed do
	(
		if DotNetObject == undefined then
		(
			-- older Max didn't have functionality for getMultiOpenFilenames
			local filename = getOpenFileName types:"ActorX Mesh (*.psk,*.pskx)|*.psk;*.pskx|All (*.*)|*.*|" filename:g_lastDir1
			if filename != undefined then
			(
				MeshFileName = filename
				g_lastDir1 = getFilenamePath MeshFileName
				if DoesFileExist MeshFileName then ImportPskFile MeshFileName skelOnly:g_skelOnly
			)
		)
		else
		(
			local filenames = getMultiOpenFilenames types:"ActorX Mesh (*.psk,*.pskx)|*.psk;*.pskx|All (*.*)|*.*" filename:g_lastDir1
			if filenames != undefined then
			(
				for filename in filenames do
				(
					MeshFileName = filename
					g_lastDir1 = getFilenamePath MeshFileName
					if DoesFileExist MeshFileName then ImportPskFile MeshFileName skelOnly:g_skelOnly
				)
			)
		)
	)
)


rollout axAnimImportRollout "Animation Import"
(
	Group "Animation Import"
	(
		button   BtnImportPsa  "Import PSA ..."
		listbox  LstAnims      "Animations:"     height:13
		checkbox ChkAnimTime   "Update animation length" checked:g_updateTime
		checkbox ChkFixLooping "Fix loop animation" checked:g_fixLooping tooltip:"Append 1st keyframe to animation\ntrack for smooth loop"
		checkbox ChkPlayAnim   "Play animation" checked:g_playAnim
		checkbox ChkAtSlider   "Import at slider position" checked:g_animAtSlider
		dropdownlist LstTransMode "Translation mode" items:#("Use from AnimSet", "Force mesh translation", "Force AnimSet translation") selection:g_animTransMode
		button   BtnImportTrk  "Load track" across:2
		button   BtnImportAll  "Load all" tooltip:"Load all animations as a single track"
	)

	-- event handlers

	on BtnImportPsa pressed do
	(
		local filename = getOpenFileName types:"ActorX Animation (*.psa)|*.psa|All (*.*)|*.*|" filename:g_lastDir2

		if filename != undefined then
		(
			AnimFileName = filename
			g_lastDir2 = getFilenamePath AnimFileName
			if DoesFileExist AnimFileName then
			(
				ImportPsaFile AnimFileName -1
				LstAnims.items = for a in Anims collect (a.Name + " [" + (a.NumRawFrames as string) + "]")
			)
		)
	)

	on BtnImportTrk pressed       do axLoadAnimation LstAnims.selection
	on BtnImportAll pressed       do ImportPsaFile AnimFileName 1 all:true
	on LstAnims doubleClicked sel do axLoadAnimation sel

	on ChkAnimTime   changed state do g_updateTime    = state
	on ChkFixLooping changed state do g_fixLooping    = state
	on ChkPlayAnim   changed state do g_playAnim      = state
	on ChkAtSlider   changed state do g_animAtSlider  = state
	on LstTransMode  selected mode do g_animTransMode = mode

	on axAnimImportRollout open do
	(
		-- fill LstAnims
		LstAnims.items = for a in Anims collect (a.Name + " [" + (a.NumRawFrames as string) + "]")
	)
)


rollout axTexturesRollout "Materials"
(
	edittext EdTexPath     "Path to materials" text:g_texDir width:180 across:2
	button   BtnBrowseTex  "..."     align:#right height:16
	checkbox ChkTexRecurse "Look in subfolders" checked:g_texRecurse
	label    LblMissingTex "On missing texture:" across:2
	radiobuttons RadMissingTex labels:#("do nothing", "ask") default:g_texMissAction align:#left columns:1

	on EdTexPath    changed val do g_texDir = val
	on BtnBrowseTex pressed do
	(
		dir = getSavePath caption:"Directory for texture lookup" initialDir:g_texDir
		if dir != undefined then
		(
			g_texDir       = dir
			EdTexPath.text = dir
		)
	)

	on ChkTexRecurse changed state do g_texRecurse    = state
	on RadMissingTex changed state do g_texMissAction = state
)


rollout axToolsRollout "Tools"
(
	button BtnReset "Reset to defaults" width:180

	button BtnRestoreBindpose "Restore BindPose" width:180
	button BtnRemoveAnimation "Remove animation" width:180
	button BtnClearScene      "Clear scene"      width:180
	button BtnBatchExport     "Batch export"     width:180
	button BtnReloadScript    "Reload importer"  width:180

	on BtnReset pressed do
	(
		if configFile != undefined then deleteFile configFile
		axDefaultSettings()
		-- reset controls
		axShowUI()
	)
	on BtnRestoreBindpose pressed do RestoreBindpose()
	on BtnRemoveAnimation pressed do RemoveAnimation()
	on BtnClearScene      pressed do ClearMaxScene()
	on BtnBatchExport     pressed do fileIn "export_fbx.ms"
	on BtnReloadScript    pressed do
	(
		if getSourceFileName != undefined then	-- checking Max version (Max9+) ...
		(
			axStoreLayout axInfoRollout
			fileIn(getSourceFileName())
		)
	)
)


rollout axSettingsRollout "Mesh Settings"
(
	spinner SpnBoneSize  "Bone size"  range:[0.1,10,g_boneSize]     type:#float scale:0.1  align:#left  across:2
	spinner SpnMeshScale "Mesh scale" range:[0.01,1000,g_meshScale] type:#float scale:0.01 align:#right
	checkbox ChkRepBones "Reposition existing bones" checked:g_reposBones

	group "Mesh rotation"
	(
		spinner  SpnRY "Yaw"   range:[-180,180,g_rotY] type:#integer scale:90 fieldwidth:35 align:#left across:3
		spinner  SpnRP "Pitch" range:[-180,180,g_rotP] type:#integer scale:90 fieldwidth:35
		spinner  SpnRR "Roll"  range:[-180,180,g_rotR] type:#integer scale:90 fieldwidth:35 align:#right
		button   BtnRotMaya    "Maya" across:3
		button   BtnRotReset   "Reset"
		button   BtnRotApply   "Apply"
	)

	group "Mesh offset"
	(
		spinner  SpnTX "X"     range:[-10000,10000,g_transX] type:#float scale:0.01 fieldwidth:50 align:#left across:3
		spinner  SpnTY "Y"     range:[-10000,10000,g_transY] type:#float scale:0.01 fieldwidth:50
		spinner  SpnTZ "Z"     range:[-10000,10000,g_transZ] type:#float scale:0.01 fieldwidth:50 align:#right
	)

	-- event handlers
	on SpnBoneSize  changed val do g_boneSize  = val
	on SpnMeshScale changed val do g_meshScale = val
	on ChkRepBones  changed state do g_reposBones = state

	on SpnRY changed val do g_rotY = val
	on SpnRP changed val do g_rotP = val
	on SpnRR changed val do g_rotR = val
	on SpnTX changed val do g_transX = val
	on SpnTY changed val do g_transY = val
	on SpnTZ changed val do g_transZ = val

	on BtnRotMaya pressed do
	(
		g_rotY = SpnRY.value = -90
		g_rotP = SpnRP.value =   0
		g_rotR = SpnRR.value =  90
		RestoreBindpose()
	)
	on BtnRotReset pressed do
	(
		g_rotY = SpnRY.value = 0
		g_rotP = SpnRP.value = 0
		g_rotR = SpnRR.value = 0
		RestoreBindpose()
	)
	on BtnRotApply pressed do RestoreBindpose()
)


rollout axAdvSettingsRollout "Advanced Settings"
(
	label Lbl1                "WARNING: do not modify these settings"
	label Lbl2                "unless you know what you are doing!"
	checkbox ChkReorientBones "Reorient bones" checked:g_reorientBones
	checkbox ChkDontConjRoot  "Don't conjugate root bone" checked:g_dontConjugateRoot

	-- event handlers
	on ChkReorientBones changed state do g_reorientBones = state
	on ChkDontConjRoot changed state do g_dontConjugateRoot = state
)


global axImportFloater

fn axShowUI =
(
	-- request position of previous window, if it was already opened
	local x = 30
	local y = 100
	local w = 250
	local h = 700
	if axImportFloater != undefined then
	(
		x = axImportFloater.pos.x
		y = axImportFloater.pos.y
		w = axImportFloater.size.x
		h = axImportFloater.size.y
		-- close old window
		closeRolloutFloater axImportFloater
	)
	-- Create plugin window
	axImportFloater = newRolloutFloater "ActorX Import" w h x y				-- create a new window

	-- init axRolloutList
	axRolloutList = #(axInfoRollout, axMeshImportRollout, axAnimImportRollout, axTexturesRollout, axToolsRollout, axSettingsRollout, axAdvSettingsRollout)

	-- add controls
	for i = 1 to axRolloutList.count do
	(
		addRollout axRolloutList[i] axImportFloater
	)

	axRestoreLayout axInfoRollout
)


-------------------------------------------------------------------------------
--	Plugin startup
-------------------------------------------------------------------------------

global g_axImporterVersion

if (g_axImporterVersion == undefined) then
(
	-- initialize plugin
	heapSize += 33554432	-- 32 Mb; will speedup most tasks
	Anims     = #()
	g_axImporterVersion = AX_IMPORTER_VERSION
	axDefaultSettings()
	axSerializeSettings(true)

	if getSourceFileName != undefined then	-- checking Max version (Max9+) ...
	(
		-- Add action handler (macro script).
		-- Max will copy contents of macroScript() block to the "AppData/Local/Autodesk/3dsMax/*/ENU/usermacros".
		-- To avoid copying of entire file we're generating string which will simply execute THIS file.
		str = "macroScript GildorTools_ActorXImporter category:\"Gildor Tools\" buttontext:\"ActorX Importer\" tooltip:\"ActorX Importer\"\n" \
			+ "(\n" \
			+ "    fileIn \"" + getSourceFileName() + "\"\n" \
			+ ")\n"
		execute str
	)
)


if (g_axImporterVersion != AX_IMPORTER_VERSION) then
(
	format "ActorX Importer has been updated while 3ds Max is running.\nReloading settings.\n"
	-- copy-paste of code above
	g_axImporterVersion = AX_IMPORTER_VERSION
	axDefaultSettings()
	axSerializeSettings(true)
)

axShowUI()
