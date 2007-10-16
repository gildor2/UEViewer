#ifndef __MESHINSTANCE_H__
#define __MESHINSTANCE_H__

/*-----------------------------------------------------------------------------
	Basic CMeshInstance class
-----------------------------------------------------------------------------*/

class CMeshInstance
{
public:
	// linked data
	ULodMesh		*pMesh;
	CMeshViewer		*Viewport;
	// common properties
	CCoords			BaseTransform;
	CCoords			BaseTransformScaled;

	CMeshInstance(ULodMesh *Mesh, CMeshViewer *Viewer)
	:	pMesh(Mesh)
	,	Viewport(Viewer)
	{
		SetAxis(Mesh->RotOrigin, BaseTransform.axis);
		BaseTransform.origin[0] = Mesh->MeshOrigin.X * Mesh->MeshScale.X;
		BaseTransform.origin[1] = Mesh->MeshOrigin.Y * Mesh->MeshScale.Y;
		BaseTransform.origin[2] = Mesh->MeshOrigin.Z * Mesh->MeshScale.Z;

		BaseTransformScaled.axis = BaseTransform.axis;
		BaseTransformScaled.axis.PrescaleSource((CVec3&)Mesh->MeshScale);
		BaseTransformScaled.origin = (CVec3&)Mesh->MeshOrigin;
	}
	virtual ~CMeshInstance()
	{}

	void SetMaterial(int Index)
	{
		if (!Viewport->bColorMaterials)
		{
			int TexIndex = pMesh->Materials[Index].TextureIndex;
			if (TexIndex >= pMesh->Textures.Num())		// possible situation; see ONSWeapons-A.ukx/ParasiteMine
				TexIndex = 0;
			UMaterial *Mat = pMesh->Textures[TexIndex];
			if (Mat)
				Mat->Bind();
			else
				BindDefaultMaterial();
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_CULL_FACE);
			int color = Index + 1;
			if (color > 7) color = 2;
			glColor3f(color & 1, (color & 2) >> 1, (color & 4) >> 2);
		}
	}

	virtual void Draw() = NULL;
};


/*-----------------------------------------------------------------------------
	CVertMeshInstance class
-----------------------------------------------------------------------------*/

class CVertMeshInstance : public CMeshInstance
{
public:
	// mesh state
	int			FrameNum;

	CVertMeshInstance(UVertMesh *Mesh, CVertMeshViewer *Viewer)
	:	CMeshInstance(Mesh, Viewer)
	,	FrameNum(0)
	{}
	virtual void Draw();
};


/*-----------------------------------------------------------------------------
	CSkelMeshInstance class
-----------------------------------------------------------------------------*/

class CSkelMeshInstance : public CMeshInstance
{
public:
	// mesh data
	CCoords		*BoneCoords;
	CCoords		*RefBoneCoords;
	CCoords		*BoneTransform;		// transformation for verts from reference pose to deformed
	CVec3		*MeshVerts;
	int			*BoneMap;			// remap indices of Mesh.FMeshBone[] to Anim.FNamedBone[]
	// mesh state
	int			LodNum;
	int			CurrAnim;
	float		AnimTime;

	CSkelMeshInstance(USkeletalMesh *Mesh, CSkelMeshViewer *Viewer);
	virtual ~CSkelMeshInstance();
	virtual void Draw();

	void UpdateSkeleton(int Seq, float Time);
	void DrawSkeleton();
	void DrawBaseSkeletalMesh();
	void DrawLodSkeletalMesh(const FStaticLODModel *lod);
};


#endif // __MESHINSTANCE_H__
