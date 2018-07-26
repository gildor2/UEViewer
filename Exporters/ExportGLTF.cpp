#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"

#include "SkeletalMesh.h"
#include "StaticMesh.h"

#include "Exporters.h"
#include "../UmodelTool/Version.h"

//?? TODO: remove this function
static CVec3 GetMaterialDebugColor(int Index)
{
	// most of this code is targetted to make maximal color combinations
	// which are maximally different for adjancent BoneIndex values
	static const float table[]  = { 0.9f, 0.3f, 0.6f, 1.0f };
	static const int  table2[] = { 0, 1, 2, 4, 7, 3, 5, 6 };
	Index = (Index & 0xFFF8) | table2[Index & 7] ^ 7;
	#define C(x)	( (x) & 1 ) | ( ((x) >> 2) & 2 )
	CVec3 r;
	r[0] = table[C(Index)];
	r[1] = table[C(Index >> 1)];
	r[2] = table[C(Index >> 2)];
	#undef C
	return r;
}

static void ExportMaterial(UUnrealMaterial* Mat, FArchive& Ar, int index, bool bLast)
{
	char dummyName[64];
	appSprintf(ARRAY_ARG(dummyName), "dummy_material_%d", index);

	CVec3 Color = GetMaterialDebugColor(index);
	Ar.Printf(
		"    {\n"
		"      \"name\" : \"%s\",\n"
		"      \"pbrMetallicRoughness\" : {\n"
		"        \"baseColorFactor\" : [ %g, %g, %g, 1.0 ],\n"
		"        \"metallicFactor\" : 0.1,\n"
		"        \"roughnessFactor\" : 0.5\n"
		"      }\n"
		"    }%s\n",
		Mat ? Mat->Name : dummyName,
		Color[0], Color[1], Color[2],
		bLast ? "" : ","
	);
}

struct BufferData
{
	enum
	{
		BYTE = 5120,
		UNSIGNED_BYTE = 5121,
		SHORT = 5122,
		UNSIGNED_SHORT = 5123,
		UNSIGNED_INT = 5125,
		FLOAT = 5126
	};

	byte* Data;
	int DataSize;
	int ComponentType;
	int Count;
	const char* Type;

	BufferData()
	: Data(NULL)
	, DataSize(0)
	{}

	~BufferData()
	{
		if (Data) appFree(Data);
	}

	void Setup(int InCount, const char* InType, int InComponentType, int InItemSize)
	{
		Count = InCount;
		Type = InType;
		ComponentType = InComponentType;
		DataSize = InCount * InItemSize;
		// Align all buffrrs by 4, as requested by glTF format
		DataSize = Align(DataSize, 4);
		// Use aligned alloc for CVec4
		Data = (byte*) appMalloc(DataSize, 16);
	}
};

static void ExportSection(const CStaticMeshLod& Lod, int SectonIndex, FArchive& Ar, TArray<BufferData>& Data)
{
	guard(ExportSection);

	const CMeshSection& S = Lod.Sections[SectonIndex];
	bool bLast = (SectonIndex == Lod.Sections.Num()-1);

	// Remap section indices to local indices
	CIndexBuffer::IndexAccessor_t GetIndex = Lod.Indices.GetAccessor();
	TArray<int> indexRemap; // old vertex index -> new vertex index
	indexRemap.Init(-1, Lod.NumVerts);
	int numLocalVerts = 0;
	int numLocalIndices = S.NumFaces * 3;
	for (int idx = 0; idx < numLocalIndices; idx++)
	{
		int vertIndex = GetIndex(S.FirstIndex + idx);
		if (indexRemap[vertIndex] == -1)
		{
			indexRemap[vertIndex] = numLocalVerts++;
		}
	}

	// Prepare buffers
	int IndexBufIndex = Data.AddZeroed();
	int PositionBufIndex = Data.AddZeroed();
	int NormalBufIndex = Data.AddZeroed();
	int TangentBufIndex = Data.AddZeroed();
	int UVBufIndex[MAX_MESH_UV_SETS];
	for (int i = 0; i < Lod.NumTexCoords; i++)
	{
		UVBufIndex[i] = Data.AddZeroed();
	}

	BufferData& IndexBuf = Data[IndexBufIndex];
	BufferData& PositionBuf = Data[PositionBufIndex];
	BufferData& NormalBuf = Data[NormalBufIndex];
	BufferData& TangentBuf = Data[TangentBufIndex];
	BufferData* UVBuf[MAX_MESH_UV_SETS];

	PositionBuf.Setup(numLocalVerts, "VEC3", BufferData::FLOAT, sizeof(CVec3));
	NormalBuf.Setup(numLocalVerts, "VEC3", BufferData::FLOAT, sizeof(CVec3));
	TangentBuf.Setup(numLocalVerts, "VEC4", BufferData::FLOAT, sizeof(CVec4));
	for (int i = 0; i < Lod.NumTexCoords; i++)
	{
		UVBuf[i] = &Data[UVBufIndex[i]];
		UVBuf[i]->Setup(numLocalVerts, "VEC2", BufferData::FLOAT, sizeof(CMeshUVFloat));
	}

	// Prepare and build indices
	TArray<int> localIndices;
	localIndices.AddUninitialized(numLocalIndices);
	int* pIndex = localIndices.GetData();
	for (int i = 0; i < numLocalIndices; i++)
	{
		*pIndex++ = GetIndex(S.FirstIndex + i);
	}

	if (numLocalVerts <= 65536)
	{
		IndexBuf.Setup(numLocalIndices, "SCALAR", BufferData::UNSIGNED_SHORT, sizeof(uint16));
		uint16* p = (uint16*) IndexBuf.Data;
		for (int idx = 0; idx < numLocalIndices; idx++)
		{
			*p++ = indexRemap[localIndices[idx]];
		}
	}
	else
	{
		IndexBuf.Setup(numLocalIndices, "SCALAR", BufferData::UNSIGNED_INT, sizeof(uint32));
		uint32* p = (uint32*) IndexBuf.Data;
		for (int idx = 0; idx < numLocalIndices; idx++)
		{
			*p++ = indexRemap[localIndices[idx]];
		}
	}

	// Build reverse index map for fast loopup of vertex by its new index.
	// It maps new vertex index to old vertex index.
	TArray<int> revIndexMap;
	revIndexMap.AddUninitialized(numLocalVerts);
	for (int i = 0; i < indexRemap.Num(); i++)
	{
		int newIndex = indexRemap[i];
		if (newIndex != -1)
		{
			revIndexMap[newIndex] = i;
		}
	}

	// Build vertices
	CVec3* pPos = (CVec3*) PositionBuf.Data;
	CVec3* pNormal = (CVec3*) NormalBuf.Data;
	CVec4* pTangent = (CVec4*) TangentBuf.Data;
	CMeshUVFloat* pUV0 = (CMeshUVFloat*) UVBuf[0]->Data;

	for (int i = 0; i < numLocalVerts; i++)
	{
		int vertIndex = revIndexMap[i];
		const CMeshVertex& V = Lod.Verts[vertIndex];
		*pPos++ = V.Position;
		*pUV0++ = V.UV;

		CVec4 tmpNormal, tmpTangent;
		Unpack(tmpNormal, V.Normal);
		Unpack(tmpTangent, V.Tangent);
		// Unreal (and we are) using normal.w for computing binormal. glTF
		// uses tangent.w for that.
		tmpTangent.w = tmpNormal.w;
		*pNormal++ = tmpNormal;
		*pTangent++ = tmpTangent;
	}

	// Fix vertex orientation: glTF has right-handed coordinate system with Y up,
	// Unreal uses left-handed system with Z up. Also, Unreal uses 'cm' scale,
	// while glTF 'm'.
	pPos = (CVec3*) PositionBuf.Data;
	pNormal = (CVec3*) NormalBuf.Data;
	pTangent = (CVec4*) TangentBuf.Data;
	for (int i = 0; i < numLocalVerts; i++)
	{
		Exchange((*pPos)[1], (*pPos)[2]);
		Exchange((*pNormal)[1], (*pNormal)[2]);
		Exchange((*pTangent)[1], (*pTangent)[2]);
		pPos->Scale(0.01f);
//		(*pTangent)[3] = -1; //??
		pPos++;
		pNormal++;
		pTangent++;
	}

	// Secondary UVs
	for (int uvIndex = 1; uvIndex < Lod.NumTexCoords; uvIndex++)
	{
		CMeshUVFloat* pUV = (CMeshUVFloat*) UVBuf[uvIndex]->Data;
		const CMeshUVFloat* srcUV = Lod.ExtraUV[uvIndex-1];
		for (int i = 0; i < numLocalVerts; i++)
		{
			int vertIndex = revIndexMap[i];
			*pUV++ = srcUV[vertIndex];
		}
	}

	// Write primitive information to json
	Ar.Printf(
		"        {\n"
		"          \"attributes\" : {\n"
		"            \"POSITION\" : %d,\n"
		"            \"NORMAL\" : %d,\n"
		"            \"TANGENT\" : %d,\n",
		PositionBufIndex, NormalBufIndex, TangentBufIndex
	);
	for (int i = 0; i < Lod.NumTexCoords; i++)
	{
		Ar.Printf(
			"            \"TEXCOORD_%d\" : %d%s\n",
			i, UVBufIndex[i], i < (Lod.NumTexCoords-1) ? "," : ""
		);
	}

	Ar.Printf(
		"          },\n"
		"          \"indices\" : %d,\n"
		"          \"material\" : %d\n"
		"        }%s\n",
		IndexBufIndex, SectonIndex,
		SectonIndex < (Lod.Sections.Num()-1) ? "," : ""
	);

	unguard;
}

static void ExportStaticMeshLod(const char* MeshName, const CStaticMeshLod& Lod, FArchive& Ar, FArchive& Ar2)
{
	// Opening brace
	Ar.Printf("{\n");

	// Asset
	Ar.Printf(
		"  \"asset\" : {\n"
		"    \"generator\" : \"UE Viewer (umodel) build %s\",\n"
		"    \"version\" : \"2.0\"\n"
		"  },\n",
		STR(GIT_REVISION));

	// Scene
	Ar.Printf(
		"  \"nodes\" : [\n"
		"    {\n"
		"      \"mesh\" : 0,\n"
		"      \"name\" : \"%s\"\n"
		"    }\n"
		"  ],\n",
		MeshName);
	Ar.Printf(
		"  \"scene\" : 0,\n"
		"  \"scenes\" : [\n"
		"    {\n"
		"      \"nodes\" : [ 0 ]\n"
		"    }\n"
		"  ],\n"
	);

	// Materials
	Ar.Printf("  \"materials\" : [\n");
	for (int i = 0; i < Lod.Sections.Num(); i++)
	{
		ExportMaterial(Lod.Sections[i].Material, Ar, i, i == Lod.Sections.Num() - 1);
	}
	Ar.Printf("  ],\n");

	// Meshes
	TArray<BufferData> Data;

	Ar.Printf(
		"  \"meshes\" : [\n"
		"    {\n"
		"      \"primitives\" : [\n"
	);
	for (int i = 0; i < Lod.Sections.Num(); i++)
	{
		ExportSection(Lod, i, Ar, Data);
	}
	Ar.Printf(
		"      ],\n"
		"      \"name\" : \"%s\"\n"
		"    }\n"
		"  ],\n",
		MeshName
	);

	int bufferLength = 0;
	for (int i = 0; i < Data.Num(); i++)
	{
		bufferLength += Data[i].DataSize;
	}

	// Write buffer
	Ar.Printf(
		"  \"buffers\" : [\n"
		"    {\n"
		"      \"uri\" : \"%s.bin\",\n"
		"      \"byteLength\" : %d\n"
		"    }\n"
		"  ],\n",
		MeshName, bufferLength
	);

	// Write bufferViews
	Ar.Printf(
		"  \"bufferViews\" : [\n"
	);
	int bufferOffset = 0;
	for (int i = 0; i < Data.Num(); i++)
	{
		Ar.Printf(
			"    {\n"
			"      \"buffer\" : 0,\n"
			"      \"byteOffset\" : %d,\n"
			"      \"byteLength\" : %d\n"
			"    }%s\n",
			bufferOffset,
			Data[i].DataSize,
			i == (Data.Num()-1) ? "" : ","
		);
		bufferOffset += Data[i].DataSize;
	}
	Ar.Printf(
		"  ],\n"
	);

	// Write accessors
	Ar.Printf(
		"  \"accessors\" : [\n"
	);
	for (int i = 0; i < Data.Num(); i++)
	{
		const BufferData& B = Data[i];
		Ar.Printf(
			"    {\n"
			"      \"bufferView\" : %d,\n"
			"      \"componentType\" : %d,\n"
			"      \"count\" : %d,\n"
			"      \"type\" : \"%s\"\n"
			"    }%s\n",
			i,
			B.ComponentType,
			B.Count,
			B.Type,
			i == (Data.Num()-1) ? "" : ","
		);
	}
	Ar.Printf(
		"  ]\n"
	);

	// Write binary data
	for (int i = 0; i < Data.Num(); i++)
	{
		const BufferData& B = Data[i];
		Ar2.Serialize(B.Data, B.DataSize);
	}

	// Closing brace
	Ar.Printf("}\n");
}

void ExportStaticMeshGLTF(const CStaticMesh* Mesh)
{
	guard(ExportStaticMeshGLTF);

	UObject *OriginalMesh = Mesh->OriginalMesh;
	if (!Mesh->Lods.Num())
	{
		appNotify("Mesh %s has 0 lods", OriginalMesh->Name);
		return;
	}

	FArchive* Ar = CreateExportArchive(OriginalMesh, FAO_TextFile, "%s.gltf", OriginalMesh->Name);
	if (Ar)
	{
		FArchive* Ar2 = CreateExportArchive(OriginalMesh, 0, "%s.bin", OriginalMesh->Name);
		assert(Ar2);
		ExportStaticMeshLod(OriginalMesh->Name, Mesh->Lods[0], *Ar, *Ar2);
		delete Ar;
		delete Ar2;
	}

	unguard;
}
