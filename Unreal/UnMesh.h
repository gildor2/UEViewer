#ifndef __UNMESH_H__
#define __UNMESH_H__

/*-----------------------------------------------------------------------------
UE1 CLASS TREE:
~~~~~~~~~~~~~~~
	UObject
		UPrimitive
			UMesh
				ULodMesh
					USkeletalMesh
			USkelModel (Rune)

-----------------------------------------------------------------------------*/

//?? declare separately? place to UnCore?
float half2float(uint16 h);

struct FPackedNormal;
struct CMeshVertex;
void UnpackNormals(const FPackedNormal SrcNormal[3], CMeshVertex &V);

//?? move these declarations outside
class CSkeletalMesh;
struct CSkelMeshLod;
class CAnimSet;
class CAnimSequence;
class CStaticMesh;


//?? Eliminate GET_DWORD() macro - it could be compiler- and endian-dependent,
//?? unpack "unsigned int" manually.
//?? After that, can remove #include UnMesh.h from some places.
#define GET_DWORD(v) (*(uint32*)&(v))


/*-----------------------------------------------------------------------------
	Common mesh structures
-----------------------------------------------------------------------------*/

// corresponds to UT1 FMeshFloatUV
// UE2 name: FMeshUV
struct FMeshUVFloat
{
	float			U;
	float			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUVFloat &M)
	{
		return Ar << M.U << M.V;
	}
};

SIMPLE_TYPE(FMeshUVFloat, float)


struct FMeshUVHalf
{
	uint16			U;
	uint16			V;

	friend FArchive& operator<<(FArchive &Ar, FMeshUVHalf &V)
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

SIMPLE_TYPE(FMeshUVHalf, uint16)


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
#if UNREAL3
		if (Ar.ArVer >= 224)
		{
#if ENDWAR
			if (Ar.Game == GAME_EndWar)
			{
				// End War has W in FVector, but VJointPos will not serialize W for Position since ArVer 295
				Ar << P.Orientation << P.Position.X << P.Position.Y << P.Position.Z;
				return Ar;
			}
#endif // ENDWAR
			Ar << P.Orientation << P.Position;
#if ARMYOF2
			if (Ar.Game == GAME_ArmyOf2 && Ar.ArVer >= 481)
			{
				int pad;
				Ar << pad;
			}
#endif // ARMYOF2
			return Ar;
		}
#endif
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
		guard(FMeshBone<<);
#if XIII
		if (Ar.Game == GAME_XIII && Ar.ArLicenseeVer > 26)					// real version is unknown; beta = old code, retail = new code
			return Ar << B.Name << B.Flags << B.BonePos << B.ParentIndex;	// no NumChildren
#endif // XIII
#if BATMAN
		if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4 && Ar.ArLicenseeVer >= 31)
		{
			Ar << B.BonePos << B.Name << B.ParentIndex;						// no Flags and NumChildren fields
			goto ue3_unk;
		}
#endif // BATMAN
		Ar << B.Name << B.Flags << B.BonePos << B.NumChildren << B.ParentIndex;
#if AA2
		if (Ar.Game == GAME_AA2)
		{
			int		unk44;
			float	unk40, unk48;
			byte	unk4C;
			if (Ar.ArLicenseeVer >= 9)
				Ar << unk40 << unk44 << unk48;
			if (Ar.ArLicenseeVer >= 20)
				Ar << unk4C;
		}
#endif // AA2
#if ARMYOF2
		if (Ar.Game == GAME_ArmyOf2 && Ar.ArVer >= 459)
		{
			int unk3C;						// FColor?
			Ar << unk3C;
		}
#endif // ARMYOF2
#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers) return Ar; // version 537, but really not upgraded
#endif
#if UNREAL3
	ue3_unk:
		if (Ar.ArVer >= 515)
		{
			int unk44;						// byte[4] ? default is 0xFF x 4
			Ar << unk44;
		}
#endif // UNREAL3
		return Ar;
		unguard;
	}
};


#endif // __UNMESH_H__
