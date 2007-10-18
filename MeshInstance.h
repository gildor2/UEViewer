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
	CCoords			BaseTransform;			// rotation for mesh; have identity axis
	CCoords			BaseTransformScaled;	// rotation for mesh with scaled axis

	CMeshInstance(ULodMesh *Mesh, CMeshViewer *Viewer)
	:	pMesh(Mesh)
	,	Viewport(Viewer)
	{
		SetAxis(Mesh->RotOrigin, BaseTransform.axis);
		BaseTransform.origin[0] = Mesh->MeshOrigin.X * Mesh->MeshScale.X;
		BaseTransform.origin[1] = Mesh->MeshOrigin.Y * Mesh->MeshScale.Y;
		BaseTransform.origin[2] = Mesh->MeshOrigin.Z * Mesh->MeshScale.Z;

		BaseTransformScaled.axis = BaseTransform.axis;
		CVec3 tmp;
		tmp[0] = 1.0f / Mesh->MeshScale.X;
		tmp[1] = 1.0f / Mesh->MeshScale.Y;
		tmp[2] = 1.0f / Mesh->MeshScale.Z;
		BaseTransformScaled.axis.PrescaleSource(tmp);
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
			if (color > 7) color = 7;
			glColor3f(color & 1, (color & 2) >> 1, (color & 4) >> 2);
		}
	}

	virtual void Draw() = NULL;
	virtual void Tick(float TimeDelta)
	{}
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
	// mesh state
	int			LodNum;

	CSkelMeshInstance(USkeletalMesh *Mesh, CSkelMeshViewer *Viewer);
	virtual ~CSkelMeshInstance();
	virtual void Draw();

	void DrawSkeleton();
	void DrawBaseSkeletalMesh();
	void DrawLodSkeletalMesh(const FStaticLODModel *lod);

	// skeleton configuration
	void SetBoneScale(const char *BoneName, float scale = 1.0f);

	// animation
	void PlayAnim(const char *AnimName, float Rate = 1 /*, float TweenTime = 0, int Channel = 0*/)
	{
		PlayAnimInternal(AnimName, Rate, false);
	}
	void LoopAnim(const char *AnimName, float Rate = 1 /*, float TweenTime = 0, int Channel = 0*/)
	{
		PlayAnimInternal(AnimName, Rate, true);
	}
	void FreezeAnimAt(float Time       /*, int Channel = 0*/);
	// get animation information:
	// Frame     = 0..NumFrames
	// NumFrames = animation length, frames
	// Rate      = frames per seconds
	void GetAnimParams(/*int Channel,*/ const char *&AnimName, float &Frame, float &NumFrames, float &Rate);

	virtual void Tick(float TimeDelta);

private:
	// mesh data
	struct CMeshBoneData *BoneData;
	CVec3		*MeshVerts;
	// animation state
	//!! make multi-channel
	int			AnimIndex;
	float		AnimTime;
	float		AnimRate;
	bool		AnimLooped;

	int FindBone(const char *BoneName) const;
	int FindAnim(const char *AnimName) const;
	void PlayAnimInternal(const char *AnimName, float Rate /*, TweenTime, Channel */, bool Looped);
	void UpdateSkeleton();
};


#endif // __MESHINSTANCE_H__
