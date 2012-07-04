#ifndef __TYPECONVERT_H__
#define __TYPECONVERT_H__

/*-----------------------------------------------------------------------------
	Compatibility between Core and UnCore math types
-----------------------------------------------------------------------------*/

struct FMeshUVFloat;
struct CMeshUVFloat;


#define CONVERTER(From, To)					\
FORCEINLINE To& CVT(From &V)				\
{											\
	return (To&)V;							\
}											\
FORCEINLINE const To& CVT(const From &V)	\
{											\
	return (const To&)V;					\
}

//?? combine CONVERTER(T) with CONVERTER(TArray<T>)?

CONVERTER(FVector,         CVec3          )
CONVERTER(FQuat,           CQuat          )
CONVERTER(FCoords,         CCoords        )
CONVERTER(FMeshUVFloat,    CMeshUVFloat   )
CONVERTER(TArray<FVector>, TArray<CVec3>  )
CONVERTER(TArray<FQuat>,   TArray<CQuat>  )
CONVERTER(TArray<FCoords>, TArray<CCoords>)

#undef CONVERTER

// declare Core math as SIMPLE_TYPE for better arrays support
SIMPLE_TYPE(CVec3, float)
SIMPLE_TYPE(CQuat, float)
SIMPLE_TYPE(CCoords, float)


#endif // __TYPECONVERT_H__
