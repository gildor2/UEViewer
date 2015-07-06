#ifndef __STATICMESH_H__
#define __STATICMESH_H__

#include "MeshCommon.h"

/*-----------------------------------------------------------------------------
	Local StaticMesh class, encapsulated all UE versions
-----------------------------------------------------------------------------*/

struct CStaticMeshVertex : public CMeshVertex
{
	// everything is from the CMeshVertex
};


struct CStaticMeshLod
{
	// generic properties
	int						NumTexCoords;
	bool					HasNormals;
	bool					HasTangents;
	// geometry
	TArray<CMeshSection>	Sections;
	CStaticMeshVertex		*Verts;
	CMeshUVFloat*			ExtraUV[MAX_MESH_UV_SETS-1];
	int						NumVerts;
	CIndexBuffer			Indices;

	~CStaticMeshLod()
	{
		if (Verts) appFree(Verts);
		for (int i = 0; i < NumTexCoords-1; i++)
			appFree(ExtraUV[i]);
	}

	void BuildNormals()
	{
		if (HasNormals) return;
		BuildNormalsCommon(Verts, sizeof(CStaticMeshVertex), NumVerts, Indices);
		HasNormals = true;
	}

	void BuildTangents()
	{
		if (HasTangents) return;
		BuildTangentsCommon(Verts, sizeof(CStaticMeshVertex), Indices);
		HasTangents = true;
	}

	void AllocateVerts(int Count)
	{
		guard(CStaticMeshLod::AllocateVerts);
		assert(Verts == NULL);
		Verts    = (CStaticMeshVertex*)appMalloc(sizeof(CStaticMeshVertex) * Count, 16);		// alignment for SSE
		NumVerts = Count;
		for (int i = 0; i < NumTexCoords-1; i++)
			ExtraUV[i] = (CMeshUVFloat*)appMalloc(sizeof(CMeshUVFloat) * Count);
		unguard;
	}

#if DECLARE_VIEWER_PROPS
	DECLARE_STRUCT(CStaticMeshLod)
	BEGIN_PROP_TABLE
		PROP_ARRAY(Sections, CMeshSection)
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

	void FinalizeMesh()
	{
		for (int i = 0; i < Lods.Num(); i++)
			Lods[i].BuildNormals();
	}

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
	REGISTER_CLASS(CStaticMeshLod)


#endif // __STATICMESH_H__
