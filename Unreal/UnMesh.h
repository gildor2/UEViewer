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
float half2float(word h);

//?? move these declarations outside
class CSkeletalMesh;
struct CSkelMeshLod;
class CAnimSet;
class CStaticMesh;


#define GET_DWORD(v) (*(unsigned*)&(v))


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
	short			U;
	short			V;

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

SIMPLE_TYPE(FMeshUVHalf, word)


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
#if XIII
		if (Ar.Game == GAME_XIII && Ar.ArLicenseeVer > 26)					// real version is unknown; beta = old code, retail = new code
			return Ar << B.Name << B.Flags << B.BonePos << B.ParentIndex;	// no NumChildren
#endif // XIII
		Ar << B.Name << B.Flags << B.BonePos << B.NumChildren << B.ParentIndex;
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
		if (Ar.ArVer >= 515)
		{
			int unk44;						// byte[4] ? default is 0xFF x 4
			Ar << unk44;
		}
#endif // UNREAL3
		return Ar;
	}
};


#endif // __UNMESH_H__
