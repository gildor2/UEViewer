#ifndef __UNMESH2_H__
#define __UNMESH2_H__

/*-----------------------------------------------------------------------------

UE2 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		UPrimitive
			ULodMesh
				USkeletalMesh
				UVertMesh
		UStaticMesh

-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	UStaticMesh class
-----------------------------------------------------------------------------*/

struct FStaticMeshSection
{
	int						f4;				// always 0 ??
	word					FirstIndex;		// first index
	word					FirstVertex;	// first used vertex
	word					LastVertex;		// last used vertex
	word					fE;				// ALMOST always equals to f10
	word					NumFaces;		// number of faces in section
	// Note: UE2X uses "NumFaces" as "LastIndex"

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshSection &S)
	{
		guard(FStaticMeshSection<<);
		assert(Ar.ArVer >= 112);
		Ar << S.f4 << S.FirstIndex << S.FirstVertex << S.LastVertex << S.fE << S.NumFaces;
//		appPrintf("... f4=%d FirstIndex=%d FirstVertex=%d LastVertex=%d fE=%d NumFaces=%d\n", S.f4, S.FirstIndex, S.FirstVertex, S.LastVertex, S.fE, S.NumFaces); //!!!
		return Ar;
		unguard;
	}
};

struct FStaticMeshVertex
{
	FVector					Pos;
	FVector					Normal;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertex &V)
	{
		assert(Ar.ArVer >= 112);
		return Ar << V.Pos << V.Normal;
		// Ver < 112 -- +2 floats
		// Ver < 112 && LicVer >= 5 -- +2 floats
		// Ver == 111 -- +4 bytes (FColor?)
		//!! check SIMPLE_TYPE() possibility when modifying this function
	}
};

SIMPLE_TYPE(FStaticMeshVertex, float)		//?? check each version

#if UC2

static FVector GUC2VectorScale, GUC2VectorBase;

struct FStaticMeshVertexUC2
{
	FUC2Vector				Pos;
	FUC2Normal				Normal;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexUC2 &V)
	{
		return Ar << V.Pos << V.Normal;
	}

	operator FStaticMeshVertex() const
	{
		FStaticMeshVertex r;
		r.Pos.X = Pos.X / 32767.0f / GUC2VectorScale.X + GUC2VectorBase.X;
		r.Pos.Y = Pos.Y / 32767.0f / GUC2VectorScale.Y + GUC2VectorBase.Y;
		r.Pos.Z = Pos.Z / 32767.0f / GUC2VectorScale.Z + GUC2VectorBase.Z;
		r.Normal.Set(0, 0, 0);		//?? decode
		return r;
	}
};

RAW_TYPE(FStaticMeshVertexUC2)

#endif // UC2

struct FStaticMeshVertexStream
{
	TArray<FStaticMeshVertex> Vert;
	int						Revision;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexStream &S)
	{
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			TArray<FStaticMeshVertexUC2> UC2Verts;
			int Flag;
			Ar << Flag;
			if (!Flag)
			{
				Ar << UC2Verts;
				CopyArray(S.Vert, UC2Verts);
			}
			return Ar << S.Revision;
		}
#endif // UC2
#if TRIBES3
		TRIBES_HDR(Ar, 0xD);
#endif
		Ar << S.Vert << S.Revision;
#if TRIBES3
		if ((Ar.Game == GAME_Tribes3 || Ar.Game == GAME_Swat4) && t3_hdrSV >= 1)
		{
			TArray<T3_BasisVector> unk1C;
			Ar << unk1C;
		}
#endif // TRIBES3
		return Ar;
	}
};

struct FRawColorStream
{
	TArray<FColor>			Color;
	int						Revision;

	friend FArchive& operator<<(FArchive &Ar, FRawColorStream &S)
	{
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			int Flag;
			Ar << Flag;
			if (!Flag) Ar << S.Color;
			return Ar << S.Revision;
		}
#endif // UC2
		return Ar << S.Color << S.Revision;
	}
};


#if BIOSHOCK
struct FStaticMeshUVBio2
{
	short					U;
	short					V;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVBio2 &V)
	{
		Ar << V.U << V.V;
		return Ar;
	}

	operator FMeshUVFloat() const
	{
		FMeshUVFloat r;
		r.U = half2float(U);
		r.V = half2float(V);
		return r;
	}
};

SIMPLE_TYPE(FStaticMeshUVBio2, short)

#endif // BIOSHOCK


struct FStaticMeshUVStream
{
	TArray<FMeshUVFloat>	Data;
	int						f10;
	int						f1C;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshUVStream &S)
	{
#if BIOSHOCK
		if (Ar.Game == GAME_Bioshock)
		{
			if (Ar.ArLicenseeVer < 59)
			{
				// Bioshock 1 & Bioshock 2 MP
				Ar << S.Data;
			}
			else
			{
				// Bioshock 2 SP; real version detection code is unknown
				TArray<FStaticMeshUVBio2> UV;
				Ar << UV;
				CopyArray(S.Data, UV);
			}
			return Ar << S.f10;
		}
#endif // BIOSHOCK
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			int Flag;
			Ar << Flag;
			if (!Flag) Ar << S.Data;
			return Ar << S.f10 << S.f1C;
		}
#endif // UC2
		return Ar << S.Data << S.f10 << S.f1C;
	}
};

struct FStaticMeshTriangleUnk
{
	float					unk1[2];
	float					unk2[2];
	float					unk3[2];
};

// complex FStaticMeshTriangle structure
struct FStaticMeshTriangle
{
	FVector					f0;
	FVector					fC;
	FVector					f18;
	FStaticMeshTriangleUnk	f24[8];
	byte					fE4[12];
	int						fF0;
	int						fF4;
	int						fF8;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshTriangle &T)
	{
		guard(FStaticMeshTriangle<<);
		int i;

		assert(Ar.ArVer >= 112);
		Ar << T.f0 << T.fC << T.f18;
		Ar << T.fF8;
		assert(T.fF8 <= ARRAY_COUNT(T.f24));
		for (i = 0; i < T.fF8; i++)
		{
			FStaticMeshTriangleUnk &V = T.f24[i];
			Ar << V.unk1[0] << V.unk1[1] << V.unk2[0] << V.unk2[1] << V.unk3[0] << V.unk3[1];
		}
		for (i = 0; i < 12; i++)
			Ar << T.fE4[i];			// UT2 has strange order of field serialization: [2,1,0,3] x 3 times
		Ar << T.fF0 << T.fF4;
		// extra fields for older version (<= 111)

		return Ar;
		unguard;
	}
};

struct FkDOPNode
{
	float					unk1[3];
	float					unk2[3];
	int						unk3;		//?? index * 32 ?
	short					unk4;
	short					unk5;

	friend FArchive& operator<<(FArchive &Ar, FkDOPNode &N)
	{
		guard(FkDOPNode<<);
		int i;
		for (i = 0; i < 3; i++)
			Ar << N.unk1[i];
		for (i = 0; i < 3; i++)
			Ar << N.unk2[i];
		Ar << N.unk3 << N.unk4 << N.unk5;
		return Ar;
		unguard;
	}
};

RAW_TYPE(FkDOPNode)

struct FkDOPCollisionTriangle
{
	short					v[4];

	friend FArchive& operator<<(FArchive &Ar, FkDOPCollisionTriangle &T)
	{
		return Ar << T.v[0] << T.v[1] << T.v[2] << T.v[3];
	}
};

SIMPLE_TYPE(FkDOPCollisionTriangle, short)

struct FStaticMeshCollisionNode
{
	int						f1[4];
	float					f2[6];
	byte					f3;

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshCollisionNode &N)
	{
		int i;
		for (i = 0; i < 4; i++) Ar << AR_INDEX(N.f1[i]);
		for (i = 0; i < 6; i++) Ar << N.f2[i];
		Ar << N.f3;
		return Ar;
	}
};

struct FStaticMeshCollisionTriangle
{
	float					f1[16];
	int						f2[4];

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshCollisionTriangle &T)
	{
		int i;
		for (i = 0; i < 16; i++) Ar << T.f1[i];
		for (i = 0; i <  4; i++) Ar << AR_INDEX(T.f2[i]);
		return Ar;
	}
};

struct FStaticMeshMaterial
{
	DECLARE_STRUCT(FStaticMeshMaterial);
	UMaterial				*Material;
	bool					EnableCollision;

	BEGIN_PROP_TABLE
		PROP_OBJ(Material)
		PROP_BOOL(EnableCollision)
#if LINEAGE2
		PROP_DROP(EnableCollisionforShadow)
		PROP_DROP(bNoDynamicShadowCast)
#endif // LINEAGE2
	END_PROP_TABLE
};

class UStaticMesh : public UPrimitive
{
	DECLARE_CLASS(UStaticMesh, UPrimitive);
public:
	TArray<FStaticMeshMaterial> Materials;
	TArray<FStaticMeshSection>  Sections;
	FStaticMeshVertexStream	VertexStream;
	FRawColorStream			ColorStream1;	// for Verts
	FRawColorStream			ColorStream2;
	TArray<FStaticMeshUVStream> UVStream;
	FRawIndexBuffer			IndexStream1;
	FRawIndexBuffer			IndexStream2;
	TArray<FkDOPNode>		kDOPNodes;
	TArray<FkDOPCollisionTriangle> kDOPCollisionFaces;
	TLazyArray<FStaticMeshTriangle> Faces;
	UObject					*f108;
	int						f124;			//?? FRotator?
	int						f128;
	int						f12C;
	int						Version;		// set to 11 when saving mesh
	TArray<float>			f150;
	int						f15C;
	UObject					*f16C;
	bool					UseSimpleLineCollision;
	bool					UseSimpleBoxCollision;
	bool					UseSimpleKarmaCollision;

	CStaticMesh				*ConvertedMesh;

	BEGIN_PROP_TABLE
		PROP_ARRAY(Materials, FStaticMeshMaterial)
		PROP_BOOL(UseSimpleLineCollision)
		PROP_BOOL(UseSimpleBoxCollision)
		PROP_BOOL(UseSimpleKarmaCollision)
		PROP_DROP(UseSimpleRigidBodyCollision)
		PROP_DROP(BodySetup)				//?? UE3 only
		PROP_DROP(LODDistanceRatio)
#if LINEAGE2
		PROP_DROP(bMakeTwoSideMesh)
#endif
#if BIOSHOCK
		PROP_DROP(HavokCollisionTypeStatic)
		PROP_DROP(HavokCollisionTypeDynamic)
		PROP_DROP(UseSimpleVisionCollision)
		PROP_DROP(UseSimpleFootIKCollision)
		PROP_DROP(NeverCollide)
		PROP_DROP(SortShape)
		PROP_DROP(LightMapCoordinateIndex)
		PROP_DROP(LightMapScale)
#endif // BIOSHOCK
	END_PROP_TABLE

	virtual ~UStaticMesh();

#if UC2
	void LoadExternalUC2Data();
#endif
#if BIOSHOCK
	void SerializeBioshockMesh(FArchive &Ar);
#endif
	void ConvertMesh2();

	virtual void Serialize(FArchive &Ar)
	{
		guard(UStaticMesh::Serialize);

		assert(Ar.Game < GAME_UE3);

#if BIOSHOCK
		if (Ar.Game == GAME_Bioshock)
		{
			SerializeBioshockMesh(Ar);
			return;
		}
#endif // BIOSHOCK

		if (Ar.ArVer < 112)
		{
			appNotify("StaticMesh of old version %d/%d has been found", Ar.ArVer, Ar.ArLicenseeVer);
		skip_remaining:
			Ar.Seek(Ar.GetStopper());
			ConvertMesh2();
			return;
		}

		//!! copy common part inside BIOSHOCK and UC2 code paths
		//!! separate BIOSHOCK and UC2 code paths to cpps
		//!! separate specific data type declarations into cpps
		//!! UC2 code: can integrate LoadExternalUC2Data() into serializer
		Super::Serialize(Ar);
#if TRIBES3
		TRIBES_HDR(Ar, 3);
#endif
		Ar << Sections;
		Ar << BoundingBox;			// UPrimitive field, serialized twice ...
#if UC2
		if (Ar.Engine() == GAME_UE2X)
		{
			FVector f120, VectorScale, VectorBase;	// defaults: vec(1.0), Scale=vec(1.0), Base=vec(0.0)
			int     f154, f158, f15C, f160;
			if (Ar.ArVer >= 135)
			{
				Ar << f120 << VectorScale << f154 << f158 << f15C << f160;
				if (Ar.ArVer >= 137) Ar << VectorBase;
			}
			GUC2VectorScale = VectorScale;
			GUC2VectorBase  = VectorBase;
			Ar << VertexStream << ColorStream1 << ColorStream2 << UVStream << IndexStream1;
			if (Ar.ArLicenseeVer != 1) Ar << IndexStream2;
			//!!!!!
//			appPrintf("v:%d c1:%d c2:%d uv:%d idx1:%d\n", VertexStream.Vert.Num(), ColorStream1.Color.Num(), ColorStream2.Color.Num(),
//				UVStream.Num() ? UVStream[0].Data.Num() : -1, IndexStream1.Indices.Num());
			Ar << f108;

			LoadExternalUC2Data();

			// skip collision information
			goto skip_remaining;
		}
#endif // UC2
#if SWRC
		if (Ar.Game == GAME_RepCommando)
		{
			int f164, f160;
			Ar << VertexStream;
			if (Ar.ArVer >= 155) Ar << f164;
			if (Ar.ArVer >= 149) Ar << f160;
			Ar << ColorStream1 << ColorStream2 << UVStream << IndexStream1 << IndexStream2 << f108;
			goto skip_remaining;
		}
#endif // SWRC
		Ar << VertexStream << ColorStream1 << ColorStream2 << UVStream << IndexStream1 << IndexStream2 << f108;

#if 1
		// UT2 and UE2Runtime has very different collision structures
		// We don't need it, so don't bother serializing it
		goto skip_remaining;
#else
		if (Ar.ArVer < 126)
		{
			assert(Ar.ArVer >= 112);
			TArray<FStaticMeshCollisionTriangle> CollisionFaces;
			TArray<FStaticMeshCollisionNode>     CollisionNodes;
			Ar << CollisionFaces << CollisionNodes;
		}
		else
		{
			// this is an UT2 code, UE2Runtime has different structures
			Ar << kDOPNodes << kDOPCollisionFaces;
		}
		if (Ar.ArVer < 114)
			Ar << f124 << f128 << f12C;

		Ar << Faces;					// can skip this array
		Ar << Version;

#if UT2
		if (Ar.Game == GAME_UT2)
		{
			//?? check for generic UE2
			if (Ar.ArLicenseeVer == 22)
			{
				float unk;				// predecessor of f150
				Ar << unk;
			}
			else if (Ar.ArLicenseeVer >= 23)
				Ar << f150;
			Ar << f16C;
			if (Ar.ArVer >= 120)
				Ar << f15C;
		}
#endif // UT2
#endif // 0

		ConvertMesh2();

		unguard;
	}
};


#define REGISTER_MESH_CLASSES_U2	\
	REGISTER_CLASS(UStaticMesh)	\
	REGISTER_CLASS(FStaticMeshMaterial)

#endif // __UNMESH2_H__
