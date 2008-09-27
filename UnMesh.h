#ifndef __UNMESH_H__
#define __UNMESH_H__


#define COPY_ARRAY(Src, Dst)					\
		if (Src.Num() && !Dst.Num())			\
		{										\
			guard(Src);							\
			Dst.Empty(Src.Num());				\
			Dst.Add(Src.Num());					\
			for (int i = 0; i < Src.Num(); i++)	\
				Dst[i] = Src[i];				\
			unguard;							\
		}


/*-----------------------------------------------------------------------------
	UPrimitive class
-----------------------------------------------------------------------------*/

class UPrimitive : public UObject
{
	DECLARE_CLASS(UPrimitive, UObject);
public:
	FBox			BoundingBox;
	FSphere			BoundingSphere;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UPrimitive::Serialize);
		Super::Serialize(Ar);
		Ar << BoundingBox << BoundingSphere;
		unguard;
	}
};


/*-----------------------------------------------------------------------------
	ULodMesh class and common data structures
-----------------------------------------------------------------------------*/

// Packed mesh vertex point for vertex meshes
#define GET_MESHVERT_DWORD(mv) (*(unsigned*)&(mv))

struct FMeshVert
{
	int X:11; int Y:11; int Z:10;

	friend FArchive& operator<<(FArchive &Ar, FMeshVert &V)
	{
		return Ar << GET_MESHVERT_DWORD(V);
	}
};

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


// Packed mesh vertex point for skinned meshes.
#define GET_MESHNORM_DWORD(mv) (*(unsigned*)&(mv))

struct FMeshNorm
{
	// non-normalized vector, to convert to float should use
	// (value - 512) / 512
	unsigned X:10; unsigned Y:10; unsigned Z:10;

	friend FArchive& operator<<(FArchive &Ar, FMeshNorm &V)
	{
		return Ar << GET_MESHNORM_DWORD(V);
	}
};


// corresponds to UT1 FMeshFloatUV
struct FMeshUV
{
	float			U;
	float			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUV &M)
	{
		return Ar << M.U << M.V;
	}
};

// UE1 FMeshUV
struct FMeshUV1
{
	byte			U;
	byte			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUV1 &M)
	{
		return Ar << M.U << M.V;
	}
};


// LOD-style triangular polygon in a mesh, which references three textured vertices.
struct FMeshFace
{
	word			iWedge[3];				// Textured Vertex indices.
	word			MaterialIndex;			// Source Material (= texture plus unique flags) index.

	friend FArchive& operator<<(FArchive &Ar, FMeshFace &F)
	{
		Ar << F.iWedge[0] << F.iWedge[1] << F.iWedge[2];
		Ar << F.MaterialIndex;
		return Ar;
	}
};


// temp structure, skipped while mesh loading
struct FMeshTri
{
	word			iVertex[3];				// Vertex indices.
	FMeshUV			Tex[3];					// Texture UV coordinates.
	unsigned		PolyFlags;				// Surface flags.
	int				TextureIndex;			// Source texture index.

	friend FArchive& operator<<(FArchive& Ar, FMeshTri &T)
	{
		Ar << T.iVertex[0] << T.iVertex[1] << T.iVertex[2];
		Ar << T.Tex[0] << T.Tex[1] << T.Tex[2];
		Ar << T.PolyFlags << T.TextureIndex;
		return Ar;
	}
};


// LOD-style Textured vertex struct. references one vertex, and
// contains texture U,V information.
// One triangular polygon in a mesh, which references three vertices,
// and various drawing/texturing information.
// Corresponds to UT1 FMeshExtWedge.
struct FMeshWedge
{
	word			iVertex;				// Vertex index.
	FMeshUV			TexUV;					// Texture UV coordinates.
	friend FArchive& operator<<(FArchive &Ar, FMeshWedge &T)
	{
		Ar << T.iVertex << T.TexUV;
		return Ar;
	}
};

// UE1 FMeshWedge
struct FMeshWedge1
{
	word			iVertex;
	FMeshUV1		TexUV;

	operator FMeshWedge() const
	{
		FMeshWedge r;
		r.iVertex = iVertex;
		r.TexUV.U = TexUV.U / 256.0f;
		r.TexUV.V = TexUV.V / 256.0f;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FMeshWedge1 &T)
	{
		Ar << T.iVertex << T.TexUV;
		return Ar;
	}
};


// LOD-style mesh material.
struct FMeshMaterial
{
	unsigned		PolyFlags;				// Surface flags.
	int				TextureIndex;			// Source texture index.
	friend FArchive& operator<<(FArchive &Ar, FMeshMaterial &M)
	{
		return Ar << M.PolyFlags << M.TextureIndex;
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


// An actor notification event associated with an animation sequence.
struct FMeshAnimNotify
{
	float			Time;					// Time to occur, 0.0-1.0.
	FName			Function;				// Name of the actor function to call.
	UObject			*NotifyObj;				//?? UAnimNotify

	friend FArchive& operator<<(FArchive &Ar, FMeshAnimNotify &N)
	{
		guard(FMeshAnimNotify<<);
		Ar << N.Time << N.Function;
#if SPLINTER_CELL
		if (Ar.IsSplinterCell())
		{
			FName Obj;
			Ar << Obj;						// instead of UObject*
		}
#endif
		if (Ar.ArVer > 111)
			Ar << N.NotifyObj;
		return Ar;
		unguard;
	}
};


// Information about one animation sequence associated with a mesh,
// a group of contiguous frames.
struct FMeshAnimSeq
{
	FName					Name;			// Sequence's name.
	TArray<FName>			Groups;			// Group.
	int						StartFrame;		// Starting frame number.
	int						NumFrames;		// Number of frames in sequence.
	float					Rate;			// Playback rate in frames per second.
	TArray<FMeshAnimNotify> Notifys;		// Notifications.
	float					f28;			//??
	friend FArchive& operator<<(FArchive &Ar, FMeshAnimSeq &A)
	{
		if (Ar.ArVer > 114)
			Ar << A.f28;
		if (Ar.ArVer < 100)
		{
			// UE1 support
			FName tmpGroup;					// single group
			return Ar << A.Name << tmpGroup << A.StartFrame << A.NumFrames << A.Notifys << A.Rate;
		}
		Ar << A.Name << A.Groups << A.StartFrame << A.NumFrames << A.Notifys << A.Rate;
#if SPLINTER_CELL
		if (Ar.IsSplinterCell())
		{
			byte unk;
			Ar << unk;
		}
#endif
		return Ar;
	}
};


// Base class for UVertMesh and USkeletalMesh; in Unreal Engine it is derived from
// abstract class UMesh (which is derived from UPrimitive)

/*
 * Possible versions:
 *	1	SplinterCell		LodMesh is same as UT, SkeletalMesh modified
 *	2	Postal 2			same as UT
 *	4	UT2003, UT2004
 */

class ULodMesh : public UPrimitive
{
	DECLARE_CLASS(ULodMesh, UPrimitive);
public:
	unsigned			AuthKey;			// used in USkeletalMesh only?
	int					Version;			// UT2 have '4' in this field, SplinterCell have '1'
	int					VertexCount;
	TArray<FMeshVert>	Verts;				// UVertMesh: NumFrames*NumVerts, USkeletalMesh: empty
	TArray<UMaterial*>	Textures;			// skins of mesh parts
	FVector				MeshScale;
	FVector				MeshOrigin;
	FRotator			RotOrigin;
	TArray<word>		FaceLevel;
	TArray<FMeshFace>	Faces;				// empty for USkeletalMesh
	TArray<word>		CollapseWedgeThus;	// ...
	TArray<FMeshWedge>	Wedges;				// ...
	TArray<FMeshMaterial> Materials;
	float				MeshScaleMax;
	float				LODStrength;
	int					LODMinVerts;
	float				LODMorph;
	float				LODZDisplace;
	float				LODHysteresis;
	// version 3 fields
	int					HasImpostor;		// sprite object to replace mesh when distance too far
	UMaterial*			SpriteMaterial;
	FVector				ImpScale;			// Impostor.Scale3D
	FRotator			ImpRotation;		// Impostor.RelativeRotation
	FVector				ImpLocation;		// Impostor.RelativeLocation
	FColor				ImpColor;
	int					ImpSpaceMode;		// EImpSpaceMode
	int					ImpDrawMode;		// EImpDrawMode
	int					ImpLightMode;		// EImpLightMode
	// version 4 fields
	float				SkinTesselationFactor; // LOD.SkinTesselationFactor

	virtual void Serialize(FArchive &Ar)
	{
		guard(ULodMesh::Serialize);
		Super::Serialize(Ar);

		Ar << Version << VertexCount << Verts;

		if (Version <= 1)
		{
			// skip FMeshTri section
			TArray<FMeshTri> tmp;
			Ar << tmp;
		}

		Ar << Textures << MeshScale << MeshOrigin << RotOrigin;

		if (Version <= 1)
		{
			// skip 2nd obsolete section
			TArray<word> tmp;
			Ar << tmp;
		}

		Ar << FaceLevel << Faces << CollapseWedgeThus << Wedges << Materials;
		Ar << MeshScaleMax << LODHysteresis << LODStrength << LODMinVerts << LODMorph << LODZDisplace;

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

		unguard;
	}

	void SerializeLodMesh1(FArchive &Ar, TArray<FMeshAnimSeq> &AnimSeqs, TArray<FBox> &BoundingBoxes,
		TArray<FSphere> &BoundingSpheres, int &FrameCount)
	{
		guard(SerializeLodMesh1);

		// UE1: different layout
		TLazyArray<FMeshVert>			tmpVerts;
		TLazyArray<FMeshTri>			tmpTris;
		TLazyArray<FMeshVertConnect>	tmpConnects;
		TLazyArray<int>					tmpVertLinks;
		float							tmpTextureLOD_65;	// version 65
		TArray<float>					tmpTextureLOD;		// version 66+
		unsigned						tmpAndFlags, tmpOrFlags;
		int								tmpCurPoly, tmpCurVertex;
		TArray<word>					tmpCollapsePointThus;
		TArray<FMeshWedge1>				tmpWedges;
		TArray<FMeshFace>				tmpSpecialFaces;
		int								tmpModelVerts, tmpSpecialVerts;
		TArray<word>					tmpRemapAnimVerts;
		int								tmpOldFrameVerts;

		// UPrimitive
		UPrimitive::Serialize(Ar);
		// UMesh
#if !DEUS_EX
		Ar << tmpVerts;
#else
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
//			appNotify("***> size=%g numVerts=%d", (float)(skipPos - Ar.ArPos) / numVerts, numVerts);
			if (skipPos - Ar.ArPos == numVerts * sizeof(FMeshVertDeus))
				isDeusEx = true;
			Ar.Seek(pos);									// and restore position for serialization as TLazyArray
		}
		if (!isDeusEx)
			Ar << tmpVerts;									// regular Unreal1 model
		else
			Ar << deusVerts;
#endif // DEUS_EX
		Ar << tmpTris << AnimSeqs << tmpConnects;
		Ar << BoundingBox << BoundingSphere;	// serialize UPrimitive fields again
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
			COPY_ARRAY(deusVerts, tmpVerts);
		}
#endif // DEUS_EX
		if (Ar.ArVer == 65)
			Ar << tmpTextureLOD_65;
		else if (Ar.ArVer >= 66)
			Ar << tmpTextureLOD;
		// ULodMesh
		Ar << tmpCollapsePointThus << FaceLevel << Faces << CollapseWedgeThus << tmpWedges;
		Ar << Materials << tmpSpecialFaces << tmpModelVerts << tmpSpecialVerts;
		Ar << MeshScaleMax << LODHysteresis << LODStrength << LODMinVerts << LODMorph << LODZDisplace;
		Ar << tmpRemapAnimVerts << tmpOldFrameVerts;
		// convert data
		COPY_ARRAY(tmpVerts,  Verts);			// TLazyArray  -> TArray
		COPY_ARRAY(tmpWedges, Wedges);			// FMeshWedge1 -> FMeshWedge
		for (int i = 0; i < Wedges.Num(); i++)	// remap wedges (skip SpecialVerts)
			Wedges[i].iVertex += tmpSpecialVerts;
		printf("spec faces: %d  verts: %d\n", tmpSpecialFaces.Num(), tmpSpecialVerts);
		if (tmpRemapAnimVerts.Num()) appNotify("RemapVerts: %d", tmpRemapAnimVerts.Num());//!!
		return;

		unguard;
	}
};


/*-----------------------------------------------------------------------------
	Vertex streams
-----------------------------------------------------------------------------*/

struct FAnimMeshVertex
{
	// geometry data
	FVector			Pos;
	FVector			Norm;
	// texturing info
	FMeshUV			Tex;

	friend FArchive& operator<<(FArchive &Ar, FAnimMeshVertex &V)
	{
		return Ar << V.Pos << V.Norm << V.Tex;
	}
};


struct FRawIndexBuffer
{
	TArray<short>	Indices;
//	int64			CacheId;
	int				Revision;

	friend FArchive& operator<<(FArchive &Ar, FRawIndexBuffer &Buf)
	{
		return Ar << Buf.Indices << Buf.Revision;
	}
};


struct FSkinVertexStream
{
	//?? unknown
//	UObject*				f4, f8;
//	int64					CacheId;
	int						Revision;
	int						f18;
	int						f1C;
	TArray<FAnimMeshVertex>	Verts;

	friend FArchive& operator<<(FArchive &Ar, FSkinVertexStream &S)
	{
		return Ar << S.Revision << S.f18 << S.f1C << S.Verts;
	}
};


#if 0
// old USkeletalMesh format
struct FAnimMeshVertexStream
{
	int						f4;			//?? unk
	TArray<FAnimMeshVertex>	Verts;
	int						f14, f18;	//?? unk
	int						Revision;
	int						f20, f24;	//?? unk
	int						PartialSize;
};
#endif


/*-----------------------------------------------------------------------------
	UVertMesh class
-----------------------------------------------------------------------------*/

class UVertMesh : public ULodMesh
{
	DECLARE_CLASS(UVertMesh, ULodMesh);
public:
	TArray<FMeshVert>		Verts2;			// empty; used ULodMesh.Verts
	TArray<FMeshNorm>		Normals;		// [NumFrames * NumVerts]
	TArray<float>			f150;			// empty?
	TArray<FMeshAnimSeq>	AnimSeqs;
	TArray<FBox>			BoundingBoxes;	// [NumFrames]
	TArray<FSphere>			BoundingSpheres;// [NumFrames]
	int						VertexCount;	// same as ULodMesh.VertexCount
	int						FrameCount;
	// FAnimVertexStream fields; used internally in UT, but serialized
	TArray<FAnimMeshVertex> AnimMeshVerts;	// empty; generated after loading
	int						StreamVersion;	// unused

	void BuildNormals()
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

	void SerializeVertMesh1(FArchive &Ar)
	{
		guard(SerializeVertMesh1);

		SerializeLodMesh1(Ar, AnimSeqs, BoundingBoxes, BoundingSpheres, FrameCount);
		VertexCount = Super::VertexCount;
		RotOrigin.Roll = -RotOrigin.Roll;
		BuildNormals();

		unguard;
	}

	virtual void Serialize(FArchive &Ar)
	{
		guard(UVertMesh::Serialize);

		if (Ar.ArVer < 100)
		{
			SerializeVertMesh1(Ar);
			return;
		}

		Super::Serialize(Ar);

		Ar << AnimMeshVerts << StreamVersion; // FAnimMeshVertexStream: may skip this (simply seek archive)
		Ar << Verts2 << f150;
		Ar << AnimSeqs << Normals;
		Ar << VertexCount << FrameCount;
		Ar << BoundingBoxes << BoundingSpheres;
		unguard;
	}
};


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
		r.W = sqrt(1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z));
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatComp2 &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z;
	}
};

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

#endif // SPLINTER_CELL


// 'Analog' animation key track (for single bone/element.)
// Either KeyPos or KeyQuat can be single/empty? entries to signify no movement at all;
// for N>1 entries there's always N keytimers available.
struct AnalogTrack
{
	unsigned		Flags;					// reserved
	TArray<FQuat>	KeyQuat;				// Orientation key track (count = 1 or KeyTime.Count)
	TArray<FVector>	KeyPos;					// Position key track (count = 1 or KeyTime.Count)
	TArray<float>	KeyTime;				// For each key, time when next key takes effect (measured from start of track)

	friend FArchive& operator<<(FArchive &Ar, AnalogTrack &A)
	{
		guard(AnalogTrack<<);
#if SPLINTER_CELL
		if (Ar.IsSplinterCell())
		{
			if (Ar.ArLicenseeVer >= 0x0D)	// compressed Quat and Time tracks
			{
				TArray<FQuatComp> KeyQuat2;
				TArray<word>      KeyTime2;
				Ar << KeyQuat2 << A.KeyPos << KeyTime2;
				// copy with conversion
				COPY_ARRAY(KeyQuat2, A.KeyQuat);
				COPY_ARRAY(KeyTime2, A.KeyTime);
				return Ar;
			}
		}
#endif // SPLINTER_CELL
		return Ar << A.Flags << A.KeyQuat << A.KeyPos << A.KeyTime;
		unguard;
	}
};


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

#endif // SPLINTER_CELL


// Individual animation; subgroup of bones with compressed animation.
// Note: SplinterCell uses MotionChunkFixedPoint and MotionChunkFloat structures
struct MotionChunk
{
	FVector					RootSpeed3D;	// Net 3d speed.
	float					TrackTime;		// Total time (Same for each track.)
	int						StartBone;		// If we're a partial-hierarchy-movement, this is the lowest bone.
	unsigned				Flags;			// Reserved

	TArray<int>				BoneIndices;	// Refbones number of Bone indices (-1 or valid one) to fast-find tracks for a particular bone.
	// Frame-less, compressed animation tracks. NumBones times NumAnims tracks in total
	TArray<AnalogTrack>		AnimTracks;		// Compressed key tracks (one for each bone)
	AnalogTrack				RootTrack;		// May or may not be used; actual traverse-a-scene root tracks for use
	// with cutscenes / special physics modes, in addition to the regular skeletal root track.

	friend FArchive& operator<<(FArchive &Ar, MotionChunk &M)
	{
		return Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags << M.BoneIndices << M.AnimTracks << M.RootTrack;
	}
};


#if SPLINTER_CELL

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

#endif // SPLINTER_CELL


// Named bone for the animating skeleton data.
// Note: bone set may slightly differ in USkeletalMesh and UMeshAnimation objects (missing bones, different order)
// - should compute a map from one bone set to another; skeleton hierarchy should be taken from mesh (may differ
// too)
struct FNamedBone
{
	FName			Name;					// Bone's name (== single 32-bit index to name)
	unsigned		Flags;					// reserved
	int				ParentIndex;			// same meaning as FMeshBone.ParentIndex; when drawing model, should
											// use bone info from mesh, not from animation (may be different)

	friend FArchive& operator<<(FArchive &Ar, FNamedBone &F)
	{
		return Ar << F.Name << F.Flags << F.ParentIndex;
	}
};


class UMeshAnimation : public UObject
{
	DECLARE_CLASS(UMeshAnimation, UObject);
public:
	int						Version;		// always zero?
	TArray<FNamedBone>		RefBones;
	TArray<MotionChunk>		Moves;
	TArray<FMeshAnimSeq>	AnimSeqs;

#if SPLINTER_CELL
	void SerializeSCell(FArchive &Ar)
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
#endif

	void Upgrade()
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

	virtual void Serialize(FArchive &Ar)
	{
		guard(UMeshAnimation.Serialize);
		Super::Serialize(Ar);
		if (Ar.ArVer >= 100)
			Ar << Version;					// no such field in UE1
		Ar << RefBones << Moves << AnimSeqs;
#if SPLINTER_CELL
		if (Ar.IsSplinterCell())
			SerializeSCell(Ar);
#endif
		if (Ar.ArVer < 100) Upgrade();		// UE1 code

		unguard;
	}
};


/*-----------------------------------------------------------------------------
	USkeletalMesh class
-----------------------------------------------------------------------------*/

//
//	Bones
//

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FQuat			Orientation;
	FVector			Position;
	float			Length;
	FVector			Size;

	friend FArchive& operator<<(FArchive &Ar, VJointPos &P)
	{
		return Ar << P.Orientation << P.Position << P.Length << P.Size;
	}
};


struct FMeshBone
{
	FName			Name;
	unsigned		Flags;
	VJointPos		BonePos;
	int				ParentIndex;			// 0 if this is the root bone.
	int				NumChildren;

	friend FArchive& operator<<(FArchive &Ar, FMeshBone &B)
	{
		return Ar << B.Name << B.Flags << B.BonePos << B.NumChildren << B.ParentIndex;
	}
};


// similar to VRawBoneInfluence, but used 'word' instead of 'int'
struct FVertInfluences
{
	float			Weight;
	word			PointIndex;
	word			BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, FVertInfluences &I)
	{
		return Ar << I.Weight << I.PointIndex << I.BoneIndex;
	}
};


struct VWeightIndex
{
	TArray<word>	BoneInfIndices;			// array of vertex indices (in Points array)
	int				StartBoneInf;			// start index in BoneInfluences array

	friend FArchive& operator<<(FArchive &Ar, VWeightIndex &I)
	{
		return Ar << I.BoneInfIndices << I.StartBoneInf;
	}
};


struct VBoneInfluence1						// Weight and vertex number
{
	word			PointIndex;
	word			BoneWeight;				// 0..63363 == 0..1

	friend FArchive& operator<<(FArchive &Ar, VBoneInfluence1 &V)
	{
		return Ar << V.PointIndex << V.BoneWeight;
	}
};


struct VBoneInfluence						// Weight and bone number
{
	word			BoneWeight;				// 0..65535 == 0..1
	word			BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, VBoneInfluence &V)
	{
		return Ar << V.BoneWeight << V.BoneIndex;
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


//
//	Collision data
//

// MEPBonePrimSphere from MeshEditProps.uc
struct FSkelBoneSphere
{
	FName			BoneName;
	FVector			Offset;
	float			Radius;
	int				bBlockKarma;
	int				bBlockNonZeroExtent;
	int				bBlockZeroExtent;

	friend FArchive& operator<<(FArchive &Ar, FSkelBoneSphere &B)
	{
		Ar << B.BoneName << B.Offset << B.Radius;
		if (Ar.IsLoading)
		{
			B.bBlockKarma = B.bBlockNonZeroExtent = B.bBlockZeroExtent = 1;
		}
		if (Ar.ArVer > 123)
			Ar << B.bBlockKarma;
		if (Ar.ArVer > 124)
			Ar << B.bBlockZeroExtent << B.bBlockNonZeroExtent;
		return Ar;
	}
};


// MEPBonePrimBox from MeshEditProps.uc
struct FSkelBoneBox
{
	FName			BoneName;
	FVector			Offset;
	FVector			Radii;
	int				bBlockKarma;
	int				bBlockNonZeroExtent;
	int				bBlockZeroExtent;

	friend FArchive& operator<<(FArchive &Ar, FSkelBoneBox &B)
	{
		Ar << B.BoneName << B.Offset << B.Radii;
		if (Ar.IsLoading)
		{
			B.bBlockKarma = B.bBlockNonZeroExtent = B.bBlockZeroExtent = 1;
		}
		if (Ar.ArVer > 123)
			Ar << B.bBlockKarma;
		if (Ar.ArVer > 124)
			Ar << B.bBlockZeroExtent << B.bBlockNonZeroExtent;
		return Ar;
	}
};


//
//	Mesh
//

// Textured triangle.
// This is an extended FMeshFace structure
struct VTriangle
{
	word			WedgeIndex[3];			// Point to three vertices in the vertex list.
	byte			MatIndex;				// Materials can be anything.
	byte			AuxMatIndex;			// Second material (unused).
	unsigned		SmoothingGroups;		// 32-bit flag for smoothing groups.

	friend FArchive& operator<<(FArchive &Ar, VTriangle &T)
	{
		Ar << T.WedgeIndex[0] << T.WedgeIndex[1] << T.WedgeIndex[2];
		Ar << T.MatIndex << T.AuxMatIndex << T.SmoothingGroups;
		return Ar;
	}
};


struct FSkinPoint
{
	FVector			Point;
	FMeshNorm		Normal;

	friend FArchive& operator<<(FArchive &Ar, FSkinPoint &P)
	{
		return Ar << P.Point << P.Normal;
	}
};


struct FSkelMeshSection
{
	word			MaterialIndex;
	word			MinStreamIndex;			// rigid section only
	word			MinWedgeIndex;
	word			MaxWedgeIndex;
	word			NumStreamIndices;		// rigid section only
//	word			fA;						// not serialized, not used
	word			BoneIndex;				// rigid section only
	word			fE;						// serialized, not used
	word			FirstFace;
	word			NumFaces;
	// Rigid sections:
	//	MinWedgeIndex/MaxWedgeIndex -> FStaticLODModel.Wedges
	//	NumStreamIndices = NumFaces*3 == MaxWedgeIndex-MinWedgeIndex+1
	//	MinStreamIndex/NumStreamIndices -> FStaticLODModel.StaticIndices.Indices
	//	MinWedgeIndex -> FStaticLODModel.VertexStream.Verts
	// Smooth sections:
	//	NumStreamIndices should contain MaxWedgeIndex-MinWedgeIndex+1 (new imported meshes?),
	//	but older package versions contains something different (should not rely on this field)
	//	Other fields are unitialized

	friend FArchive& operator<<(FArchive &Ar, FSkelMeshSection &S)
	{
		Ar << S.MaterialIndex << S.MinStreamIndex << S.MinWedgeIndex << S.MaxWedgeIndex << S.NumStreamIndices;
		Ar << S.BoneIndex << S.fE << S.FirstFace << S.NumFaces;
		return Ar;
	}
};


struct FStaticLODModel
{
	TArray<unsigned>		f0;				//?? floating stream format, contains U/V, weights etc
	TArray<FSkinPoint>		SkinPoints;		// smooth surface points
	int						NumDynWedges;	// number of wedges in smooth sections
	TArray<FSkelMeshSection> SmoothSections;
	TArray<FSkelMeshSection> RigidSections;
	FRawIndexBuffer			SmoothIndices;
	FRawIndexBuffer			RigidIndices;
	FSkinVertexStream		VertexStream;	// for rigid parts
	TLazyArray<FVertInfluences> VertInfluences;
	TLazyArray<FMeshWedge>	Wedges;
	TLazyArray<FMeshFace>	Faces;
	TLazyArray<FVector>		Points;			// all surface points
	float					LODDistanceFactor;
	float					LODHysteresis;
	int						NumSharedVerts;	// number of verts, which is used in more than one wedge
	int						LODMaxInfluences;
	int						f114;
	int						f118;
	// have another params: float ReductionError, int Coherence

	friend FArchive& operator<<(FArchive &Ar, FStaticLODModel &M)
	{
		guard(FStaticLODModel<<);

		Ar << M.f0 << M.SkinPoints << M.NumDynWedges;
		Ar << M.SmoothSections << M.RigidSections << M.SmoothIndices << M.RigidIndices;
		Ar << M.VertexStream;
		Ar << M.VertInfluences << M.Wedges << M.Faces << M.Points;
		Ar << M.LODDistanceFactor << M.LODHysteresis << M.NumSharedVerts;
		Ar << M.LODMaxInfluences << M.f114 << M.f118;
		return Ar;

		unguard;
	}
};


// old USkeletalMesh
struct FLODMeshSection
{
	short					f0, f2, f4;
	short					LastIndex;
	short					f8, fA, fC;
	short					iFace;
	short					f10;
//	FAnimMeshVertexStream	VertStream;
//	FRawIndexBuffer			IndexBuffer;

	friend FArchive& operator<<(FArchive &Ar, FLODMeshSection &S)
	{
		return Ar << S.f0 << S.f2 << S.f4 << S.LastIndex <<
					 S.f8 << S.fA << S.fC << S.iFace << S.f10;
	}
};


#if SPLINTER_CELL

struct FSCellUnk1
{
	int						f0, f4, f8, fC;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk1 &S)
	{
		return Ar << S.f0 << S.f4 << S.f8 << S.fC;
	}
};

struct FSCellUnk2
{
	int						f0, f4, f8, fC, f10;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk2 &S)
	{
		return Ar << S.f0 << S.f4 << S.f10 << S.f8 << S.fC;
	}
};

struct FSCellUnk3
{
	int						f0, f4, f8, fC;

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk3 &S)
	{
		return Ar << S.f0 << S.fC << S.f4 << S.f8;
	}
};

struct FSCellUnk4a
{
	FVector					f0;
	FVector					fC;
	int						f18;			// float?

	friend FArchive& operator<<(FArchive &Ar, FSCellUnk4a &S)
	{
		return Ar << S.f0 << S.fC << S.f18;
	}
};

struct FSCellUnk4
{
	int						Size;
	int						Count;
	TArray<FString>			BoneNames;		// BoneNames.Num() == Count
	FString					f14;
	FSCellUnk4a**			Data;			// (FSCellUnk4a*)[Count][Size]

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

#endif


class USkeletalMesh : public ULodMesh
{
	DECLARE_CLASS(USkeletalMesh, ULodMesh);
public:
	TLazyArray<FVector>		Points;			// note: have ULodMesh.Verts
	TLazyArray<FMeshWedge>	Wedges;			// note: have ULodMesh.Wedges
	TLazyArray<VTriangle>	Triangles;
	TLazyArray<FVertInfluences> VertInfluences;
	TLazyArray<word>		CollapseWedge;	// Num == Wedges.Num; used to automatically build StaticLODModels
	TLazyArray<word>		f1C8;			// Num == Triangles.Num, element = index in Points array; not used?
	TArray<FMeshBone>		RefSkeleton;
	int						SkeletalDepth;
	TArray<FStaticLODModel>	LODModels;
	TArray<FVector>			Points2;		// used for Version=1 only
	TArray<VWeightIndex>	WeightIndices;	// empty
	TArray<VBoneInfluence>	BoneInfluences;
	UMeshAnimation*			Animation;
	UObject*				f224;			//?? always NULL (seen in IDA)
	// AttachSocket data
	TArray<FName>			AttachAliases;
	TArray<FName>			AttachBoneNames;
	TArray<FCoords>			AttachCoords;
	// collision data
	TArray<FSkelBoneSphere>	BoundingSpheres;
	TArray<FSkelBoneBox>	BoundingBoxes;
	TArray<UObject*>		f32C;			// TArray<UModel*>; collision models??
	UObject*				CollisionMesh;	// UStaticMesh*
	UObject*				KarmaProps;		// UKMeshProps*

	void UpgradeFaces()
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

	void UpgradeMesh()
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

	void SerializeSkelMesh1(FArchive &Ar)
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

	virtual void Serialize(FArchive &Ar)
	{
		guard(USkeletalMesh::Serialize);

		if (Ar.ArVer < 100)
		{
			SerializeSkelMesh1(Ar);
			return;
		}

		Super::Serialize(Ar);
		Ar << Points2 << RefSkeleton << Animation;
		Ar << SkeletalDepth << WeightIndices << BoneInfluences;
		Ar << AttachAliases << AttachBoneNames << AttachCoords;
		if (Version <= 1)
		{
#if SPLINTER_CELL
			if (Ar.IsSplinterCell())
			{
				TArray<FSCellUnk1> tmp1;
				TArray<FSCellUnk2> tmp2;
				TArray<FSCellUnk3> tmp3;
				TArray<FLODMeshSection> tmp4, tmp5;
				TArray<word> tmp6;
				FSCellUnk4 complex;
				Ar << tmp1 << tmp2 << tmp3 << tmp4 << tmp5 << tmp6 << complex;
			}
			else
#endif
			{
				appNotify("SkeletalMesh of version %d\n", Version);
				TArray<FLODMeshSection> tmp1, tmp2;
				TArray<word> tmp3;
				Ar << tmp1 << tmp2 << tmp3;
			}
			// copy and convert data from old mesh format
			UpgradeMesh();
		}
		else
		{
			Ar << LODModels << f224 << Points << Wedges << Triangles << VertInfluences;
			Ar << CollapseWedge << f1C8;
		}

		if (Ar.ArVer > 119)
		{
			Ar << AuthKey;
		}
		if (Ar.ArVer > 121)
		{
			Ar << KarmaProps << BoundingSpheres << BoundingBoxes << f32C;
		}
		if (Ar.ArVer > 126)
		{
			Ar << CollisionMesh;
		}

		unguard;
	}
};


// Note: we have registered UVertMesh as ULodMesh too for UE1 compatibility
#define REGISTER_MESH_CLASSES		\
	REGISTER_CLASS(USkeletalMesh)	\
	REGISTER_CLASS(UVertMesh)		\
	REGISTER_CLASS_ALIAS(UVertMesh, ULodMesh) \
	REGISTER_CLASS(UMeshAnimation)	\
	REGISTER_CLASS_ALIAS(UMeshAnimation, UAnimation)


#endif // __UNMESH_H__
