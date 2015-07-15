#ifndef __MESHCOMMON_H__
#define __MESHCOMMON_H__

// forwards
class UUnrealMaterial;


#define USE_SSE						1

#include "MathSSE.h"

#if USE_SSE
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


struct CPackedNormal
{
	unsigned				Data;

	FORCEINLINE void SetW(float Value)
	{
		Data = (Data & 0xFFFFFF) | (appRound(Value * 127.0f) << 24);
	}
	FORCEINLINE float GetW() const
	{
		return ( (char)(Data >> 24) ) / 127.0f;
	}
};

FORCEINLINE bool operator==(CPackedNormal V1, CPackedNormal V2)
{
	return V1.Data == V2.Data;
}

FORCEINLINE void Pack(CPackedNormal& Packed, const CVec3& Unpacked)
{
	Packed.Data =  (byte)appRound(Unpacked.v[0] * 127.0f)
				+ ((byte)appRound(Unpacked.v[1] * 127.0f) << 8)
				+ ((byte)appRound(Unpacked.v[2] * 127.0f) << 16);
}

FORCEINLINE void Unpack(CVec3& Unpacked, const CPackedNormal& Packed)
{
	Unpacked.v[0] = (char)( Packed.Data        & 0xFF) / 127.0f;
	Unpacked.v[1] = (char)((Packed.Data >> 8 ) & 0xFF) / 127.0f;
	Unpacked.v[2] = (char)((Packed.Data >> 16) & 0xFF) / 127.0f;
}

#if USE_SSE
/*FORCEINLINE void Pack(CPackedNormal& Packed, const CVec4& Unpacked)
{
	// REWRITE WITH SSE, but not used yet
	Packed.Data =  (byte)appRound(Unpacked.v[0] * 127.0f)
				+ ((byte)appRound(Unpacked.v[1] * 127.0f) << 8)
				+ ((byte)appRound(Unpacked.v[2] * 127.0f) << 16);
}*/

FORCEINLINE void Unpack(CVec4& Unpacked, const CPackedNormal& Packed)
{
	Unpacked.mm = UnpackPackedChars(Packed.Data);
}

#endif // USE_SSE


// TODO: remove "Binormal", compute in shader - this will allow compiler to put UV into the same 16-byte block with normals, so
// CMeshVertex will be 16 bytes smaller (CVecT forces 16-byte alignment for structure).
/* Required changes for that:
  - when doing SkeletalMesh skinning, remove "Binormal" skinning, but pass 4th "Tangent" component to GL (i.e. use "4" instead of "3" in glAttrib...)
  - use vec4 in shader for "tangent", compute "binormal"
  - eliminate binormal computation/saving in Unreal and MeshCommon code
*/
struct CMeshVertex
{
	CVecT					Position;
	CPackedNormal			Normal;
	CPackedNormal			Tangent;
	CMeshUVFloat			UV;				// base UV channel

	void DecodeTangents(CVecT& OutNormal, CVecT& OutTangent, CVecT& OutBinormal) const
	{
		Unpack(OutNormal, Normal);
		Unpack(OutTangent, Tangent);
		cross(OutNormal, OutTangent, OutBinormal);
		OutNormal.Scale(OutNormal.v[3]);
	}
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


struct CBaseMeshLod
{
	// generic properties
	int						NumTexCoords;
	bool					HasNormals;
	bool					HasTangents;
	// geometry
	TArray<CMeshSection>	Sections;
	int						NumVerts;
	CMeshUVFloat*			ExtraUV[MAX_MESH_UV_SETS-1];
	CIndexBuffer			Indices;

	~CBaseMeshLod()
	{
		for (int i = 0; i < NumTexCoords-1; i++)
			appFree(ExtraUV[i]);
	}

	void AllocateUVBuffers()
	{
		for (int i = 0; i < NumTexCoords-1; i++)
			ExtraUV[i] = (CMeshUVFloat*)appMalloc(sizeof(CMeshUVFloat) * NumVerts);
	}

	void LockMaterials();
	void UnlockMaterials();
};

void BuildNormalsCommon(CMeshVertex *Verts, int VertexSize, int NumVerts, const CIndexBuffer &Indices);
void BuildTangentsCommon(CMeshVertex *Verts, int VertexSize, const CIndexBuffer &Indices);


#endif // __MESHCOMMON_H__
