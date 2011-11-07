#ifndef __TYPECONVERT_H__
#define __TYPECONVERT_H__

/*-----------------------------------------------------------------------------
	Compatibility between Core and UnCore math types
-----------------------------------------------------------------------------*/

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

SIMPLE_TYPE(CVec3, float)
SIMPLE_TYPE(CQuat, float)


#endif // __TYPECONVERT_H__
