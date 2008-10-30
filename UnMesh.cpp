#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMesh.h"
#include "UnPackage.h"			// for loading texures by name (Rune) and checking real class name


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
#endif


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


// Says which triangles a particular mesh vertex is associated with.
// Precomputed so that mesh triangles can be shaded with Gouraud-style
// shared, interpolated normal shading.
// Used for UE1 UMesh only
struct FMeshVertConnect
{
	int				NumVertTriangles;
	int				TriangleListOffset;
	friend FArchive &operator<<(FArchive &Ar, FMeshVertConnect &C)
	{
		return Ar << C.NumVertTriangles << C.TriangleListOffset;
	}
};


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

	// get real class name
	FObjectExport &Exp = Package->GetExport(PackageIndex);
	const char *realClassName = Package->GetObjectName(Exp.ClassIndex);
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
		int pos = Ar.ArPos;								// remember position
		int skipPos, numVerts;
		Ar << skipPos << AR_INDEX(numVerts);			// read parameters
//		appNotify("***> size=%g numVerts=%d", (float)(skipPos - Ar.ArPos) / numVerts, numVerts);
		if (skipPos - Ar.ArPos == numVerts * sizeof(FMeshVertDeus))
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
		COPY_ARRAY(tmpVerts, Verts);					// TLazyArray  -> TArray
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
			printf("Scaling DeusEx VertMech by factor %g\n", scale);
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
		COPY_ARRAY(deusVerts, Verts);
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
		COPY_ARRAY(tmpWedges, Wedges);			// FMeshWedge1 -> FMeshWedge
		for (int i = 0; i < Wedges.Num(); i++)	// remap wedges (skip SpecialVerts)
			Wedges[i].iVertex += tmpSpecialVerts;
		printf("spec faces: %d  verts: %d\n", tmpSpecialFaces.Num(), tmpSpecialVerts);
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
			COPY_ARRAY(NewVerts, Verts)
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

#if SPLINTER_CELL

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
		COPY_ARRAY(KeyQuat, D.KeyQuat);						\
		COPY_ARRAY(KeyPos,  D.KeyPos );						\
		COPY_ARRAY(KeyTime, D.KeyTime);						\
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
		COPY_ARRAY(BoneIndices, D.BoneIndices);
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


#if TRIBES3 || HP3

struct FlexTrackStatic
{
	FVector		v1;
	FVector		v2;

	friend FArchive& operator<<(FArchive &Ar, FlexTrackStatic &T)
	{
		return Ar << T.v1 << T.v2;
	}
};

void SerializeFlexTracks(FArchive &Ar)
{
	guard(SerializeFlexTracks);

	int numTracks;
	Ar << AR_INDEX(numTracks);

	if (numTracks) appNotify("%d FlexTracks\n", numTracks);	//!!!

	for (int i = 0; i < numTracks; i++)
	{
		int trackType;
		Ar << trackType;

		switch (trackType)
		{
		case 1:
			// FlexTrackStatic
			{
				appError("1");
				FlexTrackStatic track;
				Ar << track;
			}
			break;

		case 2:
			appError("2");
			break;

		case 3:
			// FlexTrack48
			appError("3");
			break;

		case 4:
			// FlexTrack48RotOnly
			appError("4");
			break;
		}
		printf("flex: %d\n", trackType);
	}

	unguard;
}

#endif // TRIBES3 || HP3

#if TRIBES3

void FixTribesMotionChunk(MotionChunk &M)
{
	int numBones = M.AnimTracks.Num();
	for (int i = 0; i < numBones; i++)
	{
		AnalogTrack &A = M.AnimTracks[i];
		if (A.Flags & 0x1000)
		{
			if (!M.BoneIndices.Num())
			{
				// create BoneIndices
				M.BoneIndices.Empty(numBones);
				for (int j = 0; j < numBones; j++)
					M.BoneIndices.AddItem(j);
			}
			// bone overrided by Impersonator LipSinc
//			A.KeyQuat.Empty();
//			A.KeyPos.Empty();
//			A.KeyTime.Empty();
			M.BoneIndices[i] = INDEX_NONE;
		}
	}
}

#endif // TRIBES3


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
			for (k = 0; k < A.KeyTime.Num(); k++)
				A.KeyTime[k] = k;
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
	COPY_ARRAY(Points2, Points)
	COPY_ARRAY(ULodMesh::Wedges, Wedges);
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
	COPY_ARRAY(tmpWedges, Wedges);		// TLazyArray -> TArray
	COPY_ARRAY(tmpPoints, Points);		// ...
	COPY_ARRAY(Super::Wedges, Wedges);
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
		COPY_ARRAY(SS.Groups, S->Groups);
		S->StartFrame = 0;
		S->NumFrames  = SS.NumFrames;
		S->Rate       = SS.Rate;
		//?? S->Notifys
		// create MotionChunk
		MotionChunk *M = new(Anim.Moves) MotionChunk;
		M->TrackTime  = SS.NumFrames;
		// dummy bone remap
		M->BoneIndices.Empty(numBones);
		for (j = 0; j < numBones; j++)
			M->BoneIndices.AddItem(j);
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
				A.KeyTime.AddItem(frame);
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
		BP = (CVec3&)A.KeyPos[0];
		BO = (CQuat&)A.KeyQuat[0];
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
		USkeletalMesh *sm = static_cast<USkeletalMesh*>(CreateClass("SkeletalMesh"));
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
	Anim = static_cast<UMeshAnimation*>(CreateClass("MeshAnimation"));
	Anim->Name         = Name;
	Anim->Package      = Package;
	Anim->PackageIndex = INDEX_NONE;		// not really exported
	ConvertRuneAnimations(*Anim, joints, AnimSeqs);
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
			BoneCoords[v1.joint1].UnTransformPoint((CVec3&)v1.point1, (CVec3&)*V);
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
				printf("ERROR: unable to find export \"%s\" for mesh \"%s\" (%d)\n",
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
		sm->BoundingBox    = F.bounds;
		mins = (CVec3&)F.bounds.Min;
		maxs = (CVec3&)F.bounds.Max;
		CVec3 &center = (CVec3&)sm->BoundingSphere;
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
		free(MeshNames[i]);			// allocated with strdup()
}


#endif // RUNE
