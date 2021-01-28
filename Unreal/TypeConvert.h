#ifndef __TYPE_CONVERT_H__
#define __TYPE_CONVERT_H__

/*-----------------------------------------------------------------------------
	Compatibility between Core and UnCore math types
-----------------------------------------------------------------------------*/

struct FMeshUVFloat;
struct CMeshUVFloat;
struct FPackedNormal;

// Convert Unreal type to Core type (e.g. FVector to CVec3) for cases when
// they have identical memory layout
#define CONVERTER_ONE_DIRECTION(From, To)	\
FORCEINLINE To& CVT(From &V)				\
{											\
	return (To&)V;							\
}											\
FORCEINLINE const To& CVT(const From &V)	\
{											\
	return (const To&)V;					\
}											\
FORCEINLINE To* CVT(From *V)				\
{											\
	return (To*)V;							\
}											\
FORCEINLINE const To* CVT(const From *V)	\
{											\
	return (const To*)V;					\
}

// Convert types in both directions
#define CONVERTER(From, To)					\
	CONVERTER_ONE_DIRECTION(From, To)		\
	CONVERTER_ONE_DIRECTION(To, From)

//?? combine CONVERTER(T) with CONVERTER(TArray<T>)?

CONVERTER(FVector,         CVec3          )
CONVERTER(FQuat,           CQuat          )
CONVERTER(FCoords,         CCoords        )
CONVERTER(FMeshUVFloat,    CMeshUVFloat   )
#if UNREAL3
CONVERTER_ONE_DIRECTION(FVector2D, CMeshUVFloat)
#endif
CONVERTER(TArray<FVector>, TArray<CVec3>  )
CONVERTER(TArray<FQuat>,   TArray<CQuat>  )
CONVERTER(TArray<FCoords>, TArray<CCoords>)

#undef CONVERTER


#if defined(__MESH_COMMON_H__) && defined(__UNMESH_TYPES_H__) //todo: not nice

FORCEINLINE CPackedNormal CVT(FPackedNormal V)
{
	CPackedNormal ret;
	ret.Data = V.Data ^ 0x80808080;		// offset by 128
	return ret;
}

FORCEINLINE FPackedNormal CVT(CPackedNormal V)
{
	FPackedNormal ret;
	ret.Data = V.Data ^ 0x80808080;		// offset by 128
	return ret;
}

#endif // __MESH_COMMON_H__


// declare Core math as SIMPLE_TYPE for faster arrays support
SIMPLE_TYPE(CVec3, float)
SIMPLE_TYPE(CQuat, float)
SIMPLE_TYPE(CCoords, float)


#endif // __TYPE_CONVERT_H__
