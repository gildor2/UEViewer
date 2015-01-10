#include "Core.h"

#if UNREAL4

#include "UnrealClasses.h"
#include "UnMesh4.h"
#include "UnMeshTypes.h"

#include "StaticMesh.h"
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


#if NAX_STATIC_UV_SETS_UE4 != NUM_MESH_UV_SETS
//!!#error MAX_STATIC_UV_SETS_UE4 and NUM_MESH_UV_SETS are not matching!
#endif


UStaticMesh4::UStaticMesh4()
{}

UStaticMesh4::~UStaticMesh4()
{
	delete ConvertedMesh;
}


// Ambient occlusion data
// When changed, constant DISTANCEFIELD_DERIVEDDATA_VER TEXT is updated
struct FDistanceFieldVolumeData
{
	TArray<short>	DistanceFieldVolume;	// TArray<Float16>
	FIntVector		Size;
	FBox			LocalBoundingBox;
	bool			bMeshWasClosed;
	bool			bBuiltAsIfTwoSided;
	bool			bMeshWasPlane;

	friend FArchive& operator<<(FArchive& Ar, FDistanceFieldVolumeData& V)
	{
		Ar << V.DistanceFieldVolume << V.Size << V.LocalBoundingBox << V.bMeshWasClosed;
		/// reference: 28.08.2014 - f5238f04
		if (Ar.ArVer >= VER_UE4_RENAME_CROUCHMOVESCHARACTERDOWN)
			Ar << V.bBuiltAsIfTwoSided;
		/// reference: 12.09.2014 - 890f1205
		if (Ar.ArVer >= VER_UE4_DEPRECATE_UMG_STYLE_ASSETS)
			Ar << V.bMeshWasPlane;
		return Ar;
	}
};


struct FStaticMeshSection4
{
	int				MaterialIndex;
	int				FirstIndex;
	int				NumTriangles;
	int				MinVertexIndex;
	int				MaxVertexIndex;
	bool			bEnableCollision;
	bool			bCastShadow;

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshSection4& S)
	{
		Ar << S.MaterialIndex;
		Ar << S.FirstIndex << S.NumTriangles;
		Ar << S.MinVertexIndex << S.MaxVertexIndex;
		Ar << S.bEnableCollision << S.bCastShadow;
		return Ar;
	}
};


struct FPositionVertexBuffer4
{
	TArray<FVector>	Verts;
	int				Stride;
	int				NumVertices;

	friend FArchive& operator<<(FArchive& Ar, FPositionVertexBuffer4& S)
	{
		guard(FPositionVertexBuffer4<<);

		Ar << S.Stride << S.NumVertices;
		DBG_STAT("StaticMesh PositionStream: IS:%d NV:%d\n", S.Stride, S.NumVertices);
		Ar << RAW_ARRAY(S.Verts);
		return Ar;

		unguard;
	}
};


static int  GNumStaticUVSets   = 1;
static bool GUseStaticFloatUVs = true;

struct FStaticMeshUVItem4
{
	FPackedNormal	Normal[3];
	FMeshUVFloat	UV[MAX_STATIC_UV_SETS_UE4];

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshUVItem4& V)
	{
		Ar << V.Normal[0] << V.Normal[2];	// TangentX and TangentZ

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
		return Ar;
	}
};


struct FStaticMeshVertexBuffer4
{
	int				NumTexCoords;
	int				Stride;
	int				NumVertices;
	bool			bUseFullPrecisionUVs;
	TArray<FStaticMeshUVItem4> UV;

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshVertexBuffer4& S)
	{
		guard(FStaticMeshVertexBuffer4<<);

		FStripDataFlags StripFlags(Ar, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);
		Ar << S.NumTexCoords << S.Stride << S.NumVertices;
		Ar << S.bUseFullPrecisionUVs;
		DBG_STAT("StaticMesh UV stream: TC:%d IS:%d NV:%d FloatUV:%d\n", S.NumTexCoords, S.Stride, S.NumVertices, S.bUseFullPrecisionUVs);

		if (!StripFlags.IsDataStrippedForServer())
		{
			GNumStaticUVSets = S.NumTexCoords;
			GUseStaticFloatUVs = S.bUseFullPrecisionUVs;
			Ar << RAW_ARRAY(S.UV);
		}

		return Ar;

		unguard;
	}
};


struct FColorVertexBuffer4
{
	int				Stride;
	int				NumVertices;
	TArray<FColor>	Data;

	friend FArchive& operator<<(FArchive& Ar, FColorVertexBuffer4& S)
	{
		guard(FColorVertexBuffer4<<);

		FStripDataFlags StripFlags(Ar, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);
		Ar << S.Stride << S.NumVertices;
		DBG_STAT("StaticMesh ColorStream: IS:%d NV:%d\n", S.Stride, S.NumVertices);
		if (!StripFlags.IsDataStrippedForServer() && (S.NumVertices > 0)) // zero size arrays are not serialized
			Ar << RAW_ARRAY(S.Data);
		return Ar;

		unguard;
	}
};


//!! if use this class for SkeletalMesh - use both DBG_STAT and DBG_SKEL
struct FRawStaticIndexBuffer4
{
	TArray<word>		Indices16;
	TArray<unsigned>	Indices32;

	FORCEINLINE bool Is32Bit() const
	{
		return (Indices32.Num() != 0);
	}

	friend FArchive& operator<<(FArchive &Ar, FRawStaticIndexBuffer4 &S)
	{
		guard(FRawStaticIndexBuffer4<<);

		if (Ar.ArVer < VER_UE4_SUPPORT_32BIT_STATIC_MESH_INDICES)
		{
			Ar << RAW_ARRAY(S.Indices16);
			DBG_STAT("RawIndexBuffer, old format - %d indices\n", S.Indices16.Num());
		}
		else
		{
			// serialize all indices as byte array
			bool is32bit;
			TArray<byte> data;
			Ar << is32bit;
			Ar << RAW_ARRAY(data);
			DBG_STAT("RawIndexBuffer, 32 bit = %d, %d indices (data size = %d)\n", is32bit, data.Num() / (is32bit ? 4 : 2), data.Num());
			if (!data.Num()) return Ar;

			// convert data
			if (is32bit)
			{
				int count = data.Num() / 4;
				byte* src = &data[0];
				if (Ar.ReverseBytes)
					appReverseBytes(src, count, 4);
				S.Indices32.Add(count);
				for (int i = 0; i < count; i++, src += 4)
					S.Indices32[i] = *(int*)src;
			}
			else
			{
				int count = data.Num() / 2;
				byte* src = &data[0];
				if (Ar.ReverseBytes)
					appReverseBytes(src, count, 2);
				S.Indices16.Add(count);
				for (int i = 0; i < count; i++, src += 2)
					S.Indices16[i] = *(word*)src;
			}
		}

		return Ar;

		unguard;
	}
};


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
	FRawStaticIndexBuffer4   DepthOnlyIndexBuffer;
	FRawStaticIndexBuffer4   WireframeIndexBuffer;
	FRawStaticIndexBuffer4   AdjacencyIndexBuffer;
	float                    MaxDeviation;

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshLODModel4 &Lod)
	{
		guard(FStaticMeshLODModel4<<);

		FStripDataFlags StripFlags(Ar);

		Ar << Lod.Sections;
#if DEBUG_STATICMESH
		appPrintf("%d sections\n", Lod.Sections.Num());
		for (int i = 0; i < Lod.Sections.Num(); i++)
		{
			FStaticMeshSection4 &S = Lod.Sections[i];
			appPrintf("  mat=%d firstIdx=%d numTris=%d firstVers=%d maxVert=%d\n", S.MaterialIndex, S.FirstIndex, S.NumTriangles,
				S.MinVertexIndex, S.MaxVertexIndex);
		}
#endif // DEBUG_STATICMESH

		Ar << Lod.MaxDeviation;

		if (!StripFlags.IsDataStrippedForServer())
		{
			Ar << Lod.PositionVertexBuffer;
			Ar << Lod.VertexBuffer;
			Ar << Lod.ColorVertexBuffer;
			Ar << Lod.IndexBuffer;
			Ar << Lod.DepthOnlyIndexBuffer;

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

			if (!StripFlags.IsClassDataStripped(1))
				Ar << Lod.AdjacencyIndexBuffer;
		}

		return Ar;
		unguard;
	}
};


void UStaticMesh4::Serialize(FArchive &Ar)
{
	guard(UStaticMesh4::Serialize);

	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);
	bool bCooked;
	Ar << bCooked;

	Ar << BodySetup;

	if (Ar.ArVer >= VER_UE4_STATIC_MESH_STORE_NAV_COLLISION)
		Ar << NavCollision;
	if (!StripFlags.IsEditorDataStripped())
	{
		// thumbnail, etc
		appError("UE4 editor packages are not supported");
	}

	Ar << LightingGuid;

	//!! TODO: support sockets
	Ar << Sockets;
	if (Sockets.Num()) appNotify("StaticMesh has %d sockets", Sockets.Num());

	// serialize FStaticMeshRenderData

	DBG_STAT("Serializing LODs\n");
	Ar << Lods;

	if (bCooked && (Ar.ArVer >= VER_UE4_RENAME_CROUCHMOVESCHARACTERDOWN))
	{
		/// reference: 28.08.2014 - f5238f04
		bool stripped = false;
		if (Ar.ArVer >= VER_UE4_RENAME_WIDGET_VISIBILITY)
		{
			/// reference: 13.11.2014 - 48a3c9b7
			FStripDataFlags StripFlags2(Ar);
			stripped = StripFlags.IsDataStrippedForServer();
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

	Ar << Bounds;
	Ar << bLODsShareStaticLighting << bReducedBySimplygon;

	// StreamingTextureFactor for each UV set
	for (int i = 0; i < MAX_STATIC_UV_SETS_UE4; i++)
		Ar << StreamingTextureFactors[i];
	Ar << MaxStreamingTextureFactor;

	if (bCooked)
	{
		// ScreenSize for each LOD
		for (int i = 0; i < MAX_STATIC_LODS_UE4; i++)
			Ar << ScreenSize[i];
	}

	// remaining is SpeedTree data
	DROP_REMAINING_DATA(Ar);

	ConvertMesh();

	unguard;
}


void UStaticMesh4::ConvertMesh()
{
	guard(UStaticMesh4::ConvertedMesh);

	CStaticMesh *Mesh = new CStaticMesh(this);
	ConvertedMesh = Mesh;

	// convert bounds
	Mesh->BoundingSphere.R = Bounds.SphereRadius / 2;			//?? UE3 meshes has radius 2 times larger than mesh itself; verifty for UE4
	VectorSubtract(CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Min));
	VectorAdd     (CVT(Bounds.Origin), CVT(Bounds.BoxExtent), CVT(Mesh->BoundingBox.Max));

	// convert lods
	Mesh->Lods.Empty(Lods.Num());
	for (int lod = 0; lod < Lods.Num(); lod++)
	{
		guard(ConvertLod);

		const FStaticMeshLODModel4 &SrcLod = Lods[lod];
		CStaticMeshLod *Lod = new (Mesh->Lods) CStaticMeshLod;

		int NumTexCoords = SrcLod.VertexBuffer.NumTexCoords;
		int NumVerts     = SrcLod.PositionVertexBuffer.Verts.Num();

		Lod->NumTexCoords = NumTexCoords;
		Lod->HasNormals   = true;
		Lod->HasTangents  = true;
		if (NumTexCoords > NUM_MESH_UV_SETS)	//!! support 8 UV sets
			appError("StaticMesh has %d UV sets", NumTexCoords);

		// sections
		Lod->Sections.Add(SrcLod.Sections.Num());
		for (int i = 0; i < SrcLod.Sections.Num(); i++)
		{
			CMeshSection &Dst = Lod->Sections[i];
			const FStaticMeshSection4 &Src = SrcLod.Sections[i];
			if (Src.MaterialIndex < Materials.Num())
				Dst.Material = (UUnrealMaterial*)Materials[Src.MaterialIndex];
			Dst.FirstIndex = Src.FirstIndex;
			Dst.NumFaces   = Src.NumTriangles;
		}

		// vertices
		Lod->AllocateVerts(NumVerts);
		for (int i = 0; i < NumVerts; i++)
		{
			const FStaticMeshUVItem4 &SUV = SrcLod.VertexBuffer.UV[i];
			CStaticMeshVertex &V = Lod->Verts[i];

			V.Position = CVT(SrcLod.PositionVertexBuffer.Verts[i]);
			UnpackNormals(SUV.Normal, V);
			// copy UV
#if 1
			//!! TODO: support memcpy path?
			for (int j = 0; j < NumTexCoords; j++)
				V.UV[j] = (CMeshUVFloat&)SUV.UV[j];
#else
			staticAssert((sizeof(CMeshUVFloat) == sizeof(FMeshUVFloat)) && (sizeof(V.UV) == sizeof(SUV.UV)), Incompatible_CStaticMeshUV);
			memcpy(V.UV, SUV.UV, sizeof(V.UV));
#endif
			//!! also has ColorStream
		}

		// indices
		Lod->Indices.Initialize(&SrcLod.IndexBuffer.Indices16, &SrcLod.IndexBuffer.Indices32);
		if (Lod->Indices.Num() == 0) appError("This StaticMesh doesn't have an index buffer");

		unguardf("lod=%d", lod);
	}

	Mesh->FinalizeMesh();

	unguard;
}


#endif // UNREAL4
