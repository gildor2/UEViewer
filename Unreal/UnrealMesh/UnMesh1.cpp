#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMesh2.h"
#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMaterial/UnMaterial2.h" // for UMaterial* serialization


#if UNREAL1

/*-----------------------------------------------------------------------------
	ULodMesh class
-----------------------------------------------------------------------------*/

#if DEUS_EX

struct FMeshVertDeus
{
	union
	{
		struct
		{
			int X:16; int Y:16; int Z:16; int Pad:16;
		};
		struct
		{
			uint32 D1; uint32 D2;
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


// UE1 FMeshWedge
struct FMeshWedge1
{
	uint16			iVertex;
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
	TArray<uint16>					tmpCollapsePointThus;
	TArray<FMeshWedge1>				tmpWedges;
	TArray<FMeshFace>				tmpSpecialFaces;
	int								tmpModelVerts, tmpSpecialVerts;
	TArray<uint16>					tmpRemapAnimVerts;
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
			NewVerts.AddZeroed(FrameCount * VertexCount);
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


/*-----------------------------------------------------------------------------
	UVertMesh class
-----------------------------------------------------------------------------*/

void UVertMesh::SerializeVertMesh1(FArchive &Ar)
{
	guard(SerializeVertMesh1);

	SerializeLodMesh1(Ar, AnimSeqs, BoundingBoxes, BoundingSpheres, FrameCount);
	VertexCount = Super::VertexCount;
	BuildNormals();

	unguard;
}


/*-----------------------------------------------------------------------------
	USkeletalMesh class
-----------------------------------------------------------------------------*/

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


/*-----------------------------------------------------------------------------
	USkeletalMesh class
-----------------------------------------------------------------------------*/

struct VBoneInfluence1						// Weight and vertex number
{
	uint16			PointIndex;
	uint16			BoneWeight;				// 0..63363 == 0..1

	friend FArchive& operator<<(FArchive &Ar, VBoneInfluence1 &V)
	{
		return Ar << V.PointIndex << V.BoneWeight;
	}
};

SIMPLE_TYPE(VBoneInfluence1, uint16)

struct VBoneInfIndex
{
	uint16			WeightIndex;
	uint16			Number;
	uint16			DetailA;
	uint16			DetailB;

	friend FArchive& operator<<(FArchive &Ar, VBoneInfIndex &V)
	{
		return Ar << V.WeightIndex << V.Number << V.DetailA << V.DetailB;
	}
};

SIMPLE_TYPE(VBoneInfIndex, uint16)


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

	// convert VBoneInfluence and VWeightIndex to FVertInfluence
	// count total influences
	guard(Influences);
	int numInfluences = tmpBoneWeights.Num();
	VertInfluences.Empty(numInfluences);
	VertInfluences.AddZeroed(numInfluences);
	int vIndex = 0;
	assert(tmpBoneWeightIdx.Num() == RefSkeleton.Num());
	for (int bone = 0; bone < tmpBoneWeightIdx.Num(); bone++) // loop by bones
	{
		const VBoneInfIndex &BI = tmpBoneWeightIdx[bone];
		if (!BI.Number) continue;							// no influences for this bone
		for (int j = 0; j < BI.Number; j++)					// loop by vertices
		{
			const VBoneInfluence1 &V = tmpBoneWeights[j + BI.WeightIndex];
			FVertInfluence &I = VertInfluences[vIndex++];
			I.Weight     = V.BoneWeight / 65535.0f;
			I.BoneIndex  = bone;
			I.PointIndex = V.PointIndex;
		}
	}
	unguard;

#if HP2
		if (Ar.Game == GAME_HarryPotter2)
		{
			int i;
			for (i = 0; i < Points.Num(); i++)
				Points[i].Y *= -1;
			for (i = 0; i < Triangles.Num(); i++)
				Exchange(Triangles[i].WedgeIndex[0], Triangles[i].WedgeIndex[1]);
			for (i = 0; i < RefSkeleton.Num(); i++)
			{
				FMeshBone &S = RefSkeleton[i];
				S.BonePos.Position.Y    *= -1;
				S.BonePos.Orientation.Y *= -1;
				S.BonePos.Orientation.W *= -1;
			}

			ConvertMesh();
			return;
		}
#endif // HP2

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

	ConvertMesh();

	unguard;
}

#if HP2

struct FAnimVec
{
	int16 X = 0, Y = 0, Z = 0;

	// Convert back to Vector, with additional scale.
	FVector Vector( float Scale ) const
	{
		FVector OutVector;
		Scale /= BASE_SCALE;
		OutVector.Set(X * Scale, Y * Scale, Z * Scale);
		return OutVector;
	}

	enum { BASE_SCALE = (1<<15)-1 };

	FQuat Quat() const
	{
		static float Scale = 1.57079633 / BASE_SCALE;

		const float _X = sin(X * Scale);
		const float _Y = sin(Y * Scale);
		const float _Z = sin(Z * Scale);

		const float W = sqrt(max(0.0f, 1.f - _X * _X - _Y * _Y - _Z * _Z));

		FQuat OutQuat;
		OutQuat.Set(_X, _Y, _Z, W);
		return OutQuat;
	}

	friend FArchive& operator<<(FArchive &Ar, FAnimVec &A)
	{
		return Ar << A.X << A.Y << A.Z;
	}
};

SIMPLE_TYPE(FAnimVec,  int16)

struct AnalogTrack_HP2
{
	unsigned		Flags = 0;					// reserved
	TStaticArray<FAnimVec, 1>	KeyQuat;				// Orientation key track (count = 1 or KeyTime.Count)
	TStaticArray<FAnimVec, 1>	KeyPos;					// Position key track (count = 1 or KeyTime.Count)
	TStaticArray<byte, 1>	KeyDelta;				// For each key, time when next key takes effect (measured from start of track)
	float PosScale = 0.0f; // Scale for position track.
	float TimeScale = 0.0f; // Scale for time track.

	inline FVector GetKeyPos(int i) const
	{
		return KeyPos[i].Vector( PosScale );
	}

	friend FArchive& operator<<(FArchive &Ar, AnalogTrack_HP2 &A)
	{
		guard(AnalogTrack_HP2<<);
		return Ar << A.Flags << A.KeyQuat << A.KeyPos << A.KeyDelta << A.PosScale << A.TimeScale;
		unguard;
	}
};

struct MotionChunk_HP2
{
	FVector					RootSpeed3D;	// Net 3d speed.
	float					TrackTime;		// Total time (Same for each track.)
	int						StartBone;		// If we're a partial-hierarchy-movement, this is the lowest bone.
	unsigned				Flags;			// Reserved; equals to UMeshAnimation.Version in UE2.5

	TArray<int>				BoneIndices;	// Refbones number of Bone indices (-1 or valid one) to fast-find tracks for a particular bone.
	// Frame-less, compressed animation tracks. NumBones times NumAnims tracks in total
	TArray<AnalogTrack_HP2>		AnimTracks;		// Compressed key tracks (one for each bone)
	// AnalogTrack				RootTrack;		// May or may not be used; actual traverse-a-scene root tracks for use
	// with cutscenes / special physics modes, in addition to the regular skeletal root track.

	friend FArchive& operator<<(FArchive &Ar, MotionChunk_HP2 &M)
	{
		guard(MotionChunk<<);
		return Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags << M.BoneIndices << M.AnimTracks;
		unguard;
	}
};

struct FMasterTrack
{
	TArray<FAnimVec> KeyQuat;
	TArray<FAnimVec> KeyPos;
	TArray<byte> KeyDelta;

	friend FArchive& operator<<(FArchive &Ar, FMasterTrack &M)
	{
		guard(FMasterTrack<<);
		return Ar << M.KeyQuat << M.KeyPos << M.KeyDelta;
		unguard;
	}
};

#include "Mesh/SkeletalMesh.h"

void UMeshAnimation::SerializeHP2Moves(FArchive &Ar)
{
	guard(SerializeHP2Moves);

	TArray<MotionChunk_HP2> HPMoves;
	FMasterTrack MasterTrack;
	Ar << HPMoves << AnimSeqs << MasterTrack;

	int Q = 0, P = 0, T = 0;

	for (MotionChunk_HP2& Move : HPMoves)
	{
		for (AnalogTrack_HP2& Track : Move.AnimTracks)
		{
			const int CurrentKeyQuatNum = Track.KeyQuat.Num();
			const int CurrentKeyPosNum =  Track.KeyPos.Num();
			const int CurrentKeyDeltaNum = Track.KeyDelta.Num();

			for (int QuatIndex = 0; QuatIndex < CurrentKeyQuatNum; ++QuatIndex)
			{
				const int TargetIndex = (Q + QuatIndex);
				Track.KeyQuat[QuatIndex] = MasterTrack.KeyQuat[TargetIndex];
			}
			for (int PosIndex = 0; PosIndex < CurrentKeyPosNum; ++PosIndex)
			{
				const int TargetIndex = (P + PosIndex);
				Track.KeyPos[PosIndex] = MasterTrack.KeyPos[TargetIndex];
			}
			for (int DeltaIndex = 0; DeltaIndex < CurrentKeyDeltaNum; ++DeltaIndex)
			{
				const int TargetIndex = (T + DeltaIndex);
				Track.KeyDelta[DeltaIndex] = MasterTrack.KeyDelta[TargetIndex];
			}

			Q += CurrentKeyQuatNum;
			P += CurrentKeyPosNum;
			T += CurrentKeyDeltaNum;
		}
	}

	Moves.SetNum(HPMoves.Num());

	for (int MoveIndex = 0; MoveIndex < HPMoves.Num(); MoveIndex++)
	{
		FMeshAnimSeq& AnimSeq = AnimSeqs[MoveIndex];

		MotionChunk_HP2& SourceMove = HPMoves[MoveIndex];
		MotionChunk& TargetMove = Moves[MoveIndex];

		TargetMove.RootSpeed3D = SourceMove.RootSpeed3D;
		TargetMove.TrackTime = SourceMove.TrackTime;
		TargetMove.StartBone = SourceMove.StartBone;
		TargetMove.Flags = SourceMove.Flags;

		CopyArray(TargetMove.BoneIndices, SourceMove.BoneIndices);

		// For every bone
		TargetMove.AnimTracks.SetNum(SourceMove.AnimTracks.Num());

		for (int BoneIndex = 0; BoneIndex < TargetMove.AnimTracks.Num(); BoneIndex++)
		{
			AnalogTrack& TargetAnalogTrack = TargetMove.AnimTracks[BoneIndex];
			AnalogTrack_HP2& SourceAnalogTrack = SourceMove.AnimTracks[BoneIndex];

			TargetAnalogTrack.Flags = SourceAnalogTrack.Flags;
			TargetAnalogTrack.KeyQuat.SetNum(SourceAnalogTrack.KeyQuat.Num());
			TargetAnalogTrack.KeyPos.SetNum(SourceAnalogTrack.KeyPos.Num());
			TargetAnalogTrack.KeyTime.SetNum(SourceAnalogTrack.KeyDelta.Num());

			for (int KeyQuatIndex = 0; KeyQuatIndex < TargetAnalogTrack.KeyQuat.Num(); KeyQuatIndex++)
			{
				FQuat TargetQuat = SourceAnalogTrack.KeyQuat[KeyQuatIndex].Quat();
				TargetQuat.Y *= -1.0f;

				// "Due to historical idiosyncrasy, root orientation is negated. !" From UE1
				if (BoneIndex != 0)
				{
					TargetQuat.W *= -1.0f;
				}

				TargetAnalogTrack.KeyQuat[KeyQuatIndex] = TargetQuat;
			}
			for (int KeyPosIndex = 0; KeyPosIndex < TargetAnalogTrack.KeyPos.Num(); KeyPosIndex++)
			{
				TargetAnalogTrack.KeyPos[KeyPosIndex] = SourceAnalogTrack.GetKeyPos(KeyPosIndex);
				TargetAnalogTrack.KeyPos[KeyPosIndex].Y *= -1.0f;
			}

			int CurrentTime = 0;
			for (int KeyTimeIndex = 0; KeyTimeIndex < TargetAnalogTrack.KeyTime.Num(); KeyTimeIndex++)
			{
				TargetAnalogTrack.KeyTime[KeyTimeIndex] = CurrentTime + SourceAnalogTrack.KeyDelta[KeyTimeIndex];
				CurrentTime += SourceAnalogTrack.KeyDelta[KeyTimeIndex];
			}
		}
	}

	unguard;
}
#endif // HP2

#endif // UNREAL1
