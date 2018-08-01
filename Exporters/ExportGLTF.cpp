#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"

#include "SkeletalMesh.h"
#include "StaticMesh.h"

#include "UnMathTools.h"

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

// Fix vector orientation: glTF has right-handed coordinate system with Y up,
// Unreal uses left-handed system with Z up. Also, Unreal uses 'cm' scale,
// while glTF 'm'.

inline void TransformPosition(CVec3& pos)
{
	Exchange(pos[1], pos[2]);
	pos.Scale(0.01f);
}

inline void TransformDirection(CVec3& vec)
{
	Exchange(vec[1], vec[2]);
}

inline void TransformRotation(CQuat& q)
{
	Exchange(q.y, q.z);		// same logic as for vector
//??	q.w *= -1;				// changing left-handed to right-handed, so inverse rotation - works correctly without this line
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
	bool bNormalized;

	// Data for filling buffer
	byte* FillPtr;
#if MAX_DEBUG
	int FillCount;
	int ItemSize;
#endif

	FString BoundsMin;
	FString BoundsMax;

	BufferData()
	: Data(NULL)
	, DataSize(0)
	{}

	~BufferData()
	{
		if (Data) appFree(Data);
	}

	void Setup(int InCount, const char* InType, int InComponentType, int InItemSize, bool InNormalized = false)
	{
		Count = InCount;
		Type = InType;
		bNormalized = InNormalized;
		ComponentType = InComponentType;
		DataSize = InCount * InItemSize;
		// Align all buffrrs by 4, as requested by glTF format
		DataSize = Align(DataSize, 4);
		// Use aligned alloc for CVec4
		Data = (byte*) appMalloc(DataSize, 16);

		FillPtr = Data;
#if MAX_DEBUG
		FillCount = 0;
		ItemSize = InItemSize;
#endif
	}

	template<typename T>
	inline void Put(const T& p)
	{
#if MAX_DEBUG
		assert(sizeof(T) == ItemSize);
		assert(FillCount++ < Count);
#endif
		*(T*)FillPtr = p;
		FillPtr += sizeof(T);
	}
};

struct ExportContext
{
	const char* MeshName;
	const CSkeletalMesh* SkelMesh;
	const CStaticMesh* StatMesh;

	TArray<BufferData> Data;

	ExportContext()
	{
		memset(this, 0, sizeof(*this));
	}

	inline bool IsSkeletal() const
	{
		return SkelMesh != NULL;
	}
};

#define VERT(n)		*OffsetPointer(Verts, (n) * VertexSize)

static void ExportSection(ExportContext& Context, const CBaseMeshLod& Lod, const CMeshVertex* Verts, int SectonIndex, FArchive& Ar)
{
	guard(ExportSection);

	int VertexSize = Context.IsSkeletal() ? sizeof(CSkelMeshVertex) : sizeof(CStaticMeshVertex);

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
	int IndexBufIndex = Context.Data.AddZeroed();
	int PositionBufIndex = Context.Data.AddZeroed();
	int NormalBufIndex = Context.Data.AddZeroed();
	int TangentBufIndex = Context.Data.AddZeroed();

	int BonesBufIndex = -1;
	int WeightsBufIndex = -1;
	if (Context.IsSkeletal())
	{
		BonesBufIndex = Context.Data.AddZeroed();
		WeightsBufIndex = Context.Data.AddZeroed();
	}

	int UVBufIndex[MAX_MESH_UV_SETS];
	for (int i = 0; i < Lod.NumTexCoords; i++)
	{
		UVBufIndex[i] = Context.Data.AddZeroed();
	}

	BufferData& IndexBuf = Context.Data[IndexBufIndex];
	BufferData& PositionBuf = Context.Data[PositionBufIndex];
	BufferData& NormalBuf = Context.Data[NormalBufIndex];
	BufferData& TangentBuf = Context.Data[TangentBufIndex];
	BufferData* UVBuf[MAX_MESH_UV_SETS];
	BufferData* BonesBuf = NULL;
	BufferData* WeightsBuf = NULL;

	PositionBuf.Setup(numLocalVerts, "VEC3", BufferData::FLOAT, sizeof(CVec3));
	NormalBuf.Setup(numLocalVerts, "VEC3", BufferData::FLOAT, sizeof(CVec3));
	TangentBuf.Setup(numLocalVerts, "VEC4", BufferData::FLOAT, sizeof(CVec4));
	for (int i = 0; i < Lod.NumTexCoords; i++)
	{
		UVBuf[i] = &Context.Data[UVBufIndex[i]];
		UVBuf[i]->Setup(numLocalVerts, "VEC2", BufferData::FLOAT, sizeof(CMeshUVFloat));
	}

	if (Context.IsSkeletal())
	{
		BonesBuf = &Context.Data[BonesBufIndex];
		WeightsBuf = &Context.Data[WeightsBufIndex];
		BonesBuf->Setup(numLocalVerts, "VEC4", BufferData::UNSIGNED_SHORT, sizeof(uint16)*4);
		WeightsBuf->Setup(numLocalVerts, "VEC4", BufferData::UNSIGNED_BYTE, sizeof(uint32), /*InNormalized=*/ true);
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
		for (int idx = 0; idx < numLocalIndices; idx++)
		{
			IndexBuf.Put<uint16>(indexRemap[localIndices[idx]]);
		}
	}
	else
	{
		IndexBuf.Setup(numLocalIndices, "SCALAR", BufferData::UNSIGNED_INT, sizeof(uint32));
		for (int idx = 0; idx < numLocalIndices; idx++)
		{
			IndexBuf.Put<uint32>(indexRemap[localIndices[idx]]);
		}
	}

	// Build reverse index map for fast lookup of vertex by its new index.
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
	for (int i = 0; i < numLocalVerts; i++)
	{
		int vertIndex = revIndexMap[i];
		const CMeshVertex& V = VERT(vertIndex);

		CVec3 Position = V.Position;

		CVec4 Normal, Tangent;
		Unpack(Normal, V.Normal);
		Unpack(Tangent, V.Tangent);
		// Unreal (and we are) using normal.w for computing binormal. glTF
		// uses tangent.w for that. Make this value exactly 1.0 of -1.0 to make glTF
		// validator happy.
	#if 0
		// There's some problem: V.Normal.W == 0x80 -> -1.008 instead of -1.0
		if (Normal.w > 1.001 || Normal.w < -1.001)
		{
			appError("%X -> %g\n", V.Normal.Data, Normal.w);
		}
	#endif
		Tangent.w = (Normal.w < 0) ? -1 : 1;

		TransformPosition(Position);
		TransformDirection(Normal);
		TransformDirection(Tangent);

		Normal.Normalize();
		Tangent.Normalize();

		// Fill buffers
		PositionBuf.Put(Position);
		NormalBuf.Put(Normal.xyz);
		TangentBuf.Put(Tangent);
		UVBuf[0]->Put(V.UV);
	}

	// Compute bounds for PositionBuf
	CVec3 Mins, Maxs;
	ComputeBounds((CVec3*)PositionBuf.Data, numLocalVerts, sizeof(CVec3), Mins, Maxs);
	char buf[256];
	appSprintf(ARRAY_ARG(buf), "[ %g, %g, %g ]", VECTOR_ARG(Mins));
	PositionBuf.BoundsMin = buf;
	appSprintf(ARRAY_ARG(buf), "[ %g, %g, %g ]", VECTOR_ARG(Maxs));
	PositionBuf.BoundsMax = buf;

	if (Context.IsSkeletal())
	{
		for (int i = 0; i < numLocalVerts; i++)
		{
			int vertIndex = revIndexMap[i];
			const CMeshVertex& V0 = VERT(vertIndex);
			const CSkelMeshVertex& V = static_cast<const CSkelMeshVertex&>(V0);

			int16 Bones[NUM_INFLUENCES];
			static_assert(NUM_INFLUENCES == 4, "Code designed for 4 influences");
			static_assert(sizeof(Bones) == sizeof(V.Bone), "Unexpected V.Bones size");
			memcpy(Bones, V.Bone, sizeof(Bones));
			for (int j = 0; j < NUM_INFLUENCES; j++)
			{
				// We have INDEX_NONE as list terminator, should replace with something else for glTF
				if (Bones[j] == INDEX_NONE)
				{
					Bones[j] = 0;
				}
			}

			BonesBuf->Put(*(uint64*)&Bones);
			WeightsBuf->Put(V.PackedWeights);
		}
	}

	// Secondary UVs
	for (int uvIndex = 1; uvIndex < Lod.NumTexCoords; uvIndex++)
	{
		BufferData* pBuf = UVBuf[uvIndex];
		const CMeshUVFloat* srcUV = Lod.ExtraUV[uvIndex-1];
		for (int i = 0; i < numLocalVerts; i++)
		{
			int vertIndex = revIndexMap[i];
			pBuf->Put(srcUV[vertIndex]);
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
	if (Context.IsSkeletal())
	{
		Ar.Printf(
			"            \"JOINTS_0\" : %d,\n"
			"            \"WEIGHTS_0\" : %d,\n",
			BonesBufIndex, WeightsBufIndex
		);
	}
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

struct CMat4
{
	float v[16];

	CMat4(const CCoords& C)
	{
		v[0] = C.axis[0][0];
		v[1] = C.axis[0][1];
		v[2] = C.axis[0][2];
		v[3] = 0;
		v[4] = C.axis[1][0];
		v[5] = C.axis[1][1];
		v[6] = C.axis[1][2];
		v[7] = 0;
		v[8] = C.axis[2][0];
		v[9] = C.axis[2][1];
		v[10] = C.axis[2][2];
		v[11] = 0;
		v[12] = C.origin[0];
		v[13] = C.origin[1];
		v[14] = C.origin[2];
		v[15] = 1;
	}
};

static void ExportSkinData(ExportContext& Context, const CSkelMeshLod& Lod, FArchive& Ar)
{
	guard(ExportSkinData);

	int numBones = Context.SkelMesh->RefSkeleton.Num();

	int MatrixBufIndex = Context.Data.AddZeroed();
	BufferData& MatrixBuf = Context.Data[MatrixBufIndex];
	MatrixBuf.Setup(numBones, "MAT4", BufferData::FLOAT, sizeof(CMat4));

	Ar.Printf(
		"  \"nodes\" : [\n"
		"    {\n"
		"      \"name\" : \"%s\",\n"
		"      \"mesh\" : 0,\n"
		"      \"skin\" : 0,\n"
		"      \"children\" : [ 1 ]\n"
		"    },\n",
		Context.MeshName);

	TArray<CCoords> BoneCoords;
	BoneCoords.AddZeroed(numBones);

	for (int boneIndex = 0; boneIndex < numBones; boneIndex++)
	{
		const CSkelMeshBone& B = Context.SkelMesh->RefSkeleton[boneIndex];

		// Find all children
		TStaticArray<int, 32> children;
		for (int j = 0; j < numBones; j++)
		{
			if (boneIndex == j) continue;
			const CSkelMeshBone& B2 = Context.SkelMesh->RefSkeleton[j];
			if (B2.ParentIndex == boneIndex)
			{
				children.Add(j);
			}
		}

		Ar.Printf(
			"    {\n"
			"      \"name\" : \"%s\",\n",
			*B.Name
		);

		// Write children
		if (children.Num())
		{
			Ar.Printf("      \"children\" : [ %d", children[0]+1);
			for (int j = 1; j < children.Num(); j++)
			{
				Ar.Printf(", %d", children[j]+1);
			}
			Ar.Printf(" ],\n");
		}

		// Bone transform
		CVec3 bonePos = B.Position;
		CQuat boneRot = B.Orientation;
		TransformPosition(bonePos);
		TransformRotation(boneRot);

		Ar.Printf(
			"      \"translation\" : [ %g, %g, %g ],\n"
			"      \"rotation\" : [ %g, %g, %g, %g ]\n",
			bonePos[0], bonePos[1], bonePos[2],
			boneRot.x, boneRot.y, boneRot.z, boneRot.w
		);

		boneRot.w *= -1;

		CCoords& BC = BoneCoords[boneIndex];
		BC.origin = bonePos;
		boneRot.ToAxis(BC.axis);
		if (boneIndex)
		{
			// World coordinate
			BoneCoords[B.ParentIndex].UnTransformCoords(BC, BC);
		}
		CCoords InvCoords;
		InvertCoords(BC, InvCoords);

		CMat4 BC4x4(InvCoords);
		MatrixBuf.Put(BC4x4);

		// Closing brace
		Ar.Printf(
			"    }%s\n",
			boneIndex == (numBones-1) ? "" : ","
		);
	}

	// Close "nodes" array
	Ar.Printf("  ],\n");

	// Make "skins"
	Ar.Printf(
		"  \"skins\" : [\n"
		"    {\n"
		"      \"inverseBindMatrices\" : %d,\n"
		"      \"skeleton\" : 1,\n"
		"      \"joints\" : [",
		MatrixBufIndex
	);
	for (int i = 0; i < numBones; i++)
	{
		if ((i & 31) == 0) Ar.Printf("\n        ");
		Ar.Printf("%d%s", i+1, (i == numBones-1) ? "" : ",");
	}
	Ar.Printf(
		"\n"
		"      ]\n"
		"    }\n"
		"  ],\n"
	);

	unguard;
}

static void ExportMeshLod(ExportContext& Context, const CBaseMeshLod& Lod, const CMeshVertex* Verts, FArchive& Ar, FArchive& Ar2)
{
	guard(ExportMeshLod);

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
		"  \"scene\" : 0,\n"
		"  \"scenes\" : [\n"
		"    {\n"
		"      \"nodes\" : [ 0 ]\n"
		"    }\n"
		"  ],\n"
	);

	// Nodes
	if (!Context.IsSkeletal())
	{
		Ar.Printf(
			"  \"nodes\" : [\n"
			"    {\n"
			"      \"name\" : \"%s\",\n"
			"      \"mesh\" : 0\n"
			"    }\n"
			"  ],\n",
			Context.MeshName
		);
	}
	else
	{
		ExportSkinData(Context, static_cast<const CSkelMeshLod&>(Lod), Ar);
	}

	// Materials
	Ar.Printf("  \"materials\" : [\n");
	for (int i = 0; i < Lod.Sections.Num(); i++)
	{
		ExportMaterial(Lod.Sections[i].Material, Ar, i, i == Lod.Sections.Num() - 1);
	}
	Ar.Printf("  ],\n");

	// Meshes
	Ar.Printf(
		"  \"meshes\" : [\n"
		"    {\n"
		"      \"primitives\" : [\n"
	);
	for (int i = 0; i < Lod.Sections.Num(); i++)
	{
		ExportSection(Context, Lod, Verts, i, Ar);
	}
	Ar.Printf(
		"      ],\n"
		"      \"name\" : \"%s\"\n"
		"    }\n"
		"  ],\n",
		Context.MeshName
	);

	int bufferLength = 0;
	for (int i = 0; i < Context.Data.Num(); i++)
	{
		bufferLength += Context.Data[i].DataSize;
	}

	// Write buffer
	Ar.Printf(
		"  \"buffers\" : [\n"
		"    {\n"
		"      \"uri\" : \"%s.bin\",\n"
		"      \"byteLength\" : %d\n"
		"    }\n"
		"  ],\n",
		Context.MeshName, bufferLength
	);

	// Write bufferViews
	Ar.Printf(
		"  \"bufferViews\" : [\n"
	);
	int bufferOffset = 0;
	for (int i = 0; i < Context.Data.Num(); i++)
	{
		const BufferData& B = Context.Data[i];
		Ar.Printf(
			"    {\n"
			"      \"buffer\" : 0,\n"
			"      \"byteOffset\" : %d,\n"
			"      \"byteLength\" : %d\n"
			"    }%s\n",
			bufferOffset,
			B.DataSize,
			i == (Context.Data.Num()-1) ? "" : ","
		);
		bufferOffset += B.DataSize;
	}
	Ar.Printf(
		"  ],\n"
	);

	// Write accessors
	Ar.Printf(
		"  \"accessors\" : [\n"
	);
	for (int i = 0; i < Context.Data.Num(); i++)
	{
		const BufferData& B = Context.Data[i];
		Ar.Printf(
			"    {\n"
			"      \"bufferView\" : %d,\n",
			i
		);
		if (B.bNormalized)
		{
			Ar.Printf("      \"normalized\" : true,\n");
		}
		if (B.BoundsMin.Len())
		{
			Ar.Printf(
				"      \"min\" : %s,\n"
				"      \"max\" : %s,\n",
				*B.BoundsMin, *B.BoundsMax
			);
		}
		Ar.Printf(
			"      \"componentType\" : %d,\n"
			"      \"count\" : %d,\n"
			"      \"type\" : \"%s\"\n"
			"    }%s\n",
			B.ComponentType,
			B.Count,
			B.Type,
			i == (Context.Data.Num()-1) ? "" : ","
		);
	}
	Ar.Printf(
		"  ]\n"
	);

	// Write binary data
	guard(WriteBIN);
	for (int i = 0; i < Context.Data.Num(); i++)
	{
		const BufferData& B = Context.Data[i];
#if MAX_DEBUG
		assert(B.FillCount == B.Count);
#endif
		Ar2.Serialize(B.Data, B.DataSize);
	}
	unguard;

	// Closing brace
	Ar.Printf("}\n");

	unguard;
}

void ExportSkeletalMeshGLTF(const CSkeletalMesh* Mesh)
{
	guard(ExportSkeletalMeshGLTF);

	UObject *OriginalMesh = Mesh->OriginalMesh;
	if (!Mesh->Lods.Num())
	{
		appNotify("Mesh %s has 0 lods", OriginalMesh->Name);
		return;
	}

	FArchive* Ar = CreateExportArchive(OriginalMesh, FAO_TextFile, "%s.gltf", OriginalMesh->Name);
	if (Ar)
	{
		ExportContext Context;
		Context.MeshName = OriginalMesh->Name;
		Context.SkelMesh = Mesh;

		FArchive* Ar2 = CreateExportArchive(OriginalMesh, 0, "%s.bin", OriginalMesh->Name);
		assert(Ar2);
		ExportMeshLod(Context, Mesh->Lods[0], Mesh->Lods[0].Verts, *Ar, *Ar2);
		delete Ar;
		delete Ar2;
	}

	unguard;
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
		ExportContext Context;
		Context.MeshName = OriginalMesh->Name;
		Context.StatMesh = Mesh;

		FArchive* Ar2 = CreateExportArchive(OriginalMesh, 0, "%s.bin", OriginalMesh->Name);
		assert(Ar2);
		ExportMeshLod(Context, Mesh->Lods[0], Mesh->Lods[0].Verts, *Ar, *Ar2);
		delete Ar;
		delete Ar2;
	}

	unguard;
}
