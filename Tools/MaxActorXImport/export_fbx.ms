/*

 ActorX batch converter for 3ds Max

 Created:	December 15 2010

 Author:	Konstantin Nosov (aka Gildor)

 Web page:	http://www.gildor.org/projects/unactorx

	29.12.2010 v1.01
	- added output format selection (fbx, ase, max)

	15.12.2010 v1.00
	- first public release

*/


/* TODO
	- save settings to the ActorX Importer ini (using its API)
*/

global g_axImporterInited

global g_meshDir
global g_meshRecurse
global g_fbxSmGroups
global g_fbxSmMesh
global g_outFormat
global g_useDefaultFBXSettings

if (g_meshDir     == undefined) then g_meshDir     = ""
if (g_meshRecurse == undefined) then g_meshRecurse = false
if (g_fbxSmGroups == undefined) then g_fbxSmGroups = true
if (g_fbxSmMesh   == undefined) then g_fbxSmMesh   = true
if (g_outFormat   == undefined) then g_outFormat   = 1
if (g_useDefaultFBXSettings == undefined) then g_useDefaultFBXSettings = true


fn VerifyAXI =
(
	if (g_axImporterInited != true) then
	(
		messageBox "ActorX Importer is not loaded!"
		return false
	)
	if (AX_IMPORTER_VERSION < 13) then
	(
		messageBox "Your ActorX Importer is too old, please upgrade!"
		return false
	)
	return true
)


-- configure FBX exporter
fn SetupFBX =
(
	if g_useDefaultFBXSettings then return undefined

	-- http://www.the-area.com/forum/autodesk-fbx/fbx-plug-ins-import-export-discussions/maxscript-export-dialog-properties/
	-- both commands should be used to ensure all commands are functional
	pluginManager.loadClass FBXIMP
	pluginManager.loadClass FBXEXP

	-- FbxExporterSetParam "Geometries" true -- <bool>
	-- Controls the state of the "Geometries" checkbox in the FBX Export dialog.
	FbxExporterSetParam "NormalsPerPoly" true -- <bool>
	-- Controls the state of the "Support normals per polygon vertex" checkbox in the FBX Export dialog.
	FbxExporterSetParam "Cameras" false -- <bool>
	-- Controls the state of the "Cameras" checkbox in the FBX Export dialog.
	FbxExporterSetParam "Lights" false -- <bool>
	-- Controls the state of the "Lights" checkbox in the FBX Export dialog.
	FbxExporterSetParam "GeomAsBone" true -- <bool>
	-- Controls the state of the "Geometries used as bones, exported as bones" checkbox in the FBX Export dialog.
	FbxExporterSetParam "Shape" false -- <bool>
	-- Controls the state of the "Shape (Morph modifier)" checkbox in the FBX Export dialog.
	FbxExporterSetParam "Skin" true -- <bool>
	-- Controls the state of the "Skins (Skin Modifier and Physique)" checkbox in the FBX Export dialog.
	FbxExporterSetParam "Animation" true -- <bool>
	-- Controls the state of the "Animation" checkbox in the FBX Export dialog.
	-- FbxExporterSetParam "Resampling" -- <float>
	-- Controls the value of the "Resampling rate (when necessary)" field in the FBX Export dialog.
	FbxExporterSetParam "ShowWarnings" false -- <bool>
	-- Controls the state of the "Show warnings" checkbox in the FBX Export dialog.
	FbxExporterSetParam "EmbedTextures" false -- <bool>
	-- Controls the state of the "Embed textures in export file" checkbox in the FBX Export dialog.
	FbxExporterSetParam "SmoothingGroups" g_fbxSmGroups -- <bool>
	-- True or false. See Smoothing Groups for an explanation of this setting.
	FbxExporterSetParam "SmoothMeshExport" g_fbxSmMesh -- <bool>
	-- True or false. See TurboSmooth for an explanation of this setting.
)


fn GetExportSubDir =
(
	if (g_outFormat == 1) then
	(
		return "FBX"
	)
	if (g_outFormat == 2) then
	(
		return "ase"
	)
	if (g_outFormat == 3) then
	(
		return "max"
	)
	return "unknown"		-- should not get here
)


fn SaveAXFile filename =
(
	if (g_outFormat == 1) then
	(
		-- FBX
		exportFile filename #noPrompt using:FBXEXP
		return undefined
	)
	if (g_outFormat == 2) then
	(
		-- ASE
		exportFile (filename + ".ase") #noPrompt
		return undefined
	)
	if (g_outFormat == 3) then
	(
		-- MAX
		saveMaxFile filename
		return undefined
	)
)


fn ExportFbxAnim =
(
	if (not VerifyAXI()) then return undefined

	if (MeshBones.count == 0) then
	(
		messageBox "Mesh is not loaded!"
		return undefined
	)

	if (Anims.count == 0) then
	(
		messageBox "AnimSet is not loaded!"
		return undefined
	)

	SetupFBX()

	-- configure ActorX Importer
	local playAnim = g_playAnim	-- save
	g_playAnim = false

	-- create target directory
	local dir = getFilenamePath(AnimFileName) + GetExportSubDir() + "\\" + getFilenameFile(AnimFileName) + "\\"
	makeDir dir all:true

	-- export all animations
	for i = 1 to Anims.count do
	(
		local track = Anims[i]
		local trackName = track.Name
		local filename = dir + trackName
		format "Exporting animation % (% frames) -> %\n" trackName track.NumRawFrames filename

		ImportPsaFile AnimFileName i
		SaveAXFile filename
	)

	g_playAnim = playAnim		-- restore
)


fn ExportFbxMesh psk_filename =
(
	if (not VerifyAXI()) then return undefined
	SetupFBX()

--	format "MESH: %\n" filename

	-- create target directory
	local dir = (getFilenamePath psk_filename) + GetExportSubDir()
	makeDir dir all:true
	local filename = dir + "\\" + getFilenameFile(psk_filename)
	format "Exporting mesh % -> %\n" psk_filename filename

	ClearMaxScene()
	ImportPskFile psk_filename
	SaveAXFile filename
)

fn ExportFbxMeshes path recurse =
(
	if (not VerifyAXI()) then return undefined
--	format "EXPORT DIR % %\n" path recurse

	local files = getFiles(path + "/*.psk*")
	for file in files do ExportFbxMesh file
	if recurse then
	(
		local dirs = getDirectories(path + "/*")
		for dir in dirs do ExportFbxMeshes dir recurse
	)

	ClearMaxScene()
)


-- UI
rollout fbxExportRollout "ActorX Batch Export"
(
	-- copyright label
	label     Lbl1 "Version 1.01"
	label     Lbl2 "\xA9 2010-2012 Konstantin Nosov (Gildor)"
	hyperlink Lbl3 "http://www.gildor.org/" \
					address:"http://www.gildor.org/projects/unactorx" align:#center \
					color:black hovercolor:blue visitedcolor:black

	group "Common"
	(
		label    LblOutFormat      "Output format:" across:2
		radiobuttons RadOutFormat labels:#("fbx", "ase", "max") default:g_outFormat align:#left columns:1
		checkbox ChkDefFbxSettings "Use default FBX settings" checked:g_useDefaultFBXSettings
	)

	group "Meshes"
	(
		edittext EdMeshPath     "Path to PSK" text:g_meshDir width:180 across:2
		button   BtnBrowseMesh  "..."     align:#right height:16
		checkbox ChkMeshRecurse "Recurse" checked:g_meshRecurse
		checkbox ChkSmGroups    "Smoothing groups" checked:g_fbxSmGroups
		checkbox ChkSmMesh      "Smooth mesh export" checked:g_fbxSmMesh
		button BtnExportMeshes  "Export meshes"
	)

	group "Animations"
	(
		button BtnExportAnims "Export animations"
	)

	on RadOutFormat      changed state do g_outFormat             = state
	on ChkDefFbxSettings changed state do g_useDefaultFBXSettings = state
	on BtnExportAnims    pressed do ExportFbxAnim()
	on BtnExportMeshes   pressed do ExportFbxMeshes g_meshDir g_meshRecurse

	on EdMeshPath        changed val do g_meshDir = val
	on BtnBrowseMesh     pressed do
	(
		dir = getSavePath caption:"Directory for mesh lookup" initialDir:g_meshDir
		if dir != undefined then
		(
			g_meshDir       = dir
			EdMeshPath.text = dir
		)
	)
	on ChkMeshRecurse changed state do g_meshRecurse = state

	on ChkSmGroups changed state do g_fbxSmGroups = state
	on ChkSmMesh   changed state do g_fbxSmMesh   = state
)


if fbxExportFloater != undefined do closeRolloutFloater fbxExportFloater	-- close old window if visible
fbxExportFloater = newRolloutFloater "FBX Batch Export" 250 380 290 100 	-- create new window

addRollout fbxExportRollout fbxExportFloater
