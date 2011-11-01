#ifndef __STATICMESH_H__
#define __STATICMESH_H__


// forwards
class UUnrealMaterial;


//?? common structures

struct CIndexBuffer
{
	typedef int (*IndexAccessor_t)(const CIndexBuffer&, int);

	TArray<word>			Indices16;			// used when mesh has less than 64k indices
	TArray<unsigned>		Indices32;			// used when mesh has more than 64k indices

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
		return Is32Bit() ? GetIndex32 : GetIndex16;
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


/*-----------------------------------------------------------------------------
	Local StaticMesh class, encapsulated all UE versions
-----------------------------------------------------------------------------*/

#define NUM_STATIC_MESH_UV_SETS		4

struct CStaticMeshSection
{
	UUnrealMaterial			*Material;
	int						FirstIndex;
	int						NumFaces;

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CStaticMeshSection)
	BEGIN_PROP_TABLE
		PROP_OBJ(Material)
		PROP_INT(FirstIndex)
		PROP_INT(NumFaces)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};


struct CStaticMeshUV
{
	CVec3					Normal;
	CVec3					Tangent;
	CVec3					Binormal;
	float					U, V;
};


struct CStaticMeshVertex
{
	CVec3					Position;
	CStaticMeshUV			UV[NUM_STATIC_MESH_UV_SETS];
	//?? do not store extra UV when not needed - each vertex is 11*4+3 ints, or 192 bytes!
};


struct CStaticMeshLod
{
	// generic properties
	int						NumTexCoords;
	bool					HasTangents;
	// geometry
	TArray<CStaticMeshSection> Sections;
	TArray<CStaticMeshVertex>  Verts;
	CIndexBuffer			Indices;

	void BuildTangents();

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CStaticMeshLod)
	BEGIN_PROP_TABLE
		PROP_ARRAY(Sections, CStaticMeshSection)
		VPROP_ARRAY_COUNT(Verts, VertexCount)
		VPROP_ARRAY_COUNT(Indices.Indices16, IndexCount)
		PROP_INT(NumTexCoords)
	END_PROP_TABLE
#endif // DECLARE_VIEWER_PROPS
};


class CStaticMesh
{
public:
	UObject					*OriginalMesh;			//?? make common for all mesh classes
	FBox					BoundingBox;			//?? common
	FSphere					BoundingSphere;			//?? common
	TArray<CStaticMeshLod>	Lods;

	CStaticMesh(UObject *Original)
	:	OriginalMesh(Original)
	{}

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CStaticMesh)
	BEGIN_PROP_TABLE
		PROP_ARRAY(Lods, CStaticMeshLod)
		VPROP_ARRAY_COUNT(Lods, LodCount)
	END_PROP_TABLE
private:
	CStaticMesh()									// for InternalConstructor()
	{}
#endif // DECLARE_VIEWER_PROPS
};


#define REGISTER_STATICMESH_VCLASSES \
	REGISTER_CLASS(CStaticMeshSection) \
	REGISTER_CLASS(CStaticMeshLod)


#endif // __STATICMESH_H__
