#ifndef __MESHCOMMON_H__
#define __MESHCOMMON_H__

// forwards
class UUnrealMaterial;


#define USE_SSE						1

#if USE_SSE
#include "MathSSE.h"
typedef CVec4 CVecT;
#else
typedef CVec3 CVecT;
#endif


#define MAX_MESH_UV_SETS			8


struct CIndexBuffer
{
	struct IndexAccessor_t
	{
		FORCEINLINE int operator()(int Index)
		{
			return Function(*Buffer, Index);
		}
		int (*Function)(const CIndexBuffer&, int);
		const CIndexBuffer	*Buffer;
	};

	TArray<word>			Indices16;			// used when mesh has less than 64k verts
	TArray<unsigned>		Indices32;			// used when mesh has more than 64k verts

	FORCEINLINE bool Is32Bit() const
	{
		return (Indices32.Num() != 0);
	}

	int Num() const
	{
		return Is32Bit() ? Indices32.Num() : Indices16.Num();
	}

	IndexAccessor_t GetAccessor() const
	{
		IndexAccessor_t result;
		result.Function = Is32Bit() ? GetIndex32 : GetIndex16;
		result.Buffer   = this;
		return result;
	}

	void Initialize(const TArray<word> *Idx16, const TArray<unsigned> *Idx32 = NULL)
	{
		if (Idx32 && Idx32->Num())
			CopyArray(Indices32, *Idx32);
		else
			CopyArray(Indices16, *Idx16);
	}

private:
	static int GetIndex16(const CIndexBuffer &Buf, int Index)
	{
		return Buf.Indices16[Index];
	}
	static int GetIndex32(const CIndexBuffer &Buf, int Index)
	{
		return Buf.Indices32[Index];
	}
};


struct CMeshUVFloat
{
	float					U, V;
};


struct CMeshVertex
{
	CVecT					Position;
	CVecT					Normal;
	CVecT					Tangent;
	CVecT					Binormal;
	CMeshUVFloat			UV;				// base UV channel
};


struct CMeshSection
{
	UUnrealMaterial			*Material;
	int						FirstIndex;
	int						NumFaces;

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CMeshSection)
	BEGIN_PROP_TABLE
		PROP_OBJ(Material)
		PROP_INT(FirstIndex)
		PROP_INT(NumFaces)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};


void BuildNormalsCommon(CMeshVertex *Verts, int VertexSize, int NumVerts, const CIndexBuffer &Indices);
void BuildTangentsCommon(CMeshVertex *Verts, int VertexSize, const CIndexBuffer &Indices);


#endif // __MESHCOMMON_H__
