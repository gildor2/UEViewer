/*

 Actor X mesh (psk) and animation (psa) importer for 3ds Max

 Created:	September 18 2009

 Author:	Konstantin Nosov (aka Gildor)

 Revision History:

	22.09.2009 v1.0
	- first public release

*/


/*
TODO:
- Menu item for plugin
- try to not use setBoneEnable false ...
- progressbar for mesh and animation loading
*/

-------------------------------------------------------------------------------
--	Global variables
-------------------------------------------------------------------------------

global g_seeThru
global g_updateTime
global g_playAnim
global Anims


-------------------------------------------------------------------------------
--	Default settings
-------------------------------------------------------------------------------

boneheight = .50
bonewidth  = .50

global g_axImporterInited
if (g_axImporterInited != true) then
(
	g_axImporterInited = true
	Anims = #()
	-- defaults settings
	g_seeThru    = false
	g_updateTime = true
	g_playAnim   = false
)


-------------------------------------------------------------------------------
--	Service functions
-------------------------------------------------------------------------------

fn ReadFixedString bstream fixedLen =
(
	local str = ""
	for i = 1 to fixedLen do
	(
		str += bit.intAsChar (ReadByte bstream #unsigned)
	)
	str
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


-------------------------------------------------------------------------------
--	Actor X data structures
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

fn LOAD_CHUNK bstream name =
(
	hdr = ReadChunkHeader bstream
	if (hdr.ChunkID != name) then
	(
		local msg = ("ERROR: bad chunk header: \"" + name + "\" expected but \"" + hdr.ChunkID + "\" found\n")
		format "%\n" msg
		messageBox msg
		throw msg
	)
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
	v1.PointIndex - v2.PointIndex
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
--	Loading PSK file
-------------------------------------------------------------------------------

fn pskimportfile filename =
(
	set coordsys world

	local Verts     = #()
	local Wedges    = #()
	local Tris      = #()
	local Materials = #()
	local Bones     = #()
	local Infs      = #()

	--------- Read the file ---------

	try
	(
		file = fopen filename "rb"

		-- Header --
		hdr = LOAD_CHUNK file "ACTRHEAD"

		-- Points --
		hdr = LOAD_CHUNK file "PNTS0000"
		numVerts = hdr.DataCount;
		Verts[numVerts] = [ 0, 0, 0 ]		-- preallocate
		for i = 1 to numVerts do Verts[i] = ReadFVector file

		-- Wedges --
		hdr = LOAD_CHUNK file "VTXW0000"
		numWedges = hdr.DataCount
		Wedges[numWedges] = VVertex ()		-- preallocate
		for i = 1 to numWedges do Wedges[i] = ReadVVertex file

		-- Faces --
		hdr = LOAD_CHUNK file "FACE0000"
		numTris = hdr.DataCount
		Tris[numTris] = VTriangle ()		-- preallocate
		for i = 1 to numTris do Tris[i] = ReadVTriangle file

		-- Materials --
		hdr = LOAD_CHUNK file "MATT0000"
		numMaterials = hdr.DataCount
		Materials[numMaterials] = VMaterial ()	-- preallocate
		for i = 1 to numMaterials do Materials[i] = ReadVMaterial file

		-- Bones --
		hdr = LOAD_CHUNK file "REFSKELT"
		numBones = hdr.DataCount
		Bones[numBones] = VBone ()		-- preallocate
		for i = 1 to numBones do
		(
			Bones[i] = ReadVBone file
--			format "Bone[%] = %\n" (i-1) Bones[i].Name
		)

		-- Weights --
		hdr = LOAD_CHUNK file "RAWWEIGHTS"
		numInfluences = hdr.DataCount
		Infs[numInfluences] = VRawBoneInfluence () -- preallocate
		for i = 1 to numInfluences do Infs[i] = ReadVRawBoneInfluence file
	)
	catch
	(
		fclose file
		messageBox ("Error loading file " + filename)
		format "FATAL ERROR: %\n" getCurrentException()
		return undefined
	)

	format "Read mesh: % verts, % wedges, % tris, % materials, % bones, % influences\n" \
		numVerts numWedges numTris numMaterials numBones numInfluences
	fclose file

	--------- File is completely read now ---------

	-- generate skeleton
	MaxBones = #()
	for i = 1 to numBones do
	(
		bn = Bones[i]
		-- build bone matrix
		q = bn.Orientation
		if (i == 1) then q = conjugate q
		mat = (normalize q) as matrix3;
		mat.row4 = bn.Position;
		-- transform from parent bone coordinate space to world space
		if (i > 1) then
		(
			bn.Matrix = mat * Bones[bn.ParentIndex + 1].Matrix
		)
		else
		(
			bn.Matrix = mat
		)

		-- get bone length (just for visual appearance)
		childBone = FindFirstChild Bones i
		if (childBone != undefined) then
		(
			len = length childBone.Position
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
			newBone = bonesys.createbone	\
				  bn.Matrix.row4	\
				  (bn.Matrix.row4 + len * (normalize bn.Matrix.row1)) \
				  (normalize bn.Matrix.row3)
			newBone.name   = bn.Name
			newBone.width  = bonewidth
			newBone.height = boneheight
			newBone.setBoneEnable false 0
			newBone.rotation.controller = TCB_rotation ()	-- required for correct animation
			-- setup parent
			if (i > 0) then
			(
				if (bn.ParentIndex >= i) then
				(
					format "Invalid parent % for bone % (%)" bn.ParentIndex (i-1) bn.Name
					return undefined
				)
				newBone.parent = MaxBones[bn.ParentIndex + 1]
			)
		)
		MaxBones[i] = newBone
	)

	-- generate mesh
	nameComponents = filterstring filename "/\\."
	MaxFaces = #()
	MaxVerts = #()
	MaxFaces[numTris]   = [ 0, 0, 0 ]		-- preallocate
	MaxVerts[numWedges] = [ 0, 0, 0 ]		-- ...
	for i = 1 to numWedges do
	(
		MaxVerts[i] = Verts[Wedges[i].PointIndex + 1]
	)
	for i = 1 to numTris do
	(
		tri = Tris[i]
		w0 = tri.Wedge0
		w1 = tri.Wedge1
		w2 = tri.Wedge2
		MaxFaces[i] = [ w1+1, w0+1, w2+1 ]	-- note: reversing vertex order
	)
	newMesh = mesh vertices:MaxVerts faces:MaxFaces name:nameComponents[nameComponents.count-1]
	-- texturing
	newMesh.xray = g_seeThru
	meshop.setMapSupport newMesh 1 true		-- enable texturemap channel
	meshop.setNumMapVerts newMesh 1 numWedges	-- set number of texture vertices
	for i = 1 to numWedges do
	(
		w = Wedges[i]
		meshop.setMapVert newMesh 1 i [ w.U, 1-w.V, 1-w.V ]	-- V coordinate is flipped
	)
	for i = 1 to numTris do
	(
		tri = Tris[i]
		w0 = tri.Wedge0
		w1 = tri.Wedge1
		w2 = tri.Wedge2
		meshop.setMapFace newMesh 1 i [ w1+1, w0+1, w2+1 ]
		setFaceMatId newMesh i (tri.MatIndex+1)
	)
	newMat = multiMaterial numsubs:numMaterials
	for i = 1 to numMaterials do
	(
		subMat = standardMaterial name:Materials[i].MaterialName
		subMat.diffuseMap = bitmapTexture name:Materials[i].MaterialName
--		subMat.diffuseMap.fileName = Materials[i].MaterialName
		subMat.diffuseMap.fileName = getFilenamePath filename + Materials[i].MaterialName + ".tga"	-- testing: apply textures immediately
		newMat.materialList[i] = subMat
		showTextureMap subMat subMat.diffuseMap true
--		format "Material[%] = %\n" i Materials[i].MaterialName
	)
	newMesh.material = newMat

	-- generate skin modifier
	max modify mode
	select newMesh

	skinMod = skin ()
	addModifier newMesh skinMod
	for i = 1 to numBones do
	(
		if i != numBones then
			skinOps.addBone skinMod MaxBones[i] 0
		else
			skinOps.addBone skinMod MaxBones[i] 1
	)
	update newMesh
--	redrawViews()

	modPanel.setCurrentObject skinMod

	-- setup vertex influences (weights)
	qsort Infs InfluenceSort
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
			for i = 1 to numInfs do
			(
				v = Infs[start + i - 1]
				skinOps.SetVertexWeights skinMod wedge (v.BoneIndex+1) v.Weight
			)
/*
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
			-- cannot reuse them */
--			progressUpdate (100.0 * wedge / numWedges)
		)
	)
	catch
	(
		enableSceneRedraw()
		progressEnd()
		throw()
	)
	enableSceneRedraw()
--	progressEnd()

	gc()
)


-------------------------------------------------------------------------------
--	Loading PSA file
-------------------------------------------------------------------------------

fn psaimportfile filename trackNum =
(
	local Bones = #()
	      Anims = #()
	local Keys  = #()
	local numBones
	local numAnims
	local numKeys

	--------- Read the file ---------

	try
	(
		file = fopen filename "rb"

		-- Header --
		hdr = LOAD_CHUNK file "ANIMHEAD"

		-- Bone links --
		hdr = LOAD_CHUNK file "BONENAMES"
		numBones = hdr.DataCount
		Bones[numBones] = VBone ()		-- preallocate
		for i = 1 to numBones do Bones[i] = ReadVBone file

		-- Animation sequence info --
		hdr = LOAD_CHUNK file "ANIMINFO"
		numAnims = hdr.DataCount
		Anims[numAnims] = AnimInfoBinary ()	-- preallocate
		for i = 1 to numAnims do Anims[i] = ReadAnimInfoBinary file

		if trackNum < 0 then return undefined	-- information only

		-- Key data --
		hdr = LOAD_CHUNK file "ANIMKEYS"
		if trackNum > 1 then
		(
			keyIndex = 0
			for i = 1 to trackNum - 1 do keyIndex += Anims[i].NumRawFrames * numBones
			fseek file (keyIndex * 32) #seek_cur	-- skip
		)
		track = Anims[trackNum]
		numKeys = track.NumRawFrames * Bones.count
		format "Loading track % (%), % keys\n" trackNum Anims[trackNum].Name numKeys
		Keys[numKeys] = VQuatAnimKey ()		-- preallocate
		for i = 1 to numKeys do Keys[i] = ReadVQuatAnimKey file
	)
	catch
	(
		fclose file
		messageBox ("Error loading file " + filename)
		format "FATAL ERROR: %\n" (getCurrentException())
		throw()
		return undefined
	)

	-- find bones
	MaxBones = #()
	for i = 1 to numBones do
	(
		MaxBones[i] = getNodeByName Bones[i].Name exact:true ignoreCase:false
		if MaxBones[i] == undefined then format "WARNING: cannot find the bone %\n" Bones[i].Name
	)

	set coordsys world
	startframe = 1	-- can modify layer ...

	animate on
	(
		keyIndex = 1
		for i = 1 to track.NumRawFrames do
		(
			at time (startframe + i - 1)
			for b = 1 to Bones.count do
			(
				-- get key
				k = Keys[keyIndex]
				keyIndex += 1
				bone = MaxBones[b]
				p = k.Position
				q = k.Orientation
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
			)
		)
	)

	stopAnimation()
	sliderTime = 1

	if g_updateTime then
	(
		ar_start = startframe
		ar_end   = startframe + track.NumRawFrames - 1
	)
	else
	(
		ar_start = animationRange.start.frame
		ar_end   = animationRange.end.frame
		if animationRange.start.frame > startframe then
			ar_start = startframe
		if animationRange.end.frame < startframe + track.NumRawFrames then
			ar_end   = startframe + track.NumRawFrames - 1
	)
	if (ar_end == ar_start) then ar_end = ar_end + 1 -- avoid zero-length intervals

	animationRange = interval ar_start ar_end
--	frameRate      = track.AnimRate

	if g_playAnim then playAnimation immediateReturn:true
)


-------------------------------------------------------------------------------
--	User interface
-------------------------------------------------------------------------------

global MeshFileName
global AnimFileName

fn LoadAnimation index =
(
	if (index > 0) and (index <= Anims.count) then psaimportfile AnimFileName index
)

rollout axImportRollout "Actor X Importer"
(
	-- copyright label
	label Lbl1 "Version 1.0"
	label Lbl2 "\xA9 2009 Konstantin Nosov (Gildor)"
	label Lbl3 "http://www.gildor.org/"

	Group "Mesh Import"
	(
		checkbox ChkSeeThru    "See-Thru Mesh" checked:g_seeThru
		button   BtnImportPsk  "Import PSK ..."
	)

	Group "Animation Import"
	(
		button   BtnImportPsa  "Import PSA ..."
		listbox  LstAnims      "Animations:"     height:13
		checkbox ChkAnimTime   "Update animation length" checked:g_updateTime
		checkbox ChkPlayAnim   "Play animation" checked:g_playAnim
		button   BtnImportTrk  "Load animation track"
	)

	on BtnImportPsk pressed do
	(
		MeshFileName = getOpenFileName types:"Actor X Mesh (*.psk)|*.psk|All (*.*)|*.*|"
		if MeshFileName != undefined then 
		(
			if DoesFileExist MeshFileName then pskimportfile MeshFileName
		)
	)

	on ChkSeeThru changed state do
	(
		g_seeThru = ChkSeeThru.checked
	)

	on BtnImportPsa pressed do
	(
		AnimFileName = getOpenFileName types:"Actor X Animation (*.psa)|*.psa|All (*.*)|*.*|"
	
		if AnimFileName != undefined then 
		(
			if DoesFileExist AnimFileName then psaimportfile AnimFileName -1
			LstAnims.items = for a in Anims collect (a.Name + " [" + (a.NumRawFrames as string) + "]")
		)
	)

	on BtnImportTrk pressed       do LoadAnimation LstAnims.selection
	on LstAnims doubleClicked sel do LoadAnimation sel

	on ChkAnimTime changed state do g_updateTime = ChkAnimTime.checked
	on ChkPlayAnim changed state do g_playAnim   = ChkPlayAnim.checked

	on axImportRollout open do
	(
		-- fill LstAnims
		LstAnims.items = for a in Anims collect (a.Name + " [" + (a.NumRawFrames as string) + "]")
	)
)

-- Create plugin window
if axImportFloater != undefined do closeRolloutFloater axImportFloater	-- close old window if visible
axImportFloater = newRolloutFloater "Actor X Import" 250 488 30 100 	-- create new window
format "ROLL: %\n" axImportFloater
addRollout axImportRollout axImportFloater				-- add controls
