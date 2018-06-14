#pragma once

#define __PLACEMENT_NEW_INLINE
#include <fbxsdk.h>

enum EFbxExportCompatibility
{
	FBX_2010,
	FBX_2011,
	FBX_2012,
	FBX_2013,
	FBX_2014,
	FBX_2016,
	FBX_2018,
};

class FFbxExportOption
{
public:
	FFbxExportOption();
public:
	/** This will set the fbx sdk compatibility when exporting to fbx file. The default value is 2013 */
	EFbxExportCompatibility FbxExportCompatibility;

	/** If enabled, export with X axis as the front axis instead of default -Y */
	uint32 bForceFrontXAxis : 1;

	/** If enable, export vertex color */
	uint32 VertexColor : 1;

	/** If enabled, export the level of detail */
	uint32 LevelOfDetail : 1;

	/** If enabled, export collision */
	uint32 Collision : 1;

	/** If enable, export welded vertices*/
	uint32 WeldedVertices : 1;

	/** If enable, Map skeletal actor motion to the root bone of the skeleton. */
	uint32 MapSkeletalMotionToRoot : 1;
};
/**
* FBX basic data conversion class.
*/
class FFbxDataConverter
{
public:
	static void SetJointPostConversionMatrix(FbxAMatrix ConversionMatrix) { JointPostConversionMatrix = ConversionMatrix; }
	static const FbxAMatrix &GetJointPostConversionMatrix() { return JointPostConversionMatrix; }

	static FVector ConvertPos(FbxVector4 Vector);
	static FVector ConvertDir(FbxVector4 Vector);
	static FRotator ConvertEuler(FbxDouble3 Euler);
	static FVector ConvertScale(FbxDouble3 Vector);
	static FVector ConvertScale(FbxVector4 Vector);
	static FRotator ConvertRotation(FbxQuaternion Quaternion);
	static FVector ConvertRotationToFVect(FbxQuaternion Quaternion, bool bInvertRot);
	static FQuat ConvertRotToQuat(FbxQuaternion Quaternion);
	static float ConvertDist(FbxDouble Distance);
	static FTransform ConvertTransform(FbxAMatrix Matrix);
	static FMatrix ConvertMatrix(FbxAMatrix Matrix);

	/*
	* Convert fbx linear space color to sRGB FColor
	*/
	static FColor ConvertColor(FbxDouble3 Color);

	static FbxVector4 ConvertToFbxPos(FVector Vector);
	static FbxVector4 ConvertToFbxRot(FVector Vector);
	static FbxVector4 ConvertToFbxScale(FVector Vector);

	/*
	* Convert sRGB FColor to fbx linear space color
	*/
	static FbxDouble3   ConvertToFbxColor(FColor Color);
	static FbxString	ConvertToFbxString(FName Name);
	static FbxString	ConvertToFbxString(const FString& String);

private:
	static FbxAMatrix JointPostConversionMatrix;
};

class FFbxExporter
{
public:
	static FFbxExporter* GetInstance()
	{
		static FFbxExporter fbxExporter;
		return &fbxExporter;
	}
public:
	/**
	* Creates and readies an empty document for export.
	*/
	void CreateDocument();

	/**
	* Closes the FBX document, releasing its memory.
	*/
	void CloseDocument();

	/**
	* Writes the FBX document to disk and releases it by calling the CloseDocument() function.
	*/
	void WriteToFile(const char* Filename);

	void ExportStaticMesh(const CStaticMesh* StaticMesh);

private:
	FFbxExporter();
	~FFbxExporter();

	/**
	* Exports a static mesh
	* @param StaticMesh	The static mesh to export
	* @param MeshName		The name of the mesh for the FBX file
	* @param FbxActor		The fbx node representing the mesh
	* @param ExportLOD		The LOD of the mesh to export
	* @param LightmapUVChannel Optional UV channel to export
	*/
	FbxNode* ExportStaticMeshToFbx(const CStaticMesh* StaticMesh, int32 ExportLOD, const char* MeshName, FbxNode* FbxActor, int32 LightmapUVChannel = -1);
	FbxSurfaceMaterial* CreateDefaultMaterial();
	FbxSurfaceMaterial* ExportMaterial(UUnrealMaterial* Material);
	bool FillFbxTextureProperty(const char *PropertyName, const UUnrealMaterial* Texture, FbxSurfaceMaterial* FbxMaterial);

private:
	FbxManager* SdkManager;
	FbxScene* Scene;
	FbxAnimStack* AnimStack;
	FbxAnimLayer* AnimLayer;
	FbxCamera* DefaultCamera;

	FFbxDataConverter Converter;

	FFbxExportOption* ExportOptions;

private:
	TMap<const CStaticMesh*, FbxMesh*> FbxMeshes;
	TMap<const UUnrealMaterial*, FbxSurfaceMaterial*> FbxMaterials;
};