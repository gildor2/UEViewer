#ifndef __TYPECONVERT_H__
#define __TYPECONVERT_H__

/*-----------------------------------------------------------------------------
	Compatibility between Core and UnCore math types
-----------------------------------------------------------------------------*/

struct FMeshUVFloat;
struct CMeshUVFloat;

FORCEINLINE CVec3& CVT(FVector &V)
{
	return (CVec3&)V;
}

FORCEINLINE const CVec3& CVT(const FVector &V)
{
	return (const CVec3&)V;
}

FORCEINLINE CQuat& CVT(FQuat &Q)
{
	return (CQuat&)Q;
}

FORCEINLINE const CQuat& CVT(const FQuat &Q)
{
	return (const CQuat&)Q;
}

FORCEINLINE CCoords& CVT(FCoords &C)
{
	return (CCoords&)C;
}

FORCEINLINE const CCoords& CVT(const FCoords &C)
{
	return (const CCoords&)C;
}

FORCEINLINE CMeshUVFloat& CVT(FMeshUVFloat &UV)
{
	return (CMeshUVFloat&)UV;
}

FORCEINLINE const CMeshUVFloat& CVT(const FMeshUVFloat &UV)
{
	return (const CMeshUVFloat&)UV;
}

FORCEINLINE TArray<CVec3>& CVT(TArray<FVector> &Arr)
{
	return (TArray<CVec3>&)Arr;
}

FORCEINLINE const TArray<CVec3>& CVT(const TArray<FVector> &Arr)
{
	return (const TArray<CVec3>&)Arr;
}

FORCEINLINE TArray<CQuat>& CVT(TArray<FQuat> &Arr)
{
	return (TArray<CQuat>&)Arr;
}

FORCEINLINE const TArray<CQuat>& CVT(const TArray<FQuat> &Arr)
{
	return (const TArray<CQuat>&)Arr;
}

FORCEINLINE TArray<CCoords>& CVT(TArray<FCoords> &Arr)
{
	return (TArray<CCoords>&)Arr;
}

FORCEINLINE const TArray<CCoords>& CVT(const TArray<FCoords> &Arr)
{
	return (const TArray<CCoords>&)Arr;
}

SIMPLE_TYPE(CVec3, float)
SIMPLE_TYPE(CQuat, float)
SIMPLE_TYPE(CCoords, float)


#endif // __TYPECONVERT_H__
