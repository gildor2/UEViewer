#include "Core.h"

#if UNREAL4

#include "UnCore.h"
#include "UE4Version.h"

#include "UnObject.h"
#include "UnMesh4.h"
#include "UnMesh3.h"				// for FSkeletalMeshLODInfo
#include "UnMeshTypes.h"
#include "UnMathTools.h"			// for FRotator to FCoords

#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMaterial/UnMaterial3.h"

#include "Mesh/SkeletalMesh.h"
#include "Mesh/StaticMesh.h"
#include "TypeConvert.h"


//#define DEBUG_SKELMESH		1
//#define DEBUG_STATICMESH		1

#if DEBUG_SKELMESH
#define DBG_SKEL(...)			appPrintf(__VA_ARGS__);
#else
#define DBG_SKEL(...)
#endif

#if DEBUG_STATICMESH
#define DBG_STAT(...)			appPrintf(__VA_ARGS__);
#else
#define DBG_STAT(...)
#endif

#if DEBUG_SKELMESH || DEBUG_STATICMESH
#define DBG_MESH(...)			appPrintf(__VA_ARGS__);
#else
#define DBG_MESH(...)
#endif


#if NUM_INFLUENCES_UE4 != NUM_INFLUENCES
//!!#error NUM_INFLUENCES_UE4 and NUM_INFLUENCES are not matching!
#endif

#if MAX_SKELETAL_UV_SETS_UE4 > MAX_MESH_UV_SETS
#error MAX_SKELETAL_UV_SETS_UE4 too large
#endif

#if MAX_STATIC_UV_SETS_UE4 > MAX_MESH_UV_SETS
#error MAX_STATIC_UV_SETS_UE4 too large
#endif

// This is a CVar in UE4.27+. Set to false by default, some games might have it set to "true".
static bool KeepMobileMinLODSettingOnDesktop = false;

/*-----------------------------------------------------------------------------
	Common data types
-----------------------------------------------------------------------------*/

struct FColorVertexBuffer4
{
	int32			Stride;
	int32			NumVertices;
	TArray<FColor>	Data;

	friend FArchive& operator<<(FArchive& Ar, FColorVertexBuffer4& S)
	{
		guard(FColorVertexBuffer4<<);

		FStripDataFlags StripFlags(Ar, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);
		Ar << S.Stride << S.NumVertices;
		DBG_MESH("ColorStream: IS:%d NV:%d\n", S.Stride, S.NumVertices);
		if (!StripFlags.IsDataStrippedForServer() && (S.NumVertices > 0)) // zero size arrays are not serialized
			S.Data.BulkSerialize(Ar);
		return Ar;

		unguard;
	}
};

struct FPositionVertexBuffer4
{
	TArray<FVector>	Verts;
	int32			Stride;
	int32			NumVertices;

	friend FArchive& operator<<(FArchive& Ar, FPositionVertexBuffer4& S)
	{
		guard(FPositionVertexBuffer4<<);

		Ar << S.Stride << S.NumVertices;
		DBG_MESH("PositionStream: IS:%d NV:%d\n", S.Stride, S.NumVertices);

#if DAYSGONE
		if (Ar.Game == GAME_DaysGone)
		{
			// This is used only for StaticMesh
			if (S.Stride == 8)
			{
				TArray<FDGPackedVector64> PackedVerts;
				PackedVerts.BulkSerialize(Ar);
				S.Verts.AddUninitialized(PackedVerts.Num());
				for (int i = 0; i < PackedVerts.Num(); i++)
				{
					S.Verts[i] = PackedVerts[i];
				}
				//todo: should use some bounds to unpack positions, almost works but a bit distorted data
				return Ar;
			}
			else if (S.Stride == 4)
			{
				TArray<FDGPackedVector32> PackedVerts;
				// No idea which format is used here
				PackedVerts.BulkSerialize(Ar);
				S.Verts.AddUninitialized(PackedVerts.Num());
				for (int i = 0; i < PackedVerts.Num(); i++)
				{
					S.Verts[i] = PackedVerts[i];
				}
				//todo: should use some bounds to unpack positions; position is 32-bit, with full use of all bits
				return Ar;
			}
			// else - use standard format
			assert(S.Stride == 12);
		}
#endif // DAYSGONE

		S.Verts.BulkSerialize(Ar);
		return Ar;

		unguard;
	}
};

//?? TODO: rename, because these vars are now used for both mesh types (see FStaticMeshVertexBuffer4)
static int  GNumStaticUVSets   = 1;
static bool GUseStaticFloatUVs = true;
static bool GUseHighPrecisionTangents = false;

struct FStaticMeshUVItem4
{
	FPackedNormal	Normal[3];					//?? do we need 3 items here?
	FMeshUVFloat	UV[MAX_STATIC_UV_SETS_UE4];

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshUVItem4& V)
	{
		SerializeTangents(Ar, V);
		SerializeTexcoords(Ar, V);
		return Ar;
	}

	static void SerializeTangents(FArchive& Ar, FStaticMeshUVItem4& V)
	{
		if (!GUseHighPrecisionTangents)
		{
			Ar << V.Normal[0] << V.Normal[2];	// TangentX and TangentZ
		}
		else
		{
			FPackedRGBA16N Normal, Tangent;
			Ar << Normal << Tangent;
			V.Normal[0] = Normal.ToPackedNormal();
			V.Normal[2] = Tangent.ToPackedNormal();
		}
	}

	static void SerializeTexcoords(FArchive& Ar, FStaticMeshUVItem4& V)
	{
		if (GUseStaticFloatUVs)
		{
			for (int i = 0; i < GNumStaticUVSets; i++)
				Ar << V.UV[i];
		}
		else
		{
			for (int i = 0; i < GNumStaticUVSets; i++)
			{
				// read in half format and convert to float
				FMeshUVHalf UVHalf;
				Ar << UVHalf;
				V.UV[i] = UVHalf;		// convert
			}
		}
	}
};

// Note: this structure is used for both StaticMesh and SkeletalMesh
struct FStaticMeshVertexBuffer4
{
	int32			NumTexCoords;
	int32			Stride;
	int32			NumVertices;
	bool			bUseFullPrecisionUVs;
	bool			bUseHighPrecisionTangentBasis;
	TArray<FStaticMeshUVItem4> UV;

	FStaticMeshVertexBuffer4()
		: NumTexCoords(0)
		, NumVertices(0)
	{}

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshVertexBuffer4& S)
	{
		guard(FStaticMeshVertexBuffer4<<);

		S.bUseHighPrecisionTangentBasis = false;
		S.Stride = -1;

		FStripDataFlags StripFlags(Ar, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

		// FStaticMeshVertexBuffer::SerializeMetaData()
		Ar << S.NumTexCoords;
		if (Ar.Game < GAME_UE4(19))
		{
			Ar << S.Stride;				// this field disappeared in 4.19, with no version checks
		}
		Ar << S.NumVertices;
		Ar << S.bUseFullPrecisionUVs;
		if (Ar.Game >= GAME_UE4(12))
		{
			Ar << S.bUseHighPrecisionTangentBasis;
		}
		GUseHighPrecisionTangents = S.bUseHighPrecisionTangentBasis;
		DBG_MESH("UV stream: TC:%d IS:%d NV:%d FloatUV:%d HQ_Tangent:%d\n", S.NumTexCoords, S.Stride, S.NumVertices, S.bUseFullPrecisionUVs, S.bUseHighPrecisionTangentBasis);

		if (!StripFlags.IsDataStrippedForServer())
		{
			GNumStaticUVSets = S.NumTexCoords;
			GUseStaticFloatUVs = S.bUseFullPrecisionUVs;

			if (Ar.Game < GAME_UE4(19))
			{
				S.UV.BulkSerialize(Ar);
			}
			else
			{
				// In 4.19 tangents and texture coordinates are stored in separate vertex streams
				S.UV.AddZeroed(S.NumVertices);

#if JEDI
				if (Ar.Game == GAME_Jedi)
				{
					int32 bDropNormals;
					Ar << bDropNormals;
					assert(bDropNormals == 0 || bDropNormals == 1); //todo: replace with bool
					if (bDropNormals)
					{
						//!! todo: no tangents, should indicate that tangents should be recreated
						goto texcoords;
					}
				}
#endif // JEDI

				{
					// Tangents: simulate TArray::BulkSerialize()
					int32 ItemSize, ItemCount;
					Ar << ItemSize << ItemCount;
					DBG_MESH("... tangents: %d items by %d bytes\n", ItemCount, ItemSize);
					assert(ItemCount == S.NumVertices);
					int Pos = Ar.Tell();
					for (int i = 0; i < S.NumVertices; i++)
					{
						FStaticMeshUVItem4::SerializeTangents(Ar, S.UV[i]);
					}
					assert(Ar.Tell() - Pos == ItemCount * ItemSize);
				}

				{
				texcoords:
					// Texture coordinates: simulate TArray::BulkSerialize()
					int32 ItemSize, ItemCount;
					Ar << ItemSize << ItemCount;
					DBG_MESH("... texcoords: %d items by %d bytes\n", ItemCount, ItemSize);
					assert(ItemCount == S.NumVertices * S.NumTexCoords);
					int Pos = Ar.Tell();
					for (int i = 0; i < S.NumVertices; i++)
					{
						FStaticMeshUVItem4::SerializeTexcoords(Ar, S.UV[i]);
					}
					assert(Ar.Tell() - Pos == ItemCount * ItemSize);
				}
			}
		}

		return Ar;

		unguard;
	}
};



/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/

// FSkeletalMeshLODInfo is in cpp file to let use forward declaration of some types
FSkeletalMeshLODInfo::FSkeletalMeshLODInfo()
{}

FSkeletalMeshLODInfo::~FSkeletalMeshLODInfo()
{}

struct FRecomputeTangentCustomVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.12
		RuntimeRecomputeTangent = 1,
		// UE4.26
		RecomputeTangentVertexColorMask = 2,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static int Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x5579F886, 0x933A4C1F, 0x83BA087B, 0x6361B92F };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;

		if (Ar.Game < GAME_UE4(12))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(26))
			return RuntimeRecomputeTangent;
//		if (Ar.Game < GAME_UE4(27))
			return RecomputeTangentVertexColorMask;
		// NEW_ENGINE_VERSION
//		return LatestVersion;
	}
};

struct FOverlappingVerticesCustomVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.19
		DetectOVerlappingVertices = 1,
	};

	static int Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x612FBE52, 0xDA53400B, 0x910D4F91, 0x9FB1857C };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;

		if (Ar.Game < GAME_UE4(19))
			return BeforeCustomVersionWasAdded;
		return DetectOVerlappingVertices;
	}
};

static int GNumSkelInfluences = 4;

// Bone influence mapping for skeletal mesh vertex
struct FSkinWeightInfo
{
	byte				BoneIndex[NUM_INFLUENCES_UE4];
	byte				BoneWeight[NUM_INFLUENCES_UE4];

	friend FArchive& operator<<(FArchive& Ar, FSkinWeightInfo& W)
	{
		if (GNumSkelInfluences <= ARRAY_COUNT(W.BoneIndex))
		{
			for (int i = 0; i < GNumSkelInfluences; i++)
				Ar << W.BoneIndex[i];
			for (int i = 0; i < GNumSkelInfluences; i++)
				Ar << W.BoneWeight[i];
		}
		else
		{
			// possibly this vertex has more vertex influences
			assert(GNumSkelInfluences <= MAX_TOTAL_INFLUENCES_UE4);
			// serialize influences
			byte BoneIndex2[MAX_TOTAL_INFLUENCES_UE4];
			byte BoneWeight2[MAX_TOTAL_INFLUENCES_UE4];
			for (int i = 0; i < GNumSkelInfluences; i++)
				Ar << BoneIndex2[i];
			for (int i = 0; i < GNumSkelInfluences; i++)
				Ar << BoneWeight2[i];
#if 0
			// check if sorting needed (possibly 2nd half of influences has zero weight)
			uint32 PackedWeight2 = * (uint32*) &BoneWeight2[4];
			if (PackedWeight2 != 0)
			{
//				printf("# %d %d %d %d %d %d %d %d\n", BoneWeight2[0],BoneWeight2[1],BoneWeight2[2],BoneWeight2[3],BoneWeight2[4],BoneWeight2[5],BoneWeight2[6],BoneWeight2[7]);
				// Here we assume than weights are sorted by value - didn't see the sorting code in UE4, but printing of data shows that.
				// Compute weight which should be distributed between other bones to keep sum of weights identity.
				int ExtraWeight = 0;
				for (int i = NUM_INFLUENCES_UE4; i < MAX_TOTAL_INFLUENCES_UE4; i++)
					ExtraWeight += BoneWeight2[i];
				int WeightPerBone = ExtraWeight / NUM_INFLUENCES_UE4; // note: could be division with remainder!
				for (int i = 0; i < NUM_INFLUENCES_UE4; i++)
				{
					BoneWeight2[i] += WeightPerBone;
					ExtraWeight -= WeightPerBone;
				}
				// add remaining weight to the first bone
				BoneWeight2[0] += ExtraWeight;
			}
#endif
			// copy influences to vertex
			for (int i = 0; i < NUM_INFLUENCES_UE4; i++)
			{
				W.BoneIndex[i] = BoneIndex2[i];
				W.BoneWeight[i] = BoneWeight2[i];
			}
		}
		return Ar;
	}
};

struct FSkelMeshVertexBase
{
	FVector				Pos;
	FPackedNormal		Normal[3];		// Normal[1] (TangentY) is reconstructed from other 2 normals
						//!! TODO: we're cutting down influences, but holding unused normal here!
						//!! Should rename Normal[] and split into 2 separate fields
	FSkinWeightInfo		Infs;

	void SerializeForGPU(FArchive& Ar)
	{
		Ar << Normal[0] << Normal[2];
		if (FSkeletalMeshCustomVersion::Get(Ar) < FSkeletalMeshCustomVersion::UseSeparateSkinWeightBuffer)
		{
			// serialized as separate buffer starting with UE4.15
			Ar << Infs;
		}
		Ar << Pos;
	}

	void SerializeForEditor(FArchive& Ar)
	{
		Ar << Pos;
		if (FRenderingObjectVersion::Get(Ar) < FRenderingObjectVersion::IncreaseNormalPrecision)
		{
			Ar << Normal[0] << Normal[1] << Normal[2];
		}
		else
		{
			// New normals are stored with full floating point precision
			FVector NewNormalX, NewNormalY;
			FVector4 NewNormalZ;
			Ar << NewNormalX << NewNormalY << NewNormalZ;
			Normal[0] = NewNormalX;
			Normal[1] = NewNormalY;
			Normal[2] = NewNormalZ;
		}
	}
};

// Editor-only skeletal mesh vertex with color and skin data
struct FSoftVertex4 : public FSkelMeshVertexBase
{
	FMeshUVFloat		UV[MAX_SKELETAL_UV_SETS_UE4];
	FColor				Color;

	friend FArchive& operator<<(FArchive &Ar, FSoftVertex4 &V)
	{
		V.SerializeForEditor(Ar);

		for (int i = 0; i < MAX_SKELETAL_UV_SETS_UE4; i++)
			Ar << V.UV[i];

		Ar << V.Color;
		Ar << V.Infs;

		return Ar;
	}
};

// Editor-only skeletal mesh vertex used for single bone (has been deprecated after
// FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
// TODO: review - we're using exactly the same data layout as for FSoftVertex4, but different
//   serialization function - can achieve that with TArray::Serialize2<>.
struct FRigidVertex4 : public FSoftVertex4
{
	friend FArchive& operator<<(FArchive &Ar, FRigidVertex4 &V)
	{
		V.SerializeForEditor(Ar);

		for (int i = 0; i < MAX_SKELETAL_UV_SETS_UE4; i++)
			Ar << V.UV[i];

		Ar << V.Color;
		// Serialize a single bone influence and store it into V.Infs
		Ar << V.Infs.BoneIndex[0];
		V.Infs.BoneWeight[0] = 255;

		return Ar;
	}
};

static int GNumSkelUVSets = 1;

// GPU vertex with float16 UV data
struct FGPUVert4Half : public FSkelMeshVertexBase
{
	FMeshUVHalf			UV[MAX_SKELETAL_UV_SETS_UE4];

	friend FArchive& operator<<(FArchive &Ar, FGPUVert4Half &V)
	{
		V.SerializeForGPU(Ar);
		for (int i = 0; i < GNumSkelUVSets; i++) Ar << V.UV[i];
		return Ar;
	}
};

// GPU vertex with float32 UV data
struct FGPUVert4Float : public FSkelMeshVertexBase
{
	FMeshUVFloat		UV[MAX_SKELETAL_UV_SETS_UE4];

	friend FArchive& operator<<(FArchive &Ar, FGPUVert4Float &V)
	{
		V.SerializeForGPU(Ar);
		for (int i = 0; i < GNumSkelUVSets; i++) Ar << V.UV[i];
		return Ar;
	}
};

struct FApexClothPhysToRenderVertData // new structure name: FMeshToMeshVertData
{
	FVector4				PositionBaryCoordsAndDist;
	FVector4				NormalBaryCoordsAndDist;
	FVector4				TangentBaryCoordsAndDist;
	int16					SimulMeshVertIndices[4];
	int						Padding[2];

	friend FArchive& operator<<(FArchive& Ar, FApexClothPhysToRenderVertData& V)
	{
		Ar << V.PositionBaryCoordsAndDist << V.NormalBaryCoordsAndDist << V.TangentBaryCoordsAndDist;
		Ar << V.SimulMeshVertIndices[0] << V.SimulMeshVertIndices[1] << V.SimulMeshVertIndices[2] << V.SimulMeshVertIndices[3];
		Ar << V.Padding[0] << V.Padding[1];
		return Ar;
	}
};

struct FClothingSectionData
{
	FGuid					AssetGuid;
	int32					AssetLodIndex;

	friend FArchive& operator<<(FArchive& Ar, FClothingSectionData& D)
	{
		Ar << D.AssetGuid << D.AssetLodIndex;
		return Ar;
	}
};

struct FSkeletalMaterial
{
	UMaterialInterface*		Material;
	bool					bEnableShadowCasting;
	FName					MaterialSlotName;
	FMeshUVChannelInfo		UVChannelData;

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMaterial& M)
	{
		guard(FSkeletalMaterial<<);

		Ar << M.Material;

#if DAYSGONE
		if (Ar.Game == GAME_DaysGone)
		{
			int32 unk1;
			Ar << unk1;
		}
#endif // DAYSGONE

		if (FEditorObjectVersion::Get(Ar) >= FEditorObjectVersion::RefactorMeshEditorMaterials)
		{
			Ar << M.MaterialSlotName;
			bool bSerializeImportedMaterialSlotName = Ar.ContainsEditorData();
			if (FCoreObjectVersion::Get(Ar) >= FCoreObjectVersion::SkeletalMaterialEditorDataStripping)
			{
				// UE4.22+
				Ar << bSerializeImportedMaterialSlotName;
			}
			if (bSerializeImportedMaterialSlotName)
			{
				FName ImportedMaterialSlotName;
				Ar << ImportedMaterialSlotName;
			}
		}
		else
		{
			if (Ar.ArVer >= VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING)
				Ar << M.bEnableShadowCasting;

			if (FRecomputeTangentCustomVersion::Get(Ar) >= FRecomputeTangentCustomVersion::RuntimeRecomputeTangent)
			{
				bool bRecomputeTangent;
				Ar << bRecomputeTangent;
			}
		}

#if LAWBREAKERS
		if (Ar.Game == GAME_Lawbreakers)
		{
			int32 unkBool;
			Ar << unkBool;
		}
#endif // LAWBREAKERS

		if (FRenderingObjectVersion::Get(Ar) >= FRenderingObjectVersion::TextureStreamingMeshUVChannelData)
		{
			Ar << M.UVChannelData;
		}

		return Ar;
		unguard;
	}
};

struct FDuplicatedVerticesBuffer
{
	friend FArchive& operator<<(FArchive& Ar, FDuplicatedVerticesBuffer& B)
	{
		SkipFixedArray(Ar, sizeof(int32));					// TArray<int32>
		SkipFixedArray(Ar, sizeof(uint32) + sizeof(uint32)); // TArray<FIndexLengthPair>
		return Ar;
	}
};

struct FSkelMeshSection4
{
	int16					MaterialIndex;
	int32					BaseIndex;
	int32					NumTriangles;
	bool					bDisabled;				// deprecated in UE4.19
	int32					GenerateUpToLodIndex;	// added in UE4.20
	int16					CorrespondClothSectionIndex; // deprecated in UE4.19

	// Data from FSkelMeshChunk, appeared in FSkelMeshSection after UE4.13
	int32					NumVertices;
	uint32					BaseVertexIndex;
	TArray<FSoftVertex4>	SoftVertices;			// editor-only data
	TArray<uint16>			BoneMap;
	int32					MaxBoneInfluences;
	bool					HasClothData;
	// UE4.14
	bool					bCastShadow;

	friend FArchive& operator<<(FArchive& Ar, FSkelMeshSection4& S)
	{
		guard(FSkelMeshSection4<<);

		FSkeletalMeshCustomVersion::Type SkelMeshVer = FSkeletalMeshCustomVersion::Get(Ar);

		FStripDataFlags StripFlags(Ar);

		Ar << S.MaterialIndex;

		if (SkelMeshVer < FSkeletalMeshCustomVersion::CombineSectionWithChunk)
		{
			int16 ChunkIndex;
			Ar << ChunkIndex;
		}

		if (!StripFlags.IsDataStrippedForServer())
		{
			Ar << S.BaseIndex;
			Ar << S.NumTriangles;
		}
		if (SkelMeshVer < FSkeletalMeshCustomVersion::RemoveTriangleSorting)
		{
			byte TriangleSorting;		// TEnumAsByte<ETriangleSortOption>
			Ar << TriangleSorting;
		}

		if (Ar.ArVer >= VER_UE4_APEX_CLOTH)
		{
			if (SkelMeshVer < FSkeletalMeshCustomVersion::DeprecateSectionDisabledFlag)
				Ar << S.bDisabled;
			if (SkelMeshVer < FSkeletalMeshCustomVersion::RemoveDuplicatedClothingSections)
				Ar << S.CorrespondClothSectionIndex;
		}

		if (Ar.ArVer >= VER_UE4_APEX_CLOTH_LOD)
		{
			byte bEnableClothLOD_DEPRECATED;
			Ar << bEnableClothLOD_DEPRECATED;
		}

		if (FRecomputeTangentCustomVersion::Get(Ar) >= FRecomputeTangentCustomVersion::RuntimeRecomputeTangent)
		{
			bool bRecomputeTangent;
			Ar << bRecomputeTangent;
		}

		if (FRecomputeTangentCustomVersion::Get(Ar) >= FRecomputeTangentCustomVersion::RecomputeTangentVertexColorMask)
		{
			uint8 RecomputeTangentsVertexMaskChannel;
			Ar << RecomputeTangentsVertexMaskChannel;
		}

		if (FEditorObjectVersion::Get(Ar) >= FEditorObjectVersion::RefactorMeshEditorMaterials)
		{
			Ar << S.bCastShadow;
		}

		// UE4.13+ stuff
		S.HasClothData = false;
		if (SkelMeshVer >= FSkeletalMeshCustomVersion::CombineSectionWithChunk)
		{
			if (!StripFlags.IsDataStrippedForServer())
			{
				Ar << S.BaseVertexIndex;
			}
			if (!StripFlags.IsEditorDataStripped())
			{
				if (SkelMeshVer < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
				{
					TArray<FRigidVertex4> RigidVertices;
					Ar << RigidVertices;
					if (RigidVertices.Num()) appNotify("TODO: %s: dropping rigid vertices", __FUNCTION__);
					// these vertices should be converted to FSoftVertex4, but we're dropping this data anyway
				}
				GNumSkelInfluences = (Ar.ArVer >= VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES) ? MAX_TOTAL_INFLUENCES_UE4 : NUM_INFLUENCES_UE4;
				Ar << S.SoftVertices;
			}
			Ar << S.BoneMap;
			if (SkelMeshVer >= FSkeletalMeshCustomVersion::SaveNumVertices)
				Ar << S.NumVertices;
			if (SkelMeshVer < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				int32 NumRigidVerts, NumSoftVerts;
				Ar << NumRigidVerts << NumSoftVerts;
			}
			Ar << S.MaxBoneInfluences;

			// Physics data, drop
			TArray<FApexClothPhysToRenderVertData> ClothMappingData;
			TArray<FVector> PhysicalMeshVertices;
			TArray<FVector> PhysicalMeshNormals;
			int16 CorrespondClothAssetIndex;
			int16 ClothAssetSubmeshIndex;

			Ar << ClothMappingData;

			if (SkelMeshVer < FSkeletalMeshCustomVersion::RemoveDuplicatedClothingSections)
			{
				Ar << PhysicalMeshVertices << PhysicalMeshNormals;
			}

			Ar << CorrespondClothAssetIndex;

			if (SkelMeshVer < FSkeletalMeshCustomVersion::NewClothingSystemAdded)
			{
				Ar << ClothAssetSubmeshIndex;
			}
			else
			{
				// UE4.16+
				FClothingSectionData ClothingData;
				Ar << ClothingData;
			}

			S.HasClothData = ClothMappingData.Num() > 0;

#if NGB
			if (Ar.Game == GAME_NGB)
			{
				int32 unk;
				Ar << unk;
			}
#endif // NGB

#if KH3
			if (Ar.Game == GAME_KH3)
			{
				int32 unk1, unk2;		// bool + array count
				Ar << unk1 << unk2;
				if (unk1)
				{
					// "npc/n_ca201/mdl/n_ca201_0.uasset" has this, however it crashes anyway
					Ar.Seek(Ar.Tell() + unk2 * 24);
				}
			}
#endif // KH3
			// UE4.19+
			//todo: this code is never reached? (this function is for pre-4.19)
			//todo: review - may be the function is still used for editor data? Should update function comment.
			if (FOverlappingVerticesCustomVersion::Get(Ar) >= FOverlappingVerticesCustomVersion::DetectOVerlappingVertices)
			{
				TMap<int32, TArray<int32> > OverlappingVertices;
				Ar << OverlappingVertices;
			}
			if (FReleaseObjectVersion::Get(Ar) >= FReleaseObjectVersion::AddSkeletalMeshSectionDisable)
			{
				Ar << S.bDisabled;
			}
			if (FSkeletalMeshCustomVersion::Get(Ar) >= FSkeletalMeshCustomVersion::SectionIgnoreByReduceAdded)
			{
				Ar << S.GenerateUpToLodIndex;
			}
		}

		return Ar;

		unguard;
	}

	// UE4.19+. Prototype: FSkelMeshRenderSection's operator<<
	static void SerializeRenderItem(FArchive& Ar, FSkelMeshSection4& S)
	{
		guard(FSkelMeshSection4::SerializeRenderItem);

		FStripDataFlags StripFlags(Ar);

		Ar << S.MaterialIndex;
		Ar << S.BaseIndex;
		Ar << S.NumTriangles;

#if PARAGON
		if (Ar.Game == GAME_Paragon)
		{
			bool bSomething;
			Ar << bSomething;
		}
#endif // PARAGON

		bool bRecomputeTangent;
		Ar << bRecomputeTangent;
		if (FRecomputeTangentCustomVersion::Get(Ar) >= FRecomputeTangentCustomVersion::RecomputeTangentVertexColorMask)
		{
			uint8 RecomputeTangentsVertexMaskChannel;
			Ar << RecomputeTangentsVertexMaskChannel;
		}

		Ar << S.bCastShadow;
		Ar << S.BaseVertexIndex;

		TArray<FApexClothPhysToRenderVertData> ClothMappingData;
		Ar << ClothMappingData;
		S.HasClothData = (ClothMappingData.Num() > 0);

		Ar << S.BoneMap;
		Ar << S.NumVertices;
		Ar << S.MaxBoneInfluences;

		int16 CorrespondClothAssetIndex;
		Ar << CorrespondClothAssetIndex;

		FClothingSectionData ClothingData;
		Ar << ClothingData;

#if PARAGON
		if (Ar.Game == GAME_Paragon) return;
#endif

		if (Ar.Game < GAME_UE4(23) || !StripFlags.IsClassDataStripped(1)) // DuplicatedVertices, introduced in UE4.23
		{
			FDuplicatedVerticesBuffer DuplicatedVerticesBuffer;
			Ar << DuplicatedVerticesBuffer;
		}
		Ar << S.bDisabled;

		unguard;
	}
};

struct FMultisizeIndexContainer
{
	TArray<uint16>			Indices16;
	TArray<uint32>			Indices32;

	FORCEINLINE bool Is32Bit() const
	{
		return (Indices32.Num() != 0);
	}

	// very similar to UE3 version (FSkelIndexBuffer3 in UnMesh3.cpp)
	//?? combine with UE3 version?
	friend FArchive& operator<<(FArchive& Ar, FMultisizeIndexContainer& B)
	{
		guard(FMultisizeIndexContainer<<);

		if (Ar.ArVer < VER_UE4_KEEP_SKEL_MESH_INDEX_DATA)
		{
			bool bOldNeedCPUAccess;
			Ar << bOldNeedCPUAccess;
		}
		byte DataSize;
		Ar << DataSize;

		if (DataSize == 2)
		{
			B.Indices16.BulkSerialize(Ar);
		}
		else
		{
#if FABLE
			if (Ar.Game == GAME_FableLegends && DataSize == 0) return Ar;
#endif
			// Some PARAGON meshes has DataSize=0, but works well when assuming this is 32-bit integers array
			if (DataSize != 4)
				appPrintf("WARNING: FMultisizeIndexContainer data size %d, assuming int32\n", DataSize);
			B.Indices32.BulkSerialize(Ar);
		}

		return Ar;

		unguard;
	}
};

struct FSkelMeshChunk4
{
	int						BaseVertexIndex;
	TArray<FRigidVertex4>	RigidVertices;		// editor-only data
	TArray<FSoftVertex4>	SoftVertices;		// editor-only data
	TArray<uint16>			BoneMap;
	int						NumRigidVertices;
	int						NumSoftVertices;
	int						MaxBoneInfluences;
	bool					HasClothData;

	friend FArchive& operator<<(FArchive& Ar, FSkelMeshChunk4& C)
	{
		guard(FSkelMeshChunk4<<);

		FSkeletalMeshCustomVersion::Type SkelMeshVer = FSkeletalMeshCustomVersion::Get(Ar);
		assert(SkelMeshVer < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts); // no more chunks since 4.13

		FStripDataFlags StripFlags(Ar);

		if (!StripFlags.IsDataStrippedForServer())
			Ar << C.BaseVertexIndex;

		if (!StripFlags.IsEditorDataStripped())
		{
			GNumSkelInfluences = (Ar.ArVer >= VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES) ? MAX_TOTAL_INFLUENCES_UE4 : NUM_INFLUENCES_UE4;
			Ar << C.RigidVertices << C.SoftVertices;
			if (C.RigidVertices.Num()) appNotify("TODO: %s: dropping rigid vertices", __FUNCTION__);
		}

		Ar << C.BoneMap << C.NumRigidVertices << C.NumSoftVertices << C.MaxBoneInfluences;

		C.HasClothData = false;
		if (Ar.ArVer >= VER_UE4_APEX_CLOTH)
		{
			// Physics data, drop
			TArray<FApexClothPhysToRenderVertData> ClothMappingData;
			TArray<FVector> PhysicalMeshVertices;
			TArray<FVector> PhysicalMeshNormals;
			int16 CorrespondClothAssetIndex;
			int16 ClothAssetSubmeshIndex;

			Ar << ClothMappingData;
			Ar << PhysicalMeshVertices << PhysicalMeshNormals;
			Ar << CorrespondClothAssetIndex << ClothAssetSubmeshIndex;

			C.HasClothData = ClothMappingData.Num() > 0;
		}

		return Ar;

		unguard;
	}
};

struct FSkeletalMeshVertexBuffer4
{
	int32					NumTexCoords;
	FVector					MeshExtension;		// not used in engine - there's no support for packed position (look for "FPackedPosition")
	FVector					MeshOrigin;			// ...
	bool					bUseFullPrecisionUVs;
	bool					bExtraBoneInfluences;
	TArray<FGPUVert4Half>	VertsHalf;
	TArray<FGPUVert4Float>	VertsFloat;


	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshVertexBuffer4& B)
	{
		guard(FSkeletalMeshVertexBuffer4<<);

		DBG_SKEL("VertexBuffer:\n");
		FStripDataFlags StripFlags(Ar, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);
		Ar << B.NumTexCoords << B.bUseFullPrecisionUVs;
		DBG_SKEL("  TC=%d FullPrecision=%d\n", B.NumTexCoords, B.bUseFullPrecisionUVs);
		assert(B.NumTexCoords > 0 && B.NumTexCoords < 32); // just verify for some reasonable value

		if (Ar.ArVer >= VER_UE4_SUPPORT_GPUSKINNING_8_BONE_INFLUENCES &&
			FSkeletalMeshCustomVersion::Get(Ar) < FSkeletalMeshCustomVersion::UseSeparateSkinWeightBuffer)
		{
			Ar << B.bExtraBoneInfluences;
			DBG_SKEL("  ExtraInfs=%d\n", B.bExtraBoneInfluences);
		}

		Ar << B.MeshExtension << B.MeshOrigin;
		DBG_SKEL("  Ext=(%g %g %g) Org=(%g %g %g)\n", VECTOR_ARG(B.MeshExtension), VECTOR_ARG(B.MeshOrigin));

		// Serialize vertex data. Use global variables to avoid passing variables to serializers.
		GNumSkelUVSets = B.NumTexCoords;
		GNumSkelInfluences = B.bExtraBoneInfluences ? MAX_TOTAL_INFLUENCES_UE4 : NUM_INFLUENCES_UE4;
		if (!B.bUseFullPrecisionUVs)
			B.VertsHalf.BulkSerialize(Ar);
		else
			B.VertsFloat.BulkSerialize(Ar);
		DBG_SKEL("  Verts: Half[%d] Float[%d]\n", B.VertsHalf.Num(), B.VertsFloat.Num());

		return Ar;

		unguard;
	}

	inline int GetVertexCount() const
	{
		if (VertsHalf.Num()) return VertsHalf.Num();
		if (VertsFloat.Num()) return VertsFloat.Num();
		return 0;
	}
};

// Used before 4.15. After 4.15 switched to FColorVertexBuffer4
struct FSkeletalMeshVertexColorBuffer4
{
	TArray<FColor>				Data;

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshVertexColorBuffer4& B)
	{
		guard(FSkeletalMeshVertexColorBuffer4<<);
		FStripDataFlags StripFlags(Ar, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);
		if (!StripFlags.IsDataStrippedForServer())
			B.Data.BulkSerialize(Ar);
		return Ar;
		unguard;
	}
};

struct FSkeletalMeshVertexClothBuffer
{
	// don't need physics - don't serialize any data, simply skip them
	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshVertexClothBuffer& B)
	{
		FStripDataFlags StripFlags(Ar, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);
		if (!StripFlags.IsDataStrippedForServer())
		{
			DBG_SKEL("Dropping Cloth\n");
			SkipBulkArrayData(Ar);
			if (FSkeletalMeshCustomVersion::Get(Ar) >= FSkeletalMeshCustomVersion::CompactClothVertexBuffer)
			{
				TArray<uint64> ClothIndexMapping;
				Ar << ClothIndexMapping;
			}
		}
		return Ar;
	}
};

// Starting with UE4.15 influences are serialized as separate buffer.
struct FSkinWeightVertexBuffer
{
	TArray<FSkinWeightInfo> Weights;

	static int MetadataSize(FArchive& Ar)
	{
		// FSkinWeightDataVertexBuffer::SerializeMetaData()
		bool bNewWeightFormat = FAnimObjectVersion::Get(Ar) >= FAnimObjectVersion::UnlimitedBoneInfluences;
		int NumBytes = 0;
		if (Ar.Game < GAME_UE4(24))
		{
			NumBytes = 2 * sizeof(int32);
		}
		else if (!bNewWeightFormat)
		{
			NumBytes = 3 * sizeof(int32);
		}
		else
		{
			int32 MaxBoneInfluences, NumBones;
			NumBytes = 4 * sizeof(int32);
			if (FAnimObjectVersion::Get(Ar) >= FAnimObjectVersion::IncreaseBoneIndexLimitPerChunk)
				NumBytes += sizeof(int32);
		}
		// FSkinWeightLookupVertexBuffer::SerializeMetaData
		if (bNewWeightFormat)
		{
			NumBytes += sizeof(int32);
		}
		return NumBytes;
	}

	friend FArchive& operator<<(FArchive& Ar, FSkinWeightVertexBuffer& B)
	{
		guard(FSkinWeightVertexBuffer<<);

		// FSkinWeightDataVertexBuffer since 4.25

		FStripDataFlags SkinWeightStripFlags(Ar);

		bool bExtraBoneInfluences;
		int32 NumVertices;
		int32 Stride = 0;
		bool bVariableBonesPerVertex = false;
		bool bUse16BitBoneIndex = false;

		bool bNewWeightFormat = FAnimObjectVersion::Get(Ar) >= FAnimObjectVersion::UnlimitedBoneInfluences;

		// UE4.23 changed this structure. Part of data is serialized in FSkinWeightVertexBuffer::SerializeMetaData(),
		// and there's a typo: it relies on FSkeletalMeshCustomVersion::SplitModelAndRenderData which is UE4.15 constant.
		// So we're using comparison with UE4.23 instead.

		// FSkinWeightDataVertexBuffer::SerializeMetaData()
		if (Ar.Game < GAME_UE4(24))
		{
			Ar << bExtraBoneInfluences << NumVertices;
		}
		else if (!bNewWeightFormat)
		{
			Ar << bExtraBoneInfluences << Stride << NumVertices;
		}
		else
		{
			int32 MaxBoneInfluences, NumBones;
			Ar << bVariableBonesPerVertex << MaxBoneInfluences << NumBones << NumVertices;
			bExtraBoneInfluences = (MaxBoneInfluences > NUM_INFLUENCES_UE4);
			assert(MaxBoneInfluences <= MAX_TOTAL_INFLUENCES_UE4);
			if (FAnimObjectVersion::Get(Ar) >= FAnimObjectVersion::IncreaseBoneIndexLimitPerChunk)
				Ar << bUse16BitBoneIndex;
		}

		DBG_SKEL("Weights: extra=%d verts=%d stride=%d\n", bExtraBoneInfluences, NumVertices, Stride);

		TArray<byte> NewData; // used for bNewWeightFormat

		if (!SkinWeightStripFlags.IsDataStrippedForServer())
		{
			GNumSkelInfluences = bExtraBoneInfluences ? MAX_TOTAL_INFLUENCES_UE4 : NUM_INFLUENCES_UE4;
			if (!bNewWeightFormat)
			{
				B.Weights.BulkSerialize(Ar);
			}
			else
			{
				NewData.BulkSerialize(Ar);
			}
		}

		// End of FSkinWeightDataVertexBuffer

		if (bNewWeightFormat)
		{
			guard(NewInfluenceFormat);
			assert(NewData.Num() || NumVertices == 0); // empty (stripped) mesh will have empty NewData array

			// FSkinWeightLookupVertexBuffer serializer

			FStripDataFlags LookupStripFlags(Ar);

			// FSkinWeightLookupVertexBuffer::SerializeMetaData
			int32 NumLookupVertices;
			Ar << NumLookupVertices;
			assert(NumLookupVertices == 0 || NumVertices == NumLookupVertices);

			if (!LookupStripFlags.IsDataStrippedForServer())
			{
				TArray<uint32> LookupVertexBuffer;
				LookupVertexBuffer.BulkSerialize(Ar);
			}

			assert(!bVariableBonesPerVertex);
			assert(!bUse16BitBoneIndex);
			// When bVariableBonesPerVertex is set, LookupVertexBuffer is used to fetch influence data. Otherwise, data format
			// is just like before.

			// Convert influence data
			if (NewData.Num())
			{
				FMemReader Reader(NewData.GetData(), NewData.Num());
				assert(B.Weights.Num() == 0);
				B.Weights.AddUninitialized(NumVertices);
				for (int i = 0; i < NumVertices; i++)
				{
					FSkinWeightInfo Weight;
					Reader << Weight;
					B.Weights[i] = Weight;
				}
			}

			unguard;
		}

		return Ar;
		unguard;
	}
};


// UE4.23+
struct FRuntimeSkinWeightProfileData
{
	// Pre-UE4.26 struct
	struct FSkinWeightOverrideInfo
	{
		uint32 InfluencesOffset;
		uint8 NumInfluences;

		friend FArchive& operator<<(FArchive& Ar, FSkinWeightOverrideInfo& Data)
		{
			return Ar << Data.InfluencesOffset << Data.NumInfluences;
		}
	};

	TMap<uint32, uint32> VertexIndexOverrideIndex;
	// Pre-UE4.26 data
	TArray<FSkinWeightOverrideInfo> OverridesInfo;
	TArray<uint16> Weights;
	// UE4.26+
	TArray<byte> BoneIDs;
	TArray<byte> BoneWeights;
	byte NumWeightsPerVertex;

	friend FArchive& operator<<(FArchive& Ar, FRuntimeSkinWeightProfileData& Data)
	{
		guard(FRuntimeSkinWeightProfileData<<);
		if (Ar.ArVer < VER_UE4_SKINWEIGHT_PROFILE_DATA_LAYOUT_CHANGES)
		{
			Ar << Data.OverridesInfo << Data.Weights;
		}
		else
		{
			// UE4.26+
			Ar << Data.BoneIDs << Data.BoneWeights << Data.NumWeightsPerVertex;
		}
		Ar << Data.VertexIndexOverrideIndex;
		return Ar;
		unguard;
	}
};

struct FSkinWeightProfilesData
{
	TMap<FName, FRuntimeSkinWeightProfileData> OverrideData;

	friend FArchive& operator<<(FArchive& Ar, FSkinWeightProfilesData& Data)
	{
		guard(FSkinWeightProfilesData<<);
		return Ar << Data.OverrideData;
		unguard;
	}
};

struct FStaticLODModel4
{
	TArray<FSkelMeshSection4>	Sections;
	FMultisizeIndexContainer	Indices;
	FMultisizeIndexContainer	AdjacencyIndexBuffer;
	TArray<int16>				ActiveBoneIndices;
	TArray<int16>				RequiredBones;
	TArray<FSkelMeshChunk4>		Chunks;
	int32						Size;
	int32						NumVertices;
	int32						NumTexCoords;
	FIntBulkData				RawPointIndices;
	TArray<int32>				MeshToImportVertexMap;
	int							MaxImportVertex;
	FSkeletalMeshVertexBuffer4	VertexBufferGPUSkin;
	FSkeletalMeshVertexColorBuffer4 ColorVertexBuffer;		//!! TODO: switch to FColorVertexBuffer4
	FSkeletalMeshVertexClothBuffer ClothVertexBuffer;

	enum EClassDataStripFlag
	{
		CDSF_AdjacencyData = 1,
		CDSF_MinLodData = 2,
	};

	// Before 4.19, this function is FStaticLODModel::Serialize in UE4 source code. After 4.19,
	// this is FSkeletalMeshLODModel::Serialize.
	friend FArchive& operator<<(FArchive& Ar, FStaticLODModel4& Lod)
	{
		guard(FStaticLODModel4<<);

#if SEAOFTHIEVES
		if (Ar.Game == GAME_SeaOfThieves) Ar.Seek(Ar.Tell()+4);
#endif

		FStripDataFlags StripFlags(Ar);
		FSkeletalMeshCustomVersion::Type SkelMeshVer = FSkeletalMeshCustomVersion::Get(Ar);

#if SEAOFTHIEVES
		if (Ar.Game == GAME_SeaOfThieves) Ar.Seek(Ar.Tell()+4);
#endif

		Ar << Lod.Sections;
#if DEBUG_SKELMESH
		for (int i1 = 0; i1 < Lod.Sections.Num(); i1++)
		{
			FSkelMeshSection4 &S = Lod.Sections[i1];
			appPrintf("Sec[%d]: Mtl=%d, BaseIdx=%d, NumTris=%d\n", i1, S.MaterialIndex, S.BaseIndex, S.NumTriangles);
		}
#endif

		if (SkelMeshVer < FSkeletalMeshCustomVersion::SplitModelAndRenderData)
		{
			Ar << Lod.Indices;
		}
		else
		{
			// UE4.19+ uses 32-bit index buffer (for editor data)
			Ar << Lod.Indices.Indices32;
		}
		DBG_SKEL("Indices: %d (16) / %d (32)\n", Lod.Indices.Indices16.Num(), Lod.Indices.Indices32.Num());

		Ar << Lod.ActiveBoneIndices;
		DBG_SKEL("ActiveBones: %d\n", Lod.ActiveBoneIndices.Num());

		if (SkelMeshVer < FSkeletalMeshCustomVersion::CombineSectionWithChunk)
		{
			Ar << Lod.Chunks;
#if DEBUG_SKELMESH
			for (int i1 = 0; i1 < Lod.Chunks.Num(); i1++)
			{
				const FSkelMeshChunk4& C = Lod.Chunks[i1];
				appPrintf("Chunk[%d]: FirstVert=%d Rig=%d (%d), Soft=%d(%d), Bones=%d, MaxInf=%d\n", i1, C.BaseVertexIndex,
					C.RigidVertices.Num(), C.NumRigidVertices, C.SoftVertices.Num(), C.NumSoftVertices, C.BoneMap.Num(), C.MaxBoneInfluences);
			}
#endif
		}

		Ar << Lod.Size;	// legacy
		if (!StripFlags.IsDataStrippedForServer())
			Ar << Lod.NumVertices;

		Ar << Lod.RequiredBones;
		DBG_SKEL("Size=%d, NumVerts=%d, RequiredBones=%d\n", Lod.Size, Lod.NumVertices, Lod.RequiredBones.Num());

		if (!StripFlags.IsEditorDataStripped())
			Lod.RawPointIndices.Skip(Ar);

#if LAWBREAKERS || SOD2
		if (Ar.Game == GAME_Lawbreakers || Ar.Game == GAME_StateOfDecay2) goto no_import_vertex_map;
#endif

		if (Ar.ArVer >= VER_UE4_ADD_SKELMESH_MESHTOIMPORTVERTEXMAP)
		{
			Ar << Lod.MeshToImportVertexMap << Lod.MaxImportVertex;
		}

	no_import_vertex_map:

		// geometry
		if (!StripFlags.IsDataStrippedForServer())
		{
			Ar << Lod.NumTexCoords;
			DBG_SKEL("TexCoords=%d\n", Lod.NumTexCoords);

			if (SkelMeshVer < FSkeletalMeshCustomVersion::SplitModelAndRenderData)
			{
				// Pre-UE4.19 code.
				Ar << Lod.VertexBufferGPUSkin;

				if (SkelMeshVer >= FSkeletalMeshCustomVersion::UseSeparateSkinWeightBuffer)
				{
					FSkinWeightVertexBuffer SkinWeights;
					Ar << SkinWeights;

					if (SkinWeights.Weights.Num())
					{
						guard(CopyWeights);
						assert(Lod.NumVertices == SkinWeights.Weights.Num());

						const TArray<FSkinWeightInfo>& Weights = SkinWeights.Weights;

						// Copy data to VertexBufferGPUSkin
						if (Lod.VertexBufferGPUSkin.bUseFullPrecisionUVs)
						{
							for (int i = 0; i < Lod.NumVertices; i++)
							{
								Lod.VertexBufferGPUSkin.VertsFloat[i].Infs = Weights[i];
							}
						}
						else
						{
							for (int i = 0; i < Lod.NumVertices; i++)
							{
								Lod.VertexBufferGPUSkin.VertsHalf[i].Infs = Weights[i];
							}
						}
						unguard;
					}
				}

				USkeletalMesh4 *LoadingMesh = (USkeletalMesh4*)UObject::GLoadingObj;
				assert(LoadingMesh);
				if (LoadingMesh->bHasVertexColors)
				{
					if (SkelMeshVer < FSkeletalMeshCustomVersion::UseSharedColorBufferFormat)
					{
						Ar << Lod.ColorVertexBuffer;
					}
					else
					{
						FColorVertexBuffer4 NewColorVertexBuffer;
						Ar << NewColorVertexBuffer;
						Exchange(Lod.ColorVertexBuffer.Data, NewColorVertexBuffer.Data);
					}
				}

				if (Ar.ArVer < VER_UE4_REMOVE_EXTRA_SKELMESH_VERTEX_INFLUENCES)
				{
					appError("Unsupported: extra SkelMesh vertex influences (old mesh format)");
				}

#if SOD2
				if (Ar.Game == GAME_StateOfDecay2)
				{
					Ar.Seek(Ar.Tell() + 8);
					return Ar;
				}
#endif // SOD2

#if SEAOFTHIEVES
				if (Ar.Game == GAME_SeaOfThieves)
				{
					int32 SomeArraySize;
					Ar << SomeArraySize;
					Ar.Seek(Ar.Tell() + SomeArraySize * 44);

					TArray<int32> array1, array2, array3, array4;
					Ar << array1 << array2;
					Ar << array3 << array4;

					Ar.Seek(Ar.Tell() + 13);
				}
#endif // SEAOFTHIEVES

				if (!StripFlags.IsClassDataStripped(CDSF_AdjacencyData))
					Ar << Lod.AdjacencyIndexBuffer;

#if FABLE
				if (Ar.Game == GAME_FableLegends)
				{
					FMultisizeIndexContainer UnkIndices;
					Ar << UnkIndices;
				}
#endif // FABLE

				if (Ar.ArVer >= VER_UE4_APEX_CLOTH && Lod.HasClothData())
					Ar << Lod.ClothVertexBuffer;

#if DAYSGONE
				if (Ar.Game == GAME_DaysGone)
				{
					FMultisizeIndexContainer unk1;
					int32 unk2, unk3, unk4;
					Ar << unk1 << unk2 << unk3 << unk4;
				}
#endif // DAYSGONE
			}
		}

#if ASC_ONE
		if (Ar.Game == GAME_AscOne)
		{
			TArray<FSkelMeshSection4> Sections2;
			Ar << Sections2;
		}
#endif // ASC_ONE

#if SEAOFTHIEVES
		if (Ar.Game == GAME_SeaOfThieves)
		{
			FMultisizeIndexContainer indices;
			Ar << indices;
		}
#endif // SEAOFTHIEVES

		return Ar;

		unguard;
	}

	// This function is used only for UE4.19-UE4.23 data. UE4 source code prototype is
	// FSkeletalMeshLODRenderData::Serialize().
	static void SerializeRenderItem_Legacy(FArchive& Ar, FStaticLODModel4& Lod)
	{
		guard(FStaticLODModel4::SerializeRenderItem_Legacy);

#if DEBUG_SKELMESH
		DUMP_ARC_BYTES(Ar, 2, "RenderItemLegacy_StripFlags");
#endif
		FStripDataFlags StripFlags(Ar);

		Lod.Sections.Serialize2<FSkelMeshSection4::SerializeRenderItem>(Ar);
#if DEBUG_SKELMESH
		for (int i1 = 0; i1 < Lod.Sections.Num(); i1++)
		{
			FSkelMeshSection4 &S = Lod.Sections[i1];
			appPrintf("Sec[%d]: Mtl=%d, BaseIdx=%d, NumTris=%d\n", i1, S.MaterialIndex, S.BaseIndex, S.NumTriangles);
		}
#endif

		Ar << Lod.Indices;
		DBG_SKEL("Indices: %d (16) / %d (32)\n", Lod.Indices.Indices16.Num(), Lod.Indices.Indices32.Num());
		Ar << Lod.ActiveBoneIndices;
		DBG_SKEL("ActiveBones: %d\n", Lod.ActiveBoneIndices.Num());

		Ar << Lod.RequiredBones;

		if (!StripFlags.IsDataStrippedForServer() && !StripFlags.IsClassDataStripped(CDSF_MinLodData))
		{
			FPositionVertexBuffer4 PositionVertexBuffer;
			Ar << PositionVertexBuffer;

			FStaticMeshVertexBuffer4 StaticMeshVertexBuffer;
			Ar << StaticMeshVertexBuffer;

			FSkinWeightVertexBuffer SkinWeightVertexBuffer;
			Ar << SkinWeightVertexBuffer;

			USkeletalMesh4 *LoadingMesh = (USkeletalMesh4*)UObject::GLoadingObj;
			assert(LoadingMesh);

#if BORDERLANDS3
			if (Ar.Game == GAME_Borderlands3)
			{
				// This is a replacement for standard "bHasVertexColors" property
				assert(LoadingMesh->bHasVertexColors == false);
				for (int i = 0; i < LoadingMesh->NumVertexColorChannels; i++)
				{
					FColorVertexBuffer4 ColorVertexBuffer;
					Ar << ColorVertexBuffer;
				}
			}
#endif // BORDERLANDS3

			if (LoadingMesh->bHasVertexColors)
			{
				FColorVertexBuffer4 NewColorVertexBuffer;
				Ar << NewColorVertexBuffer;
				Exchange(Lod.ColorVertexBuffer.Data, NewColorVertexBuffer.Data);
				DBG_SKEL("Colors: %d\n", Lod.ColorVertexBuffer.Data.Num());
			}

			if (!StripFlags.IsClassDataStripped(CDSF_AdjacencyData))
				Ar << Lod.AdjacencyIndexBuffer;

			if (Lod.HasClothData())
				Ar << Lod.ClothVertexBuffer;

			guard(BuildVertexData);

			// Build vertex buffers (making a kind of pre-4.19 buffers from 4.19+ data)
			Lod.VertexBufferGPUSkin.bUseFullPrecisionUVs = true;
			Lod.NumVertices = PositionVertexBuffer.NumVertices;
			Lod.NumTexCoords = StaticMeshVertexBuffer.NumTexCoords;
			// build VertsFloat because FStaticMeshUVItem4 always has unpacked (float) texture coordinates
			Lod.VertexBufferGPUSkin.VertsFloat.AddZeroed(Lod.NumVertices);
			for (int i = 0; i < Lod.NumVertices; i++)
			{
				FGPUVert4Float& V = Lod.VertexBufferGPUSkin.VertsFloat[i];
				const FStaticMeshUVItem4& SV = StaticMeshVertexBuffer.UV[i];
				V.Pos = PositionVertexBuffer.Verts[i];
				V.Infs = SkinWeightVertexBuffer.Weights[i];
				static_assert(sizeof(V.Normal) == sizeof(SV.Normal), "FGPUVert4Common.Normal should match FStaticMeshUVItem4.Normal");
				memcpy(V.Normal, SV.Normal, sizeof(V.Normal));
				static_assert(sizeof(V.UV[0]) == sizeof(SV.UV[0]), "FGPUVert4Common.UV should match FStaticMeshUVItem4.UV");
				static_assert(sizeof(V.UV) <= sizeof(SV.UV), "SkeletalMesh has more UVs than StaticMesh"); // this is just for correct memcpy below
				memcpy(V.UV, SV.UV, sizeof(V.UV));
			}

			unguard;
		}

		if (Ar.Game >= GAME_UE4(23))
		{
			FSkinWeightProfilesData SkinWeightProfilesData;
			Ar << SkinWeightProfilesData;
		}

		unguard;
	}

	// This function is used only for UE4.24+ data. It adds LOD streaming functionality and
	// differs too much from previous code to try keeping code in a single function.
	// UE4 source code prototype is FSkeletalMeshLODRenderData::Serialize().
	static void SerializeRenderItem(FArchive& Ar, FStaticLODModel4& Lod)
	{
		guard(FStaticLODModel4::SerializeRenderItem);

		if (Ar.Game < GAME_UE4(24))
		{
			// Fallback to older code when needed.
			SerializeRenderItem_Legacy(Ar, Lod);
			return;
		}

	#if DEBUG_SKELMESH
		DUMP_ARC_BYTES(Ar, 2, "Begin RenderItem StripFlags");
	#endif
		FStripDataFlags StripFlags(Ar);

		bool bIsLODCookedOut, bInlined;
		Ar << bIsLODCookedOut << bInlined;
		DBG_SKEL("LOD CookedOut: %d Inlined: %d\n", bIsLODCookedOut, bInlined);

		Ar << Lod.RequiredBones;

		if (!StripFlags.IsDataStrippedForServer() && !bIsLODCookedOut)
		{
			Lod.Sections.Serialize2<FSkelMeshSection4::SerializeRenderItem>(Ar);
	#if DEBUG_SKELMESH
			for (int i1 = 0; i1 < Lod.Sections.Num(); i1++)
			{
				FSkelMeshSection4 &S = Lod.Sections[i1];
				appPrintf("Sec[%d]: Mtl=%d, BaseIdx=%d, NumTris=%d\n", i1, S.MaterialIndex, S.BaseIndex, S.NumTriangles);
			}
	#endif
			Ar << Lod.ActiveBoneIndices;
			DBG_SKEL("ActiveBones: %d\n", Lod.ActiveBoneIndices.Num());

			uint32 BuffersSize;
			Ar << BuffersSize;

			if (bInlined)
			{
				Lod.SerializeStreamedData(Ar);
			}
			else
			{
				DBG_SKEL("Serialize from bulk\n");
				FByteBulkData Bulk;
				Bulk.Serialize(Ar);
				if (Bulk.ElementCount > 0)
				{
					// perform SerializeStreamedData on bulk array
					Bulk.SerializeData(UObject::GLoadingObj);

					FMemReader Reader(Bulk.BulkData, Bulk.ElementCount); // ElementCount is the same as data size, for byte bulk data
					Reader.SetupFrom(*UObject::GLoadingObj->GetPackageArchive());
					Lod.SerializeStreamedData(Reader);

					// FSkeletalMeshLODRenderData::SerializeAvailabilityInfo
					/* ... SerializeMetaData() for all buffers
						Indices = 1x byte, 1x int32
						AdjacencyIndexBuffer = same (if present)
						StaticMeshVertexBuffer = 2x int32, 2x bool
						PositionVertexBuffer = 2x int32
						ColorVertexBuffer = 2x int32
						SkinWeightVertexBuffer = FSkinWeightVertexBuffer::MetadataSize()
						ClothVertexBuffer = TArray<uint64>, 2x uint32 (if present)
						SkinWeightProfilesData = TArray<FName>
					*/
					int SkipBytes = 5;
					if (!StripFlags.IsClassDataStripped(CDSF_AdjacencyData))
						SkipBytes += 5;
					SkipBytes += 4*4 + 2*4 + 2*4;
					SkipBytes += FSkinWeightVertexBuffer::MetadataSize(Ar);
					Ar.Seek(Ar.Tell() + SkipBytes);
					if (Lod.HasClothData())
					{
						// ClothVertexBuffer
						TArray<int64> ClothIndexMapping;
						Ar << ClothIndexMapping;
						Ar.Seek(Ar.Tell() + 2*4);
					}
					// SkinWeightProfilesData
					TArray<FName> ProfileNames;
					Ar << ProfileNames;
				}
			}
		}

		unguard;
	}

	// UE4.24+ serializer for most of LOD data
	// Reference: FSkeletalMeshLODRenderData::SerializeStreamedData
	void SerializeStreamedData(FArchive& Ar)
	{
		guard(FStaticLODModel4::SerializeStreamedData);

		FStripDataFlags StripFlags(Ar);
		Ar << Indices;
		DBG_SKEL("Indices: %d (16) / %d (32)\n", Indices.Indices16.Num(), Indices.Indices32.Num());

		FPositionVertexBuffer4 PositionVertexBuffer;
		Ar << PositionVertexBuffer;

		FStaticMeshVertexBuffer4 StaticMeshVertexBuffer;
		Ar << StaticMeshVertexBuffer;

		FSkinWeightVertexBuffer SkinWeightVertexBuffer;
		Ar << SkinWeightVertexBuffer;

		USkeletalMesh4 *LoadingMesh = (USkeletalMesh4*)UObject::GLoadingObj;
		assert(LoadingMesh);
		if (LoadingMesh->bHasVertexColors)
		{
			FColorVertexBuffer4 NewColorVertexBuffer;
			Ar << NewColorVertexBuffer;
			Exchange(ColorVertexBuffer.Data, NewColorVertexBuffer.Data);
		}

		if (!StripFlags.IsClassDataStripped(CDSF_AdjacencyData))
			Ar << AdjacencyIndexBuffer;

		if (HasClothData())
			Ar << ClothVertexBuffer;

		FSkinWeightProfilesData SkinWeightProfilesData;
		Ar << SkinWeightProfilesData;

		if (Ar.Game >= GAME_UE4(27) || Ar.Game == GAME_UE4_25_Plus)
		{
			TArray<uint8> RayTracingData;
			Ar << RayTracingData;
		}

		//todo: this is a copy-paste of SerializeRenderItem_Legacy code!
		guard(BuildVertexData);

		// Build vertex buffers (making a kind of pre-4.19 buffers from 4.19+ data)
		VertexBufferGPUSkin.bUseFullPrecisionUVs = true;
		NumVertices = PositionVertexBuffer.NumVertices;
		NumTexCoords = StaticMeshVertexBuffer.NumTexCoords;
		// build VertsFloat because FStaticMeshUVItem4 always has unpacked (float) texture coordinates
		VertexBufferGPUSkin.VertsFloat.AddZeroed(NumVertices);
		for (int i = 0; i < NumVertices; i++)
		{
			FGPUVert4Float& V = VertexBufferGPUSkin.VertsFloat[i];
			const FStaticMeshUVItem4& SV = StaticMeshVertexBuffer.UV[i];
			V.Pos = PositionVertexBuffer.Verts[i];
			V.Infs = SkinWeightVertexBuffer.Weights[i];
			static_assert(sizeof(V.Normal) == sizeof(SV.Normal), "FGPUVert4Common.Normal should match FStaticMeshUVItem4.Normal");
			memcpy(V.Normal, SV.Normal, sizeof(V.Normal));
			static_assert(sizeof(V.UV[0]) == sizeof(SV.UV[0]), "FGPUVert4Common.UV should match FStaticMeshUVItem4.UV");
			static_assert(sizeof(V.UV) <= sizeof(SV.UV), "SkeletalMesh has more UVs than StaticMesh"); // this is just for correct memcpy below
			memcpy(V.UV, SV.UV, sizeof(V.UV));
		}

		unguard;

		unguard;
	}

	bool HasClothData() const
	{
		for (int i = 0; i < Chunks.Num(); i++)
			if (Chunks[i].HasClothData)				// pre-UE4.13 code
				return true;
		for (int i = 0; i < Sections.Num(); i++)	// UE4.13+
			if (Sections[i].HasClothData)
				return true;
		return false;
	}
};

USkeletalMesh4::USkeletalMesh4()
:	bHasVertexColors(false)
,	ConvertedMesh(NULL)
{}

USkeletalMesh4::~USkeletalMesh4()
{
	delete ConvertedMesh;
}

void USkeletalMesh4::Serialize(FArchive &Ar)
{
	guard(USkeletalMesh4::Serialize);

	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);

	Ar << Bounds;
	Ar << Materials;
#if DEBUG_SKELMESH
	for (int i1 = 0; i1 < Materials.Num(); i1++)
	{
		UObject* Mat = Materials[i1].Material;
		if (Mat)
			appPrintf("Material[%d] = %s'%s'\n", i1, Mat->GetClassName(), Mat->Name);
		else
			appPrintf("Material[%d] = None\n", i1);
	}
#endif

	Ar << RefSkeleton;
#if DEBUG_SKELMESH
	appPrintf("RefSkeleton: %d bones\n", RefSkeleton.RefBoneInfo.Num());
	for (int i1 = 0; i1 < RefSkeleton.RefBoneInfo.Num(); i1++)
		appPrintf("  [%d] n=%s p=%d\n", i1, *RefSkeleton.RefBoneInfo[i1].Name, RefSkeleton.RefBoneInfo[i1].ParentIndex);
#endif

#if ASC_ONE
	if (Ar.Game == GAME_AscOne)
	{
		// Ascendant One
		int32 ArCount;
		Ar << ArCount;
		Ar.Seek(Ar.Tell() + ArCount * 4);
		int32 tmp;
		Ar << tmp;
		Ar << ArCount;
		for (int i = 0; i < ArCount; i++)
		{
			// Structure:
			// {
			//   { FString, FName BoneName1, int32, int32, FName BoneName2, FTransform }
			//   int32, int32, FVector, 4 x int32
			// }
			FString SomeName;
			Ar << SomeName;
			Ar.Seek(Ar.Tell() + 4 * 25);
		}
	}
#endif // ASC_ONE

	// Serialize FSkeletalMeshResource (contains only array of FStaticLODModel objects). Before UE4.19,
	// data was stored in a single array of FStaticLODModel structures. Starting with 4.19, editor and runtime
	// data were separated to FSkeletalMeshLODModel and FSkeletalMeshLODRenderData structures.
	if (FSkeletalMeshCustomVersion::Get(Ar) < FSkeletalMeshCustomVersion::SplitModelAndRenderData)
	{
		// Pre-UE4.19 code: single data array
		Ar << LODModels;
	}
	else
	{
		// UE4.19+: 2 data arrays, for editor and for runtime. FSkeletalMeshLODModel and FSkeletalMeshLODRenderData.
		// We're serializing them both as FStaticLODModel4.
		if (!StripFlags.IsEditorDataStripped())
		{
			// Serialize editor data
			Ar << LODModels;
		}
		bool bCooked;
		Ar << bCooked;

		if (Ar.Game >= GAME_UE4(27) && KeepMobileMinLODSettingOnDesktop)
		{
			// The serialization of this variable is cvar-dependent in UE4, so there's no clear way to understand
			// if it should be serialize in our code or not.
			int32 MinMobileLODIdx;
			Ar << MinMobileLODIdx;
		}

		if (bCooked && LODModels.Num() == 0)
		{
			// serialize cooked data only if editor data not exists - use custom array serializer function
			LODModels.Serialize2<FStaticLODModel4::SerializeRenderItem>(Ar);
		}
	}

	DROP_REMAINING_DATA(Ar);

	ConvertMesh();

	// Release original mesh data to save memory
	LODModels.Empty();

	unguard;
}

void USkeletalMesh4::PostLoad()
{
	guard(USkeletalMesh4::PostLoad);

	assert(ConvertedMesh);
	for (int i = 0; i < MorphTargets.Num(); i++)
	{
		guard(ConvertMorph)
		if (MorphTargets[i])
			ConvertedMesh->Morphs.Add(MorphTargets[i]->ConvertMorph());
		unguardf("%d/%d", i, MorphTargets.Num());
	}

	// Collect sockets from USkeletalMesh and USkeleton
	TArray<USkeletalMeshSocket4*> SrcSockets;
	int NumSockets = Sockets.Num(); // potential number of sockets
	if (Skeleton) NumSockets += Skeleton->Sockets.Num();
	SrcSockets.Empty(NumSockets);
	for (USkeletalMeshSocket4* SrcSocket : Sockets)
	{
		if (!SrcSocket) continue;
		SrcSockets.Add(SrcSocket);
	}
	if (Skeleton)
	{
		for (USkeletalMeshSocket4* SrcSocket : Skeleton->Sockets)
		{
			if (!SrcSocket) continue;
			// Check if mesh already has a socket with the same name
			bool bAlreadyExists = false;
			for (USkeletalMeshSocket4* CheckSocket : SrcSockets)
			{
				if (CheckSocket->SocketName == SrcSocket->SocketName)
				{
					bAlreadyExists = true;
					break;
				}
			}
			if (!bAlreadyExists)
			{
				SrcSockets.Add(SrcSocket);
			}
		}
	}

	// Convert all found sockets
	for (USkeletalMeshSocket4* SrcSocket : SrcSockets)
	{
		if (!SrcSocket) continue;
		CSkelMeshSocket& Socket = ConvertedMesh->Sockets.AddZeroed_GetRef();
		Socket.Name = SrcSocket->SocketName;
		Socket.Bone = SrcSocket->BoneName;
		CCoords& C = Socket.Transform;
		C.origin = CVT(SrcSocket->RelativeLocation);
		RotatorToAxis(SrcSocket->RelativeRotation, C.axis);
	}

	unguard;
}

void USkeletalMesh4::ConvertMesh()
{
	guard(USkeletalMesh4::ConvertMesh);

	CSkeletalMesh *Mesh = new CSkeletalMesh(this);
	ConvertedMesh = Mesh;

	// convert bounds
	Mesh->BoundingSphere.R = Bounds.SphereRadius / 2;		//?? UE3 meshes has radius 2 times larger than mesh
	VectorSubtract(CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Min));
	VectorAdd     (CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Max));

	// MeshScale, MeshOrigin, RotOrigin are removed in UE4
	//!! NOTE: MeshScale is integrated into RefSkeleton.RefBonePose[0].Scale3D.
	//!! Perhaps rotation/translation are integrated too!
	Mesh->MeshOrigin.Set(0, 0, 0);
	Mesh->RotOrigin.Set(0, 0, 0);
	Mesh->MeshScale.Set(1, 1, 1);							// missing in UE4

	// convert LODs
	Mesh->Lods.Empty(LODModels.Num());
	assert(LODModels.Num() == LODInfo.Num());
	for (int lod = 0; lod < LODModels.Num(); lod++)
	{
		guard(ConvertLod);

		const FStaticLODModel4 &SrcLod = LODModels[lod];
		if (SrcLod.Indices.Indices16.Num() == 0 && SrcLod.Indices.Indices32.Num() == 0)
		{
			appPrintf("Lod %d has no indices, skipping.\n", lod);
			continue;
		}

		int NumTexCoords = SrcLod.NumTexCoords;
		if (NumTexCoords > MAX_MESH_UV_SETS)
			appError("SkeletalMesh has %d UV sets", NumTexCoords);

		CSkelMeshLod *Lod = new (Mesh->Lods) CSkelMeshLod;
		Lod->NumTexCoords = NumTexCoords;
		Lod->HasNormals   = true;
		Lod->HasTangents  = true;

		guard(ProcessVerts);

		// get vertex count and determine vertex source
		int VertexCount = SrcLod.VertexBufferGPUSkin.GetVertexCount();

		bool bUseVerticesFromSections = false;
		if (VertexCount == 0 && LODModels[lod].Sections.Num() > 0 && SrcLod.Sections[0].SoftVertices.Num())
		{
			// For editor assets, count vertex count from sections. This happens with UE4.19+, where rendering
			// and editor data were separated (or may be with earlier engine version).
			bUseVerticesFromSections = true;
			for (int i = 0; i < SrcLod.Sections.Num(); i++)
			{
				VertexCount += SrcLod.Sections[i].SoftVertices.Num();
			}
		}

		// allocate the vertices
		Lod->AllocateVerts(VertexCount);

		int chunkIndex = -1;
		int lastChunkVertex = -1;
		int chunkVertexIndex = 0;

		const TArray<uint16>* BoneMap = NULL;
		const FSkeletalMeshVertexBuffer4& VertBuffer = SrcLod.VertexBufferGPUSkin;
		CSkelMeshVertex* D = Lod->Verts;

		if (SrcLod.ColorVertexBuffer.Data.Num() == VertexCount)
			Lod->AllocateVertexColorBuffer();
		else if (SrcLod.ColorVertexBuffer.Data.Num())
			appPrintf("LOD %d has invalid vertex color stream\n", lod);

		for (int Vert = 0; Vert < VertexCount; Vert++, D++)
		{
			while (Vert >= lastChunkVertex) // this will fix any issues with empty chunks or sections
			{
				// proceed to next chunk or section
				if (SrcLod.Chunks.Num())
				{
					// pre-UE4.13 code: chunks
					const FSkelMeshChunk4& C = SrcLod.Chunks[++chunkIndex];
					lastChunkVertex = C.BaseVertexIndex + C.NumRigidVertices + C.NumSoftVertices;
					BoneMap = &C.BoneMap;
				}
				else
				{
					// UE4.13+ code: chunk information migrated to sections
					const FSkelMeshSection4& S = SrcLod.Sections[++chunkIndex];
					lastChunkVertex = S.BaseVertexIndex + S.NumVertices;
					BoneMap = &S.BoneMap;
				}
				chunkVertexIndex = 0;
			}

			// get vertex from GPU skin
			const FSkelMeshVertexBase *V;				// has everything but UV[]

			if (bUseVerticesFromSections)
			{
				const FSoftVertex4& V0 = SrcLod.Sections[chunkIndex].SoftVertices[chunkVertexIndex++];
				const FMeshUVFloat *SrcUV = V0.UV;
				V = &V0;
				// UV: simply copy float data
				D->UV = CVT(SrcUV[0]);
				for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
				{
					Lod->ExtraUV[TexCoordIndex-1][Vert] = CVT(SrcUV[TexCoordIndex]);
				}
			}
			else if (!VertBuffer.bUseFullPrecisionUVs)
			{
				const FGPUVert4Half& V0 = VertBuffer.VertsHalf[Vert];
				const FMeshUVHalf* SrcUV = V0.UV;
				V = &V0;
				// UV: convert half -> float
				D->UV = CVT(SrcUV[0]);
				for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
				{
					Lod->ExtraUV[TexCoordIndex-1][Vert] = CVT(SrcUV[TexCoordIndex]);
				}
			}
			else
			{
				const FGPUVert4Float& V0 = VertBuffer.VertsFloat[Vert];
				const FMeshUVFloat *SrcUV = V0.UV;
				V = &V0;
				// UV: simply copy float data
				D->UV = CVT(SrcUV[0]);
				for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
				{
					Lod->ExtraUV[TexCoordIndex-1][Vert] = CVT(SrcUV[TexCoordIndex]);
				}
			}
			D->Position = CVT(V->Pos);
			UnpackNormals(V->Normal, *D);
			if (Lod->VertexColors)
			{
				//todo: check if this will work with "source" models - FSoftVertex4 has Color field
				Lod->VertexColors[Vert] = SrcLod.ColorVertexBuffer.Data[Vert];
			}
			// convert influences
			int i2 = 0;
			unsigned PackedWeights = 0;
			for (int i = 0; i < NUM_INFLUENCES_UE4; i++)
			{
				int BoneIndex  = V->Infs.BoneIndex[i];
				byte BoneWeight = V->Infs.BoneWeight[i];
				if (BoneWeight == 0) continue;				// skip this influence (but do not stop the loop!)
				PackedWeights |= BoneWeight << (i2 * 8);
				D->Bone[i2]   = (*BoneMap)[BoneIndex];
				i2++;
			}
			D->PackedWeights = PackedWeights;
			if (i2 < NUM_INFLUENCES_UE4) D->Bone[i2] = INDEX_NONE; // mark end of list
		}

		unguard;	// ProcessVerts

		// indices
		Lod->Indices.Initialize(&SrcLod.Indices.Indices16, &SrcLod.Indices.Indices32);

		// sections
		guard(ProcessSections);
		Lod->Sections.Empty(SrcLod.Sections.Num());
		const FSkeletalMeshLODInfo &Info = LODInfo[lod];

		for (int Sec = 0; Sec < SrcLod.Sections.Num(); Sec++)
		{
			const FSkelMeshSection4 &S = SrcLod.Sections[Sec];
			CMeshSection *Dst = new (Lod->Sections) CMeshSection;

			// Remap material for LOD
			// In comment for LODMaterialMap, INDEX_NONE means "no remap", so let's use this logic here.
			// Actually, INDEX_NONE in LODMaterialMap seems hides the mesh section in game.
			// Reference: FSkeletalMeshSceneProxy
			int MaterialIndex = S.MaterialIndex;
			if (Info.LODMaterialMap.IsValidIndex(Sec) && Materials.IsValidIndex(Info.LODMaterialMap[Sec]))
			{
				MaterialIndex = Info.LODMaterialMap[Sec];
			}

			Dst->Material = Materials.IsValidIndex(MaterialIndex) ? Materials[MaterialIndex].Material : NULL;
			Dst->FirstIndex = S.BaseIndex;
			Dst->NumFaces = S.NumTriangles;
		}

		unguard;	// ProcessSections

		unguardf("lod=%d", lod); // ConvertLod
	}

	// copy skeleton
	guard(ProcessSkeleton);
	int NumBones = RefSkeleton.RefBoneInfo.Num();
	Mesh->RefSkeleton.Empty(NumBones);
	for (int i = 0; i < NumBones; i++)
	{
		const FMeshBoneInfo &B = RefSkeleton.RefBoneInfo[i];
		const FTransform    &T = RefSkeleton.RefBonePose[i];
		CSkelMeshBone *Dst = new (Mesh->RefSkeleton) CSkelMeshBone;
		Dst->Name        = B.Name;
		Dst->ParentIndex = B.ParentIndex;
		Dst->Position    = CVT(T.Translation);
		Dst->Orientation = CVT(T.Rotation);
#if !BAKE_BONE_SCALES
		Dst->Scale       = CVT(T.Scale3D);
#endif
		// fix skeleton; all bones but 0
		if (i >= 1)
			Dst->Orientation.Conjugate();
	}
	unguard; // ProcessSkeleton

	Mesh->FinalizeMesh();

	unguard;
}


/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

UStaticMesh4::UStaticMesh4()
:	ConvertedMesh(NULL)
,	bUseHighPrecisionTangentBasis(false)
{}

UStaticMesh4::~UStaticMesh4()
{
	delete ConvertedMesh;
}

// Ambient occlusion data
// When changed, constant DISTANCEFIELD_DERIVEDDATA_VER TEXT is updated
struct FDistanceFieldVolumeData
{
	TArray<int16>	DistanceFieldVolume;	// TArray<Float16>
	FIntVector		Size;
	FBox			LocalBoundingBox;
	bool			bMeshWasClosed;
	bool			bBuiltAsIfTwoSided;
	bool			bMeshWasPlane;
	// 4.16+
	TArray<byte>	CompressedDistanceFieldVolume;
	FVector2D		DistanceMinMax;

	friend FArchive& operator<<(FArchive& Ar, FDistanceFieldVolumeData& V)
	{
		guard(FDistanceFieldVolumeData<<);
		if (Ar.Game >= GAME_UE4(16))
		{
			Ar << V.CompressedDistanceFieldVolume;
			Ar << V.Size << V.LocalBoundingBox << V.DistanceMinMax << V.bMeshWasClosed;
			Ar << V.bBuiltAsIfTwoSided << V.bMeshWasPlane;
			return Ar;
		}
	older_code:
		// Pre-4.16 version
		Ar << V.DistanceFieldVolume;
		Ar << V.Size << V.LocalBoundingBox << V.bMeshWasClosed;
		/// reference: 28.08.2014 - f5238f04
		if (Ar.ArVer >= VER_UE4_RENAME_CROUCHMOVESCHARACTERDOWN)
			Ar << V.bBuiltAsIfTwoSided;
		/// reference: 12.09.2014 - 890f1205
		if (Ar.ArVer >= VER_UE4_DEPRECATE_UMG_STYLE_ASSETS)
			Ar << V.bMeshWasPlane;
		return Ar;
		unguard;
	}
};


struct FStaticMeshSection4
{
	int32			MaterialIndex;
	int32			FirstIndex;
	int32			NumTriangles;
	int32			MinVertexIndex;
	int32			MaxVertexIndex;
	bool			bEnableCollision;
	bool			bCastShadow;
	bool			bForceOpaque;
	bool			bVisibleInRayTracing;

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshSection4& S)
	{
		Ar << S.MaterialIndex;
		Ar << S.FirstIndex << S.NumTriangles;
		Ar << S.MinVertexIndex << S.MaxVertexIndex;
		Ar << S.bEnableCollision << S.bCastShadow;
		if (FRenderingObjectVersion::Get(Ar) >= FRenderingObjectVersion::StaticMeshSectionForceOpaqueField)
		{
			Ar << S.bForceOpaque;
		}
		if (Ar.Game >= GAME_UE4(26))
		{
			Ar << S.bVisibleInRayTracing; //todo: check: StaticMesh always had a custom version for that
		}
#if DAUNTLESS
		if (Ar.Game == GAME_Dauntless) Ar.Seek(Ar.Tell()+8); // 8 zero-filled bytes here
#endif
		//?? Has editor-only data?
		return Ar;
	}
};


struct FRawStaticIndexBuffer4
{
	TArray<uint16>		Indices16;
	TArray<uint32>		Indices32;

	FORCEINLINE bool Is32Bit() const
	{
		return (Indices32.Num() != 0);
	}

	friend FArchive& operator<<(FArchive &Ar, FRawStaticIndexBuffer4 &S)
	{
		guard(FRawStaticIndexBuffer4<<);

		if (Ar.ArVer < VER_UE4_SUPPORT_32BIT_STATIC_MESH_INDICES)
		{
			S.Indices16.BulkSerialize(Ar);
			DBG_STAT("RawIndexBuffer, old format - %d indices\n", S.Indices16.Num());
		}
		else
		{
			// serialize all indices as byte array
			bool is32bit;
			TArray<byte> data;
			Ar << is32bit;
			data.BulkSerialize(Ar);

			if (Ar.Game >= GAME_UE4(25))
			{
				bool bShouldExpandTo32Bit;
				Ar << bShouldExpandTo32Bit;
			}

			DBG_STAT("RawIndexBuffer, 32 bit = %d, %d indices (data size = %d)\n", is32bit, data.Num() / (is32bit ? 4 : 2), data.Num());
			if (!data.Num()) return Ar;

			// convert data
			if (is32bit)
			{
				int count = data.Num() / 4;
				byte* src = &data[0];
				if (Ar.ReverseBytes)
					appReverseBytes(src, count, 4);
				S.Indices32.AddUninitialized(count);
				for (int i = 0; i < count; i++, src += 4)
					S.Indices32[i] = *(int*)src;
			}
			else
			{
				int count = data.Num() / 2;
				byte* src = &data[0];
				if (Ar.ReverseBytes)
					appReverseBytes(src, count, 2);
				S.Indices16.AddUninitialized(count);
				for (int i = 0; i < count; i++, src += 2)
					S.Indices16[i] = *(uint16*)src;
			}
		}

		return Ar;

		unguard;
	}
};

struct FWeightedRandomSampler
{
	TArray<float>			Prob;
	TArray<int32>			Alias;
	float					TotalWeight;

	friend FArchive& operator<<(FArchive& Ar, FWeightedRandomSampler& S)
	{
		return Ar << S.Prob << S.Alias << S.TotalWeight;
	}
};

FArchive& operator<<(FArchive& Ar, FSkeletalMeshSamplingLODBuiltData& V)
{
	// Serialize FSkeletalMeshAreaWeightedTriangleSampler AreaWeightedSampler
	// It is derived from FWeightedRandomSampler
	FWeightedRandomSampler Sampler;
	return Ar << Sampler;
}

FArchive& operator<<(FArchive& Ar, FSkeletalMeshSamplingRegionBuiltData& V)
{
	TArray<int32> TriangleIndices;
	TArray<int32> BoneIndices;

	Ar << TriangleIndices;
	Ar << BoneIndices;

	// Serialize FSkeletalMeshAreaWeightedTriangleSampler AreaWeightedSampler
	// It is derived from FWeightedRandomSampler
	FWeightedRandomSampler Sampler;
	Ar << Sampler;

	if (Ar.Game >= GAME_UE4(21)) // FNiagaraObjectVersion::SkeletalMeshVertexSampling
	{
		TArray<int32> Vertices;
		Ar << Vertices;
	}
	return Ar;
}

typedef FWeightedRandomSampler FStaticMeshSectionAreaWeightedTriangleSampler;
typedef FWeightedRandomSampler FStaticMeshAreaWeightedSectionSampler;


// FStaticMeshLODResources class (named differently here)
// NOTE: UE4 LOD models has no versioning code inside, versioning is performed before cooking, with constant STATICMESH_DERIVEDDATA_VER
// (it is changed in code to invalidate DerivedDataCache data)
struct FStaticMeshLODModel4
{
	TArray<FStaticMeshSection4> Sections;
	FStaticMeshVertexBuffer4 VertexBuffer;
	FPositionVertexBuffer4   PositionVertexBuffer;
	FColorVertexBuffer4      ColorVertexBuffer;
	FRawStaticIndexBuffer4   IndexBuffer;
	FRawStaticIndexBuffer4   ReversedIndexBuffer;
	FRawStaticIndexBuffer4   DepthOnlyIndexBuffer;
	FRawStaticIndexBuffer4   ReversedDepthOnlyIndexBuffer;
	FRawStaticIndexBuffer4   WireframeIndexBuffer;
	FRawStaticIndexBuffer4   AdjacencyIndexBuffer;
	float                    MaxDeviation;

	enum EClassDataStripFlag
	{
		CDSF_AdjacencyData = 1,
		// UE4.20+
		CDSF_MinLodData = 2,			// used to drop some LODs
		CDSF_ReversedIndexBuffer = 4,
		CDSF_RayTracingResources = 8,	// UE4.25+
	};

	static void Serialize(FArchive& Ar, FStaticMeshLODModel4& Lod)
	{
		guard(FStaticMeshLODModel4<<);

#if DEBUG_STATICMESH
		DUMP_ARC_BYTES(Ar, 2, "StripFlags");
#endif

		FStripDataFlags StripFlags(Ar);

		Ar << Lod.Sections;
#if DEBUG_STATICMESH
		appPrintf("%d sections\n", Lod.Sections.Num());
		for (const FStaticMeshSection4& S : Lod.Sections)
		{
			appPrintf("  mat=%d firstIdx=%d numTris=%d firstVers=%d maxVert=%d\n", S.MaterialIndex, S.FirstIndex, S.NumTriangles,
				S.MinVertexIndex, S.MaxVertexIndex);
		}
#endif // DEBUG_STATICMESH

		Ar << Lod.MaxDeviation;

		if (Ar.Game < GAME_UE4(23))
		{
			if (!StripFlags.IsDataStrippedForServer() && !StripFlags.IsClassDataStripped(CDSF_MinLodData))
			{
				SerializeBuffersLegacy(Ar, Lod, StripFlags);
			}
			return;
		}

		// UE4.23+
		bool bIsLODCookedOut, bInlined;
		Ar << bIsLODCookedOut << bInlined;

		if (!StripFlags.IsDataStrippedForServer() && !bIsLODCookedOut)
		{
			if (bInlined)
			{
				SerializeBuffers(Ar, Lod);
			}
			else
			{
				DBG_STAT("Serialize from bulk\n");
				FByteBulkData Bulk;
				Bulk.Serialize(Ar);
				if (Bulk.ElementCount > 0)
				{
					// perform SerializeBuffers on bulk array
					Bulk.SerializeData(UObject::GLoadingObj);

					FMemReader Reader(Bulk.BulkData, Bulk.ElementCount); // ElementCount is the same as data size, for byte bulk data
					Reader.SetupFrom(*UObject::GLoadingObj->GetPackageArchive());
					SerializeBuffers(Reader, Lod);
				}

				// FStaticMeshLODResources::SerializeAvailabilityInfo()
				uint32 DepthOnlyNumTriangles, PackedData;
				Ar << DepthOnlyNumTriangles << PackedData;
				/* ... SerializeMetaData() for all buffers
					StaticMeshVertexBuffer = 2x int32, 2x bool
					PositionVertexBuffer = 2x int32
					ColorVertexBuffer = 2x int32
					IndexBuffer = int32 + bool
					ReversedIndexBuffer
					DepthOnlyIndexBuffer
					ReversedDepthOnlyIndexBuffer
					WireframeIndexBuffer
					AdjacencyIndexBuffer
				*/
				Ar.Seek(Ar.Tell() + 4*4 + 2*4 + 2*4 + 6*(2*4));
			}
		}
		// FStaticMeshBuffersSize
		uint32 SerializedBuffersSize, DepthOnlyIBSize, ReversedIBsSize;
		Ar << SerializedBuffersSize << DepthOnlyIBSize << ReversedIBsSize;

		unguard;
	}

	// Pre-UE4.23 code
	static void SerializeBuffersLegacy(FArchive& Ar, FStaticMeshLODModel4& Lod, const FStripDataFlags& StripFlags)
	{
		guard(FStaticMeshLODModel4::SerializeBuffersLegacy);

		Ar << Lod.PositionVertexBuffer;
		Ar << Lod.VertexBuffer;

#if BORDERLANDS3
		if (Ar.Game == GAME_Borderlands3)
		{
			int32 NumColorStreams;
			Ar << NumColorStreams;
			for (int i = 0; i < NumColorStreams; i++)
			{
				FColorVertexBuffer4 tmpBuffer;
				Ar << ((i == 0) ? Lod.ColorVertexBuffer : tmpBuffer);
			}
			goto after_color_stream;
		}
#endif // BORDERLANDS3

		Ar << Lod.ColorVertexBuffer;

	after_color_stream:
		Ar << Lod.IndexBuffer;

		/// reference for VER_UE4_SOUND_CONCURRENCY_PACKAGE (UE4.9+):
		/// 25.09.2015 - 948c1698
		if (Ar.ArVer >= VER_UE4_SOUND_CONCURRENCY_PACKAGE && !StripFlags.IsClassDataStripped(CDSF_ReversedIndexBuffer))
		{
			Ar << Lod.ReversedIndexBuffer;
			Ar << Lod.DepthOnlyIndexBuffer;
			Ar << Lod.ReversedDepthOnlyIndexBuffer;
		}
		else
		{
			// UE4.8 or older, or when has CDSF_ReversedIndexBuffer
			Ar << Lod.DepthOnlyIndexBuffer;
		}

		if (Ar.ArVer >= VER_UE4_FTEXT_HISTORY && Ar.ArVer < VER_UE4_RENAME_CROUCHMOVESCHARACTERDOWN)
		{
			/// reference:
			/// 03.06.2014 - 1464dcf2
			/// 28.08.2014 - f5238f04
			FDistanceFieldVolumeData DistanceFieldData;
			Ar << DistanceFieldData;
		}

		if (!StripFlags.IsEditorDataStripped())
			Ar << Lod.WireframeIndexBuffer;

		if (!StripFlags.IsClassDataStripped(CDSF_AdjacencyData))
			Ar << Lod.AdjacencyIndexBuffer;

#if UT4
		if (Ar.Game == GAME_UT4) return;
#endif

		if (Ar.Game >= GAME_UE4(16))
		{
			TArray<FStaticMeshSectionAreaWeightedTriangleSampler> AreaWeightedSectionSamplers;
			FStaticMeshAreaWeightedSectionSampler AreaWeightedSampler;

			AreaWeightedSectionSamplers.AddZeroed(Lod.Sections.Num());
			for (int i = 0; i < AreaWeightedSectionSamplers.Num(); i++)
				Ar << AreaWeightedSectionSamplers[i];
			Ar << AreaWeightedSampler;
		}

#if FABLE
		if (Ar.Game == GAME_FableLegends)
		{
			int32 unk1;
			TArray<byte> unk2;
			Ar << unk1;
			unk2.BulkSerialize(Ar);
			// Looks like color stream, but has different layout
			FStripDataFlags UnkStrip(Ar);
			int32 UnkNumVerts;
			TArray<int32> UnkArray;
			Ar << UnkNumVerts;
			UnkArray.BulkSerialize(Ar);
		}
#endif // FABLE

#if SEAOFTHIEVES
		if (Ar.Game == GAME_SeaOfThieves) Ar.Seek(Ar.Tell()+17);
#endif

		unguard;
	}

	// UE4.23+. At the time of UE4.23, the function is exactly the same as SerializeBuffersLegacy (with exception of own
	// FStripDataFlags and use of StripFlags for additional index buffers).
	static void SerializeBuffers(FArchive& Ar, FStaticMeshLODModel4& Lod)
	{
		guard(FStaticMeshLODModel4::SerializeBuffers);

#if DEBUG_STATICMESH
		DUMP_ARC_BYTES(Ar, 2, "SerializeBuffers.StripFlags");
#endif
		FStripDataFlags StripFlags(Ar);

		Ar << Lod.PositionVertexBuffer;
		Ar << Lod.VertexBuffer;
		Ar << Lod.ColorVertexBuffer;
		Ar << Lod.IndexBuffer;

		if (!StripFlags.IsClassDataStripped(CDSF_ReversedIndexBuffer))
		{
			Ar << Lod.ReversedIndexBuffer;
		}
		Ar << Lod.DepthOnlyIndexBuffer;

		if (!StripFlags.IsClassDataStripped(CDSF_ReversedIndexBuffer))
		{
			Ar << Lod.ReversedDepthOnlyIndexBuffer;
		}

		if (!StripFlags.IsEditorDataStripped())
			Ar << Lod.WireframeIndexBuffer;

		if (!StripFlags.IsClassDataStripped(CDSF_AdjacencyData))
			Ar << Lod.AdjacencyIndexBuffer;

		// UE4.25+
		if (Ar.Game >= GAME_UE4(25) && !StripFlags.IsClassDataStripped(CDSF_RayTracingResources))
		{
			TArray<uint8> RawData;
			RawData.BulkSerialize(Ar);
		}

		TArray<FStaticMeshSectionAreaWeightedTriangleSampler> AreaWeightedSectionSamplers;
		FStaticMeshAreaWeightedSectionSampler AreaWeightedSampler;

		AreaWeightedSectionSamplers.AddZeroed(Lod.Sections.Num());
		for (int i = 0; i < AreaWeightedSectionSamplers.Num(); i++)
			Ar << AreaWeightedSectionSamplers[i];
		Ar << AreaWeightedSampler;

		unguard;
	}
};


struct FMeshSectionInfo
{
	int					MaterialIndex;
	bool				bEnableCollision;
	bool				bCastShadow;

	friend FArchive& operator<<(FArchive& Ar, FMeshSectionInfo& S)
	{
		return Ar << S.MaterialIndex << S.bEnableCollision << S.bCastShadow;
	}
};

FArchive& operator<<(FArchive& Ar, FStaticMaterial& M)
{
	guard(FStaticMaterial<<);

	Ar << M.MaterialInterface << M.MaterialSlotName;
	if (Ar.ContainsEditorData())
	{
		FName ImportedMaterialSlotName;
		Ar << ImportedMaterialSlotName;
	}
	if (FRenderingObjectVersion::Get(Ar) >= FRenderingObjectVersion::TextureStreamingMeshUVChannelData)
	{
		Ar << M.UVChannelData;
	}
	return Ar;

	unguard;
}

void UStaticMesh4::Serialize(FArchive &Ar)
{
	guard(UStaticMesh4::Serialize);

	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);
	bool bCooked;
	Ar << bCooked;
	DBG_STAT("Serializing %s StaticMesh\n", bCooked ? "cooked" : "source");

	Ar << BodySetup;

#if TEKKEN7 || HIT || GEARS4
	if (Ar.Game == GAME_Tekken7 || Ar.Game == GAME_HIT || Ar.Game == GAME_Gears4) goto no_nav_collision;
#endif

	if (Ar.ArVer >= VER_UE4_STATIC_MESH_STORE_NAV_COLLISION)
		Ar << NavCollision;

no_nav_collision:

	if (!StripFlags.IsEditorDataStripped())
	{
		if (Ar.ArVer < VER_UE4_DEPRECATED_STATIC_MESH_THUMBNAIL_PROPERTIES_REMOVED)
		{
			FRotator DummyThumbnailAngle;
			float DummyThumbnailDistance;
			Ar << DummyThumbnailAngle << DummyThumbnailDistance;
		}
		FString HighResSourceMeshName;
		unsigned HighResSourceMeshCRC;
		Ar << HighResSourceMeshName << HighResSourceMeshCRC;
	}

	Ar << LightingGuid;

	//!! TODO: support sockets
	// Warning: serialized twice - as UPROPERTY and as a binary serializer
	Ar << Sockets;
	if (Sockets.Num()) appPrintf("StaticMesh has %d sockets\n", Sockets.Num());

	// editor models
	if (!StripFlags.IsEditorDataStripped())
	{
		DBG_STAT("Serializing %d SourceModels\n", SourceModels.Num());
		// Reference: FStaticMeshSourceModel::SerializeBulkData
		for (int i = 0; i < SourceModels.Num(); i++)
		{
			if (FEditorObjectVersion::Get(Ar) < FEditorObjectVersion::StaticMeshDeprecatedRawMesh)
			{
				// Serialize FRawMeshBulkData
				SourceModels[i].BulkData.Serialize(Ar);
				// drop extra fields
				FGuid Guid;
				bool bGuidIsHash;
				Ar << Guid << bGuidIsHash;
			}
			else
			{
				bool bIsValid;
				Ar << bIsValid;
				if (bIsValid)
				{
					// FMeshDescriptionBulkData::Serialize
					FByteBulkData BulkData;
					BulkData.Serialize(Ar);
					// drop extra fields
					if (FEditorObjectVersion::Get(Ar) >= FEditorObjectVersion::MeshDescriptionBulkDataGuid)
					{
						FGuid Guid;
						Ar << Guid;
					}
					if (FEnterpriseObjectVersion::Get(Ar) >= FEnterpriseObjectVersion::MeshDescriptionBulkDataGuidIsHash)
					{
						bool bGuidIsHash;
						Ar << bGuidIsHash;
					}
					//!! todo:
					// - serialization of BulkData: FMeshDescription::Serialize(FArchive& Ar)
					//   - using CustomVersions derived from uasset's FArchive
					// - most or all fields are: operator<<( FArchive& Ar, TMeshElementArrayBase& Array ):
					//   - TBitArray AllocatedIndices, then serialized all items where bit is 1, using element's serializer
					// - using TBitArray: storing data as uint32 NumBits + uint32[NumWords], where NumWords = RoundUp(NumBits / 32)
				}
			}

		}
		if (FEditorObjectVersion::Get(Ar) < FEditorObjectVersion::UPropertryForMeshSection)
		{
			// in 4.15 this is serialized as property
			TMap<unsigned, FMeshSectionInfo> MeshSectionInfo;
			Ar << MeshSectionInfo;
		}
	}

	// serialize FStaticMeshRenderData
	if (bCooked)
	{
#if DAYSGONE
		if (Ar.Game == GAME_DaysGone)
		{
			TArray<int32> unk;
			Ar << unk;
		}
#endif // DAYSGONE

		// Note: code below still contains 'if (bCooked)' switches, this is because the same
		// code could be used to read data from DDC, for non-cooked assets.
		DBG_STAT("Serializing RenderData\n");
		if (Ar.Game >= GAME_UE4(27) && KeepMobileMinLODSettingOnDesktop)
		{
			// The serialization of this variable is cvar-dependent in UE4, so there's no clear way to understand
			// if it should be serialize in our code or not.
			int32 MinMobileLODIdx;
			Ar << MinMobileLODIdx;
		}
		if (!bCooked)
		{
			TArray<int> WedgeMap;
			TArray<int> MaterialIndexToImportIndex;
			Ar << WedgeMap << MaterialIndexToImportIndex;
		}

		Lods.Serialize2<FStaticMeshLODModel4::Serialize>(Ar); // original code: TArray<FStaticMeshLODResources> LODResources

		if (Ar.Game >= GAME_UE4(23))
		{
			uint8 NumInlinedLODs;
			Ar << NumInlinedLODs;
		}

		guard(PostLODCode);

		if (bCooked)
		{
			if (Ar.ArVer >= VER_UE4_RENAME_CROUCHMOVESCHARACTERDOWN)
			{
				// UE4.5+, added distance field, reference: 28.08.2014 - f5238f04
				bool stripped = false;
				if (Ar.ArVer >= VER_UE4_RENAME_WIDGET_VISIBILITY)
				{
					// 4.7+ - using IsDataStrippedForServer for distance field removal
					/// reference: 13.11.2014 - 48a3c9b7
					FStripDataFlags StripFlags2(Ar);
					stripped = StripFlags2.IsDataStrippedForServer();
					if (Ar.Game >= GAME_UE4(21))
					{
						// 4.21 uses additional strip flag for distance field
						const uint8 DistanceFieldDataStripFlag = 1;
						stripped |= StripFlags2.IsClassDataStripped(DistanceFieldDataStripFlag);
					}
				}
				if (!stripped)
				{
					// serialize FDistanceFieldVolumeData for each LOD
					for (int i = 0; i < Lods.Num(); i++)
					{
						bool HasDistanceDataField;
						Ar << HasDistanceDataField;
						if (HasDistanceDataField)
						{
							FDistanceFieldVolumeData VolumeData;
							Ar << VolumeData;
						}
					}
				}
			}
		}

		Ar << Bounds;

		// Note: bLODsShareStaticLighting field exists in all engine versions except UE4.15.
		if (Ar.Game >= GAME_UE4(15) && Ar.Game < GAME_UE4(16)) goto no_LODShareStaticLighting;

		Ar << bLODsShareStaticLighting;

	no_LODShareStaticLighting:

		if (Ar.Game < GAME_UE4(14))
		{
			bool bReducedBySimplygon;
			Ar << bReducedBySimplygon;
		}

		if (FRenderingObjectVersion::Get(Ar) < FRenderingObjectVersion::TextureStreamingMeshUVChannelData)
		{
			float StreamingTextureFactors[MAX_STATIC_UV_SETS_UE4];
			float MaxStreamingTextureFactor;
			// StreamingTextureFactor for each UV set
			for (int i = 0; i < MAX_STATIC_UV_SETS_UE4; i++)
				Ar << StreamingTextureFactors[i];
			Ar << MaxStreamingTextureFactor;
		}

		if (bCooked)
		{
			// ScreenSize for each LOD
			int MaxNumLods = (Ar.Game >= GAME_UE4(9)) ? MAX_STATIC_LODS_UE4 : 4;
			for (int i = 0; i < MaxNumLods; i++)
			{
				// Starting with UE4.20 it uses TPerPlatformProperty<float> = FPerPlatformFloat, which has different serializer
				if (Ar.Game >= GAME_UE4(20))
				{
					bool bFloatCooked;
					Ar << bFloatCooked;
				}
				Ar << ScreenSize[i];
			}
		}

#if BORDERLANDS3
		if (Ar.Game == GAME_Borderlands3)
		{
			// Array of non-standard array
			int Count;
			Ar << Count;
			for (int i = 0; i < Count; i++)
			{
				byte Count2;
				Ar << Count2;
				Ar.Seek(Ar.Tell() + Count2 * 12); // bool, bool, float
			}
		}
#endif // BORDERLANDS3

		unguard;
	} // end of FStaticMeshRenderData

	if (bCooked && Ar.Game >= GAME_UE4(20))
	{
		guard(SerializeOccluderData);
		// FStaticMeshOccluderData::SerializeCooked
		bool bHasOccluderData;
		Ar << bHasOccluderData;
		if (bHasOccluderData)
		{
			TArray<FVector> Vertices;
			TArray<uint16> Indices;
			Vertices.BulkSerialize(Ar);
			Indices.BulkSerialize(Ar);
		}
		unguard;
	}

	if (Ar.Game >= GAME_UE4(14) /* && StaticMaterials.Num() == 0 */) // it seems that StaticMaterials serialized as properties has no material links
	{
		// Serialize following data to obtain material references for UE4.14+.
		// Don't bother serializing anything beyond this point in earlier versions.
		// Note: really, UE4 uses VER_UE4_SPEEDTREE_STATICMESH
		bool bHasSpeedTreeWind;
		Ar << bHasSpeedTreeWind;
		if (bHasSpeedTreeWind)
		{
			//TODO - FSpeedTreeWind serialization
			DROP_REMAINING_DATA(Ar);
			char buf[1024];
			GetFullName(buf, 1024);
			appNotify("Dropping SpeedTree and material data for StaticMesh %s", buf);
		}
		else
		{
			if (FEditorObjectVersion::Get(Ar) >= FEditorObjectVersion::RefactorMeshEditorMaterials)
			{
				// UE4.14+ - "Materials" are deprecated, added StaticMaterials
				Ar << StaticMaterials;
			}
		}
	}

	// Copy StaticMaterials to Materials
	if (Materials.Num() == 0 && StaticMaterials.Num() > 0)
	{
		Materials.AddUninitialized(StaticMaterials.Num());
		for (int i = 0; i < StaticMaterials.Num(); i++)
			Materials[i] = StaticMaterials[i].MaterialInterface;
	}

	// remaining is SpeedTree data
	DROP_REMAINING_DATA(Ar);

#if DAYSGONE
	if (Ar.Game == GAME_DaysGone)
	{
		// This game has packed positions, we should unpack it. When loading positions,
		// we're converting values to range 0..1. Now we should use Bounds for it.
		FVector Offset = Bounds.Origin;
		Offset.Subtract(Bounds.BoxExtent);
		FVector Scale = Bounds.BoxExtent;
		Scale.Scale(2);
		for (FStaticMeshLODModel4& Lod : Lods)
		{
			if (Lod.PositionVertexBuffer.Stride == 8 || Lod.PositionVertexBuffer.Stride == 4)
			{
				for (FVector& V : Lod.PositionVertexBuffer.Verts)
				{
					V.Scale(Scale);
					V.Add(Offset);
				}
			}
		}
	}
#endif // DAYSGONE

	if (bCooked)
	{
		ConvertMesh();
		// Release original mesh data to save memory
		Lods.Empty();
	}
	else
	{
		ConvertSourceModels();
	}

	unguard;
}


void UStaticMesh4::ConvertMesh()
{
	guard(UStaticMesh4::ConvertMesh);

	CStaticMesh *Mesh = new CStaticMesh(this);
	ConvertedMesh = Mesh;

	// convert bounds
	Mesh->BoundingSphere.R = Bounds.SphereRadius / 2;			//?? UE3 meshes has radius 2 times larger than mesh itself; verify for UE4
	VectorSubtract(CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Min));
	VectorAdd     (CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Max));

	// convert lods
	Mesh->Lods.Empty(Lods.Num());
	for (int lodIndex = 0; lodIndex < Lods.Num(); lodIndex++)
	{
		guard(ConvertLod);

		const FStaticMeshLODModel4 &SrcLod = Lods[lodIndex];

		int NumTexCoords = SrcLod.VertexBuffer.NumTexCoords;
		int NumVerts     = SrcLod.PositionVertexBuffer.Verts.Num();

		if (NumVerts == 0 && NumTexCoords == 0 && lodIndex < Lods.Num()-1)
		{
			// UE4.20+, see CDSF_MinLodData
			appPrintf("Lod #%d is stripped, skipping ...\n", lodIndex);
			continue;
		}

		if (NumTexCoords > MAX_MESH_UV_SETS)
			appError("StaticMesh has %d UV sets", NumTexCoords);

		CStaticMeshLod *Lod = new (Mesh->Lods) CStaticMeshLod;

		Lod->NumTexCoords = NumTexCoords;
		Lod->HasNormals   = true;
		Lod->HasTangents  = true;

		// sections
		Lod->Sections.AddDefaulted(SrcLod.Sections.Num());
		for (int i = 0; i < SrcLod.Sections.Num(); i++)
		{
			CMeshSection &Dst = Lod->Sections[i];
			const FStaticMeshSection4 &Src = SrcLod.Sections[i];
			if (Materials.IsValidIndex(Src.MaterialIndex))
				Dst.Material = (UUnrealMaterial*)Materials[Src.MaterialIndex];
			Dst.FirstIndex = Src.FirstIndex;
			Dst.NumFaces   = Src.NumTriangles;
		}

		// vertices
		Lod->AllocateVerts(NumVerts);
		if (SrcLod.ColorVertexBuffer.NumVertices)
			Lod->AllocateVertexColorBuffer();

		for (int i = 0; i < NumVerts; i++)
		{
			const FStaticMeshUVItem4 &SUV = SrcLod.VertexBuffer.UV[i];
			CStaticMeshVertex &V = Lod->Verts[i];

			V.Position = CVT(SrcLod.PositionVertexBuffer.Verts[i]);
			UnpackNormals(SUV.Normal, V);
			// copy UV
			const FMeshUVFloat* fUV = &SUV.UV[0];
			V.UV = *CVT(fUV);
			for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
			{
				fUV++;
				Lod->ExtraUV[TexCoordIndex-1][i] = *CVT(fUV);
			}
			if (Lod->VertexColors)
			{
				Lod->VertexColors[i] = SrcLod.ColorVertexBuffer.Data[i];
			}
		}

		// indices
		Lod->Indices.Initialize(&SrcLod.IndexBuffer.Indices16, &SrcLod.IndexBuffer.Indices32);
		if (Lod->Indices.Num() == 0) appError("This StaticMesh doesn't have an index buffer");

		unguardf("lod=%d", lodIndex);
	}

	Mesh->FinalizeMesh();

	unguard;
}


struct FRawMesh
{
	TArray<int32>		FaceMaterialIndices;
	TArray<uint32>		FaceSmoothingMask;
	TArray<FVector>		VertexPositions;
	TArray<int32>		WedgeIndices;
	TArray<FVector>		WedgeTangent;
	TArray<FVector>		WedgeBinormal;
	TArray<FVector>		WedgeNormal;
	TArray<FVector2D>	WedgeTexCoords[MAX_STATIC_UV_SETS_UE4];
	TArray<FColor>		WedgeColors;
	TArray<int32>		MaterialIndexToImportIndex;

	void Serialize(FArchive& Ar)
	{
		guard(FRawMesh::Serialize);

		int32 Version, LicenseeVersion;
		Ar << Version << LicenseeVersion;

		Ar << FaceMaterialIndices;
		Ar << FaceSmoothingMask;
		Ar << VertexPositions;
		Ar << WedgeIndices;
		Ar << WedgeTangent;
		Ar << WedgeBinormal;
		Ar << WedgeNormal;
		for (int i = 0; i < MAX_STATIC_UV_SETS_UE4; i++)
			Ar << WedgeTexCoords[i];
		Ar << WedgeColors;

		if (Version >= 1) // RAW_MESH_VER_REMOVE_ZERO_TRIANGLE_SECTIONS
			Ar << MaterialIndexToImportIndex;

		unguard;
	}
};


void UStaticMesh4::ConvertSourceModels()
{
	guard(UStaticMesh4::ConvertSourceModels);

	CStaticMesh *Mesh = new CStaticMesh(this);
	ConvertedMesh = Mesh;

	// convert bounds
	// (note: copy-paste of ConvertedMesh's code)
	Mesh->BoundingSphere.R = Bounds.SphereRadius / 2;			//?? UE3 meshes has radius 2 times larger than mesh itself; verify for UE4
	VectorSubtract(CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Min));
	VectorAdd     (CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Max));

	// convert lods
	Mesh->Lods.Empty(Lods.Num());

	for (int LODIndex = 0; LODIndex < SourceModels.Num(); LODIndex++)
	{
		guard(ConvertLod);

		const FStaticMeshSourceModel& SrcModel = SourceModels[LODIndex];
		const FByteBulkData& Bulk = SrcModel.BulkData;
		if (Bulk.ElementCount == 0) continue;	// this SourceModel has generated LOD, not imported one

		CStaticMeshLod *Lod = new (Mesh->Lods) CStaticMeshLod;

		FRawMesh RawMesh;
		FMemReader Reader(Bulk.BulkData, Bulk.ElementCount); // ElementCount is the same as data size, for byte bulk data
		Reader.SetupFrom(*GetPackageArchive());
		RawMesh.Serialize(Reader);

		int NumTexCoords = MAX_STATIC_UV_SETS_UE4;
		for (int i = 0; i < MAX_STATIC_UV_SETS_UE4; i++)
		{
			if (!RawMesh.WedgeTexCoords[i].Num())
			{
				NumTexCoords = i;
				break;
			}
		}

		if (NumTexCoords > MAX_MESH_UV_SETS)
			appError("StaticMesh has %d UV sets", NumTexCoords);

		Lod->NumTexCoords = NumTexCoords;
		Lod->HasNormals   = !SrcModel.BuildSettings.bRecomputeNormals && (RawMesh.WedgeNormal.Num() > 0);
		Lod->HasTangents  = !SrcModel.BuildSettings.bRecomputeTangents && (RawMesh.WedgeTangent.Num() > 0) && (RawMesh.WedgeBinormal.Num() > 0)
							&& Lod->HasNormals;
		//!! TODO: should use FaceSmoothingMask for recomputing normals

		int PrevMaterialIndex = -1;
		for (int i = 0; i < RawMesh.FaceMaterialIndices.Num(); i++)
		{
			int MaterialIndex = RawMesh.FaceMaterialIndices[i];
			// We're not performing UE4-like mesh build, where multiple sections with the same
			// material will be combined into a single one. Instead, we're making a separate
			// section in that case.
			if (MaterialIndex != PrevMaterialIndex)
			{
				PrevMaterialIndex = MaterialIndex;
				CMeshSection* Sec = new (Lod->Sections) CMeshSection;
				if (Materials.IsValidIndex(MaterialIndex))
					Sec->Material = (UUnrealMaterial*)Materials[MaterialIndex];
				else
					Sec->Material = NULL;
				Sec->FirstIndex = i * 3;
			}
		}
		// Count face count per section
		for (int i = 0; i < Lod->Sections.Num(); i++)
		{
			CMeshSection& Sec = Lod->Sections[i];
			if (i < Lod->Sections.Num() - 1)
				Sec.NumFaces = (Lod->Sections[i+1].FirstIndex - Sec.FirstIndex) / 3;
			else
				Sec.NumFaces = RawMesh.FaceMaterialIndices.Num() - Sec.FirstIndex / 3;
		}

		// vertices
		int NumVerts = RawMesh.WedgeIndices.Num();
		Lod->AllocateVerts(NumVerts);
		assert(NumTexCoords >= 1);

		for (int i = 0; i < NumVerts; i++)
		{
			CStaticMeshVertex &V = Lod->Verts[i];

			int PositionIndex = RawMesh.WedgeIndices[i];
			V.Position = CVT(RawMesh.VertexPositions[PositionIndex]);
			// Pack normals
			if (Lod->HasNormals)
			{
				CVec3 Normal = CVT(RawMesh.WedgeNormal[i]);
				if (Lod->HasTangents)
				{
					CVec3 Tangent = CVT(RawMesh.WedgeTangent[i]);
					CVec3 Binormal = CVT(RawMesh.WedgeBinormal[i]);
					Pack(V.Normal, Normal);
					Pack(V.Tangent, Tangent);
					CVec3 ComputedBinormal;
					cross(Normal, Tangent, ComputedBinormal);
					float Sign = dot(Binormal, ComputedBinormal);
					V.Normal.SetW(Sign > 0 ? 1.0f : -1.0f);
				}
			}

			// copy UV
			V.UV = CVT(RawMesh.WedgeTexCoords[0][i]);
			for (int TexCoordIndex = 1; TexCoordIndex < NumTexCoords; TexCoordIndex++)
			{
				Lod->ExtraUV[TexCoordIndex-1][i] = CVT(RawMesh.WedgeTexCoords[TexCoordIndex][i]);
			}
			//!! also has ColorStream
		}

		// indices
		TArray<unsigned>& Indices32 = Lod->Indices.Indices32;
		Indices32.AddUninitialized(NumVerts);
		for (int i = 0; i < NumVerts; i++)
			Indices32[i] = i;

		unguardf("lod=%d", LODIndex);
	}

	Mesh->FinalizeMesh();

	unguard;
}


/*-----------------------------------------------------------------------------
	UMorphTarget
-----------------------------------------------------------------------------*/

/*static*/ void FMorphTargetDelta::Serialize4(FArchive& Ar, FMorphTargetDelta& V)
{
	Ar << V.PositionDelta;

	if (Ar.ArVer < VER_UE4_MORPHTARGET_CPU_TANGENTZDELTA_FORMATCHANGE)
	{
		// Pre-UE4.1
		FPackedNormal TangentZDelta;
		Ar << TangentZDelta;
		V.TangentZDelta = TangentZDelta; // unpack
	}
	else
	{
		Ar << V.TangentZDelta;
	}

	Ar << V.SourceIdx;
}

/*static*/ void FMorphTargetLODModel::Serialize4(FArchive& Ar, FMorphTargetLODModel& Lod)
{
	guard(FMorphTargetLODModel::Serialize4);

	Lod.Vertices.Serialize2<FMorphTargetDelta::Serialize4>(Ar);
	Ar << Lod.NumBaseMeshVerts;
	DBG_SKEL("MorphLod: %d verts, MeshVerts = %d\n", Lod.Vertices.Num(), Lod.NumBaseMeshVerts);

	if (FEditorObjectVersion::Get(Ar) >= FEditorObjectVersion::AddedMorphTargetSectionIndices)
	{
		Ar << Lod.SectionIndices;
	}

	if (Ar.Game >= GAME_UE4(20))
	{
		// Actually using FFortniteMainBranchObjectVersion::SaveGeneratedMorphTargetByEngine
		bool bGeneratedByEngine;
		Ar << bGeneratedByEngine;
	}

	unguard;
}

void UMorphTarget::Serialize4(FArchive& Ar)
{
	guard(UMorphTarget::Serialize4);

	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);
	if (!StripFlags.IsDataStrippedForServer())
	{
		MorphLODModels.Serialize2<FMorphTargetLODModel::Serialize4>(Ar);
	}

	unguard;
}

#endif // UNREAL4
