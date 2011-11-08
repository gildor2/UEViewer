/*

 ActorX mesh (psk) and animation (psa) importer for 3ds Max

 Created:	September 18 2009

 Author:	Konstantin Nosov (aka Gildor)

 Web page:	http://www.gildor.org/projects/unactorx

 Revision History:

	??.11.2011 v1.18
	- ???

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
- Menu item for plugin
- try to not use setBoneEnable false ...
*/

global AX_IMPORTER_VERSION = 18

-------------------------------------------------------------------------------
--	Global variables
-------------------------------------------------------------------------------

global g_seeThru
global g_skelOnly
global g_updateTime
global g_playAnim
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
global g_meshScale
global g_reorientBones
global Anims
global MeshBones


-------------------------------------------------------------------------------
--	Default settings
-------------------------------------------------------------------------------

fn DefaultSettings =
(
	-- defaults settings
	g_seeThru    = false
	g_skelOnly   = false
	g_updateTime = true
	g_playAnim   = false
	g_fixLooping = false
	g_lastDir1   = ""
	g_lastDir2   = ""
	g_texDir     = ""
	g_texRecurse = false
	g_texMissAction = 1
	g_boneSize   = 0.5
	g_reposBones = true
	g_rotY       = 0
	g_rotP       = 0
	g_rotR       = 0
	g_meshScale  = 1.0
	g_reorientBones = false
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


fn GetSetting name default =
(
	local ret = getINISetting configFile "Main" name
	if ret == "" then ret = (default as string)
	ret
)

fn SetSetting name value =
(
	setINISetting configFile "Main" name (value as string)
)


fn GetSettingBool  name default = (GetSetting name default) as BooleanClass
fn GetSettingFloat name default = (GetSetting name default) as float


tmp_v  = undefined		-- global variable, helper for DoSetting() (required for execute() ...)
g_isLoading = true		-- DoSetting() mode

fn DoSetting name var =
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


fn SerializeSettings isLoading =
(
	if isLoading then
	(
		if configFile == undefined then return undefined
		if not doesFileExist configFile then return undefined	-- no config file
	)
	g_isLoading = isLoading
	-- read/write settings
	DoSetting "LastUsedDir"   "g_lastDir1"
	DoSetting "LastUsedDir2"  "g_lastDir2"
	DoSetting "TexturesDir"   "g_texDir"
	DoSetting "TexRecurse"    "g_texRecurse"
	DoSetting "TexMissAction" "g_texMissAction"
	DoSetting "AutoPlayAnim"  "g_playAnim"
	DoSetting "UpdateTime"    "g_updateTime"
	DoSetting "FixLoopAnim"   "g_fixLooping"
	DoSetting "SeeThru"       "g_seeThru"
	DoSetting "SkelOnly"      "g_skelOnly"
	DoSetting "BoneSize"      "g_boneSize"
	DoSetting "ReposBones"    "g_reposBones"
	DoSetting "MeshYaw"       "g_rotY"
	DoSetting "MeshPitch"     "g_rotP"
	DoSetting "MeshRoll"      "g_rotR"
	DoSetting "MeshScale"     "g_meshScale"
	DoSetting "ReorientBones" "g_reorientBones"
)


global g_axImporterInited
global g_axImporterVersion
if (g_axImporterInited != true) then
(
	-- initialize plugin
	g_axImporterInited = true
	g_axImporterVersion = AX_IMPORTER_VERSION
	heapSize += 33554432	-- 32 Mb; will speedup most tasks
	Anims     = #()
	MeshBones = #()
	DefaultSettings()
	SerializeSettings(true)
)


if (g_axImporterVersion != AX_IMPORTER_VERSION) then
(
	format "ActorX Importer were updated while 3ds Max is running.\nReloading config settings.\n"
	--?? used twice, make a separate function ?
	g_axImporterVersion = AX_IMPORTER_VERSION
	DefaultSettings()
	SerializeSettings(true)
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


fn IsEndOfFile bstream =
(
	local savePos = ftell bstream
	fseek bstream 0 #seek_end			-- compute file size
	local fileSize = ftell bstream
	fseek bstream savePos #seek_set
	return (savePos >= fileSize)
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
		if finished then continue		-- has end of line before - skip remaining chars
		-- not "finished" string
		str += bit.intAsChar(c)			-- append a character
		if c != 32 then length = i		-- position of last non-space char
	)
	substring str 1 length				-- return first "length" chars
)

fn ReadVector2 bstream =
(
	local v = [ 0, 0 ]
	v.x = ReadFloat bstream
	v.y = ReadFloat bstream
	v
)

fn ReadFVector bstream =
(
	local v = [ 0, 0, 0 ]
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
fn FindFirstChild boneArray boneIndex =
(
	for i = 1 to boneArray.count do
	(
		if (i != boneIndex) then
		(
			bn = boneArray[i]
			if bn.ParentIndex == boneIndex-1 then return bn
		)
	)
	return undefined
)


fn FixBoneNames boneArray =
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


fn FindFile path filename recurse:false =
(
	local check = path + "\\" + filename
	if doesFileExist check then return check
	if recurse then
	(
		local dirs = getDirectories (path + "/*")
		for dir in dirs do
		(
			local ret = FindFile dir filename recurse:true
			if ret != undefined then return ret
		)
	)
	return undefined
)


fn GetRotationMatrix =
(
	local angles = eulerAngles g_rotR -g_rotP -g_rotY
	return angles as matrix3
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
	local cmp = v1.PointIndex - v2.PointIndex
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
--	Loading materials
-------------------------------------------------------------------------------

fn ImportMaterial matName texDir =
(
	local subMat = standardMaterial name:matName

	local texFilename
	local foundTex

	-- try to file material file
	texFilename = matName + ".mat"
	foundTex = FindFile texDir texFilename recurse:g_texRecurse
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
			foundTex = FindFile texDir (file + ".tga") recurse:g_texRecurse
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
	texFilename = matName + ".tga"
	foundTex = FindFile texDir texFilename recurse:g_texRecurse
	if foundTex != undefined then
	(
		texFilename = foundTex
	)
	else
	(
		if g_texMissAction == 2 then			-- ask
		(
			local check = getOpenFileName caption:("Get texture for material " + matName) \
				types:"TGA files (*.tga)|*.tga|All (*.*)|*.*|" filename:texFilename
			if check != undefined then texFilename = check
		)
	)
	if not doesFileExist texFilename then format "Unable to find texture %\n" texFilename
	-- continue setup
	local bitmap = bitmapTexture name:texFilename fileName:texFilename
	subMat.diffuseMap = bitmap
	-- return
	subMat
)

-------------------------------------------------------------------------------
--	MAX helpers
-------------------------------------------------------------------------------

fn RemoveAnimation =
(
	stopAnimation()
	for i = 1 to MeshBones.count do
	(
		local Bone = getNodeByName MeshBones[i].Name exact:true ignoreCase:false
		if Bone != undefined then deleteKeys Bone #allKeys
	)
	animationRange = interval 0 1
)


fn RestoreBindpose =
(
	RemoveAnimation()

	try
	(
		local rotMatrix = GetRotationMatrix()
		-- note: should rotate every bone because we are not applying parent's rotation here
		-- find bones
		for i = 1 to MeshBones.count do
		(
			local Bone = getNodeByName MeshBones[i].Name exact:true ignoreCase:false
			if Bone == undefined then
				format "WARNING: cannot find the bone %\n" MeshBones[i].Name
			else
				Bone.transform = MeshBones[i].Matrix * rotMatrix
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

	local OldMeshBones = MeshBones

	local Verts     = #()
	local Wedges    = #()
	local Tris      = #()
	local Materials = #()
		  MeshBones = #()		-- global for access to bind pose from outside
	local Infs      = #()

	--------- Read the file ---------

	local numVerts      = 0
	local numWedges     = 0
	local numTris       = 0
	local numMaterials  = 0
	local numBones      = 0
	local numInfluences = 0
	local numTexCoords  = 1

	local extraUV = #()

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
			case hdr.ChunkID of
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
					for i = 1 to numWedges do Wedges[i] = ReadVVertex file
				)

			-- Faces --
			"FACE0000":
				(
					numTris = hdr.DataCount
					Tris[numTris] = VTriangle ()		-- preallocate
					for i = 1 to numTris do Tris[i] = ReadVTriangle file
				)

			-- Materials --
			"MATT0000":
				(
					numMaterials = hdr.DataCount
					Materials[numMaterials] = VMaterial ()	-- preallocate
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
					FixBoneNames MeshBones
				)

			-- Weights --
			"RAWWEIGHTS":
				(
					numInfluences = hdr.DataCount
					if numInfluences > 0 then Infs[numInfluences] = VRawBoneInfluence () -- preallocate
					for i = 1 to numInfluences do Infs[i] = ReadVRawBoneInfluence file
				)

			-- pskx additional UV set
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
					format("Unknown chunk header: \"%\"\n" hdr.ChunkID)
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

	--------- File is completely read now ---------

	-- generate skeleton
	MaxBones = #()
	local rotMatrix = matrix3 1
	for i = 1 to numBones do
	(
		bn = MeshBones[i]
		-- build bone matrix
		q = bn.Orientation
		if (i == 1) then q = conjugate q
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
		childBone = FindFirstChild MeshBones i
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
		newBone = getNodeByName bn.Name exact:true ignoreCase:false
		if (newBone == undefined) then
		(
			if (g_reorientBones == false or childBone == undefined) then
			(
				newBone = bonesys.createbone	\
					  bn.Matrix.row4			\
					  (bn.Matrix.row4 + len * (normalize bn.Matrix.row1)) \
					  (normalize bn.Matrix.row3)
			)
			else
			(
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
			newBone.setBoneEnable false 0
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
	for i = 1 to numWedges do
	(
		MaxVerts[i] = Verts[Wedges[i].PointIndex + 1] * g_meshScale
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
		for i = 1 to numTris do
		(
			-- setup face vertices and material
			tri = Tris[i]
			meshop.setMapFace newMesh j i [ tri.Wedge1+1, tri.Wedge0+1, tri.Wedge2+1 ]
			setFaceMatId newMesh j (tri.MatIndex+1)
		)
	)

	newMat = multiMaterial numsubs:numMaterials
	if g_skelOnly then numMaterials = 0		-- do not load materials for this option
	for i = 1 to numMaterials do
	(
		local texDir
		if g_texDir != "" then
		(
			texDir = g_texDir
		)
		else
		(
			texDir = getFilenamePath filename
		)
		local subMat = ImportMaterial Materials[i].MaterialName texDir
		newMat.materialList[i] = subMat
		showTextureMap subMat true
--		format "Material[%] = %\n" i Materials[i].MaterialName
	)
	newMesh.material = newMat

	-- generate skin modifier
	max modify mode
	select newMesh

	skinMod = skin ()
	if numBones > 0 then
	(
		addModifier newMesh skinMod
		for i = 1 to numBones do
		(
			if i != numBones then
				skinOps.addBone skinMod MaxBones[i] 0
			else
				skinOps.addBone skinMod MaxBones[i] 1
		)
	)
	update newMesh

	if skelOnly then
	(
		delete newMesh		-- non-optimal way, may skip mesh creation
		return undefined
	)
	if numBones <= 0 then
	(
		return undefined
	)

--	redrawViews()

	modPanel.setCurrentObject skinMod

	-- setup vertex influences (weights)
	qsort Infs InfluenceSort

/*	for i = 1 to numInfluences-1 do
	(
		v1 = Infs[i]
		v2 = Infs[i+1]
		if (v1.PointIndex == v2.PointIndex) and (v1.BoneIndex == v2.BoneIndex) then
			format "Point % has multiple weights for bone %\n" v1.PointIndex MeshBones[v2.BoneIndex].Name
	) */

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

--	progressStart "Setting weights ..."
	disableSceneRedraw()
	try
	(
		for wedge = 1 to numWedges do
		(
			vert    = Wedges[wedge].PointIndex+1
			start   = vertInfStart[vert]
			numInfs = vertInfNum[vert]
/*
			-- This code uses SetVertexWeights; it is fast, but Max will automatically assign
			-- vertex weights to a nearest bone when creating mesh, and there is no simple
			-- way to erase them. SetVertexWeights will not erase extra weights.
			for i = 1 to numInfs do
			(
				v = Infs[start + i - 1]
--				format "Inf %(%) % : %\n" wedge vert MeshBones[v.BoneIndex+1].Name v.Weight
				skinOps.SetVertexWeights skinMod wedge (v.BoneIndex+1) v.Weight
			) */

			-- This code uses ReplaceVertexWeights with arrays, a few times slower;
			-- it is still here in a case of bugs with SetVertexWeights path
			infBones   = #()
			infWeights = #()
			for i = 1 to numInfs do
			(
				v = Infs[start + i - 1]
				append infBones   (v.BoneIndex + 1)
				append infWeights v.Weight
			)
--			format "W[%] V[%] % // %\n" wedge vert infBones infWeights
			skinOps.ReplaceVertexWeights skinMod wedge infBones infWeights
			-- NOTE: ReplaceVertexWeights will reset infBones and infWeights arrays, so we
			-- cannot reuse them
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
--	progressEnd()

	-- apply mesh rotation
	if numBones >= 1 then
	(
		MaxBones[1].transform = MaxBones[1].transform * GetRotationMatrix()
	)

	-- combine with old skeleton for correct work of combined meshes
	format "... processing % OldMeshBones\n" OldMeshBones.count
	for i = 1 to OldMeshBones.count do
	(
		boneName = OldMeshBones[i].Name
		found = false
		-- find bone in MeshBones
		for j = 1 to MeshBones.count do
		(
			if MeshBones[j].Name == boneName then
			(
--				format "..... found in MeshBones"
				found = true
				exit
			)
		)
		if not found then
		(
			-- bone already exists in MeshBones array
			-- find bone in Max scene
			if getNodeByName boneName exact:true ignoreCase:false != undefined then
			(
				-- bone still exists in Max scene, should add it to MeshBones array
				MeshBones[MeshBones.count+1] = OldMeshBones[i]
--				format "... restored bone from old mesh: %\n" boneName
			)
		)
	)

	gc()
)


-------------------------------------------------------------------------------
--	Loading PSA file
-------------------------------------------------------------------------------

fn FindPsaTrackIndex Anims Name =
(
	for i = 1 to Anims.count do
	(
		if Anims[i].Name == Name then return i
	)
	return -1
)

fn FindPsaBoneIndex Bones Name =
(
	for i = 1 to Bones.count do
	(
		if Bones[i].Name == Name then return i
	)
	return -1
)

fn LoadPsaConfig filename Anims Bones UseTranslation AnimFlags =
(
	local cfgFile = openFile filename
	if cfgFile == undefined then return undefined

	local mode = 0

	-- allocate and initialize UseTranslation array
	UseTranslation[Bones.count] = true			-- preallocate
	for i = 1 to Bones.count do UseTranslation[i] = true

	while eof cfgFile == false do
	(
		local line = readline cfgFile
		case line of
		(
		"": continue

		"[AnimSet]":
			(
				mode = 1
				continue
			)

		"[UseTranslationBoneNames]":
			(
				mode = 2
				continue
			)

		"[ForceMeshTranslationBoneNames]":
			(
				mode = 3
				continue
			)

		"[RemoveTracks]":
			(
				mode = 4
				-- allocate AnimFlags array, usually not required (currently used for UC2 animations only)
				local numKeys = Anims.count * Bones.count
				AnimFlags[numKeys] = 0		-- preallocate
				for i = 1 to numKeys do AnimFlags[i] = 0
				continue
			)

		default:
			(
				case mode of
				(
				0:
					(
						ErrorMessage "unexpected %" line
					)

				-- AnimSet
				1:
					(
						--!! ugly parsing ... but no other params yet
						if line == "bAnimRotationOnly=1" then
						(
							for i = 1 to Bones.count do UseTranslation[i] = false
						)
						else if line == "bAnimRotationOnly=0" then
						(
							-- already set to true
						)
						else
						(
							ErrorMessage "unexpected AnimSet instruction \"%\"" line
						)
					)

				-- UseTranslationBoneNames - use translation from animation, useful with bAnimRotationOnly=true only
				2:
					(
						local BoneIndex = FindPsaBoneIndex Bones line
						if BoneIndex > 0 then
						(
							UseTranslation[BoneIndex] = true
						)
						else
						(
							format "WARNING: UseTranslationBoneNames has specified unknown bone \"%\"\n" line
						)
					)

				-- ForceMeshTranslationBoneNames - use translation from mesh
				3:
					(
						local BoneIndex = FindPsaBoneIndex Bones line
						if BoneIndex > 0 then
						(
							UseTranslation[BoneIndex] = false
						)
						else
						(
							format "WARNING: ForceMeshTranslationBoneNames has specified unknown bone \"%\"\n" line
						)
					)

				-- RemoveTracks
				4:
					(
						--!! implement
--		local tok = filterString line " ="
--		format "[%] = [%]\n" tok[1] tok[2]
--		local parm = tok[1]
--		local file = tok[2]
					)

				default:
					ErrorMessage "unexpected config error"
				)
			)
		)
	)
	close cfgFile

	UseTranslation[1] = true		-- root bone is always translated
)


fn ImportPsaFile filename trackNum all:false =
(
	local Bones     = #()
	      Anims     = #()

	local UseTranslation = #()
	local AnimFlags      = #()

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
					format("Unknown chunk header: \"" + hdr.ChunkID + "\"\n")
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
		format "No ANIMKEYS chunk were found\n"
		return undefined
	)

	-- find existing scene bones
	MaxBones     = #()
	MeshBoneLink = #()
	for i = 1 to numBones do
	(
		boneName = Bones[i].Name
		MaxBones[i] = getNodeByName boneName exact:true ignoreCase:false
		MeshBoneLink[i] = undefined
		for j = 1 to MeshBones.count do
		(
			if MeshBones[j].Name == boneName then
			(
				MeshBoneLink[i] = MeshBones[j]
				exit
			)
		)
		if MaxBones[i]     == undefined then format "WARNING: cannot find scene bone %\n" boneName
		if MeshBoneLink[i] == undefined then format "WARNING: cannot find mesh bone %\n" boneName
	)

	-- verify for found root bone
	if MaxBones[1] == undefined then
	(
		messageBox ("WARNING: Unable to find root bone \"" + Bones[1].Name + "\"\nAnimation may appear incorrectly!")
	)

	set coordsys world
	startframe = 0	-- can modify layer ...

	RemoveAnimation()

	LoadPsaConfig ( (getFilenamePath filename) + (getFilenameFile filename) + ".config" ) Anims Bones UseTranslation AnimFlags

/*
	format "[% trans % flags]\n" UseTranslation.count AnimFlags.count	--!!!
	for i = 1 to UseTranslation.count do
	(
		if UseTranslation[i] then format "trans: % %\n" i Bones[i].Name
	)
*/

	format "Loading track % (%), % keys\n" trackNum Anims[trackNum].Name (numFrames * Bones.count)
	firstFrame = #()
	firstFlag  = (trackNum - 1) * numBones + 1
	flagCount  = AnimFlags.count
	fseek file keyPos #seek_set							-- seek to animation keys

	--!! add options:
	--!! - disable bAnimRotationOnly

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
					-- get bone from MeshBones
					MeshBone = MeshBoneLink[b]
					-- get animation flags
					flag = 0
					if (flagIndex < flagCount) and (MeshBone != undefined) then flag = AnimFlags[flagIndex]
					flagIndex = flagIndex + 1
					-- get key
					k = ReadVQuatAnimKey file			-- read key from file
					bone = MaxBones[b]

					if (bit.and flag 1) != 0 then		-- PSAX_FLAG_NO_TRANSLATION
						p = MeshBone.Position			-- translation from the mesh
					else if (UseTranslation.count > 0) and not UseTranslation[b] and (MeshBone != undefined) then
						p = MeshBone.Position			-- translation from the mesh
					else
						p = k.Position * g_meshScale	-- translation from animation

					if (bit.and flag 2) != 0 then		-- PSAX_FLAG_NO_ROTATION
						q = MeshBone.Orientation		-- from mesh
					else
						q = k.Orientation				-- from animation

					if (b == 1) then q = conjugate q
					-- build matrix
					mat = (q as matrix3)
					mat.row4 = p
					-- modify bone
					if bone == undefined then continue	-- missing bone
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
					MaxBones[1].transform = MaxBones[1].transform * GetRotationMatrix()
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
					if bone == undefined then continue	-- missing bone
					bone.transform = firstFrame[b]
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
--	frameRate      = track.AnimRate

	if g_playAnim then playAnimation immediateReturn:true

	gc()
)


-------------------------------------------------------------------------------
--	User interface
-------------------------------------------------------------------------------

global MeshFileName
global AnimFileName

fn LoadAnimation index =
(
	if (index > 0) and (index <= Anims.count) then ImportPsaFile AnimFileName index
)

rollout axImportRollout "ActorX Importer"
(
	-- copyright label
	label     Lbl1 "Version 1.18"
	label     Lbl2 "\xA9 2009-2011 Konstantin Nosov (Gildor)"
	hyperlink Lbl3 "http://www.gildor.org/" \
					address:"http://www.gildor.org/projects/unactorx" align:#center \
					color:black hovercolor:blue visitedcolor:black

	Group "Mesh Import"
	(
		checkbox ChkSeeThru    "See-Thru Mesh" checked:g_seeThru
		checkbox ChkSkelOnly   "Load skeleton only" checked:g_skelOnly
		button   BtnImportPsk  "Import PSK ..."
	)

	Group "Animation Import"
	(
		button   BtnImportPsa  "Import PSA ..."
		listbox  LstAnims      "Animations:"     height:13
		checkbox ChkAnimTime   "Update animation length" checked:g_updateTime
		checkbox ChkFixLooping "Fix loop animation" checked:g_fixLooping tooltip:"Append 1st keyframe to animation\ntrack for smooth loop"
		checkbox ChkPlayAnim   "Play animation" checked:g_playAnim
		button   BtnImportTrk  "Load track" across:2
		button   BtnImportAll  "Load all" tooltip:"Load all animations as a single track"
	)

	-- event handlers

	on BtnImportPsk pressed do
	(
		local filename = getOpenFileName types:"ActorX Mesh (*.psk,*.pskx)|*.psk;*.pskx|All (*.*)|*.*|" filename:g_lastDir1
		if filename != undefined then
		(
			MeshFileName = filename
			g_lastDir1 = getFilenamePath MeshFileName
			if DoesFileExist MeshFileName then ImportPskFile MeshFileName skelOnly:g_skelOnly
		)
	)

	on BtnImportPsa pressed do
	(
		local filename = getOpenFileName types:"ActorX Animation (*.psa,*.psax)|*.psa;*.psax|All (*.*)|*.*|" filename:g_lastDir2

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

	on BtnImportTrk pressed       do LoadAnimation LstAnims.selection
	on BtnImportAll pressed       do ImportPsaFile AnimFileName 1 all:true
	on LstAnims doubleClicked sel do LoadAnimation sel

	on ChkSeeThru    changed state do g_seeThru    = state
	on ChkSkelOnly   changed state do g_skelOnly   = state
	on ChkAnimTime   changed state do g_updateTime = state
	on ChkFixLooping changed state do g_fixLooping = state
	on ChkPlayAnim   changed state do g_playAnim   = state

	on axImportRollout open do
	(
		-- fill LstAnims
		LstAnims.items = for a in Anims collect (a.Name + " [" + (a.NumRawFrames as string) + "]")
	)

	on axImportRollout close do
	(
		SerializeSettings false
	)
)


rollout axToolsRollout "Tools"
(
	button BtnRestoreBindpose "Restore BindPose" width:180
	button BtnRemoveAnimation "Remove animation" width:180
	button BtnClearScene      "Clear scene"      width:180
	button BtnBatchExport     "Batch export"     width:180

	on BtnRestoreBindpose pressed do RestoreBindpose()
	on BtnRemoveAnimation pressed do RemoveAnimation()
	on BtnClearScene      pressed do ClearMaxScene()
	on BtnBatchExport     pressed do fileIn "export_fbx.ms"
)


rollout axSettingsRollout "Settings"
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

	group "Materials"
	(
		edittext EdTexPath     "Path to materials" text:g_texDir width:180 across:2
		button   BtnBrowseTex  "..."     align:#right height:16
		checkbox ChkTexRecurse "Recurse" checked:g_texRecurse
		label    LblMissingTex "On missing texture:" across:2
		radiobuttons RadMissingTex labels:#("do nothing", "ask") default:g_texMissAction align:#left columns:1
	)

	group "Advanced"
	(
		label Lbl1                "WARNING: do not modify these settings"
		label Lbl2                "unless you know what you are doing!"
		checkbox ChkReorientBones "Reorient bones" checked:g_reorientBones
	)

	button BtnReset "Reset to defaults"

	-- event handlers

	on SpnRY changed val do g_rotY = val
	on SpnRP changed val do g_rotP = val
	on SpnRR changed val do g_rotR = val
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

	on SpnBoneSize  changed val do g_boneSize  = val
	on SpnMeshScale changed val do g_meshScale = val
	on ChkRepBones   changed state do g_reposBones    = state
	on ChkTexRecurse changed state do g_texRecurse    = state
	on RadMissingTex changed state do g_texMissAction = state
	on ChkReorientBones changed state do g_reorientBones = state

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

	on BtnReset pressed do
	(
		if configFile != undefined then deleteFile configFile
		DefaultSettings()
		--?? find a way to reset GUI controls too
	)
)


-- Create plugin window
if axImportFloater != undefined do closeRolloutFloater axImportFloater	-- close old window if visible
axImportFloater = newRolloutFloater "ActorX Import" 250 550 30 100		-- create a new window

-- add controls
addRollout axImportRollout   axImportFloater
addRollout axToolsRollout    axImportFloater
addRollout axSettingsRollout axImportFloater
