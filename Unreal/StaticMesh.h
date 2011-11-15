#ifndef __STATICMESH_H__
#define __STATICMESH_H__

#include "MeshCommon.h"

/*-----------------------------------------------------------------------------
	Local StaticMesh class, encapsulated all UE versions
-----------------------------------------------------------------------------*/

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


struct CStaticMeshVertex : public CMeshVertex
{
	// everything is from the CMeshVertex
};


struct CStaticMeshLod
{
	// generic properties
	int						NumTexCoords;
	bool					HasTangents;
	// geometry
	TArray<CStaticMeshSection> Sections;
	CStaticMeshVertex		*Verts;
	int						NumVerts;
	CIndexBuffer			Indices;

	~CStaticMeshLod()
	{
		if (Verts) appFree(Verts);
	}

	void BuildTangents();

	void AllocateVerts(int Count)
	{
		guard(CStaticMeshLod::AllocateVerts);
		assert(Verts == NULL);
		Verts    = (CStaticMeshVertex*)appMalloc(sizeof(CStaticMeshVertex) * Count, 16);		// alignment for SSE
		NumVerts = Count;
		unguard;
	}

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CStaticMeshLod)
	BEGIN_PROP_TABLE
		PROP_ARRAY(Sections, CStaticMeshSection)
		PROP_INT(NumVerts)
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
