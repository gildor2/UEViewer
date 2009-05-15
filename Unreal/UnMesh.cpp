#include "Core.h"
#include "UnrealClasses.h"
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

SIMPLE_TYPE(FMeshVertDeus, unsigned)

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

RAW_TYPE(FMeshWedge1)


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
	assert(Ar.IsLoading);
	if (Ar.ArVer < 123 || Ar.ArLicenseeVer < 0x19)
	{
		// standard UE2 format
		Ar << Moves;
		return;
	}
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


#if UNREAL25

//?? possibly, use FlexTrack directly in SkelMeshInstance - can derive AnalogTrack from FlexTrackBase
//?? and implement GetBonePosition() as its virtual method ...
struct FlexTrackBase
{
	virtual void Serialize(FArchive &Ar) = 0;
	virtual void Decompress(AnalogTrack &T) = 0;
};

struct FQuatFloat96NoW
{
	float			X, Y, Z;

	inline operator FQuat() const
	{
		FQuat r;
		r.X = X;
		r.Y = Y;
		r.Z = Z;
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;	// really have data, when wSq == -0.0f, and sqrt(wSq) was returned -INF
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatFloat96NoW &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z;
	}
};

SIMPLE_TYPE(FQuatFloat96NoW, float)

// normalized quaternion with 3 16-bit fixed point fields
struct FQuatFixed48NoW
{
	word			X, Y, Z;				// unsigned short, corresponds to (float+1)*32767

	inline operator FQuat() const
	{
		FQuat r;
		r.X = (X - 32767) / 32767.0f;
		r.Y = (Y - 32767) / 32767.0f;
		r.Z = (Z - 32767) / 32767.0f;
		// check FQuatFloat96NoW ...
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatFixed48NoW &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z;
	}
};

SIMPLE_TYPE(FQuatFixed48NoW, word)

struct FlexTrackStatic : public FlexTrackBase
{
	FQuatFloat96NoW		KeyQuat;
	FVector				KeyPos;

	virtual void Serialize(FArchive &Ar)
	{
		Ar << KeyQuat << KeyPos;
	}

	virtual void Decompress(AnalogTrack &T)
	{
		T.KeyQuat.AddItem(KeyQuat);
		T.KeyPos.AddItem(KeyPos);
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


// serialize TArray<FlexTrack>
void SerializeFlexTracks(FArchive &Ar, MotionChunk &M)
{
	guard(SerializeFlexTracks);

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
		FlexTrackBase *track = NULL;

		switch (trackType)
		{
		case 1:
			track = new FlexTrackStatic;
			break;

		case 2:
			// This type uses structure with TArray<FVector>, TArray<FQuatFloat96NoW> and TArray<short>.
			// It's Footprint() method returns 0, GetRotPos() does nothing, but serializer is working.
			appError("Unsupported FlexTrack type=2");
			break;

		case 3:
			track = new FlexTrack48;
			break;

		case 4:
			track = new FlexTrack48RotOnly;
			break;
		}
		if (track)
		{
			track->Serialize(Ar);
			track->Decompress(M.AnimTracks[i]);
			delete track;
		}
		else
			appError("no track!");	//?? possibly, mask tracks
	}

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


void USkeletalMesh::RecreateMeshFromLOD()
{
	guard(USkeletalMesh::RecreateMeshFromLOD);
	if (Wedges.Num() || !LODModels.Num()) return;		// nothing to do
	printf("Restoring mesh from LOD ...\n");

	FStaticLODModel &Lod = LODModels[0];

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
	TArray<FSCellUnk1> tmp1;
	TArray<FSCellUnk2> tmp2;
	TArray<FSCellUnk3> tmp3;
	TArray<FLODMeshSection> tmp4, tmp5;
	TArray<word> tmp6;
	FSCellUnk4 complex;
	Ar << tmp1 << tmp2 << tmp3 << tmp4 << tmp5 << tmp6 << complex;
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

RAW_TYPE(RTriangle)


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
		CopyArray(S->Groups, SS.Groups);
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
		free(MeshNames[i]);				// allocated with strdup()
}


#endif // RUNE


#if LINEAGE2

void FStaticLODModel::RestoreLineageMesh()
{
	guard(FStaticLODModel::RestoreLineageMesh);

	if (Wedges.Num()) return;			// nothing to restore
	printf("Converting Lineage2 LODModel to standard LODModel ...\n");
	if (SmoothSections.Num() && RigidSections.Num())
		appNotify("have smooth & rigid sections");

	int i, j, k;
	int NumWedges = LineageWedges.Num() + VertexStream.Verts.Num(); //??
	assert(LineageWedges.Num() == 0 || VertexStream.Verts.Num() == 0);

	Wedges.Empty(NumWedges);
	Points.Empty(NumWedges);			// really, should be a smaller count
	VertInfluences.Empty(NumWedges);	// min count = NumVerts
	Faces.Empty((SmoothIndices.Indices.Num() + RigidIndices.Indices.Num()) / 3);

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
	TArray<int> PointMap;
	PointMap.Empty(NumWedges);
	// convert LineageWedges (smooth sections)
	guard(BuildSmoothWedges);
	for (i = 0; i < LineageWedges.Num(); i++)
	{
		const FLineageWedge &LW = LineageWedges[i];
		// find the same point in previous items
		int PointIndex = INDEX_NONE;
		for (j = 0; j < i; j++)
		{
			const FLineageWedge &LW1 = LineageWedges[j];
			if (LW.Point == LW1.Point && LW.Normal == LW1.Normal)
			{
				PointIndex = PointMap[j];
				break;
			}
		}
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.Add();
			Points[PointIndex] = LW.Point;
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
		PointMap.AddItem(PointIndex);
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
		// find the same point in previous items
		int PointIndex = INDEX_NONE;
		for (j = 0; j < i; j++)
		{
			const FAnimMeshVertex &LW1 = VertexStream.Verts[j];
			if (LW.Pos == LW1.Pos && LW.Norm == LW1.Norm)
			{
				PointIndex = PointMap[j];
				break;
			}
		}
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.Add();
			Points[PointIndex] = LW.Pos;
			// build influences
			const FSkelMeshSection *ms = WedgeSection[i];
			assert(ms);
			FVertInfluences *Inf = new (VertInfluences) FVertInfluences;
			Inf->Weight     = 1.0f;
			Inf->BoneIndex  = /*VertexStream.Revision; //??*/ ms->BoneIndex; //-- equals 0 in Lineage2 ...
			Inf->PointIndex = PointIndex;
		}
		PointMap.AddItem(PointIndex);
		// create wedge
		FMeshWedge *W = new (Wedges) FMeshWedge;
		W->iVertex = PointIndex;
		W->TexUV   = LW.Tex;
	}
	unguard;

	unguard;
}


#endif // LINEAGE2


#if UNREAL3

struct FSkelMeshSection3
{
	short				MaterialIndex;
	short				unk1;
	int					FirstIndex;
	short				NumTriangles;

	friend FArchive& operator<<(FArchive &Ar, FSkelMeshSection3 &S)
	{
		return Ar << S.MaterialIndex << S.unk1 << S.FirstIndex << S.NumTriangles;
	}
};

struct FIndexBuffer3
{
	TRawArray<short>	Indices;

	friend FArchive& operator<<(FArchive &Ar, FIndexBuffer3 &I)
	{
		return Ar << I.Indices;
	}
};

static bool CompareCompNormals(int Normal1, int Normal2)
{
	for (int i = 0; i < 3; i++)
	{
		char b1 = Normal1 & 0xFF;
		char b2 = Normal2 & 0xFF;
		if (abs(b1 - b2) > 10) return false;
		Normal1 >>= 8;
		Normal2 >>= 8;
	}
	return true;
}

struct FRigidVertex3
{
	FVector				Pos;
	int					Normal[3];		// FVectorComp (FVector as 4 bytes)
	float				U, V;
	byte				BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, FRigidVertex3 &V)
	{
		// note: version prior 477 have different normal/tangent format (same layout, but different
		// data meaning)
		Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2];
		Ar << V.U << V.V;
#if MEDGE
		if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 13)
		{
			float U1, V1, U2, V2;
			Ar << U1 << V1 << U2 << V2;
		}
#endif // MEDGE
		Ar << V.BoneIndex;
		return Ar;
	}
};

struct FSmoothVertex3
{
	FVector				Pos;
	int					Normal[3];		// FVectorComp (FVector as 4 bytes)
	float				U, V;
	byte				BoneIndex[4];
	byte				BoneWeight[4];

	friend FArchive& operator<<(FArchive &Ar, FSmoothVertex3 &V)
	{
		int i;
		// note: version prior 477 have different normal/tangent format (same layout, but different
		// data meaning)
		Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2] << V.U << V.V;
#if MEDGE
		if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 13)
		{
			float U1, V1, U2, V2;
			Ar << U1 << V1 << U2 << V2;
		}
#endif // MEDGE
		if (Ar.ArVer >= 333)
		{
			for (i = 0; i < 4; i++) Ar << V.BoneIndex[i];
			for (i = 0; i < 4; i++) Ar << V.BoneWeight[i];
		}
		else
		{
			for (i = 0; i < 4; i++)
				Ar << V.BoneIndex[i] << V.BoneWeight[i];
		}
		return Ar;
	}
};

struct FSkinChunk3
{
	int					FirstVertex;
	TArray<FRigidVertex3>  RigidVerts;
	TArray<FSmoothVertex3> SmoothVerts;
	TArray<short>		Bones;
	int					NumRigidVerts;
	int					NumSmoothVerts;
	int					MaxInfluences;

	friend FArchive& operator<<(FArchive &Ar, FSkinChunk3 &V)
	{
		guard(FSkinChunk3<<);
		Ar << V.FirstVertex << V.RigidVerts << V.SmoothVerts << V.Bones;
		if (Ar.ArVer >= 333)
		{
			Ar << V.NumRigidVerts << V.NumSmoothVerts;
			// note: NumRigidVerts and NumSmoothVerts may be non-zero while corresponding
			// arrays are empty - that's when GPU skin only left
		}
		else
		{
			V.NumRigidVerts  = V.RigidVerts.Num();
			V.NumSmoothVerts = V.SmoothVerts.Num();
		}
		if (Ar.ArVer >= 362)
			Ar << V.MaxInfluences;
		return Ar;
		unguard;
	}
};

struct FEdge3
{
	int					iVertex[2];
	int					iFace[2];

	friend FArchive& operator<<(FArchive &Ar, FEdge3 &V)
	{
		return Ar << V.iVertex[0] << V.iVertex[1] << V.iFace[0] << V.iFace[1];
	}
};

SIMPLE_TYPE(FEdge3, int)

struct FGPUVert3Common
{
	FVector				Pos;
	int					Normal[3];		// FVectorComp (FVector as 4 bytes)
	byte				BoneIndex[4];
	byte				BoneWeight[4];

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Common &V)
	{
		int i;
		Ar << V.Pos << V.Normal[0] << V.Normal[1];
		if (Ar.ArVer < 494)
			Ar << V.Normal[2];
		for (i = 0; i < 4; i++) Ar << V.BoneIndex[i];
		for (i = 0; i < 4; i++) Ar << V.BoneWeight[i];
		return Ar;
	}
};

/*
 * Half = Float16
 * http://www.openexr.com/  source: ilmbase-*.tar.gz/Half/toFloat.cpp
 * http://en.wikipedia.org/wiki/Half_precision
 * Also look GL_ARB_half_float_pixel
 */
struct FGPUVert3Half : FGPUVert3Common
{
	word				U, V;			//?? create class float16 ?

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Half &V)
	{
		Ar << *((FGPUVert3Common*)&V) << V.U << V.V;
		return Ar;
	}
};

struct FGPUVert3Float : FGPUVert3Common
{
	float				U, V;

	FGPUVert3Float &operator=(const FSmoothVertex3 &S)
	{
		Pos = S.Pos;
		U   = S.U;
		V   = S.V;
		for (int i = 0; i < 4; i++)
		{
			BoneIndex[i]  = S.BoneIndex[i];
			BoneWeight[i] = S.BoneWeight[i];
		}
		return *this;
	}

	friend FArchive& operator<<(FArchive &Ar, FGPUVert3Float &V)
	{
		Ar << *((FGPUVert3Common*)&V) << V.U << V.V;
		return Ar;
	}
};

struct FGPUSkin3
{
	int							bUseFullPrecisionUVs;	// 0 = half, 1 = float; copy of corresponding USkeletalMesh field
	TRawArray<FGPUVert3Half>	VertsHalf;				// only one of these vertex sets are used
	TRawArray<FGPUVert3Float>	VertsFloat;

	friend FArchive& operator<<(FArchive &Ar, FGPUSkin3 &S)
	{
		guard(FSkinData3<<);
	#if HUXLEY
		if (Ar.IsHuxley) goto old_version;
	#endif
		if (Ar.ArVer < 493)
		{
		old_version:
			// old version - FSmoothVertex3 array
			TRawArray<FSmoothVertex3> Verts;
			Ar << Verts;
			// convert verts
			CopyArray(S.VertsFloat, Verts);
			S.bUseFullPrecisionUVs = true;
			return Ar;
		}
		// new version
	#if MEDGE
		int NumUVSets = 1;
		if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 0xF)
			Ar << NumUVSets;
	#endif // MEDGE
		Ar << S.bUseFullPrecisionUVs;
		if (!S.bUseFullPrecisionUVs)
		{
	#if MEDGE
			if (NumUVSets > 1)
			{
				SkipRawArray(Ar, 0x20 + (NumUVSets - 1) * 4);
				return Ar;
			}
	#endif
			Ar << S.VertsHalf;
		}
		else
		{
	#if MEDGE
			if (NumUVSets > 1)
			{
				SkipRawArray(Ar, 0x24 + (NumUVSets - 1) * 8);
				return Ar;
			}
	#endif
			Ar << S.VertsFloat;
		}
		return Ar;
		unguard;
	}
};

struct FMesh3Unk1
{
	int					f0;
	int					f4;

	friend FArchive& operator<<(FArchive &Ar, FMesh3Unk1 &S)
	{
		return Ar << S.f0 << S.f4;
	}
};

SIMPLE_TYPE(FMesh3Unk1, int)

struct FMesh3Unk2
{
	TArray<FMesh3Unk1>	f0;

	friend FArchive& operator<<(FArchive &Ar, FMesh3Unk2 &S)
	{
		return Ar << S.f0;
	}
};

struct FStaticLODModel3
{
	TArray<FSkelMeshSection3> Sections;
	TArray<FSkinChunk3>	Chunks;
	FIndexBuffer3		IndexBuffer;
	TArray<short>		UsedBones;		// bones, value = [0, NumBones-1]
	TArray<byte>		f24;			// count = NumBones, value = [0, NumBones-1]; note: BoneIndex is 'short', not 'byte' ...
	TArray<short>		f68;			// indices, value = [0, NumVertices-1]
	TArray<byte>		f74;			// count = NumTriangles
	int					f80;
	int					NumVertices;
	TArray<FEdge3>		Edges;			// links 2 vertices and 2 faces (triangles)
	FWordBulkData		BulkData;		// ElementCount = NumVertices
	FGPUSkin3			GPUSkin;
	TArray<FMesh3Unk2>	fC4;			// unknown, has in GoW2

	friend FArchive& operator<<(FArchive &Ar, FStaticLODModel3 &Lod)
	{
		guard(FStaticLODModel3<<);

		int tmp1;
		Ar << Lod.Sections << Lod.IndexBuffer;
		if (Ar.ArVer < 297)
			Ar << tmp1;
		Ar << Lod.f68 << Lod.UsedBones << Lod.f74 << Lod.Chunks;
		Ar << Lod.f80 << Lod.NumVertices << Lod.Edges << Lod.f24;
		Lod.BulkData.Serialize(Ar);
		if (Ar.ArVer >= 333)
			Ar << Lod.GPUSkin;
#if MEDGE
		if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 0xF)
			return Ar;
#endif // MEDGE
		if (Ar.ArVer >= 534)		// GoW2 code
			Ar << Lod.fC4;
//		assert(Lod.IndexBuffer.Indices.Num() == Lod.f68.Num()); -- mostly equals (failed in CH_TwinSouls_Cine.upk)
//		assert(Lod.BulkData.ElementCount == Lod.NumVertices); -- mostly equals (failed on some GoW packages)
		return Ar;

		unguard;
	}
};

void USkeletalMesh::SerializeSkelMesh3(FArchive &Ar)
{
	guard(USkeletalMesh::SerializeSkelMesh3);

	UObject::Serialize(Ar);			// no UPrimitive ...

	FBoxSphereBounds	Bounds;
	TArray<UMaterial*>	Materials1;	// MaterialInterface*
	TArray<FStaticLODModel3> Lods;

#if MEDGE
	if (Ar.IsMirrorEdge && Ar.ArLicenseeVer >= 0xF)
	{
		int unk264;
		Ar << unk264;
	}
#endif // MEDGE
	Ar << Bounds << Materials1 << MeshOrigin << RotOrigin;
	Ar << RefSkeleton << SkeletalDepth << Lods;
#if 0
	//!! also: NameIndexMap (ArVer >= 296), PerPolyKDOPs (ArVer >= 435)
#else
	Ar.Seek(Ar.GetStopper());
#endif

	guard(ConvertMesh);
	int i;

	// convert LODs
	assert(Lods.Num() == LODInfo.Num());
	LODModels.Empty(Lods.Num());
	LODModels.Add(Lods.Num());
	for (i = 0; i < Lods.Num(); i++)
		LODModels[i].RestoreMesh3(*this, Lods[i], LODInfo[i]);

	//!! Optimize search for PointIndex in RestoreMesh3()
	/*!!
	 *	Profile results (GoW2 package): total time = 7.8s, partial:
	 *	load = 1.5s, GPU lods -> CPU = 5.5s, restore mesh = 0.7s
	 *	Loading w/o seraching for good PointIndex = 1.6s, with search = 7.7s (not depends on restore mesh!)
	 *	With disabled CompareCompNormals() = 6.3s
	 *	New version (find PointIndex using TArray::FindItem()) = 4.4s
	 */
	// convert LOD 0 to mesh
	RecreateMeshFromLOD();

	MeshOrigin.Scale(-1);

	// fix skeleton; all bones but 0
	for (i = 1; i < RefSkeleton.Num(); i++)
		RefSkeleton[i].BonePos.Orientation.W *= -1;

	// materials
	Textures.Add(Materials1.Num());
	Materials.Add(Materials1.Num());
	for (i = 0; i < Materials.Num(); i++)
	{
		Textures[i] = Materials1[i];
		Materials[i].TextureIndex = i;
	}

	// setup missing properties
	MeshScale.Set(1, 1, 1);
	BoundingSphere.R = Bounds.SphereRadius / 2;		//?? UE3 meshes has radius 2 times larger than mesh
	VectorSubtract((CVec3&)Bounds.Origin, (CVec3&)Bounds.BoxExtent, (CVec3&)BoundingBox.Min);
	VectorAdd     ((CVec3&)Bounds.Origin, (CVec3&)Bounds.BoxExtent, (CVec3&)BoundingBox.Max);

	unguard;

	unguard;
}


static float half2float(word h)
{
	union
	{
		float		f;
		unsigned	df;
	} f;

	int sign = (h >> 15) & 0x00000001;
	int exp  = (h >> 10) & 0x0000001F;
	int mant =  h        & 0x000003FF;

	exp  = exp + (127 - 15);
	mant = mant << 13;
	f.df = (sign << 31) | (exp << 23) | mant;
	return f.f;
}


void FStaticLODModel::RestoreMesh3(const USkeletalMesh &Mesh, const FStaticLODModel3 &Lod, const FSkeletalMeshLODInfo &Info)
{
	guard(FStaticLODModel::RestoreMesh3);

	// prepare arrays
	TArray<int> PointNormals;
	Points.Empty        (Lod.NumVertices);
	PointNormals.Empty  (Lod.NumVertices);
	Wedges.Empty        (Lod.NumVertices);
	VertInfluences.Empty(Lod.NumVertices * 4);

	// create wedges, vertices and influences
	guard(ProcessChunks);
	for (int Chunk = 0; Chunk < Lod.Chunks.Num(); Chunk++)
	{
		int Vert, j;
		const FSkinChunk3 &C = Lod.Chunks[Chunk];

		// when base mesh vertices are missing - try to get information from GPU skin
		if ((C.NumRigidVerts != C.RigidVerts.Num() && C.RigidVerts.Num() == 0) ||
			(C.NumSmoothVerts != C.SmoothVerts.Num() && C.SmoothVerts.Num() == 0))
		{
			guard(GPUVerts);
			if (!Chunk) printf("Restoring LOD verts from GPU skin\n", Mesh.Name);

			const FGPUSkin3 &S = Lod.GPUSkin;
			int LastVertex = C.FirstVertex + C.NumRigidVerts + C.NumSmoothVerts;

			for (Vert = C.FirstVertex; Vert < LastVertex; Vert++)
			{
				const FGPUVert3Common &V = (!S.bUseFullPrecisionUVs)
					? *(FGPUVert3Common*)&S.VertsHalf[Vert]
					: *(FGPUVert3Common*)&S.VertsFloat[Vert];
				// find the same point in previous items
#if 0
				int PointIndex = INDEX_NONE;
				for (j = 0; j < Points.Num(); j++)
				{
					if (Points[j] == V.Pos && CompareCompNormals(PointNormals[j], V.Normal[0]))
					{
						PointIndex = j;
						break;
					}
				}
#else
				int PointIndex = -1;	// start with 0, see below
				while (true)
				{
					PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
					if (PointIndex == INDEX_NONE) break;
					if (CompareCompNormals(PointNormals[PointIndex], V.Normal[0])) break;
				}
#endif
				if (PointIndex == INDEX_NONE)
				{
					// point was not found - create it
					PointIndex = Points.Add();
					Points[PointIndex] = V.Pos;
					PointNormals.AddItem(V.Normal[0]);
					// add influences
//					int TotalWeight = 0;
					for (int i = 0; i < 4; i++)
					{
						int BoneIndex  = V.BoneIndex[i];
						int BoneWeight = V.BoneWeight[i];
						if (BoneWeight == 0) continue;
						FVertInfluences *Inf = new(VertInfluences) FVertInfluences;
						Inf->PointIndex = PointIndex;
						Inf->Weight     = BoneWeight / 255.0f;
						Inf->BoneIndex  = C.Bones[BoneIndex];
//						TotalWeight += BoneWeight;
					}
//					assert(TotalWeight = 255);
				}
				// create wedge
				FMeshWedge *W = new(Wedges) FMeshWedge;
				W->iVertex = PointIndex;
				if (!S.bUseFullPrecisionUVs)
				{
					const FGPUVert3Half &V1 = *(const FGPUVert3Half*)&V;
					W->TexUV.U = half2float(V1.U);
					W->TexUV.V = half2float(V1.V);
				}
				else
				{
					const FGPUVert3Float &V1 = *(const FGPUVert3Float*)&V;
					W->TexUV.U = V1.U;
					W->TexUV.V = V1.V;
				}
			}
			unguard;

			continue;		// process other chunks
		}

		// get information from base (CPU) mesh
		guard(RigidVerts);
		for (Vert = 0; Vert < C.NumRigidVerts; Vert++)
		{
			const FRigidVertex3 &V = C.RigidVerts[Vert];
			// find the same point in previous items
#if 0
			int PointIndex = INDEX_NONE;
			for (j = 0; j < Points.Num(); j++)
			{
				if (Points[j] == V.Pos && CompareCompNormals(PointNormals[j], V.Normal[0]))
				{
					PointIndex = j;
					break;
				}
			}
#else
			int PointIndex = -1;	// start with 0, see below
			while (true)
			{
				PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
				if (PointIndex == INDEX_NONE) break;
				if (CompareCompNormals(PointNormals[PointIndex], V.Normal[0])) break;
			}
#endif
			if (PointIndex == INDEX_NONE)
			{
				// point was not found - create it
				PointIndex = Points.Add();
				Points[PointIndex] = V.Pos;
				PointNormals.AddItem(V.Normal[0]);
				// add influence
				FVertInfluences *Inf = new(VertInfluences) FVertInfluences;
				Inf->PointIndex = PointIndex;
				Inf->Weight     = 1.0f;
				Inf->BoneIndex  = C.Bones[V.BoneIndex];
			}
			// create wedge
			FMeshWedge *W = new(Wedges) FMeshWedge;
			W->iVertex = PointIndex;
			W->TexUV.U = V.U;
			W->TexUV.V = V.V;
		}
		unguard;

		guard(SmoothVerts);
		for (Vert = 0; Vert < C.NumSmoothVerts; Vert++)
		{
			const FSmoothVertex3 &V = C.SmoothVerts[Vert];
			// find the same point in previous items
#if 0
			int PointIndex = INDEX_NONE;
			for (j = 0; j < Points.Num(); j++)	//!! should compare influences too !
			{
				if (Points[j] == V.Pos && CompareCompNormals(PointNormals[j], V.Normal[0]))
				{
					PointIndex = j;
					break;
				}
			}
#else
			int PointIndex = -1;	// start with 0, see below
			while (true)
			{
				PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
				if (PointIndex == INDEX_NONE) break;
				if (CompareCompNormals(PointNormals[PointIndex], V.Normal[0])) break;
				//?? should compare influences too !
			}
#endif
			if (PointIndex == INDEX_NONE)
			{
				// point was not found - create it
				PointIndex = Points.Add();
				Points[PointIndex] = V.Pos;
				PointNormals.AddItem(V.Normal[0]);
				// add influences
				for (int i = 0; i < 4; i++)
				{
					int BoneIndex  = V.BoneIndex[i];
					int BoneWeight = V.BoneWeight[i];
					if (BoneWeight == 0) continue;
					FVertInfluences *Inf = new(VertInfluences) FVertInfluences;
					Inf->PointIndex = PointIndex;
					Inf->Weight     = BoneWeight / 255.0f;
					Inf->BoneIndex  = C.Bones[BoneIndex];
				}
			}
			// create wedge
			FMeshWedge *W = new(Wedges) FMeshWedge;
			W->iVertex = PointIndex;
			W->TexUV.U = V.U;
			W->TexUV.V = V.V;
		}
		unguard;
	}
	unguard;

	// count triangles (speed optimization for TArray allocations)
	int NumTriangles = 0;
	int Sec;
	for (Sec = 0; Sec < Lod.Sections.Num(); Sec++)
		NumTriangles += Lod.Sections[Sec].NumTriangles;
	Faces.Empty(NumTriangles);
	// create faces
	for (Sec = 0; Sec < Lod.Sections.Num(); Sec++)
	{
		const FSkelMeshSection3 &S = Lod.Sections[Sec];
		if (S.unk1 != Sec) appNotify("Sec=%d unk1=%d", Sec, S.unk1);	//??

		FSkelMeshSection *Dst = new (SmoothSections) FSkelMeshSection;
		int MaterialIndex = S.MaterialIndex;
		if (Info.LODMaterialMap.Num())
			MaterialIndex = Info.LODMaterialMap[MaterialIndex];
		Dst->MaterialIndex = MaterialIndex;
		Dst->FirstFace     = Faces.Num();
		Dst->NumFaces      = S.NumTriangles;

		int Index = S.FirstIndex;
		for (int i = 0; i < S.NumTriangles; i++)
		{
			FMeshFace *Face = new (Faces) FMeshFace;
			Face->MaterialIndex = MaterialIndex;
			Face->iWedge[0] = Lod.IndexBuffer.Indices[Index++];
			Face->iWedge[1] = Lod.IndexBuffer.Indices[Index++];
			Face->iWedge[2] = Lod.IndexBuffer.Indices[Index++];
		}
	}

	//!! recreate SmoothSections and RigidSections

	unguard;

	return;
}


// normalized quaternion with 11/11/10-bit fixed point fields
struct FQuatFixed32NoW
{
	unsigned		Z:10, Y:11, X:11;

	inline operator FQuat() const
	{
		FQuat r;
		r.X = X / 1023.0f - 1.0f;
		r.Y = Y / 1023.0f - 1.0f;
		r.Z = Z / 511.0f  - 1.0f;
		// check FQuatFloat96NoW ...
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatFixed32NoW &Q)
	{
		return Ar << GET_DWORD(Q);
	}
};

SIMPLE_TYPE(FQuatFixed32NoW, unsigned)


struct FQuatIntervalFixed32NoW
{
	unsigned		Z:10, Y:11, X:11;

	FQuat ToQuat(const FVector &Mins, const FVector &Ranges) const
	{
		FQuat r;
		r.X = (X / 1023.0f - 1.0f) * Ranges.X + Mins.X;
		r.Y = (Y / 1023.0f - 1.0f) * Ranges.Y + Mins.Y;
		r.Z = (Z / 511.0f  - 1.0f) * Ranges.Z + Mins.Z;
		// check FQuatFloat96NoW ...
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatIntervalFixed32NoW &Q)
	{
		return Ar << GET_DWORD(Q);
	}
};

SIMPLE_TYPE(FQuatIntervalFixed32NoW, unsigned)


// following defines will help finding new undocumented compression schemes
#define FIND_HOLES			1
//#define DEBUG_DECOMPRESS	1

static void ReadTimeArray(FArchive &Ar, int NumKeys, TArray<float> &Times, int NumFrames)
{
	guard(ReadTimeArray);
	if (NumKeys <= 1) return;

//	printf("  pos=%4X keys (max=%X)[ ", Ar.Tell(), NumFrames);
	if (NumFrames < 256)
	{
		for (int k = 0; k < NumKeys; k++)
		{
			byte v;
			Ar << v;
			Times.AddItem(v);
//			if (k < 4 || k > NumKeys - 5) printf(" %02X ", v);
//			else if (k == 4) printf("...");
		}
	}
	else
	{
		for (int k = 0; k < NumKeys; k++)
		{
			word v;
			Ar << v;
			Times.AddItem(v);
//			if (k < 4 || k > NumKeys - 5) printf(" %04X ", v);
//			else if (k == 4) printf("...");
		}
	}
//	printf(" ]\n");

	// align to 4 bytes
	Ar.Seek(Align(Ar.Tell(), 4));

	unguard;
}


void UAnimSet::ConvertAnims()
{
	guard(UAnimSet::ConvertAnims);

	int i;

	RefBones.Empty(TrackBoneNames.Num());
	for (i = 0; i < TrackBoneNames.Num(); i++)
	{
		FNamedBone *Bone = new (RefBones) FNamedBone;
		Bone->Name = TrackBoneNames[i];
		// Flags, ParentIndex unused
	}

#if FIND_HOLES
	bool findHoles = true;
#endif
	int NumTracks = TrackBoneNames.Num();

	for (i = 0; i < Sequences.Num(); i++)
	{
		const UAnimSequence *Seq = Sequences[i];
		if (!Seq)
		{
			printf("WARNING: %s: no sequence %d\n", Name, i);
			continue;
		}
		// some checks
		int offsetsPerBone = 4;
#if TLR
		if (Package->IsTLR) offsetsPerBone = 6;
#endif
		if (NumTracks * offsetsPerBone != Seq->CompressedTrackOffsets.Num())
		{
			//!! Solutions: (when not enough CompressedTrackOffsets)
			//!!	1) fill missing "bad" track with some constant
			//!!	2) remove "bad" track
			//!!	3) remove extra bones from all sequences
			appNotify("AnimSequence %s/%s has wrong CompressedTrackOffsets size (%d != %d), removing track",
				Name, *Seq->SequenceName, Seq->CompressedTrackOffsets.Num(), NumTracks * 4);
			continue;
		}

		// create FMeshAnimSeq
		FMeshAnimSeq *Dst = new (AnimSeqs) FMeshAnimSeq;
		Dst->Name      = Seq->SequenceName;
		Dst->NumFrames = Seq->NumFrames;
		Dst->Rate      = Seq->NumFrames / Seq->SequenceLength * Seq->RateScale;
		// create MotionChunk
		MotionChunk *M = new (Moves) MotionChunk;

#if DEBUG_DECOMPRESS
		printf("ComprTrack: %d bytes, %d frames\n", Seq->CompressedByteStream.Num(), Seq->NumFrames);
#endif

		// bone tracks ...
		M->AnimTracks.Empty(NumTracks);

		FMemReader Reader(Seq->CompressedByteStream.GetData(), Seq->CompressedByteStream.Num());
		Reader.ReverseBytes = Package->ReverseBytes;
		bool hasTimeTracks = strcmp(Seq->KeyEncodingFormat, "AKF_VariableKeyLerp") == 0;

		int offsetIndex = 0;
		for (int j = 0; j < NumTracks; j++, offsetIndex += offsetsPerBone)
		{
			AnalogTrack *A = new (M->AnimTracks) AnalogTrack;

			int k;

			if (!Seq->CompressedTrackOffsets.Num())	//?? or if RawAnimData.Num() != 0
			{
				// using RawAnimData array
				assert(Seq->RawAnimData.Num() == NumTracks);
				CopyArray(A->KeyPos,  Seq->RawAnimData[j].PosKeys);
				CopyArray(A->KeyQuat, Seq->RawAnimData[j].RotKeys);
				CopyArray(A->KeyTime, Seq->RawAnimData[j].KeyTimes);
				for (int kk = 0; kk < A->KeyTime.Num(); kk++)
					A->KeyTime[kk] *= Dst->Rate;
				continue;
			}

			// read animations
			int TransOffset = Seq->CompressedTrackOffsets[offsetIndex  ];
			int TransKeys   = Seq->CompressedTrackOffsets[offsetIndex+1];
			int RotOffset   = Seq->CompressedTrackOffsets[offsetIndex+2];
			int RotKeys     = Seq->CompressedTrackOffsets[offsetIndex+3];
#if TLR
			int ScaleOffset = 0, ScaleKeys = 0;
			if (Package->IsTLR)
			{
				ScaleOffset  = Seq->CompressedTrackOffsets[offsetIndex+4];
				ScaleKeys    = Seq->CompressedTrackOffsets[offsetIndex+5];
			}
#endif
//			printf("[%d] :  %d[%d]  %d[%d]  %d[%d]\n", TransOffset, TransKeys, RotOffset, RotKeys, ScaleOffset, ScaleKeys);

			A->KeyPos.Empty(TransKeys);
			A->KeyQuat.Empty(RotKeys);
			if (hasTimeTracks)
			{
				A->KeyPosTime.Empty(TransKeys);
				A->KeyQuatTime.Empty(RotKeys);
			}

			// read translation keys
			if (TransKeys)
			{
#if FIND_HOLES
				int hole = TransOffset - Reader.Tell();
				if (findHoles && hole/** && abs(hole) > 4*/)	//?? should not be holes at all
				{
					appNotify("AnimSet:%s Seq:%s [%d] hole (%d) before TransTrack", Name, *Seq->SequenceName, j, hole);
///					findHoles = false;
				}
#endif
				Reader.Seek(TransOffset);
				for (k = 0; k < TransKeys; k++)
				{
					FVector vec;
					Reader << vec;
					A->KeyPos.AddItem(vec);
				}
				// align to 4 bytes
				Reader.Seek(Align(Reader.Tell(), 4));
				if (hasTimeTracks)
					ReadTimeArray(Reader, TransKeys, A->KeyPosTime, Seq->NumFrames);
			}
			else
			{
				static FVector zero;
				A->KeyPos.AddItem(zero);
				appNotify("No translation keys!");
			}

#if DEBUG_DECOMPRESS
			int TransEnd = Reader.Tell();
#endif
#if FIND_HOLES
			int hole = RotOffset - Reader.Tell();
			if (findHoles && hole/** && abs(hole) > 4*/)	//?? should not be holes at all
			{
				appNotify("AnimSet:%s Seq:%s [%d] hole (%d) before RotTrack (KeyFormat=%s)",
					Name, *Seq->SequenceName, j, hole, *Seq->KeyEncodingFormat);
///				findHoles = false;
			}
#endif
			// read rotation keys
			Reader.Seek(RotOffset);
			if (RotKeys == 1)
			{
				FQuatFloat96NoW q;
				Reader << q;
				A->KeyQuat.AddItem(q);
			}
			else
			{
				// read mins/ranges
				FVector Mins, Ranges;
				Reader << Mins << Ranges;

				for (k = 0; k < RotKeys; k++)
				{
					if (!strcmp(Seq->RotationCompressionFormat, "ACF_None"))
					{
						FQuat q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
					else if (!strcmp(Seq->RotationCompressionFormat, "ACF_Float96NoW"))
					{
						FQuatFloat96NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
					else if (!strcmp(Seq->RotationCompressionFormat, "ACF_Fixed48NoW"))
					{
						FQuatFixed48NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
					else if (!strcmp(Seq->RotationCompressionFormat, "ACF_Fixed32NoW"))
					{
						FQuatFixed32NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q);
					}
					else if (!strcmp(Seq->RotationCompressionFormat, "ACF_IntervalFixed32NoW"))
					{
						FQuatIntervalFixed32NoW q;
						Reader << q;
						A->KeyQuat.AddItem(q.ToQuat(Mins, Ranges));
					}
					//!! other: ACF_Float32NoW
//FQuat qq = Seq->RawAnimData[j].RotKeys[k];
//KeyQuat.AddItem(qq);
//static int execed = 0;
//if (++execed < 10) {
//printf("q: %X : (%d %d %d)  (%g %g %g %g)\n", GET_DWORD(*q), q->X-1023, q->Y-1023, q->Z-511, FQUAT_ARG(qq));
//}
					else
						appError("Unknown compression method: %s", *Seq->RotationCompressionFormat);
				}
				// align to 4 bytes
				Reader.Seek(Align(Reader.Tell(), 4));
				if (hasTimeTracks)
					ReadTimeArray(Reader, RotKeys, A->KeyQuatTime, Seq->NumFrames);
			}
#if TLR
			if (ScaleKeys)
			{
				//?? no ScaleKeys support, simply drop data
				Reader.Seek(ScaleOffset + ScaleKeys * 12);
				Reader.Seek(Align(Reader.Tell(), 4));
			}
#endif
#if DEBUG_DECOMPRESS
//			printf("[%s : %s] Frames=%d KeyPos.Num=%d KeyQuat.Num=%d KeyFmt=%s\n", *Seq->SequenceName, *TrackBoneNames[j],
//				Seq->NumFrames, A->KeyPos.Num(), A->KeyQuat.Num(), *Seq->KeyEncodingFormat);
			printf("    [%d]: %d - %d + %d - %d (%d/%d)\n", j,
				TransOffset, TransEnd, RotOffset, Reader.Tell(), TransKeys, RotKeys);
#endif
		}
	}

	unguard;
}


#endif // UNREAL3
