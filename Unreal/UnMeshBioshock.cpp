#include "Core.h"
#include "UnrealClasses.h"
#include "UnMesh2.h"
#include "UnMeshTypes.h"		// for FPackedNormal
#include "UnHavok.h"

#include "UnMaterial2.h"		// serialize UMaterial*

#include "MeshCommon.h"			// CPackedNormal stuff
#include "TypeConvert.h"		// CVT macros

#if BIOSHOCK

#define XBOX_HACK	1			// disable XBox360 Havok parsing (endianness problems)

/*-----------------------------------------------------------------------------
	Bioshock 1 & 2 Havok structures
	Mostly for Havok-4.1.0-r1
-----------------------------------------------------------------------------*/

struct hkBone
{
	const char				*m_name;
	hkBool					m_lockTranslation;
};

struct hkSkeleton
{
	const char				*m_name;
	hkInt16					*m_parentIndices;
	hkInt32					m_numParentIndices;
	hkBone					**m_bones;
	hkInt32					m_numBones;
	hkQsTransform			*m_referencePose;
	hkInt32					m_numReferencePose;
};

struct hkaSkeleton5 : hkSkeleton		// hkaSkeleton
{
	char					**m_floatSlots;
	hkInt32					m_numFloatSlots;
	struct hkaSkeletonLocalFrameOnBone* m_localFrames;
	hkInt32					m_numLocalFrames;
};

// classes without implementation
class hkRagdollInstance;
class hkSkeletonMapper;
class ap4AnimationPackageAnimation;

struct ap4AnimationPackageRoot
{
	hkSkeleton				*m_highBoneSkeleton;
	hkRagdollInstance		*m_masterRagdollInstance;
	hkSkeletonMapper		*m_highToLowBoneSkeletonMapper;
	hkSkeletonMapper		*m_lowToHighBoneSkeletonMapper;
	ap4AnimationPackageAnimation *m_animations;
	hkInt32					m_numAnimations;
};

class hkaRagdollInstance;
class hkaSkeletonMapper;
class ap5AnimationPackageGroup;

struct ap5AnimationPackageRoot
{
	hkaSkeleton5			*m_highBoneSkeleton;
	hkaRagdollInstance		*m_masterRagdollInstance;
	hkaSkeletonMapper		*m_highToLowBoneSkeletonMapper;
	hkaSkeletonMapper		*m_lowToHighBoneSkeletonMapper;
	ap5AnimationPackageGroup *m_animationGroups;
	hkInt32					m_numM_animationGroups;
};


/*-----------------------------------------------------------------------------
	Bioshock's USkeletalMesh
-----------------------------------------------------------------------------*/

//!! NOTE: Bioshock uses EXACTLY the same rigid/soft vertex data as older UE3.
//!! Should separate vertex declarations into h-file and use here
struct FRigidVertexBio	//?? same layout as FRigidVertex3
{
	FVector				Pos;
	FPackedNormal		Normal[3];
	FMeshUVFloat		UV;
	byte				BoneIndex;

	friend FArchive& operator<<(FArchive &Ar, FRigidVertexBio &V)
	{
		Ar << V.Pos;
		if (Ar.ArVer < 142)
		{
			Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
		}
		else
		{
			// Bioshock 1 Remastered
			for (int i = 0; i < 3; i++)
			{
				FVector UnpNormal;
				Ar << UnpNormal;
				CPackedNormal PN;
				Pack(PN, CVT(UnpNormal));
				V.Normal[i] = CVT(PN);
			}
		}
		Ar << V.UV;
		Ar << V.BoneIndex;
		return Ar;
	}
};

struct FSoftVertexBio	//?? same layout as FSoftVertex3 (old version)
{
	FVector				Pos;
	FPackedNormal		Normal[3];
	FMeshUVFloat		UV;
	byte				BoneIndex[4];
	byte				BoneWeight[4];

	friend FArchive& operator<<(FArchive &Ar, FSoftVertexBio &V)
	{
		Ar << V.Pos;
		if (Ar.ArVer < 142)
		{
			Ar << V.Normal[0] << V.Normal[1] << V.Normal[2];
		}
		else
		{
			// Bioshock 1 Remastered
			for (int i = 0; i < 3; i++)
			{
				FVector UnpNormal;
				Ar << UnpNormal;
				CPackedNormal PN;
				Pack(PN, CVT(UnpNormal));
				V.Normal[i] = CVT(PN);
			}
		}
		Ar << V.UV;
		for (int i = 0; i < 4; i++)
			Ar << V.BoneIndex[i] << V.BoneWeight[i];
		return Ar;
	}
};

// Bioshock 2 replacement of FSoftVertex and FRigidVertex
struct FSkelVertexBio2
{
	FVector				Pos;
	FPackedNormal		Normal[3];		// FVectorComp (FVector as 4 bytes)
	FMeshUVHalf			UV;
	int					Pad;

	friend FArchive& operator<<(FArchive &Ar, FSkelVertexBio2 &V)
	{
		Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2] << V.UV << V.Pad;
		return Ar;
	}
};

struct FSkelInfluenceBio2
{
	byte				BoneIndex[4];
	byte				BoneWeight[4];

	friend FArchive& operator<<(FArchive &Ar, FSkelInfluenceBio2 &V)
	{
		for (int i = 0; i < 4; i++)
			Ar << V.BoneIndex[i] << V.BoneWeight[i];
		return Ar;
	}
};

struct FBioshockUnk1
{
	TArray<int16>		f0;
	TArray<int16>		fC;
	TArray<int16>		f18;
	TArray<int16>		f24;
	TArray<int16>		f30;
	TArray<int16>		f3C;
	TArray<int16>		f48;
	TArray<int16>		f54;

	friend FArchive& operator<<(FArchive &Ar, FBioshockUnk1 &S)
	{
		return Ar << S.f0 << S.fC << S.f18 << S.f24 << S.f30 << S.f3C << S.f48 << S.f54;
	}
};

struct FBioshockUnk2	//?? not used
{
	int					unk[16];

	friend FArchive& operator<<(FArchive &Ar, FBioshockUnk2 &S)
	{
		for (int i = 0; i < 16; i++) Ar << S.unk[i];
		return Ar;
	}
};

struct FBioshockUnk3
{
	FName				Name;
	FVector				Pos;
	int					f14;
	byte				f18;
	UObject				*f1C;

	friend FArchive& operator<<(FArchive &Ar, FBioshockUnk3 &S)
	{
		guard(FBioshockUnk3<<);
		Ar << S.Name << S.Pos << S.f14;
		if (Ar.ArLicenseeVer >= 33)
		{
			TRIBES_HDR(Ar, 0);
			Ar << S.f18;
			if (t3_hdrSV >= 1)
				Ar << S.f1C;
		}
//		appPrintf("... UNK3 name: %s pos: %g %g %g f14: %d f18: %d\n", *S.Name, FVECTOR_ARG(S.Pos), S.f14, S.f18);
		return Ar;
		unguard;
	}
};

struct FBioshockUnk4
{
	FName				Name;			// bone name
	FVector				Pos;
	FVector				f14;
	byte				f20;
	UMaterial			*Material;

	friend FArchive& operator<<(FArchive &Ar, FBioshockUnk4 &S)
	{
		guard(FBioshockUnk4<<);
		Ar << S.Name << S.Pos << S.f14;
		if (Ar.ArLicenseeVer >= 33)
		{
			TRIBES_HDR(Ar, 0);
			Ar << S.f20;
			if (t3_hdrSV >= 1)
				Ar << S.Material;
		}
//		appPrintf("... UNK4 name: %s pos: %g %g %g f14: %g %g %g f20: %d Mat: %s\n", *S.Name, FVECTOR_ARG(S.Pos), FVECTOR_ARG(S.f14),
//			S.f20, S.Material ? S.Material->Name : "null");
		return Ar;
		unguard;
	}
};

struct FBioshockUnk5
{
	FName				Name;
	FVector				Pos;
	UObject				*f14;
	byte				f18;
	UObject				*f1C;

	friend FArchive& operator<<(FArchive &Ar, FBioshockUnk5 &S)
	{
		guard(FBioshockUnk5<<);
		Ar << S.Name << S.Pos << S.f14;
		if (Ar.ArLicenseeVer >= 33)
		{
			TRIBES_HDR(Ar, 0);
			Ar << S.f18;
			if (t3_hdrSV >= 1)
				Ar << S.f1C;
		}
//		appPrintf("... UNK5 name: %s pos: %g %g %g f18: %d\n", *S.Name, FVECTOR_ARG(S.Pos), S.f18);
		return Ar;
		unguard;
	}
};

struct FBioshockUnk7					// Bioshock 2
{
	int					Version;		// 2
	int					Size;
	int					Flags;

	friend FArchive& operator<<(FArchive &Ar, FBioshockUnk7 &S)
	{
		assert(Ar.IsLoading);
		Ar << S.Version << S.Size << S.Flags;
		Ar.Seek(Ar.Tell() + S.Size);
		return Ar;
	}
};

struct FMeshNormalBio
{
	FVector				Normal[3];

	friend FArchive& operator<<(FArchive &Ar, FMeshNormalBio &N)
	{
		return Ar << N.Normal[0] << N.Normal[1] << N.Normal[2];
	}
};

struct FStaticLODModelBio
{
	TArray<FSkelMeshSection> Sections;			// standard format, but 1 section = 1 material (rigid and soft verts
												// are placed into a single section)
	TArray<int16>			Bones;
	FRawIndexBuffer			IndexBuffer;
	TArray<FSoftVertexBio>	SoftVerts;
	TArray<FRigidVertexBio>	RigidVerts;
	TArray<FBioshockUnk1>	f58;
	float					f64;
	float					f68;
	int						f6C;
	int						f70;
	int						f74;
	int						f78;
	float					f7C;
	TLazyArray<FVertInfluence> VertInfluences;
	TLazyArray<FMeshWedge>	Wedges;
	TLazyArray<FMeshFace>	Faces;
	TLazyArray<FVector>		Points;				// all surface points
	TLazyArray<FMeshNormalBio> Normals;

	friend FArchive& operator<<(FArchive &Ar, FStaticLODModelBio &Lod)
	{
		guard(FStaticLODModelBio<<);
		TRIBES_HDR(Ar, 0);
		Ar << Lod.Sections << Lod.Bones << Lod.IndexBuffer;
		if (t3_hdrSV < 4)
		{
			// Bioshock 1
			Ar << Lod.SoftVerts << Lod.RigidVerts;
		}
		else
		{
			// Bioshock 2
			int NumRigidVerts;
			TArray<FSkelVertexBio2> Verts;
			TArray<FSkelInfluenceBio2> Infs;
			// serialize
			Ar << NumRigidVerts << Verts << Infs;
			// convert to old version
			int NumVerts = Verts.Num();
			Lod.SoftVerts.Empty(NumVerts);
			Lod.SoftVerts.AddUninitialized(NumVerts);
			for (int i = 0; i < NumVerts; i++)
			{
				const FSkelVertexBio2 &V1    = Verts[i];
				const FSkelInfluenceBio2 &V2 = Infs[i];
				FSoftVertexBio &V = Lod.SoftVerts[i];
				V.Pos = V1.Pos;
				V.Normal[0] = V1.Normal[0];
				V.Normal[1] = V1.Normal[1];
				V.Normal[2] = V1.Normal[2];
				V.UV = V1.UV;		// convert
				for (int j = 0; j < 4; j++)
				{
					V.BoneIndex[j]  = V2.BoneIndex[j];
					V.BoneWeight[j] = V2.BoneWeight[j];
				}
			}
		}
		Ar << Lod.f64 << Lod.f68 << Lod.f6C << Lod.f70 << Lod.f74 << Lod.f78;
		if (t3_hdrSV >= 1)
			Ar << Lod.f7C;						// default = -1.0f
		if (t3_hdrSV >= 3)
			Ar << Lod.f58;
		Ar << Lod.VertInfluences << Lod.Wedges << Lod.Faces << Lod.Points << Lod.Normals;
#if 0
		appPrintf("sm:%d rig:%d idx:%d bones:%d 58:%d 64:%g 68:%g 6C:%d 70:%d 74:%d 78:%d\n",
			Lod.SoftVerts.Num(), Lod.RigidVerts.Num(), Lod.IndexBuffer.Indices.Num(), Lod.Bones.Num(), Lod.f58.Num(),
			Lod.f64, Lod.f68, Lod.f6C, Lod.f70, Lod.f74, Lod.f78);
		appPrintf("inf:%d wedg:%d fac:%d pts:%d norm:%d\n", Lod.VertInfluences.Num(), Lod.Wedges.Num(), Lod.Faces.Num(), Lod.Points.Num(), Lod.Normals.Num());
		int j;
//		for (j = 0; j < Lod.Bones.Num(); j++)
//			appPrintf("Bones[%d] = %d\n", j, Lod.Bones[j]);
		for (j = 0; j < Lod.Sections.Num(); j++)
		{
			const FSkelMeshSection &S = Lod.Sections[j];
			appPrintf("... SEC[%d]:  mat=%d %d [w=%d .. %d] %d b=%d %d [f=%d + %d]\n", j,
				S.MaterialIndex, S.MinStreamIndex, S.MinWedgeIndex, S.MaxWedgeIndex,
				S.NumStreamIndices, S.BoneIndex, S.fE, S.FirstFace, S.NumFaces);
		}
#endif
		return Ar;
		unguard;
	}
};

static bool CompareCompNormals(FPackedNormal Normal1, FPackedNormal Normal2)
{
	uint32 N1 = Normal1.Data;
	uint32 N2 = Normal2.Data;
	for (int i = 0; i < 3; i++)
	{
		char b1 = N1 & 0xFF;
		char b2 = N2 & 0xFF;
		if (abs(b1 - b2) > 10) return false;
		N1 >>= 8;
		N2 >>= 8;
	}
	return true;
}

// partially based on FStaticLODModel::RestoreMesh3()
//!! convert directly to CSkeletalMesh, eliminate CompareCompNormals()
//!! also this will allow to use normals and tangents from mesh (currently dropped due to UE2 limitations)
void FStaticLODModel::RestoreMeshBio(const USkeletalMesh &Mesh, const FStaticLODModelBio &Lod)
{
	guard(FStaticLODModel::RestoreMeshBio);

	int NumVertices = Lod.SoftVerts.Num() + Lod.RigidVerts.Num();

	// prepare arrays
	TArray<FPackedNormal> PointNormals;
	Points.Empty        (NumVertices);
	PointNormals.Empty  (NumVertices);
	Wedges.Empty        (NumVertices);
	VertInfluences.Empty(NumVertices * 4);

	int Vert;

	guard(RigidVerts);
	for (Vert = 0; Vert < Lod.RigidVerts.Num(); Vert++)
	{
		const FRigidVertexBio &V = Lod.RigidVerts[Vert];
		// find the same point in previous items
		int PointIndex = -1;	// start with 0, see below
		while (true)
		{
			PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
			if (PointIndex == INDEX_NONE) break;
			if (CompareCompNormals(PointNormals[PointIndex], V.Normal[0])) break;
		}
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.AddUninitialized();
			Points[PointIndex] = V.Pos;
			PointNormals.Add(V.Normal[0]);
			// add influence
			FVertInfluence *Inf = new(VertInfluences) FVertInfluence;
			Inf->PointIndex = PointIndex;
			Inf->Weight     = 1.0f;
			Inf->BoneIndex  = Lod.Bones[V.BoneIndex];
		}
		// create wedge
		FMeshWedge *W = new(Wedges) FMeshWedge;
		W->iVertex = PointIndex;
		W->TexUV   = V.UV;
	}
	unguard;

	guard(SoftVerts);
	for (Vert = 0; Vert < Lod.SoftVerts.Num(); Vert++)
	{
		const FSoftVertexBio &V = Lod.SoftVerts[Vert];
		// find the same point in previous items
		int PointIndex = -1;	// start with 0, see below
		while (true)
		{
			PointIndex = Points.FindItem(V.Pos, PointIndex + 1);
			if (PointIndex == INDEX_NONE) break;
			if (CompareCompNormals(PointNormals[PointIndex], V.Normal[0])) break;
			//?? should compare influences too !
		}
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.AddUninitialized();
			Points[PointIndex] = V.Pos;
			PointNormals.Add(V.Normal[0]);
			// add influences
//			appPrintf("point[%d]: %d/%d  %d/%d  %d/%d  %d/%d\n", PointIndex, V.BoneIndex[0], V.BoneWeight[0],
//				V.BoneIndex[1], V.BoneWeight[1], V.BoneIndex[2], V.BoneWeight[2], V.BoneIndex[3], V.BoneWeight[3]);
			for (int i = 0; i < 4; i++)
			{
				int BoneIndex  = V.BoneIndex[i];
				int BoneWeight = V.BoneWeight[i];
				if (BoneWeight == 0) continue;
				FVertInfluence *Inf = new(VertInfluences) FVertInfluence;
				Inf->PointIndex = PointIndex;
				Inf->Weight     = BoneWeight / 255.0f;
				Inf->BoneIndex  = Lod.Bones[BoneIndex];
			}
		}
		// create wedge
		FMeshWedge *W = new(Wedges) FMeshWedge;
		W->iVertex = PointIndex;
		W->TexUV   = V.UV;
	}
	unguard;

	// count triangles (speed optimization for TArray allocations)
	int NumTriangles = 0;
	int Sec;
	for (Sec = 0; Sec < Lod.Sections.Num(); Sec++)
		NumTriangles += Lod.Sections[Sec].NumFaces;
	Faces.Empty(NumTriangles);
	// create faces
	for (Sec = 0; Sec < Lod.Sections.Num(); Sec++)
	{
		const FSkelMeshSection &S = Lod.Sections[Sec];
		FSkelMeshSection *Dst = new (SoftSections) FSkelMeshSection;
		int MaterialIndex = S.MaterialIndex;
		Dst->MaterialIndex = MaterialIndex;
		Dst->FirstFace     = Faces.Num();
		Dst->NumFaces      = S.NumFaces;
//		appPrintf("3a : %d/%d+%d faces, %d indices\n", S.FirstFace, Dst->FirstFace, S.NumFaces, Lod.IndexBuffer.Indices.Num());
		int Index = S.FirstFace * 3; //?? Dst->FirstFace * 3;
#if 1
		//?? Bug in few Bioshock meshes: not enough indices (usually 12 indices = 4 faces)
		//?? In that case, Dst->FirstFace == S.FirstFace - 4 (incorrect S.FirstFace value ?)
		//?? Possible solution: use "Index = Dst->FirstFace * 3" in code above
		if (Index + Dst->NumFaces * 3 > Lod.IndexBuffer.Indices.Num())
		{
			int cut = Index + Dst->NumFaces * 3 - Lod.IndexBuffer.Indices.Num();
			appNotify("Bioshock LOD: missing %d indices", cut);
			Dst->NumFaces -= cut / 3;
		}
#endif
		for (int i = 0; i < Dst->NumFaces; i++)
		{
			FMeshFace *Face = new (Faces) FMeshFace;
			Face->MaterialIndex = MaterialIndex;
			Face->iWedge[0] = Lod.IndexBuffer.Indices[Index++];
			Face->iWedge[1] = Lod.IndexBuffer.Indices[Index++];
			Face->iWedge[2] = Lod.IndexBuffer.Indices[Index++];
		}
	}

	unguard;
}


void USkeletalMesh::SerializeBioshockMesh(FArchive &Ar)
{
	guard(USkeletalMesh::SerializeBioshock);

	UPrimitive::Serialize(Ar);				// Bounding box and sphere
	TRIBES_HDR(Ar, 0);

	float					unk90, unk94;
	int						unk98, unk9C;	//?? 9C type is unknown (float/int)
	int						f68;
	TArray<FStaticLODModelBio> bioLODModels;
	TLazyArray<FMeshNormalBio> bioNormals;
	UObject					*fCC;			//??
	TArray<FBioshockUnk3>	f104;			// count = num bones or 0
	TArray<FBioshockUnk4>	f110;			// count = num bones or 0
	TArray<FBioshockUnk5>	f11C;			// count = num bones or 0
	TArray<UObject*>		f128;			// TArray<UModel*>, count = num bones or 0
	TArray<FVector>			f134;			// count = num bones or 0
	TArray<FName>			f14C;
	UObject					*HkMeshProxy;	// UHkMeshProxy* - Havok data
	TArray<FName>			drop1;

	Ar << Textures << MeshScale << MeshOrigin << RotOrigin;
	Ar << unk90 << unk94 << unk98 << unk9C;
	Ar << RefSkeleton << Animation << SkeletalDepth << AttachAliases << AttachBoneNames << AttachCoords;
	Ar << bioLODModels;
	Ar << fCC;
	Ar << Points << Wedges << Triangles << VertInfluences;	//?? check: Bio1_Remastered serializes FMeshFace instead of VTriangle here (see 'Triangles')
	Ar << CollapseWedge << f1C8;
	Ar << bioNormals;

	SkipLazyArray(Ar);	// FMeshFace	f254
	SkipLazyArray(Ar);	// FMeshWedge	f234
	SkipLazyArray(Ar);	// int16		f274

	if (t3_hdrSV >= 6 && Ar.ArLicenseeVer != 57) // not Bioshock 2 MP
	{
		// Bioshock 2
		FBioshockUnk7 f2A4;
		TArray<int> f2B0;
		Ar << f2A4 << f2B0;
	}

	Ar << f68 << f104 << f110 << f128 << f11C;
	Ar << HkMeshProxy;

	if (Ar.ArLicenseeVer == 57) // Bioshock 2 MP
	{
		byte unk2;
		Ar << unk2;
		assert(unk2 == 0);
	}
	if (t3_hdrSV <= 5) Ar << drop1;
	if (t3_hdrSV >= 4) Ar << havokObjects;
	if (t3_hdrSV >= 5) Ar << f134;
	// Bioshock 2
	if (t3_hdrSV >= 7) Ar << f14C;
	if (Ar.ArLicenseeVer == 57)	// Bioshock 2 MP; not verified code!
	{
		TArray<float> unk;
		Ar << unk;
	}

	// convert LODs
	int i;
	LODModels.Empty(bioLODModels.Num());
	LODModels.AddDefaulted(bioLODModels.Num());
	for (i = 0; i < bioLODModels.Num(); i++)
		LODModels[i].RestoreMeshBio(*this, bioLODModels[i]);

	// materials
	Materials.AddDefaulted(Textures.Num());
	for (i = 0; i < Materials.Num(); i++)
		Materials[i].TextureIndex = i;

#if 0
appPrintf("u104:%d u110:%d u128:%d u11C:%d dr1:%d dr2:%d f134:%d\n", f104.Num(), f110.Num(), f128.Num(), f11C.Num(), drop1.Num(), havokObjects.Num(), f134.Num());
for (i=0;i<drop1.Num();i++)appPrintf("drop[%d]=%s\n",i,*drop1[i]);
for (int j = 0; j < 10; j++)
{
for (i = 0; i < 16; i++)
{
byte b; Ar << b;
appPrintf("  %02X",b);
}
appPrintf("\n");
}
appPrintf("skel: %d (depth=%d)\n", RefSkeleton.Num(), SkeletalDepth);
for (i = 0; i < RefSkeleton.Num(); i++) appPrintf("%d: %s\n", i, *RefSkeleton[i].Name);
#endif

	unguard;
}


void USkeletalMesh::PostLoadBioshockMesh()
{
	guard(USkeletalMesh::PostLoadBioshockMesh);

	if (RefSkeleton.Num())
	{
//		appNotify("Bioshock mesh %s has RefSkeleton!", Name);
		// do the conversion after skeleton loading
		ConvertMesh();
		return;
	}

	int i;
	// find UAnimationPackageWrapper object
	const UAnimationPackageWrapper *AW = NULL;
	for (i = 0; i < havokObjects.Num(); i++)
	{
		UObject *Obj = havokObjects[i];
		if (!Obj) continue;
		if (Obj->IsA("AnimationPackageWrapper"))
		{
			AW = (const UAnimationPackageWrapper*)Obj;
			break;
		}
	}
	assert(AW);

	void *pObject;
	const char *ClassName;
	GetHavokPackfileContents(&AW->HavokData[0], &pObject, &ClassName);
	if (!strcmp(ClassName, "ap4AnimationPackageRoot"))
	{
		// Bioshock 1
		ap4AnimationPackageRoot *Object = (ap4AnimationPackageRoot*)pObject;
		// convert skeleton
		const hkSkeleton *Skel = Object->m_highBoneSkeleton;
//		assert(RefSkeleton.Num() == 0);
		RefSkeleton.AddDefaulted(Skel->m_numBones);
		for (i = 0; i < Skel->m_numBones; i++)
		{
			FMeshBone &B = RefSkeleton[i];
			B.Name.Str    = Skel->m_bones[i]->m_name;				//?? hack: FName assignment
			B.ParentIndex = max(Skel->m_parentIndices[i], (hkInt16)0);
			const hkQsTransform &t = Skel->m_referencePose[i];
			B.BonePos.Orientation = (FQuat&)   t.m_rotation;
			B.BonePos.Position    = (FVector&) t.m_translation;
			B.BonePos.Orientation.W *= -1;
//			if (!i) (CVT(B.BonePos.Orientation)).Conjugate();	-- not needed for Bioshock 1
		}
	}
	else if (!strcmp(ClassName, "ap5AnimationPackageRoot"))
	{
		// Bioshock 2
		ap5AnimationPackageRoot *Object = (ap5AnimationPackageRoot*)pObject;
		// convert skeleton
		const hkaSkeleton5 *Skel = Object->m_highBoneSkeleton;
//		assert(RefSkeleton.Num() == 0);
		RefSkeleton.AddDefaulted(Skel->m_numBones);
		for (i = 0; i < Skel->m_numBones; i++)
		{
			FMeshBone &B = RefSkeleton[i];
			B.Name.Str    = Skel->m_bones[i]->m_name;				//?? hack: FName assignment
			B.ParentIndex = max(Skel->m_parentIndices[i], (hkInt16)0);
			const hkQsTransform &t = Skel->m_referencePose[i];
			B.BonePos.Orientation = (FQuat&)   t.m_rotation;
			B.BonePos.Position    = (FVector&) t.m_translation;
			B.BonePos.Orientation.W *= -1;
			if (!i) (CVT(B.BonePos.Orientation)).Conjugate();	// needed for Bioshock 2
		}
	}
	else
	{
		appError("Unknown Havok class: %s", ClassName);
	}

	// do the conversion after skeleton loading
	ConvertMesh();

	unguard;
}


void UAnimationPackageWrapper::Process()
{
	guard(UAnimationPackageWrapper::Process);
#if XBOX_HACK
	if (GetPackageArchive()->Platform == PLATFORM_XBOX360) return;
#endif
	FixupHavokPackfile(Name, &HavokData[0]);
#if 0
	//?? testing
	ap4AnimationPackageRoot *Object;
	const char *ClassName;
	GetHavokPackfileContents(&HavokData[0], (void**)&Object, &ClassName);
	assert(!strcmp(ClassName, "ap4AnimationPackageRoot"));
	const hkSkeleton *Skel = Object->m_highBoneSkeleton;
	appPrintf("skel=%s\n", Skel->m_name);
	for (int i = 0; i < Skel->m_numBones; i++)
	{
		appPrintf("[%d] %s (parent=%d)\n", i, Skel->m_bones[i]->m_name, Skel->m_parentIndices[i]);
		hkQsTransform &t = Skel->m_referencePose[i];
		appPrintf("   {%g %g %g} - {%g %g %g %g} / {%g %g %g}\n", FVECTOR_ARG(t.m_translation), FQUAT_ARG(t.m_rotation), FVECTOR_ARG(t.m_scale));
	}
#endif
	unguard;
}


// Bioshock 1
struct FStaticMeshVertexBio
{
	FVector					Pos;
	FPackedNormal			Normal[3];

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexBio &V)
	{
		return Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2];
	}

	operator FStaticMeshVertex() const
	{
		FStaticMeshVertex r;
		r.Pos    = Pos;
		r.Normal = Normal[2];
		return r;
	}
};

SIMPLE_TYPE(FStaticMeshVertexBio, int)

// Bioshock 1 remastered
struct FStaticMeshVertexBio1R
{
	FVector					Pos;
	FVector					Normal[3];

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexBio1R &V)
	{
		return Ar << V.Pos << V.Normal[0] << V.Normal[1] << V.Normal[2];
	}

	operator FStaticMeshVertex() const
	{
		FStaticMeshVertex r;
		r.Pos    = Pos;
		r.Normal = Normal[2];
		return r;
	}
};

SIMPLE_TYPE(FStaticMeshVertexBio1R, float)

// Bioshock 2 (normal and remastered)
struct FStaticMeshVertexBio2
{
	uint16					Pos[4];
	FPackedNormal			Normal[3];

	friend FArchive& operator<<(FArchive &Ar, FStaticMeshVertexBio2 &V)
	{
		return Ar << V.Pos[0] << V.Pos[1] << V.Pos[2] << V.Pos[3] << V.Normal[0] << V.Normal[1] << V.Normal[2];
	}

	operator FStaticMeshVertex() const
	{
		FStaticMeshVertex r;
		r.Pos.X  = half2float(Pos[0]);
		r.Pos.Y  = half2float(Pos[1]);
		r.Pos.Z  = half2float(Pos[2]);
		r.Normal = Normal[2];
		return r;
	}
};

RAW_TYPE(FStaticMeshVertexBio2)


void UStaticMesh::SerializeBioshockMesh(FArchive &Ar)
{
	guard(UStaticMesh::SerializeBioshockMesh);

	Super::Serialize(Ar);
	TRIBES_HDR(Ar, 3);
	Ar << Sections;
	Ar << BoundingBox;			// UPrimitive field, serialized twice ...

	// serialize VertexStream
	if (t3_hdrSV < 9)
	{
		// Bioshock 1
		if (Ar.ArVer < 142)
		{
			TArray<FStaticMeshVertexBio> BioVerts;
			Ar << BioVerts;
			CopyArray(VertexStream.Vert, BioVerts);
		}
		else
		{
			// Bioshock 1 remastered
			TArray<FStaticMeshVertexBio1R> BioVerts;
			Ar << BioVerts;
			CopyArray(VertexStream.Vert, BioVerts);
		}
	}
	else
	{
		// Bioshock 2 and Bioshock 2 remastered
		TArray<FStaticMeshVertexBio2> BioVerts2;
		Ar << BioVerts2;
		CopyArray(VertexStream.Vert, BioVerts2);
	}
	// serialize UVStream
	Ar << UVStream;
	// serialize IndexStream
	Ar << IndexStream1;
	// also: t3_hdrSV < 2 => IndexStream2

	DROP_REMAINING_DATA(Ar);

	ConvertMesh();

	unguard;
}

#endif // BIOSHOCK
