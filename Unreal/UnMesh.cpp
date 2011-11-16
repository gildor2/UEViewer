#include "Core.h"
#include "UnrealClasses.h"
#include "UnMesh.h"
#include "UnMesh2.h"
#include "UnMeshTypes.h"
#include "UnPackage.h"			// for loading texures by name (Rune) and checking real class name

#include "UnMaterial2.h"

#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "TypeConvert.h"


/*-----------------------------------------------------------------------------
	ULodMesh class and common data structures
-----------------------------------------------------------------------------*/

#if DEUS_EX

struct FMeshVertDeus
{
	union
	{
		struct
		{
			int X:16; int Y:16; short Z:16; short Pad:16;
		};
		struct
		{
			unsigned D1; unsigned D2;
		};
	};

	inline operator FMeshVert() const
	{
		FMeshVert r;
		r.X = X;
		r.Y = Y;
		r.Z = Z;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FMeshVertDeus &V)
	{
//		return Ar << V.X << V.Y << V.Z << V.Pad;
		return Ar << V.D1 << V.D2;
	}
};

SIMPLE_TYPE(FMeshVertDeus, unsigned)

#endif // DEUS_EX


void ULodMesh::Serialize(FArchive &Ar)
{
	guard(ULodMesh::Serialize);
	Super::Serialize(Ar);

	Ar << Version << VertexCount << Verts;

	if (Version <= 1 || Ar.Game == GAME_SplinterCell)
	{
		// skip FMeshTri2 section
		TArray<FMeshTri2> tmp;
		Ar << tmp;
	}

	Ar << Textures;
#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell && Version >= 3)
	{
		TArray<UObject*> unk80;
		Ar << unk80;
	}
#endif // SPLINTER_CELL
#if LOCO
	if (Ar.Game == GAME_Loco)
	{
		int               unk7C;
		TArray<FName>     unk80;
		TArray<FLocoUnk2> unk8C;
		TArray<FLocoUnk1> unk98;
		if (Version >= 5) Ar << unk98;
		if (Version >= 6) Ar << unk80;
		if (Version >= 7) Ar << unk8C << unk7C;
	}
#endif // LOCO
	Ar << MeshScale << MeshOrigin << RotOrigin;

	if (Version <= 1 || Ar.Game == GAME_SplinterCell)
	{
		// skip 2nd obsolete section
		TArray<word> tmp;
		Ar << tmp;
	}

	Ar << FaceLevel << Faces << CollapseWedgeThus << Wedges << Materials;
	Ar << MeshScaleMax << LODHysteresis << LODStrength << LODMinVerts << LODMorph << LODZDisplace;

#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell) return;
#endif

	if (Version >= 3)
	{
		Ar << HasImpostor << SpriteMaterial;
		Ar << ImpLocation << ImpRotation << ImpScale << ImpColor;
		Ar << ImpSpaceMode << ImpDrawMode << ImpLightMode;
	}

	if (Version >= 4)
	{
		Ar << SkinTesselationFactor;
	}
#if LINEAGE2
	if (Ar.Game == GAME_Lineage2 && Version >= 5)
	{
		int unk;
		Ar << unk;
	}
#endif // LINEAGE2
#if BATTLE_TERR
	if (Ar.Game == GAME_BattleTerr && Version >= 5)
	{
		TArray<int> unk;
		Ar << unk;
		if (Ar.ArVer >= 134) Ar.Seek(Ar.Tell()+1);	//?????? did not found in disassembly
	}
#endif // BATTLE_TERR
#if SWRC
	if (Ar.Game == GAME_RepCommando && Version >= 7)
	{
		int unkD4, unkD8, unkDC, unkE0;
		Ar << unkD4 << unkD8 << unkDC << unkE0;
	}
#endif // SWRC

	unguard;
}


#if UNREAL1

// UE1 FMeshWedge
struct FMeshWedge1
{
	word			iVertex;
	FMeshUV1		TexUV;

	operator FMeshWedge() const
	{
		FMeshWedge r;
		r.iVertex = iVertex;
		r.TexUV   = TexUV;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FMeshWedge1 &T)
	{
		Ar << T.iVertex << T.TexUV;
		return Ar;
	}
};

RAW_TYPE(FMeshWedge1)


// Says which triangles a particular mesh vertex is associated with.
// Precomputed so that mesh triangles can be shaded with Gouraud-style
// shared, interpolated normal shading.
// Used for UE1 UMesh only
struct FMeshVertConnect
{
	int				NumVertTriangles;
	int				TriangleListOffset;

	friend FArchive& operator<<(FArchive &Ar, FMeshVertConnect &C)
	{
		return Ar << C.NumVertTriangles << C.TriangleListOffset;
	}
};

SIMPLE_TYPE(FMeshVertConnect, int)


void ULodMesh::SerializeLodMesh1(FArchive &Ar, TArray<FMeshAnimSeq> &AnimSeqs, TArray<FBox> &BoundingBoxes,
	TArray<FSphere> &BoundingSpheres, int &FrameCount)
{
	guard(SerializeLodMesh1);

	// UE1: different layout
	// UMesh fields
	TLazyArray<FMeshTri>			tmpTris;
	TLazyArray<FMeshVertConnect>	tmpConnects;
	TLazyArray<int>					tmpVertLinks;
	float							tmpTextureLOD_65;	// version 65
	TArray<float>					tmpTextureLOD;		// version 66+
	unsigned						tmpAndFlags, tmpOrFlags;
	int								tmpCurPoly, tmpCurVertex;
	// ULodMesh fields
	TArray<word>					tmpCollapsePointThus;
	TArray<FMeshWedge1>				tmpWedges;
	TArray<FMeshFace>				tmpSpecialFaces;
	int								tmpModelVerts, tmpSpecialVerts;
	TArray<word>					tmpRemapAnimVerts;
	int								tmpOldFrameVerts;

	const char *realClassName = GetRealClassName();
	// here: realClassName may be "LodMesh" or "Mesh"

	// UPrimitive
	UPrimitive::Serialize(Ar);
	// UMesh
#if DEUS_EX
	// DeusEx have larger FMeshVert structure, and there is no way to detect this game by package ...
	// But file uses TLazyArray<FMeshVert>, which have ability to detect item size - here we will
	// analyze it
	bool isDeusEx = false;
	TLazyArray<FMeshVertDeus> deusVerts;
	if (Ar.ArVer > 61)									// part of TLazyArray serializer code
	{
		int pos = Ar.Tell();							// remember position
		int skipPos, numVerts;
		Ar << skipPos << AR_INDEX(numVerts);			// read parameters
//		appNotify("***> size=%g numVerts=%d", (float)(skipPos - Ar.ArPos) / numVerts, numVerts);
		if (skipPos - Ar.Tell() == numVerts * sizeof(FMeshVertDeus))
			isDeusEx = true;
		Ar.Seek(pos);									// and restore position for serialization as TLazyArray
	}
	if (isDeusEx)
	{
		Ar << deusVerts;
	}
	else
#endif // DEUS_EX
	{
		TLazyArray<FMeshVert> tmpVerts;
		Ar << tmpVerts;									// regular Unreal1 model
		CopyArray(Verts, tmpVerts);						// TLazyArray  -> TArray
	}
	Ar << tmpTris << AnimSeqs << tmpConnects;
	Ar << BoundingBox << BoundingSphere;				// serialize UPrimitive fields again
	Ar << tmpVertLinks << Textures << BoundingBoxes << BoundingSpheres << VertexCount << FrameCount;
	Ar << tmpAndFlags << tmpOrFlags << MeshScale << MeshOrigin << RotOrigin;
	Ar << tmpCurPoly << tmpCurVertex;
#if DEUS_EX
	if (isDeusEx)
	{
		// rescale mesh and copy verts
		int i;
		// detect mesh extents
		int maxCoord = 0;
		for (i = 0; i < deusVerts.Num(); i++)
		{
			const FMeshVertDeus& V = deusVerts[i];
			int c;
	#define STEP(x)	\
			c = abs(V.x); \
			if (c > maxCoord) maxCoord = c;
			STEP(X); STEP(Y); STEP(Z);
	#undef STEP
		}
		if (maxCoord > 511)
		{
			float scale = 511.0f / maxCoord;
			appPrintf("Scaling DeusEx VertMech by factor %g\n", scale);
			MeshScale.Scale(1 / scale);
			MeshOrigin.Scale(scale);
			BoundingBox.Min.Scale(scale);
			BoundingBox.Max.Scale(scale);
			BoundingSphere.R *= scale;

			for (i = 0; i < deusVerts.Num(); i++)
			{
				FMeshVertDeus& V = deusVerts[i];
				V.X = appRound(V.X * scale);
				V.Y = appRound(V.Y * scale);
				V.Z = appRound(V.Z * scale);
			}
		}
		CopyArray(Verts, deusVerts);
	}
#endif // DEUS_EX
	if (Ar.ArVer == 65)
		Ar << tmpTextureLOD_65;
	else if (Ar.ArVer >= 66)
		Ar << tmpTextureLOD;

	if (strcmp(realClassName, "Mesh") != 0)
	{
		guard(ULodMesh1::Serialize);

		// ULodMesh
		Ar << tmpCollapsePointThus << FaceLevel << Faces << CollapseWedgeThus << tmpWedges;
		Ar << Materials << tmpSpecialFaces << tmpModelVerts << tmpSpecialVerts;
		Ar << MeshScaleMax << LODHysteresis << LODStrength << LODMinVerts << LODMorph << LODZDisplace;
		Ar << tmpRemapAnimVerts << tmpOldFrameVerts;
		// convert data
		CopyArray(Wedges, tmpWedges);			// FMeshWedge1 -> FMeshWedge
		for (int i = 0; i < Wedges.Num(); i++)	// remap wedges (skip SpecialVerts)
			Wedges[i].iVertex += tmpSpecialVerts;
		appPrintf("spec faces: %d  verts: %d\n", tmpSpecialFaces.Num(), tmpSpecialVerts);
		// remap animation vertices, if needed
		if (tmpRemapAnimVerts.Num())
		{
			guard(RemapVerts);
			TArray<FMeshVert> NewVerts;
			NewVerts.Add(FrameCount * VertexCount);
			for (int j = 0; j < FrameCount; j++)
			{
				int base    = VertexCount * j;
				int oldBase = tmpOldFrameVerts * j;
				for (int k = 0; k < VertexCount; k++)
					NewVerts[base + k] = Verts[oldBase + tmpRemapAnimVerts[k]];
			}
			CopyArray(Verts, NewVerts);
			unguard;
		}

		unguard;
	}
	else
	{
		int i, j;
		// we have loaded UMesh, should upgrade it to ULodMesh
		// create materials
		Materials.Empty(Textures.Num());
		for (i = 0; i < Textures.Num(); i++)
		{
			FMeshMaterial *M = new(Materials) FMeshMaterial;
			M->PolyFlags    = 0;	// should take from triangles - will be OR of all tris.PolyFlags
			M->TextureIndex = i;
		}
		// generate faces and wedges; have similar code in Rune's USkelModel::Serialize()
		int TrisCount = tmpTris.Num();
		Faces.Empty(TrisCount);
		Wedges.Empty(TrisCount * 3);
		for (i = 0; i < TrisCount; i++)
		{
			const FMeshTri &SrcTri = tmpTris[i];
			FMeshFace *F = new(Faces) FMeshFace;
			F->MaterialIndex = SrcTri.TextureIndex;
			Materials[SrcTri.TextureIndex].PolyFlags |= SrcTri.PolyFlags;
			for (j = 0; j < 3; j++)
			{
				F->iWedge[j] = Wedges.Num();
				FMeshWedge *W = new(Wedges) FMeshWedge;
				W->iVertex = SrcTri.iVertex[j];
				W->TexUV   = SrcTri.Tex[j];
			}
		}
	}

	unguard;
}

#endif // UNREAL1


/*-----------------------------------------------------------------------------
	UVertMesh class
-----------------------------------------------------------------------------*/

void UVertMesh::BuildNormals()
{
	// UE1 meshes have no stored normals, should build them
	// This function is similar to BuildNormals() from SkelMeshInstance.cpp
	int numVerts = Verts.Num();
	int i;
	Normals.Empty(numVerts);
	Normals.Add(numVerts);
	TArray<CVec3> tmpVerts, tmpNormals;
	tmpVerts.Add(numVerts);
	tmpNormals.Add(numVerts);
	// convert verts
	for (i = 0; i < numVerts; i++)
	{
		const FMeshVert &SV = Verts[i];
		CVec3           &DV = tmpVerts[i];
		DV[0] = SV.X * MeshScale.X;
		DV[1] = SV.Y * MeshScale.Y;
		DV[2] = SV.Z * MeshScale.Z;
	}
	// iterate faces
	for (i = 0; i < Faces.Num(); i++)
	{
		const FMeshFace &F = Faces[i];
		// get vertex indices
		int i1 = Wedges[F.iWedge[0]].iVertex;
		int i2 = Wedges[F.iWedge[2]].iVertex;		// note: reverse order in comparison with SkeletalMesh
		int i3 = Wedges[F.iWedge[1]].iVertex;
		// iterate all frames
		for (int j = 0; j < FrameCount; j++)
		{
			int base = VertexCount * j;
			// compute edges
			const CVec3 &V1 = tmpVerts[base + i1];
			const CVec3 &V2 = tmpVerts[base + i2];
			const CVec3 &V3 = tmpVerts[base + i3];
			CVec3 D1, D2, D3;
			VectorSubtract(V2, V1, D1);
			VectorSubtract(V3, V2, D2);
			VectorSubtract(V1, V3, D3);
			// compute normal
			CVec3 norm;
			cross(D2, D1, norm);
			norm.Normalize();
			// compute angles
			D1.Normalize();
			D2.Normalize();
			D3.Normalize();
			float angle1 = acos(-dot(D1, D3));
			float angle2 = acos(-dot(D1, D2));
			float angle3 = acos(-dot(D2, D3));
			// add normals for triangle verts
			VectorMA(tmpNormals[base + i1], angle1, norm);
			VectorMA(tmpNormals[base + i2], angle2, norm);
			VectorMA(tmpNormals[base + i3], angle3, norm);
		}
	}
	// normalize and convert computed normals
	for (i = 0; i < numVerts; i++)
	{
		CVec3 &SN     = tmpNormals[i];
		FMeshNorm &DN = Normals[i];
		SN.Normalize();
		DN.X = appRound(SN[0] * 511 + 512);
		DN.Y = appRound(SN[1] * 511 + 512);
		DN.Z = appRound(SN[2] * 511 + 512);
	}
}


void UVertMesh::Serialize(FArchive &Ar)
{
	guard(UVertMesh::Serialize);

#if UNREAL1
	if (Ar.Engine() == GAME_UE1)
	{
		SerializeVertMesh1(Ar);
		RotOrigin.Roll = -RotOrigin.Roll;	//??
		return;
	}
#endif // UNREAL1

	Super::Serialize(Ar);
	RotOrigin.Roll = -RotOrigin.Roll;		//??

	Ar << AnimMeshVerts << StreamVersion; // FAnimMeshVertexStream: may skip this (simply seek archive)
	Ar << Verts2 << f150;
	Ar << AnimSeqs << Normals;
	Ar << VertexCount << FrameCount;
	Ar << BoundingBoxes << BoundingSpheres;

	unguard;
}


#if UNREAL1

void UVertMesh::SerializeVertMesh1(FArchive &Ar)
{
	guard(SerializeVertMesh1);

	SerializeLodMesh1(Ar, AnimSeqs, BoundingBoxes, BoundingSpheres, FrameCount);
	VertexCount = Super::VertexCount;
	BuildNormals();

	unguard;
}

#endif // UNREAL1


/*-----------------------------------------------------------------------------
	UMeshAnimation class
-----------------------------------------------------------------------------*/

UMeshAnimation::~UMeshAnimation()
{
	delete ConvertedAnim;
}


void UMeshAnimation::ConvertAnims()
{
	guard(UMeshAnimation::ConvertAnims);

	int i, j;

	CAnimSet *AnimSet = new CAnimSet(this);
	ConvertedAnim = AnimSet;

	// TrackBoneNames
	int numBones = RefBones.Num();
	AnimSet->TrackBoneNames.Add(numBones);
	for (i = 0; i < numBones; i++)
		AnimSet->TrackBoneNames[i] = RefBones[i].Name;

	// Sequences
	int numSeqs = AnimSeqs.Num();
	AnimSet->Sequences.Add(numSeqs);
	for (i = 0; i < numSeqs; i++)
	{
		CAnimSequence &S = AnimSet->Sequences[i];
		const FMeshAnimSeq &Src = AnimSeqs[i];
		const MotionChunk  &M   = Moves[i];

		// attributes
		S.Name      = Src.Name;
		S.NumFrames = Src.NumFrames;
		S.Rate      = Src.Rate;

		// S.Tracks
		S.Tracks.Add(numBones);
		for (j = 0; j < numBones; j++)
		{
			CAnimTrack &T = S.Tracks[j];
			const AnalogTrack &A = M.AnimTracks[j];
			CopyArray(T.KeyPos,  CVT(A.KeyPos));
			CopyArray(T.KeyQuat, CVT(A.KeyQuat));
			CopyArray(T.KeyTime, A.KeyTime);
		}
	}

	unguard;
}


#if SPLINTER_CELL

// quaternion with 4 16-bit fixed point fields
struct FQuatComp
{
	short			X, Y, Z, W;				// signed short, corresponds to float*32767

	inline operator FQuat() const
	{
		FQuat r;
		r.X = X / 32767.0f;
		r.Y = Y / 32767.0f;
		r.Z = Z / 32767.0f;
		r.W = W / 32767.0f;
		return r;
	}
	inline operator FVector() const			//?? for FixedPointTrack
	{
		FVector r;
		r.X = X / 64.0f;
		r.Y = Y / 64.0f;
		r.Z = Z / 64.0f;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatComp &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z << Q.W;
	}
};

SIMPLE_TYPE(FQuatComp, short)

// normalized quaternion with 3 16-bit fixed point fields
struct FQuatComp2
{
	short			X, Y, Z;				// signed short, corresponds to float*32767

	inline operator FQuat() const
	{
		FQuat r;
		r.X = X / 32767.0f;
		r.Y = Y / 32767.0f;
		r.Z = Z / 32767.0f;
		// check FQuatFloat96NoW ...
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatComp2 &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z;
	}
};

SIMPLE_TYPE(FQuatComp2, short)

struct FVectorComp
{
	short			X, Y, Z;

	inline operator FVector() const
	{
		FVector r;
		r.X = X / 64.0f;
		r.Y = Y / 64.0f;
		r.Z = Z / 64.0f;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FVectorComp &V)
	{
		return Ar << V.X << V.Y << V.Z;
	}
};

SIMPLE_TYPE(FVectorComp, short)


#define SCELL_TRACK(Name,Quat,Pos,Time)						\
struct Name													\
{															\
	TArray<Quat>			KeyQuat;						\
	TArray<Pos>				KeyPos;							\
	TArray<Time>			KeyTime;						\
															\
	void Decompress(AnalogTrack &D)							\
	{														\
		D.Flags = 0;										\
		CopyArray(D.KeyQuat, KeyQuat);						\
		CopyArray(D.KeyPos,  KeyPos );						\
		CopyArray(D.KeyTime, KeyTime);						\
	}														\
															\
	friend FArchive& operator<<(FArchive &Ar, Name &A)		\
	{														\
		return Ar << A.KeyQuat << A.KeyPos << A.KeyTime;	\
	}														\
};

SCELL_TRACK(FixedPointTrack, FQuatComp,  FQuatComp,   word)
SCELL_TRACK(Quat16Track,     FQuatComp2, FVector,     word)	// all types are "large"
SCELL_TRACK(FixPosTrack,     FQuatComp2, FVectorComp, word)	// "small" KeyPos
SCELL_TRACK(FixTimeTrack,    FQuatComp2, FVector,     byte)	// "small" KeyTime
SCELL_TRACK(FixPosTimeTrack, FQuatComp2, FVectorComp, byte)	// "small" KeyPos and KeyTime


void AnalogTrack::SerializeSCell(FArchive &Ar)
{
	TArray<FQuatComp> KeyQuat2;
	TArray<word>      KeyTime2;
	Ar << KeyQuat2 << KeyPos << KeyTime2;
	// copy with conversion
	CopyArray(KeyQuat, KeyQuat2);
	CopyArray(KeyTime, KeyTime2);
}


struct MotionChunkFixedPoint
{
	FVector					RootSpeed3D;
	float					TrackTime;
	int						StartBone;
	unsigned				Flags;
	TArray<FixedPointTrack>	AnimTracks;

	friend FArchive& operator<<(FArchive &Ar, MotionChunkFixedPoint &M)
	{
		return Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags << M.AnimTracks;
	}
};

// Note: standard UE2 MotionChunk is equalent to MotionChunkCompress<AnalogTrack>
struct MotionChunkCompressBase
{
	FVector					RootSpeed3D;
	float					TrackTime;
	int						StartBone;
	unsigned				Flags;
	TArray<int>				BoneIndices;

	virtual void Decompress(MotionChunk &D)
	{
		D.RootSpeed3D = RootSpeed3D;
		D.TrackTime   = TrackTime;
		D.StartBone   = StartBone;
		D.Flags       = Flags;
	}
};

template<class T> struct MotionChunkCompress : public MotionChunkCompressBase
{
	TArray<T>				AnimTracks;
	AnalogTrack				RootTrack;		// standard track

	virtual void Decompress(MotionChunk &D)
	{
		guard(Decompress);
		MotionChunkCompressBase::Decompress(D);
		// copy/convert tracks
		CopyArray(D.BoneIndices, BoneIndices);
		int numAnims = AnimTracks.Num();
		D.AnimTracks.Empty(numAnims);
		D.AnimTracks.Add(numAnims);
		for (int i = 0; i < numAnims; i++)
			AnimTracks[i].Decompress(D.AnimTracks[i]);
		//?? do nothing with RootTrack ...
		unguard;
	}

#if _MSC_VER == 1200	// VC6 bugs
	friend FArchive& operator<<(FArchive &Ar, MotionChunkCompress &M);
#else
	template<class T2> friend FArchive& operator<<(FArchive &Ar, MotionChunkCompress<T2> &M);
#endif
};

template<class T> FArchive& operator<<(FArchive &Ar, MotionChunkCompress<T> &M)
{
	return Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags << M.BoneIndices << M.AnimTracks << M.RootTrack;
}


void UMeshAnimation::SerializeSCell(FArchive &Ar)
{
	guard(SerializeSCell);

	// for logic of track decompression check UMeshAnimation::Moves() function
	int OldCompression = 0, CompressType = 0;
	TArray<MotionChunkFixedPoint>					T0;		// OldCompression!=0, CompressType=0
	TArray<MotionChunkCompress<Quat16Track> >		T1;		// CompressType=1
	TArray<MotionChunkCompress<FixPosTrack> >		T2;		// CompressType=2
	TArray<MotionChunkCompress<FixTimeTrack> >		T3;		// CompressType=3
	TArray<MotionChunkCompress<FixPosTimeTrack> >	T4;		// CompressType=4
	if (Version >= 1000)
	{
		Ar << OldCompression << T0;
		// note: this compression type is not supported (absent BoneIndices in MotionChunkFixedPoint)
	}
	if (Version >= 2000)
	{
		Ar << CompressType << T1 << T2 << T3 << T4;
		// decompress motion
		if (CompressType)
		{
			int i = 0, Count = 1;
			while (i < Count)
			{
				MotionChunkCompressBase *M = NULL;
				switch (CompressType)
				{
				case 1: Count = T1.Num(); M = &T1[i]; break;
				case 2: Count = T2.Num(); M = &T2[i]; break;
				case 3: Count = T3.Num(); M = &T3[i]; break;
				case 4: Count = T4.Num(); M = &T4[i]; break;
				default:
					appError("Unsupported CompressType: %d", CompressType);
				}
				if (!Count)
				{
					appNotify("CompressType=%d with no tracks", CompressType);
					break;
				}
				if (!i)
				{
					// 1st iteration, prepare Moves[] array
					Moves.Empty(Count);
					Moves.Add(Count);
				}
				// decompress current track
				M->Decompress(Moves[i]);
				// next track
				i++;
			}
		}
	}
	if (OldCompression) appNotify("OldCompression=%d", OldCompression, CompressType);//!!

	unguard;
}

#endif // SPLINTER_CELL


#if LINEAGE2

void UMeshAnimation::SerializeLineageMoves(FArchive &Ar)
{
	guard(UMeshAnimation::SerializeLineageMoves);
	if (Ar.ArVer < 123 || Ar.ArLicenseeVer < 0x19)
	{
		// standard UE2 format
		Ar << Moves;
		return;
	}
	assert(Ar.IsLoading);
	int pos, count;						// pos = global skip pos, count = data count
	Ar << pos << AR_INDEX(count);
	Moves.Empty(count);
	for (int i = 0; i < count; i++)
	{
		int localPos;
		Ar << localPos;
		MotionChunk *M = new(Moves) MotionChunk;
		Ar << *M;
		assert(Ar.Tell() == localPos);
	}
	assert(Ar.Tell() == pos);
	unguard;
}

#endif // LINEAGE2


#if SWRC

struct FVectorShortSWRC
{
	short					X, Y, Z;

	friend FArchive& operator<<(FArchive &Ar, FVectorShortSWRC &V)
	{
		return Ar << V.X << V.Y << V.Z;
	}

	FVector ToFVector(float Scale) const
	{
		FVector r;
		float s = Scale / 32767.0f;
		r.X = X * s;
		r.Y = Y * s;
		r.Z = Z * s;
		return r;
	}

	FQuat ToFQuatOld() const							// for version older than 151
	{
		static const float s = 0.000095876726845745f;	// pi/32767
		float X2 = X * s;
		float Y2 = Y * s;
		float Z2 = Z * s;
		float tmp = sqrt(X2*X2 + Y2*Y2 + Z2*Z2);
		if (tmp > 0)
		{
			float scale = sin(tmp / 2) / tmp;			// strange code ...
			X2 *= scale;
			Y2 *= scale;
			Z2 *= scale;
		}
		float W2 = 1.0f - (X2*X2 + Y2*Y2 + Z2*Z2);
		if (W2 < 0) W2 = 0;
		else W2 = sqrt(W2);
		FQuat r;
		r.Set(X2, Y2, Z2, W2);
		return r;
	}

	FQuat ToFQuat() const
	{
		static const float s = 0.70710678118f / 32767;	// short -> range(sqrt(2))
		float A = short(X & 0xFFFE) * s;
		float B = short(Y & 0xFFFE) * s;
		float C = short(Z & 0xFFFE) * s;
		float D = sqrt(1.0f - (A*A + B*B + C*C));
		if (Z & 1) D = -D;
		FQuat r;
		if (Y & 1)
		{
			if (X & 1)	r.Set(D, A, B, C);
			else		r.Set(C, D, A, B);
		}
		else
		{
			if (X & 1)	r.Set(B, C, D, A);
			else		r.Set(A, B, C, D);
		}
		return r;
	}
};

SIMPLE_TYPE(FVectorShortSWRC, short)


void AnalogTrack::SerializeSWRC(FArchive &Ar)
{
	guard(AnalogTrack::SerializeSWRC);

	float					 PosScale;
	TArray<FVectorShortSWRC> PosTrack;		// scaled by PosScale
	TArray<FVectorShortSWRC> RotTrack;
	TArray<byte>			 TimeTrack;		// frame duration

	Ar << PosScale << PosTrack << RotTrack << TimeTrack;

	// unpack data

	// time track
	int NumKeys, i;
	NumKeys = TimeTrack.Num();
	KeyTime.Empty(NumKeys);
	KeyTime.Add(NumKeys);
	int Time = 0;
	for (i = 0; i < NumKeys; i++)
	{
		KeyTime[i] = Time;
		Time += TimeTrack[i];
	}

	// rotation track
	NumKeys = RotTrack.Num();
	KeyQuat.Empty(NumKeys);
	KeyQuat.Add(NumKeys);
	for (i = 0; i < NumKeys; i++)
	{
		FQuat Q;
		if (Ar.ArVer >= 151) Q = RotTrack[i].ToFQuat();
		else				 Q = RotTrack[i].ToFQuatOld();
		// note: FMeshBone rotation is mirrored for ArVer >= 142
		Q.X *= -1;
		Q.Y *= -1;
		Q.Z *= -1;
		KeyQuat[i] = Q;
	}

	// translation track
	NumKeys = PosTrack.Num();
	KeyPos.Empty(NumKeys);
	KeyPos.Add(NumKeys);
	for (i = 0; i < NumKeys; i++)
		KeyPos[i] = PosTrack[i].ToFVector(PosScale);

	unguard;
}


void UMeshAnimation::SerializeSWRCAnims(FArchive &Ar)
{
	guard(UMeshAnimation::SerializeSWRCAnims);

	// serialize TArray<FSkelAnimSeq>
	// FSkelAnimSeq is a combined (and modified) FMeshAnimSeq and MotionChunk
	// count
	int NumAnims;
	Ar << AR_INDEX(NumAnims);		// TArray.Num
	// prepare arrays
	Moves.Empty(NumAnims);
	Moves.Add(NumAnims);
	AnimSeqs.Empty(NumAnims);
	AnimSeqs.Add(NumAnims);
	// serialize items
	for (int i = 0; i < NumAnims; i++)
	{
		// serialize
		int						f50;
		int						f54;
		int						f58;
		guard(FSkelAnimSeq<<);
		Ar << AnimSeqs[i];
		int drop;
		if (Ar.ArVer < 143) Ar << drop;
		Ar << f50 << f54;
		if (Ar.ArVer >= 143) Ar << f58;
		Ar << Moves[i].AnimTracks;
		unguard;
	}

	unguard;
}

#endif // SWRC


#if UC1

struct FVectorShortUC1
{
	short					X, Y, Z;

	friend FArchive& operator<<(FArchive &Ar, FVectorShortUC1 &V)
	{
		return Ar << V.X << V.Y << V.Z;
	}

	operator FVector() const
	{
		FVector r;
		float s = 1.0f / 100.0f;
		r.X = X * s;
		r.Y = Y * s;
		r.Z = Z * s;
		return r;
	}
};

SIMPLE_TYPE(FVectorShortUC1, short)

struct FQuatShortUC1
{
	short					X, Y, Z, W;

	friend FArchive& operator<<(FArchive &Ar, FQuatShortUC1 &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z << Q.W;
	}

	operator FQuat() const
	{
		FQuat r;
		float s = 1.0f / 16383.0f;
		r.X = X * s;
		r.Y = Y * s;
		r.Z = Z * s;
		r.W = W * s;
		return r;
	}
};

SIMPLE_TYPE(FQuatShortUC1, short)


void AnalogTrack::SerializeUC1(FArchive &Ar)
{
	guard(AnalogTrack::SerializeUC1);
	TArray<FQuatShortUC1>   PackedKeyQuat;
	TArray<FVectorShortUC1> PackedKeyPos;
	Ar << Flags << PackedKeyQuat << PackedKeyPos << KeyTime;
	CopyArray(KeyQuat, PackedKeyQuat);
	CopyArray(KeyPos, PackedKeyPos);
	unguard;
}

#endif // UC1


#if UC2

//?? move from UnTexture.cpp to UnCore.cpp?
byte *FindXprData(const char *Name, int *DataSize);

// special array type ...
//!! document purpose of this class in UE2X
template<class T> class TRawArrayUC2 : public TArray<T>
{
	// We require "using TArray<T>::*" for gcc 3.4+ compilation
	// http://gcc.gnu.org/gcc-3.4/changes.html
	// - look for "unqualified names"
	// - "temp.dep/3" section of the C++ standard [ISO/IEC 14882:2003]
	using TArray<T>::DataPtr;
	using TArray<T>::DataCount;
	using TArray<T>::MaxCount;
public:
	void Serialize(FArchive &DataAr, FArchive &CountAr)
	{
		guard(TRawArrayUC2<<);

		assert(DataAr.IsLoading && CountAr.IsLoading);
		// serialize memory size from "CountAr"
		int DataSize;
		CountAr << DataSize;
		assert(DataSize % sizeof(T) == 0);
		// setup array
		DataCount = DataSize / sizeof(T);
		DataPtr   = (DataCount) ? appMalloc(sizeof(T) * DataCount) : NULL;
		MaxCount  = DataCount;
		// serialize items from "DataAr"
		for (int i = 0; i < DataCount; i++)
			DataAr << *((T*)DataPtr + i);

		unguard;
	}
};

// helper function for RAW_ARRAY macro
template<class T> inline TRawArrayUC2<T>& ToRawArrayUC2(TArray<T> &Arr)
{
	return (TRawArrayUC2<T>&)Arr;
}

#define RAW_ARRAY_UC2(Arr)		ToRawArrayUC2(Arr)


#endif // UC2


#if UNREAL25

struct FlexTrackBase
{
	virtual ~FlexTrackBase()
	{}
	virtual void Serialize(FArchive &Ar) = 0;
#if UC2
	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		appError("FlexTrack::Serialize2() is not implemented");
	}
#endif // UC2
	virtual void Decompress(AnalogTrack &T) = 0;
};

struct FlexTrackStatic : public FlexTrackBase
{
	FQuatFloat96NoW		KeyQuat;
	TArray<FVector>		KeyPos;

	virtual void Serialize(FArchive &Ar)
	{
		Ar << KeyQuat;
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 130)
			Ar << KeyPos;
		else
#endif // UC2
		{
			FVector pos;
			Ar << pos;
			KeyPos.AddItem(pos);
		}
	}
#if UC2
	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		int Count;
		Ar2 << Count; assert(Count == 12);
		Ar << KeyQuat;
		RAW_ARRAY_UC2(KeyPos).Serialize(Ar, Ar2);
	}
#endif // UC2

	virtual void Decompress(AnalogTrack &T)
	{
		T.KeyQuat.AddItem(KeyQuat);
		CopyArray(T.KeyPos, KeyPos);
	}
};

struct FlexTrack48 : public FlexTrackBase
{
	TArray<FQuatFixed48NoW>	KeyQuat;
	TArray<short>		KeyTime;
	TArray<FVector>		KeyPos;

	virtual void Serialize(FArchive &Ar)
	{
		Ar << KeyQuat << KeyPos << KeyTime;
	}
#if UC2
	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		RAW_ARRAY_UC2(KeyQuat).Serialize(Ar, Ar2);
		RAW_ARRAY_UC2(KeyPos).Serialize(Ar, Ar2);
		RAW_ARRAY_UC2(KeyTime).Serialize(Ar, Ar2);
	}
#endif // UC2

	virtual void Decompress(AnalogTrack &T)
	{
		CopyArray(T.KeyQuat, KeyQuat);
		CopyArray(T.KeyPos,  KeyPos );
		CopyArray(T.KeyTime, KeyTime);
	}
};

struct FlexTrack48RotOnly : public FlexTrackBase
{
	TArray<FQuatFixed48NoW>	KeyQuat;
	TArray<short>		KeyTime;
	FVector				KeyPos;

	virtual void Serialize(FArchive &Ar)
	{
		Ar << KeyQuat << KeyTime << KeyPos;
	}

	virtual void Decompress(AnalogTrack &T)
	{
		CopyArray(T.KeyQuat, KeyQuat);
		CopyArray(T.KeyTime, KeyTime);
		T.KeyPos.Empty(1);
		T.KeyPos.AddItem(KeyPos);
	}
};

#if UC2

// Animation without translation (translation is from bind pose)
struct FlexTrack5 : public FlexTrackBase
{
	TArray<FQuatFixed48NoW>	KeyQuat;
	TArray<short>		KeyTime;

	virtual void Serialize(FArchive &Ar)
	{}

	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		RAW_ARRAY_UC2(KeyQuat).Serialize(Ar, Ar2);
		RAW_ARRAY_UC2(KeyTime).Serialize(Ar, Ar2);
	}

	virtual void Decompress(AnalogTrack &T)
	{
		CopyArray(T.KeyQuat, KeyQuat);
		CopyArray(T.KeyTime, KeyTime);
	}
};

// static pose with packed quaternion
struct FlexTrack6 : public FlexTrackBase
{
	FQuatFixed48NoW		KeyQuat;
	FVector				KeyPos;

	virtual void Serialize(FArchive &Ar)
	{}

	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		int Count;
		Ar2 << Count; assert(Count == 6);
		Ar << KeyQuat;
		Ar2 << Count; assert(Count == 12);
		Ar << KeyPos;
	}

	virtual void Decompress(AnalogTrack &T)
	{
		T.KeyQuat.AddItem(KeyQuat);
		T.KeyPos.AddItem(KeyPos);
	}
};

// static pose without translation
struct FlexTrack7 : public FlexTrackBase
{
	FQuatFixed48NoW		KeyQuat;

	virtual void Serialize(FArchive &Ar)
	{}

	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		int Count;
		Ar2 << Count;
		assert(Count == 6);
		Ar << KeyQuat;
	}

	virtual void Decompress(AnalogTrack &T)
	{
		T.KeyQuat.AddItem(KeyQuat);
	}
};

#endif // UC2


FlexTrackBase *CreateFlexTrack(int TrackType)
{
	switch (TrackType)
	{
	case 0:
		return NULL;		// no track for this bone (bone should be in a bind pose)

	case 1:
		return new FlexTrackStatic;

//	case 2:
//		// This type uses structure with TArray<FVector>, TArray<FQuatFloat96NoW> and TArray<short>.
//		// It's Footprint() method returns 0, GetRotPos() does nothing, but serializer is working.
//		appError("Unsupported FlexTrack type=2");

	case 3:
		return new FlexTrack48;

	case 4:
		return new FlexTrack48RotOnly;

#if UC2
	case 5:
		return new FlexTrack5;

	case 6:
		return new FlexTrack6;

	case 7:
		return new FlexTrack7;
#endif // UC2

	default:
		appError("Unknown FlexTrack type=%d", TrackType);
	}
	return NULL;
}

struct FlexTrackBasePtr
{
	FlexTrackBase* Track;

	~FlexTrackBasePtr()
	{
		if (Track) delete Track;
	}

	friend FArchive& operator<<(FArchive &Ar, FlexTrackBasePtr &T)
	{
		guard(SerializeFlexTrack);
		int TrackType;
		Ar << TrackType;
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 130 && TrackType == 4) TrackType = 3;	// replaced type: FlexTrack48RotOnly -> FlexTrack48
#endif
		T.Track = CreateFlexTrack(TrackType);
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 147) return Ar;
#endif
		if (T.Track) T.Track->Serialize(Ar);
		return Ar;
		unguard;
	}
};

// serialize TArray<FlexTrackBasePtr>
void SerializeFlexTracks(FArchive &Ar, MotionChunk &M)
{
	guard(SerializeFlexTracks);

#if 0
	int numTracks;
	Ar << AR_INDEX(numTracks);
	if (!numTracks) return;

	assert(M.AnimTracks.Num() == 0);
	M.AnimTracks.Empty(numTracks);
	M.AnimTracks.Add(numTracks);

	for (int i = 0; i < numTracks; i++)
	{
		int trackType;
		Ar << trackType;
		FlexTrackBase *track = CreateFlexTrack(trackType);
		if (track)
		{
			track->Serialize(Ar);
			track->Decompress(M.AnimTracks[i]);
			delete track;
		}
		else
			appError("no track!");	//?? possibly, mask tracks
	}
#else
	//!! Test this code
	TArray<FlexTrackBasePtr> FT;
	Ar << FT;
	int numTracks = FT.Num();
	if (!numTracks) return;
	M.AnimTracks.Empty(numTracks);
	M.AnimTracks.Add(numTracks);
	for (int i = 0; i < numTracks; i++)
		FT[i].Track->Decompress(M.AnimTracks[i]);
#endif

	unguard;
}

#endif // UNREAL25

#if TRIBES3

void FixTribesMotionChunk(MotionChunk &M)
{
	int numBones = M.AnimTracks.Num();
	for (int i = 0; i < numBones; i++)
	{
		AnalogTrack &A = M.AnimTracks[i];
		if (A.Flags & 0x1000)
		{
			// bone overrided by Impersonator LipSinc
			// remove translation and rotation tracks (they are not correct anyway)
			A.KeyQuat.Empty();
			A.KeyPos.Empty();
			A.KeyTime.Empty();
		}
	}
}

#endif // TRIBES3


#if UC2

struct MotionChunkUC2 : public MotionChunk
{
	TArray<FlexTrackBasePtr> FlexTracks;

	friend FArchive& operator<<(FArchive &Ar, MotionChunkUC2 &M)
	{
		//?? note: can merge this structure into MotionChunk (will require FlexTrack declarations in h-file)
		guard(MotionChunkUC2<<);
		assert(Ar.ArVer >= 147);
		// start is the same, but arrays are serialized in a different way
		Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags;
		if (Ar.ArVer < 149) Ar << M.BoneIndices;
		Ar << M.AnimTracks << M.RootTrack;
		if (M.Flags >= 3)
			Ar << M.FlexTracks;
		assert(M.AnimTracks.Num() == 0 || M.FlexTracks.Num() == 0); // only one kind of tracks at a time
		assert(M.Flags != 3);				// Version == 3 has TLazyArray<FlexTrack2>

		return Ar;
		unguard;
	}
};


bool UMeshAnimation::SerializeUE2XMoves(FArchive &Ar)
{
	guard(UMeshAnimation::SerializeUE2XMoves);
	if (Ar.ArVer < 147)
	{
		// standard UE2 format
		Ar << Moves;
		return true;
	}
	assert(Ar.IsLoading);

	// read FByteBuffer
	byte *BufferData = NULL;
	int DataSize;
	int DataFlag;
	Ar << DataSize;
	DataFlag = 0;
	if (Ar.ArLicenseeVer == 1)
		Ar << DataFlag;
#if 0
	assert(DataFlag == 0);
#else
	if (DataFlag != 0)
	{
		guard(GetExternalAnim);
		// animation is stored in xpr file
		int Size;
		BufferData = FindXprData(va("%s_anim", Name), &Size);
		if (!BufferData)
		{
			appNotify("Missing external animations for %s", Name);
			return false;
		}
		assert(DataSize <= Size);
		appPrintf("Loading external animation for %s\n", Name);
		unguard;
	}
#endif
	if (!DataFlag && DataSize)
	{
		BufferData = (byte*)appMalloc(DataSize);
		Ar.Serialize(BufferData, DataSize);
	}
	TArray<MotionChunkUC2> Moves2;
	Ar << Moves2;

	FMemReader Reader(BufferData, DataSize);

	// serialize Moves2 and copy Moves2 to Moves
	int numMoves = Moves2.Num();
	Moves.Empty(numMoves);
	Moves.Add(numMoves);
	for (int mi = 0; mi < numMoves; mi++)
	{
		MotionChunkUC2 &M = Moves2[mi];
		MotionChunk &DM = Moves[mi];

		// serialize AnimTracks
		int numATracks = M.AnimTracks.Num();
		if (numATracks)
		{
			DM.AnimTracks.Add(numATracks);
			for (int ti = 0; ti < numATracks; ti++)
			{
				AnalogTrack &A = DM.AnimTracks[ti];
				RAW_ARRAY_UC2(A.KeyQuat).Serialize(Reader, Ar);
				RAW_ARRAY_UC2(A.KeyPos).Serialize(Reader, Ar);
				RAW_ARRAY_UC2(A.KeyTime).Serialize(Reader, Ar);
			}
		}
		// "serialize" RootTrack
		int i1, i2, i3;
		Ar << i1 << i2 << i3;				// KeyQuat, KeyPos and KeyTime
		assert(i1 == 0 && i2 == 0 && i3 == 0);
		// serialize FlexTracks
		int numFTracks = M.FlexTracks.Num();
		if (numFTracks)
		{
			DM.AnimTracks.Add(numFTracks);
			for (int ti = 0; ti < numFTracks; ti++)
			{
				FlexTrackBase *Track = M.FlexTracks[ti].Track;
				if (Track)
				{
					Track->Serialize2(Reader, Ar);
					Track->Decompress(DM.AnimTracks[ti]);
				}
//				else -- keep empty track, will be ignored by animation system
			}
		}
	}
	assert(Reader.GetStopper() == Reader.Tell());

	// cleanup
	if (BufferData) appFree(BufferData);

	return true;

	unguard;
}

#endif // UC2


#if UNREAL1

void UMeshAnimation::Upgrade()
{
	guard(UMeshAnimation.Upgrade);
	for (int i = 0; i < Moves.Num(); i++)
	{
		MotionChunk &M = Moves[i];
		for (int j = 0; j < M.AnimTracks.Num(); j++)
		{
			AnalogTrack &A = M.AnimTracks[j];
			int k;
			// fix time tracks
			A.KeyTime.Empty();
			// mirror position and orientation
			for (k = 0; k < A.KeyPos.Num(); k++)
				A.KeyPos[k].X *= -1;
			for (k = 0; k < A.KeyQuat.Num(); k++)
			{
				FQuat &Q = A.KeyQuat[k];
				Q.X *= -1;
				Q.W *= -1;
			}
		}
	}
	unguard;
}

#endif // UNREAL1


/*-----------------------------------------------------------------------------
	USkeletalMesh class
-----------------------------------------------------------------------------*/

// Implement constructor in cpp to avoid inlining (it's large enough).
// It's useful to declare TArray<> structures as forward declarations in header file.
USkeletalMesh::USkeletalMesh()
{}


USkeletalMesh::~USkeletalMesh()
{
	delete ConvertedMesh;
}


#if SWRC

struct FAttachSocketSWRC
{
	FName		Alias;
	FName		BoneName;
	FMatrix		Matrix;

	friend FArchive& operator<<(FArchive &Ar, FAttachSocketSWRC &S)
	{
		return Ar << S.Alias << S.BoneName << S.Matrix;
	}
};

struct FMeshAnimLinkSWRC
{
	int			Flags;
	UMeshAnimation *Anim;

	friend FArchive& operator<<(FArchive &Ar, FMeshAnimLinkSWRC &S)
	{
		if (Ar.ArVer >= 151)
			Ar << S.Flags;
		else
			S.Flags = 1;
		return Ar << S.Anim;
	}
};

#endif // SWRC


void USkeletalMesh::Serialize(FArchive &Ar)
{
	guard(USkeletalMesh::Serialize);

	assert(Ar.Game < GAME_UE3);

#if UNREAL1
	if (Ar.Engine() == GAME_UE1)
	{
		SerializeSkelMesh1(Ar);
		return;
	}
#endif
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		SerializeBioshockMesh(Ar);
		return;
	}
#endif

	Super::Serialize(Ar);
#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell)
	{
		SerializeSCell(Ar);
		return;
	}
#endif // SPLINTER_CELL
#if TRIBES3
	TRIBES_HDR(Ar, 4);
#endif
	Ar << Points2 << RefSkeleton;
#if SWRC
	if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 142)
	{
		for (int i = 0; i < RefSkeleton.Num(); i++)
		{
			FMeshBone &B = RefSkeleton[i];
			B.BonePos.Orientation.X *= -1;
			B.BonePos.Orientation.Y *= -1;
			B.BonePos.Orientation.Z *= -1;
		}
	}
	if (Ar.Game == GAME_RepCommando && Version >= 5)
	{
		TArray<FMeshAnimLinkSWRC> Anims;
		Ar << Anims;
		if (Anims.Num() >= 1) Animation = Anims[0].Anim;
	}
	else
#endif // SWRC
		Ar << Animation;
	Ar << SkeletalDepth << WeightIndices << BoneInfluences;
#if SWRC
	if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 140)
	{
		TArray<FAttachSocketSWRC> Sockets;
		Ar << Sockets;	//?? convert
	}
	else
#endif // SWRC
	{
		Ar << AttachAliases << AttachBoneNames << AttachCoords;
	}
	if (Version <= 1)
	{
//		appNotify("SkeletalMesh of version %d\n", Version);
		TArray<FLODMeshSection> tmp1, tmp2;
		TArray<word> tmp3;
		Ar << tmp1 << tmp2 << tmp3;
		// copy and convert data from old mesh format
		UpgradeMesh();
	}
	else
	{
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 136)
		{
			int f338;
			Ar << f338;
		}
#endif // UC2
#if SWRC
		if (Ar.Game == GAME_RepCommando)
		{
			int f1C4;
			if (Version >= 6) Ar << f1C4;
			Ar << LODModels;
			if (Version < 5) Ar << f224;
			Ar << Points << Wedges << Triangles << VertInfluences;
			Ar << CollapseWedge << f1C8;
			goto skip_remaining;
		}
#endif // SWRC
		Ar << LODModels << f224 << Points << Wedges << Triangles << VertInfluences;
		Ar << CollapseWedge << f1C8;
	}

#if TRIBES3
	if ((Ar.Game == GAME_Tribes3 || Ar.Game == GAME_Swat4) && t3_hdrSV >= 3)
	{
	#if 0
		// it looks like format of following data was chenged sinse
		// data was prepared, and game executeble does not load these
		// LazyArrays (otherwise error should occur) -- so we are
		// simply skipping these arrays
		TLazyArray<FT3Unk1>    unk1;
		TLazyArray<FMeshWedge> unk2;
		TLazyArray<word>       unk3;
		Ar << unk1 << unk2 << unk3;
	#else
		SkipLazyArray(Ar);
		SkipLazyArray(Ar);
		SkipLazyArray(Ar);
	#endif
		// nothing interesting below ...
		goto skip_remaining;
	}
#endif // TRIBES3
#if BATTLE_TERR
	if (Ar.Game == GAME_BattleTerr) goto skip_remaining;
#endif
#if UC2
	if (Ar.Engine() == GAME_UE2X) goto skip_remaining;
#endif

#if LINEAGE2
	if (Ar.Game == GAME_Lineage2)
	{
		int unk1, unk3, unk4;
		TArray<float> unk2;
		if (Ar.ArVer >= 118 && Ar.ArLicenseeVer >= 3)
			Ar << unk1;
		if (Ar.ArVer >= 123 && Ar.ArLicenseeVer >= 0x12)
			Ar << unk2;
		if (Ar.ArVer >= 120)
			Ar << unk3;		// AuthKey ?
		if (Ar.ArLicenseeVer >= 0x23)
			Ar << unk4;
		RecreateMeshFromLOD();
		return;
	}
#endif // LINEAGE2

	if (Ar.ArVer >= 120)
	{
		Ar << AuthKey;
	}

#if LOCO
	if (Ar.Game == GAME_Loco) goto skip_remaining;	// Loco codepath is similar to UT2004, but sometimes has different version switches
#endif

#if UT2
	if (Ar.Game == GAME_UT2)
	{
		// UT2004 has branched version of UE2, which is slightly different
		// in comparison with generic UE2, which is used in all other UE2 games.
		if (Ar.ArVer >= 122)
			Ar << KarmaProps << BoundingSpheres << BoundingBoxes << f32C;
		if (Ar.ArVer >= 127)
			Ar << CollisionMesh;
		return;
	}
#endif // UT2

	// generic UE2 code
	if (Ar.ArVer >= 124)
		Ar << KarmaProps << BoundingSpheres << BoundingBoxes;
	if (Ar.ArVer >= 125)
		Ar << f32C;

#if XIII
	if (Ar.Game == GAME_XIII) goto skip_remaining;
#endif
#if RAGNAROK2
	if (Ar.Game == GAME_Ragnarok2 && Ar.ArVer >= 131)
	{
		float unk1, unk2;
		Ar << unk1 << unk2;
	}
#endif // RAGNAROK2

	if (Ar.ArLicenseeVer && (Ar.Tell() != Ar.GetStopper()))
	{
		appNotify("Serializing SkeletalMesh'%s' of unknown game: %d unreal bytes", Name, Ar.GetStopper() - Ar.Tell());
	skip_remaining:
		DROP_REMAINING_DATA(Ar);
	}

	unguard;
}


void USkeletalMesh::PostLoad()
{
#if BIOSHOCK
	if (Package->Game == GAME_Bioshock)
		PostLoadBioshockMesh();		// should be called after loading of all used objects
#endif // BIOSHOCK
}


void USkeletalMesh::UpgradeFaces()
{
	guard(UpgradeFaces);
	// convert 'FMeshFace Faces' to 'VTriangle Triangles'
	if (Faces.Num() && !Triangles.Num())
	{
		Triangles.Empty(Faces.Num());
		Triangles.Add(Faces.Num());
		for (int i = 0; i < Faces.Num(); i++)
		{
			const FMeshFace &F = Faces[i];
			VTriangle &T = Triangles[i];
			T.WedgeIndex[0] = F.iWedge[0];
			T.WedgeIndex[1] = F.iWedge[1];
			T.WedgeIndex[2] = F.iWedge[2];
			T.MatIndex      = F.MaterialIndex;
		}
	}
	unguard;
}

void USkeletalMesh::UpgradeMesh()
{
	guard(USkeletalMesh.UpgradeMesh);

	int i;
	CopyArray(Points, Points2);
	CopyArray(Wedges, Super::Wedges);
	UpgradeFaces();
	// convert VBoneInfluence and VWeightIndex to FVertInfluences
	// count total influences
	int numInfluences = 0;
	for (i = 0; i < WeightIndices.Num(); i++)
		numInfluences += WeightIndices[i].BoneInfIndices.Num() * (i + 1);
	VertInfluences.Empty(numInfluences);
	VertInfluences.Add(numInfluences);
	int vIndex = 0;
	for (i = 0; i < WeightIndices.Num(); i++)				// loop by influence count per vertex
	{
		const VWeightIndex &WI = WeightIndices[i];
		int index = WI.StartBoneInf;
		for (int j = 0; j < WI.BoneInfIndices.Num(); j++)	// loop by vertices
		{
			int iVertex = WI.BoneInfIndices[j];
			for (int k = 0; k <= i; k++)					// enumerate all bones per vertex
			{
				const VBoneInfluence &BI = BoneInfluences[index++];
				FVertInfluences &I = VertInfluences[vIndex++];
				I.Weight     = BI.BoneWeight / 65535.0f;
				I.BoneIndex  = BI.BoneIndex;
				I.PointIndex = iVertex;
			}
		}
	}

	unguard;
}


void USkeletalMesh::RecreateMeshFromLOD(int LodIndex, bool Force)
{
	guard(USkeletalMesh::RecreateMeshFromLOD);
	if (Wedges.Num() && !Force) return;					// nothing to do
	if (LODModels.Num() <= LodIndex) return;			// no such LOD mesh
	appPrintf("Restoring mesh from LOD %d ...\n", LodIndex);

	FStaticLODModel &Lod = LODModels[LodIndex];

	CopyArray(Wedges, Lod.Wedges);
	CopyArray(Points, Lod.Points);
	CopyArray(VertInfluences, Lod.VertInfluences);
	CopyArray(Triangles, Lod.Faces);
	VertexCount = Points.Num();

	unguard;
}

#if UNREAL1

struct VBoneInfluence1						// Weight and vertex number
{
	word			PointIndex;
	word			BoneWeight;				// 0..63363 == 0..1

	friend FArchive& operator<<(FArchive &Ar, VBoneInfluence1 &V)
	{
		return Ar << V.PointIndex << V.BoneWeight;
	}
};

SIMPLE_TYPE(VBoneInfluence1, word)

struct VBoneInfIndex
{
	word			WeightIndex;
	word			Number;
	word			DetailA;
	word			DetailB;

	friend FArchive& operator<<(FArchive &Ar, VBoneInfIndex &V)
	{
		return Ar << V.WeightIndex << V.Number << V.DetailA << V.DetailB;
	}
};

SIMPLE_TYPE(VBoneInfIndex, word)


void USkeletalMesh::SerializeSkelMesh1(FArchive &Ar)
{
	guard(SerializeSkelMesh1);
	TArray<FMeshAnimSeq>	tmpAnimSeqs;
	TArray<FBox>			tmpBoundingBoxes;
	TArray<FSphere>			tmpBoundingSpheres;
	int						tmpFrameCount;
	TArray<FMeshWedge>		tmpWedges;
	TArray<FVector>			tmpPoints;
	TArray<VBoneInfIndex>	tmpBoneWeightIdx;
	TArray<VBoneInfluence1>	tmpBoneWeights;
	int						tmpWeaponBoneIndex;
	FCoords					tmpWeaponAdjust;

	// serialize data
	SerializeLodMesh1(Ar, tmpAnimSeqs, tmpBoundingBoxes, tmpBoundingSpheres, tmpFrameCount);
	Ar << tmpWedges << tmpPoints << RefSkeleton << tmpBoneWeightIdx << tmpBoneWeights;
	Ar << Points2;						// LocalPoints
	Ar << SkeletalDepth << Animation << tmpWeaponBoneIndex << tmpWeaponAdjust;

	// convert data
	CopyArray(Wedges, tmpWedges);		// TLazyArray -> TArray
	CopyArray(Points, tmpPoints);		// ...
	CopyArray(Wedges, Super::Wedges);
	UpgradeFaces();
	RotOrigin.Yaw = -RotOrigin.Yaw;

	// convert VBoneInfluence and VWeightIndex to FVertInfluences
	// count total influences
	guard(Influences);
	int numInfluences = tmpBoneWeights.Num();
	VertInfluences.Empty(numInfluences);
	VertInfluences.Add(numInfluences);
	int vIndex = 0;
	assert(tmpBoneWeightIdx.Num() == RefSkeleton.Num());
	for (int bone = 0; bone < tmpBoneWeightIdx.Num(); bone++) // loop by bones
	{
		const VBoneInfIndex &BI = tmpBoneWeightIdx[bone];
		if (!BI.Number) continue;							// no influences for this bone
		for (int j = 0; j < BI.Number; j++)					// loop by vertices
		{
			const VBoneInfluence1 &V = tmpBoneWeights[j + BI.WeightIndex];
			FVertInfluences &I = VertInfluences[vIndex++];
			I.Weight     = V.BoneWeight / 65535.0f;
			I.BoneIndex  = bone;
			I.PointIndex = V.PointIndex;
		}
	}
	unguard;

	// mirror model: points, faces and skeleton
	int i;
	for (i = 0; i < Points.Num(); i++)
		Points[i].X *= -1;
	for (i = 0; i < Triangles.Num(); i++)
		Exchange(Triangles[i].WedgeIndex[0], Triangles[i].WedgeIndex[1]);
	for (i = 0; i < RefSkeleton.Num(); i++)
	{
		FMeshBone &S = RefSkeleton[i];
		S.BonePos.Position.X    *= -1;
		S.BonePos.Orientation.X *= -1;
		S.BonePos.Orientation.W *= -1;
	}

	unguard;
}

#endif // UNREAL1


#if SPLINTER_CELL

struct FSCellUnk1
{
	int				f0, f4, f8, fC;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk1 &S)
	{
		return Ar << S.f0 << S.f4 << S.f8 << S.fC;
	}
};

SIMPLE_TYPE(FSCellUnk1, int)

struct FSCellUnk2
{
	int				f0, f4, f8, fC, f10;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk2 &S)
	{
		return Ar << S.f0 << S.f4 << S.f10 << S.f8 << S.fC;
	}
};

struct FSCellUnk3
{
	int				f0, f4, f8, fC;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk3 &S)
	{
		return Ar << S.f0 << S.fC << S.f4 << S.f8;
	}
};

struct FSCellUnk4a
{
	FVector			f0;
	FVector			fC;
	int				f18;					// float?

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk4a &S)
	{
		return Ar << S.f0 << S.fC << S.f18;
	}
};

SIMPLE_TYPE(FSCellUnk4a, float)

struct FSCellUnk4
{
	int				Size;
	int				Count;
	TArray<FString>	BoneNames;				// BoneNames.Num() == Count
	FString			f14;
	FSCellUnk4a**	Data;					// (FSCellUnk4a*)[Count][Size]

	FSCellUnk4()
	:	Data(NULL)
	{}
	~FSCellUnk4()
	{
		// cleanup data
		if (Data)
		{
			for (int i = 0; i < Count; i++)
				delete Data[i];
			delete Data;
		}
	}

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk4 &S)
	{
		int i, j;

		// serialize scalars
		Ar << S.Size << S.Count << S.BoneNames << S.f14;

		if (Ar.IsLoading)
		{
			// allocate array of pointers
			S.Data = new FSCellUnk4a* [S.Count];
			// allocate arrays
			for (i = 0; i < S.Count; i++)
				S.Data[i] = new FSCellUnk4a [S.Size];
		}
		// serialize arrays
		for (i = 0; i < S.Count; i++)
		{
			FSCellUnk4a* Ptr = S.Data[i];
			for (j = 0; j < S.Size; j++, Ptr++)
				Ar << *Ptr;
		}
		return Ar;
	}
};

void USkeletalMesh::SerializeSCell(FArchive &Ar)
{
	if (Version >= 2) Ar << Version;		// interesting code
	Ar << Points2;
	if (Ar.ArLicenseeVer >= 48)
	{
		TArray<FVector> unk;
		Ar << unk;
	}
	Ar << RefSkeleton;
	Ar << Animation;
	if (Ar.ArLicenseeVer >= 155)
	{
		TArray<UObject*> unk218;
		Ar << unk218;
	}
	Ar << SkeletalDepth << WeightIndices << BoneInfluences;
	Ar << AttachAliases << AttachBoneNames << AttachCoords;
	DROP_REMAINING_DATA(Ar);
	UpgradeMesh();

/*	TArray<FSCellUnk1> tmp1;
	TArray<FSCellUnk2> tmp2;
	TArray<FSCellUnk3> tmp3;
	TArray<FLODMeshSection> tmp4, tmp5;
	TArray<word> tmp6;
	FSCellUnk4 complex;
	Ar << tmp1 << tmp2 << tmp3 << tmp4 << tmp5 << tmp6 << complex; */
}

#endif // SPLINTER_CELL


#if RUNE

#define NUM_POLYGROUPS		16
#define MAX_CHILD_JOINTS	4


struct RVertex
{
	FVector			point1;					// position of vertex relative to joint1
	FVector			point2;					// position of vertex relative to joint2
	int				joint1;
	int				joint2;
	float			weight1;

	friend FArchive& operator<<(FArchive &Ar, RVertex &V)
	{
		return Ar << V.point1 << V.point2 << V.joint1 << V.joint2 << V.weight1;
	}
};

SIMPLE_TYPE(RVertex, int)


struct RTriangle
{
	short			vIndex[3];				// Vertex indices
	FMeshUV1		tex[3];					// Texture UV coordinates
	byte			polygroup;				// polygroup this tri belongs to

	friend FArchive& operator<<(FArchive &Ar, RTriangle &T)
	{
		Ar << T.vIndex[0] << T.vIndex[1] << T.vIndex[2];
		Ar << T.tex[0] << T.tex[1] << T.tex[2];
		Ar << T.polygroup;
		return Ar;
	}
};


struct RMesh
{
	int				numverts;
	int				numtris;

	TArray<RTriangle> tris;
	TArray<RVertex>	verts;
	int				GroupFlags[NUM_POLYGROUPS];
	FName			PolyGroupSkinNames[NUM_POLYGROUPS];

	int				decCount;
	TArray<byte>	decdata;

	friend FArchive& operator<<(FArchive &Ar, RMesh &M)
	{
		Ar << M.numverts << M.numtris;
		Ar << M.tris << M.verts;
		Ar << M.decCount << M.decdata;
		for (int i = 0; i < NUM_POLYGROUPS; i++)
			Ar << M.GroupFlags[i] << M.PolyGroupSkinNames[i];
		return Ar;
	}
};


struct RJoint
{
	// Skeletal structure implimentation
	int				parent;
	int				children[MAX_CHILD_JOINTS];
	FName			name;					// Name of joint
	int				jointgroup;				// Body group this belongs (allows us to animate groups seperately)
	int				flags;					// Default joint flags
	FRotator		baserot;				// Rotational delta to base orientation
	FPlane			planes[6];				// Local joint coordinate planes defining a collision box

	// Serializer.
	friend FArchive& operator<<(FArchive &Ar, RJoint &J)
	{
		int i;
		Ar << J.parent;
		for (i = 0; i < MAX_CHILD_JOINTS; i++)
			Ar << J.children[i];
		Ar << J.name << J.jointgroup << J.flags;
		Ar << J.baserot;
		for (i = 0; i < ARRAY_COUNT(J.planes); i++)
			Ar << J.planes[i];
		return Ar;
	}
};


struct FRSkelAnimSeq : public FMeshAnimSeq
{
	TArray<byte>	animdata;				// contains compressed animation data

	friend FArchive& operator<<(FArchive &Ar, FRSkelAnimSeq &S)
	{
		return Ar << (FMeshAnimSeq&)S << S.animdata;
	}
};


struct RJointState
{
	FVector			pos;
	FRotator		rot;
	FScale			scale;					//?? check this - possibly, require to extend animation with bone scales

	friend FArchive& operator<<(FArchive &Ar, RJointState &J)
	{
		return Ar << J.pos << J.rot << J.scale;
	}
};

SIMPLE_TYPE(RJointState, float)


struct RAnimFrame
{
	short			sequenceID;				// Sequence this frame belongs to
	FName			event;					// Event to call during this frame
	FBox			bounds;					// Bounding box of frame
	TArray<RJointState> jointanim;			// O(numjoints) only used for preprocess (transient)

	friend FArchive& operator<<(FArchive &Ar, RAnimFrame &F)
	{
		return Ar << F.sequenceID << F.event << F.bounds << F.jointanim;
	}
};


static FQuat EulerToQuat(const FRotator &Rot)
{
	// signs was taken from experiments
	float yaw   =  Rot.Yaw   / 32768.0f * M_PI;
	float pitch = -Rot.Pitch / 32768.0f * M_PI;
	float roll  = -Rot.Roll  / 32768.0f * M_PI;
	// derived from Doom3 idAngles::ToQuat()
	float sz = sin(yaw / 2);
	float cz = cos(yaw / 2);
	float sy = sin(pitch / 2);
	float cy = cos(pitch / 2);
	float sx = sin(roll / 2);
	float cx = cos(roll / 2);
	float sxcy = sx * cy;
	float cxcy = cx * cy;
	float sxsy = sx * sy;
	float cxsy = cx * sy;
	FQuat Q;
	Q.Set(cxsy*sz - sxcy*cz, -cxsy*cz - sxcy*sz, sxsy*cz - cxcy*sz, cxcy*cz + sxsy*sz);
	return Q;
}


static void ConvertRuneAnimations(UMeshAnimation &Anim, const TArray<RJoint> &Bones,
	const TArray<FRSkelAnimSeq> &Seqs)
{
	guard(ConvertRuneAnimations);

	int i, j, k;
	int numBones = Bones.Num();
	// create RefBones
	Anim.RefBones.Empty(Bones.Num());
	for (i = 0; i < Bones.Num(); i++)
	{
		const RJoint &SB = Bones[i];
		FNamedBone *B = new(Anim.RefBones) FNamedBone;
		B->Name        = SB.name;
		B->Flags       = 0;
		B->ParentIndex = SB.parent;
	}
	// create AnimSeqs
	Anim.AnimSeqs.Empty(Seqs.Num());
	Anim.Moves.Empty(Seqs.Num());
	for (i = 0; i < Seqs.Num(); i++)
	{
		// create FMeshAnimSeq
		const FRSkelAnimSeq &SS = Seqs[i];
		FMeshAnimSeq *S = new(Anim.AnimSeqs) FMeshAnimSeq;
		S->Name       = SS.Name;
		CopyArray(S->Groups, SS.Groups);
		S->StartFrame = 0;
		S->NumFrames  = SS.NumFrames;
		S->Rate       = SS.Rate;
		//?? S->Notifys
		// create MotionChunk
		MotionChunk *M = new(Anim.Moves) MotionChunk;
		M->TrackTime  = SS.NumFrames;
		// dummy bone remap
		M->AnimTracks.Empty(numBones);
		// convert animation data
		const byte *data = &SS.animdata[0];
		for (j = 0; j < numBones; j++)
		{
			// prepare AnalogTrack
			AnalogTrack *A = new(M->AnimTracks) AnalogTrack;
			A->KeyQuat.Empty(SS.NumFrames);
			A->KeyPos.Empty(SS.NumFrames);
			A->KeyTime.Empty(SS.NumFrames);
		}
		for (int frame = 0; frame < SS.NumFrames; frame++)
		{
			for (int joint = 0; joint < numBones; joint++)
			{
				AnalogTrack &A = M->AnimTracks[joint];

				FVector pos, scale;
				pos.Set(0, 0, 0);
				scale.Set(1, 1, 1);
				FRotator rot;
				rot.Set(0, 0, 0);

				byte f = *data++;
				short d;
#define GET		d = data[0] + (data[1] << 8); data += 2;
#define GETF(v)	{ GET; v = (float)d / 256.0f; }
#define GETI(v)	{ GET; v = d; }
				// decode position
				if (f & 1)    GETF(pos.X);
				if (f & 2)    GETF(pos.Y);
				if (f & 4)    GETF(pos.Z);
				// decode scale
				if (f & 8)  { GETF(scale.X); GETF(scale.Z); }
				if (f & 0x10) GETF(scale.Y);
				// decode rotation
				if (f & 0x20) GETI(rot.Pitch);
				if (f & 0x40) GETI(rot.Yaw);
				if (f & 0x80) GETI(rot.Roll);
#undef GET
#undef GETF
#undef GETI
				A.KeyQuat.AddItem(EulerToQuat(rot));
				A.KeyPos.AddItem(pos);
				//?? notify about scale!=(1,1,1)
			}
		}
		assert(data == &SS.animdata[0] + SS.animdata.Num());
	}

	unguard;
}


// function is similar to part of CSkelMeshInstance::SetMesh()
static void BuildSkeleton(TArray<CCoords> &Coords, const TArray<RJoint> &Bones, const TArray<AnalogTrack> &Anim)
{
	guard(BuildSkeleton);

	int numBones = Anim.Num();
	Coords.Empty(numBones);
	Coords.Add(numBones);

	for (int i = 0; i < numBones; i++)
	{
		const AnalogTrack &A = Anim[i];
		const RJoint &B = Bones[i];
		CCoords &BC = Coords[i];
		// compute reference bone coords
		CVec3 BP;
		CQuat BO;
		// get default pose
		BP = CVT(A.KeyPos[0]);
		BO = CVT(A.KeyQuat[0]);
		if (!i) BO.Conjugate();

		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
		{
			assert(B.parent >= 0);
			Coords[B.parent].UnTransformCoords(BC, BC);
		}
	}

	unguard;
}


void USkelModel::Serialize(FArchive &Ar)
{
	guard(USkelModel::Serialize);

	assert(Ar.IsLoading);					// no saving ...
	Super::Serialize(Ar);

	// USkelModel data
	int nummeshes;
	int numjoints;
	int numframes;
	int numsequences;
	int numskins;
	int rootjoint;

	FVector					PosOffset;		// Offset of creature relative to base
	FRotator				RotOffset;		// Offset of creatures rotation

	TArray<RMesh>			meshes;
	TArray<RJoint>			joints;
	TArray<FRSkelAnimSeq>	AnimSeqs;		// Compressed animation data for sequence
	TArray<RAnimFrame>		frames;

	Ar << nummeshes << numjoints << numframes << numsequences << numskins << rootjoint;
	Ar << meshes << joints << AnimSeqs << frames << PosOffset << RotOffset;

	int modelIdx;
	// create all meshes first, then fill them (for better view order)
	for (modelIdx = 0; modelIdx < meshes.Num(); modelIdx++)
	{
		// create new USkeletalMesh
		USkeletalMesh *sm = static_cast<USkeletalMesh*>(CreateClass("SkeletalMesh"));	//?? new USkeletalMesh ?
		char nameBuf[256];
		appSprintf(ARRAY_ARG(nameBuf), "%s_%d", Name, modelIdx);
		char *name = strdup(nameBuf);
		Meshes.AddItem(sm);
		MeshNames.AddItem(name);
		// setup UOnject
		sm->Name         = name;
		sm->Package      = Package;
		sm->PackageIndex = INDEX_NONE;		// not really exported
	}
	// create animation
	Anim = static_cast<UMeshAnimation*>(CreateClass("MeshAnimation"));	//?? new UMeshAnimation ?
	Anim->Name         = Name;
	Anim->Package      = Package;
	Anim->PackageIndex = INDEX_NONE;		// not really exported
	ConvertRuneAnimations(*Anim, joints, AnimSeqs);
	Anim->ConvertAnims();					//?? second conversion
	// get baseframe
	assert(strcmp(Anim->AnimSeqs[0].Name, "baseframe") == 0);
	const TArray<AnalogTrack> &BaseAnim = Anim->Moves[0].AnimTracks;
	// compute bone coordinates
	TArray<CCoords> BoneCoords;
	BuildSkeleton(BoneCoords, joints, BaseAnim);

	// setup meshes
	for (modelIdx = 0; modelIdx < meshes.Num(); modelIdx++)
	{
		int i, j;
		const RMesh  &src = meshes[modelIdx];
		USkeletalMesh *sm = Meshes[modelIdx];
		sm->Animation = Anim;
		// setup ULodMesh
		sm->RotOrigin.Set(0, 16384, -16384);
		sm->MeshScale.Set(1, 1, 1);
		sm->MeshOrigin.Set(0, 0, 0);
		// copy skeleton
		sm->RefSkeleton.Empty(joints.Num());
		for (i = 0; i < joints.Num(); i++)
		{
			const RJoint &J = joints[i];
			FMeshBone *B = new(sm->RefSkeleton) FMeshBone;
			B->Name = J.name;
			B->Flags = 0;
			B->ParentIndex = (J.parent > 0) ? J.parent : 0;		// -1 -> 0
			// copy bone orientations from base animation frame
			B->BonePos.Orientation = BaseAnim[i].KeyQuat[0];
			B->BonePos.Position    = BaseAnim[i].KeyPos[0];
		}
		// copy vertices
		int VertexCount = sm->VertexCount = src.verts.Num();
		sm->Points.Empty(VertexCount);
		for (i = 0; i < VertexCount; i++)
		{
			const RVertex &v1 = src.verts[i];
			FVector *V = new(sm->Points) FVector;
			// transform point from local bone space to model space
			BoneCoords[v1.joint1].UnTransformPoint(CVT(v1.point1), CVT(*V));
		}
		// copy triangles and create wedges
		// here we create 3 wedges for each triangle.
		// it is possible to reduce number of wedges by finding duplicates, but we don't
		// need it here ...
		int TrisCount = src.tris.Num();
		sm->Triangles.Empty(TrisCount);
		sm->Wedges.Empty(TrisCount * 3);
		int numMaterials = 0;		// should detect real material count
		for (i = 0; i < TrisCount; i++)
		{
			const RTriangle &tri = src.tris[i];
			// create triangle
			VTriangle *T = new(sm->Triangles) VTriangle;
			T->MatIndex = tri.polygroup;
			if (numMaterials <= tri.polygroup)
				numMaterials = tri.polygroup+1;
			// create wedges
			for (j = 0; j < 3; j++)
			{
				T->WedgeIndex[j] = sm->Wedges.Num();
				FMeshWedge *W = new(sm->Wedges) FMeshWedge;
				W->iVertex = tri.vIndex[j];
				W->TexUV   = tri.tex[j];
			}
			// reverse order of triangle vertices
			Exchange(T->WedgeIndex[0], T->WedgeIndex[1]);
		}
		// build influences
		for (i = 0; i < VertexCount; i++)
		{
			const RVertex &v1 = src.verts[i];
			FVertInfluences *Inf = new(sm->VertInfluences) FVertInfluences;
			Inf->PointIndex = i;
			Inf->BoneIndex  = v1.joint1;
			Inf->Weight     = v1.weight1;
			if (Inf->Weight != 1.0f)
			{
				// influence for 2nd bone
				Inf = new(sm->VertInfluences) FVertInfluences;
				Inf->PointIndex = i;
				Inf->BoneIndex  = v1.joint2;
				Inf->Weight     = 1.0f - v1.weight1;
			}
		}
		// create materials
		for (i = 0; i < numMaterials; i++)
		{
			const char *texName = src.PolyGroupSkinNames[i];
			FMeshMaterial *M1 = new(sm->Materials) FMeshMaterial;
			M1->PolyFlags    = src.GroupFlags[i];
			M1->TextureIndex = sm->Textures.Num();
			if (strcmp(texName, "None") == 0)
			{
				// texture should be set from script
				sm->Textures.AddItem(NULL);
				continue;
			}
			// find texture in object's package
			int texExportIdx = Package->FindExport(texName);
			if (texExportIdx == INDEX_NONE)
			{
				appPrintf("ERROR: unable to find export \"%s\" for mesh \"%s\" (%d)\n",
					texName, Name, modelIdx);
				continue;
			}
			// load and remember texture
			UMaterial *Tex = static_cast<UMaterial*>(Package->CreateExport(texExportIdx));
			sm->Textures.AddItem(Tex);
		}
		// setup UPrimitive properties using 1st animation frame
		// note: this->BoundingBox and this->BoundingSphere are null
		const RAnimFrame &F = frames[0];
		assert(strcmp(AnimSeqs[0].Name, "baseframe") == 0 && AnimSeqs[0].StartFrame == 0);
		CVec3 mins, maxs;
		sm->BoundingBox = F.bounds;
		mins = CVT(F.bounds.Min);
		maxs = CVT(F.bounds.Max);
		CVec3 &center = CVT(sm->BoundingSphere);
		for (i = 0; i < 3; i++)
			center[i] = (mins[i] + maxs[i]) / 2;
		sm->BoundingSphere.R = VectorDistance(center, mins);
	}

	unguard;
}


USkelModel::~USkelModel()
{
	// free holded data
	int i;
	for (i = 0; i < Meshes.Num(); i++)
		delete Meshes[i];
	delete Anim;
	for (i = 0; i < MeshNames.Num(); i++)
		free(MeshNames[i]);				// allocated with strdup()
}


#endif // RUNE


#if LINEAGE2

void FStaticLODModel::RestoreLineageMesh()
{
	guard(FStaticLODModel::RestoreLineageMesh);

	if (Wedges.Num()) return;			// nothing to restore
	appPrintf("Converting Lineage2 LODModel to standard LODModel ...\n");
	if (SmoothSections.Num() && RigidSections.Num())
		appNotify("have smooth & rigid sections");

	int i, j, k;
	int NumWedges = LineageWedges.Num() + VertexStream.Verts.Num(); //??
	if (!NumWedges)
	{
		appNotify("Cannot restore mesh: no wedges");
		return;
	}
	assert(LineageWedges.Num() == 0 || VertexStream.Verts.Num() == 0);

	Wedges.Empty(NumWedges);
	Points.Empty(NumWedges);			// really, should be a smaller count
	VertInfluences.Empty(NumWedges);	// min count = NumVerts
	Faces.Empty((SmoothIndices.Indices.Num() + RigidIndices.Indices.Num()) / 3);
	TArray<FVector> PointNormals;
	PointNormals.Empty(NumWedges);

	// remap bones and build faces
	TArray<const FSkelMeshSection*> WedgeSection;
	WedgeSection.Empty(NumWedges);
	for (i = 0; i < NumWedges; i++)
		WedgeSection.AddItem(NULL);
	// smooth sections
	guard(SmoothWedges);
	for (k = 0; k < SmoothSections.Num(); k++)
	{
		const FSkelMeshSection &ms = SmoothSections[k];
		for (i = 0; i < ms.NumFaces; i++)
		{
			FMeshFace *F = new (Faces) FMeshFace;
			F->MaterialIndex = ms.MaterialIndex;
			for (j = 0; j < 3; j++)
			{
				int WedgeIndex = SmoothIndices.Indices[(ms.FirstFace + i) * 3 + j];
				assert(WedgeSection[WedgeIndex] == NULL || WedgeSection[WedgeIndex] == &ms);
				WedgeSection[WedgeIndex] = &ms;
				F->iWedge[j] = WedgeIndex;
			}
		}
	}
	unguard;
	guard(RigidWedges);
	// and the same code for rigid sections
	for (k = 0; k < RigidSections.Num(); k++)
	{
		const FSkelMeshSection &ms = RigidSections[k];
		for (i = 0; i < ms.NumFaces; i++)
		{
			FMeshFace *F = new (Faces) FMeshFace;
			F->MaterialIndex = ms.MaterialIndex;
			for (j = 0; j < 3; j++)
			{
				int WedgeIndex = RigidIndices.Indices[(ms.FirstFace + i) * 3 + j];
				assert(WedgeSection[WedgeIndex] == NULL || WedgeSection[WedgeIndex] == &ms);
				WedgeSection[WedgeIndex] = &ms;
				F->iWedge[j] = WedgeIndex;
			}
		}
	}
	unguard;

	// process wedges

	// convert LineageWedges (smooth sections)
	guard(BuildSmoothWedges);
	for (i = 0; i < LineageWedges.Num(); i++)
	{
		const FLineageWedge &LW = LineageWedges[i];
		FVector VPos = LW.Point;
		// find the same point in previous items
		int PointIndex = -1;
		while (true)
		{
			PointIndex = Points.FindItem(VPos, PointIndex + 1);
			if (PointIndex == INDEX_NONE) break;
			if (PointNormals[PointIndex] == LW.Normal) break;
		}
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.Add();
			Points[PointIndex] = LW.Point;
			PointNormals.AddItem(LW.Normal);
			// build influences
			const FSkelMeshSection *ms = WedgeSection[i];
			assert(ms);
			for (j = 0; j < 4; j++)
			{
				if (LW.Bones[j] == 255) continue;	// no bone assigned
				float Weight = LW.Weights[j];
				if (Weight < 0.000001f) continue;	// zero weight
				FVertInfluences *Inf = new (VertInfluences) FVertInfluences;
				Inf->Weight     = Weight;
				Inf->BoneIndex  = ms->LineageBoneMap[LW.Bones[j]];
				Inf->PointIndex = PointIndex;
			}
		}
		// create wedge
		FMeshWedge *W = new (Wedges) FMeshWedge;
		W->iVertex = PointIndex;
		W->TexUV   = LW.Tex;
	}
	unguard;
	// similar code for VertexStream (rigid sections)
	guard(BuildRigidWedges);
	for (i = 0; i < VertexStream.Verts.Num(); i++)
	{
		const FAnimMeshVertex &LW = VertexStream.Verts[i];
		FVector VPos = LW.Pos;
		// find the same point in previous items
		int PointIndex = -1;
		while (true)
		{
			PointIndex = Points.FindItem(VPos, PointIndex + 1);
			if (PointIndex == INDEX_NONE) break;
			if (LW.Norm == PointNormals[PointIndex]) break;
		}
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.Add();
			Points[PointIndex] = LW.Pos;
			PointNormals.AddItem(LW.Norm);
			// build influences
			const FSkelMeshSection *ms = WedgeSection[i];
			assert(ms);
			FVertInfluences *Inf = new (VertInfluences) FVertInfluences;
			Inf->Weight     = 1.0f;
			Inf->BoneIndex  = /*VertexStream.Revision; //??*/ ms->BoneIndex; //-- equals 0 in Lineage2 ...
			Inf->PointIndex = PointIndex;
		}
		// create wedge
		FMeshWedge *W = new (Wedges) FMeshWedge;
		W->iVertex = PointIndex;
		W->TexUV   = LW.Tex;
	}
	unguard;

	unguard;
}


#endif // LINEAGE2


/*-----------------------------------------------------------------------------
	UStaticMesh class
-----------------------------------------------------------------------------*/

UStaticMesh::~UStaticMesh()
{
	delete ConvertedMesh;
}

#if UC2

void UStaticMesh::LoadExternalUC2Data()
{
	guard(UStaticMesh::LoadExternalUC2Data);

	int i, Size;
	void *Data;

	//?? S.NumFaces is used as NumIndices, but it is not a multiply of 3
	//?? (sometimes is is N*3, sometimes = N*3+1, sometimes - N*3+2 ...)
	//?? May be UC2 uses triangle strips instead of trangles?
	assert(IndexStream1.Indices.Num() == 0);	//???
/*	int NumIndices = 0;
	for (i = 0; i < Sections.Num(); i++)
	{
		FStaticMeshSection &S = Sections[i];
		int idx = S.FirstIndex + S.NumFaces;
	} */
	/*
		FindXprData will return block in following format:
			dword	unk
			dword	itemSize (6 for index stream = 3 verts * sizeof(short))
					(or unknown meanung in a case of trangle strips)
			dword	unk
			data[]
			dword*5	unk (may be include padding?)
	*/

	for (i = 0; i < Sections.Num(); i++)
	{
		FStaticMeshSection &S = Sections[i];
		Data = FindXprData(va("%s_%d_pb", Name, i), &Size);
		if (!Data)
		{
			appNotify("Missing external index stream for mesh %s", Name);
			return;
		}
		//!! use
//		appPrintf("...[%d] f4=%d FirstIndex=%d FirstVertex=%d LastVertex=%d fE=%d NumFaces=%d\n", i, S.f4, S.FirstIndex, S.FirstVertex, S.LastVertex, S.fE, S.NumFaces);
		appFree(Data);
	}

	if (!VertexStream.Vert.Num())
	{
		Data = FindXprData(va("%s_VS", Name), &Size);
		if (!Data)
		{
			appNotify("Missing external vertex stream for mesh %s", Name);
			return;
		}
		//!! use
		appFree(Data);
	}

	// other streams:
	//	ColorStream1 = CS
	//	ColorStream2 = AS

	for (i = 0; i < UVStream.Num(); i++)
	{
		if (UVStream[i].Data.Num()) continue;
		Data = FindXprData(va("%s_UV%d", Name, i), &Size);
		if (!Data)
		{
			appNotify("Missing external UV stream for mesh %s", Name);
			return;
		}
		//!! use
		appFree(Data);
	}

	unguard;
}

#endif // UC2


void UStaticMesh::ConvertMesh2()
{
	guard(UStaticMesh::ConvertMesh2);

	CStaticMesh *Mesh = new CStaticMesh(this);
	ConvertedMesh = Mesh;
	Mesh->BoundingBox    = BoundingBox;
	Mesh->BoundingSphere = BoundingSphere;

	CStaticMeshLod *Lod = new (Mesh->Lods) CStaticMeshLod;
	Lod->HasTangents = false;

	// convert sections
	Lod->Sections.Add(Sections.Num());
	for (int i = 0; i < Sections.Num(); i++)
	{
		CStaticMeshSection &Dst = Lod->Sections[i];
		const FStaticMeshSection &Src = Sections[i];
		Dst.Material   = Materials[i].Material;
		Dst.FirstIndex = Src.FirstIndex;
		Dst.NumFaces   = Src.NumFaces;
	}

	// convert vertices
	int NumVerts = VertexStream.Vert.Num();
	int NumTexCoords = UVStream.Num();
	if (NumTexCoords > NUM_MESH_UV_SETS)
	{
		appNotify("StaticMesh has %d UV sets", NumTexCoords);
		NumTexCoords = NUM_MESH_UV_SETS;
	}
	Lod->NumTexCoords = NumTexCoords;

	Lod->AllocateVerts(NumVerts);
	for (int i = 0; i < NumVerts; i++)
	{
		CStaticMeshVertex &V = Lod->Verts[i];
		const FStaticMeshVertex &SV = VertexStream.Vert[i];
		V.Position = CVT(SV.Pos);
		V.Normal   = CVT(SV.Normal);
		for (int j = 0; j < NumTexCoords; j++)
		{
			const FMeshUVFloat &SUV = UVStream[j].Data[i];
			V.UV[j].U      = SUV.U;
			V.UV[j].V      = SUV.V;
		}
	}

	// copy indices
	CopyArray(Lod->Indices.Indices16, IndexStream1.Indices);

	unguard;
}
