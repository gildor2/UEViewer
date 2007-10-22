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
	virtual void UpdateAnimation(float TimeDelta) = NULL;

	// animation control
	void PlayAnim(const char *AnimName, float Rate = 1, float TweenTime = 0 /*, int Channel = 0*/)
	{
		PlayAnimInternal(AnimName, Rate, TweenTime, false);
	}
	void LoopAnim(const char *AnimName, float Rate = 1, float TweenTime = 0 /*, int Channel = 0*/)
	{
		PlayAnimInternal(AnimName, Rate, TweenTime, true);
	}
	void TweenAnim(const char *AnimName, float TweenTime /*, int Channel */)
	{
		PlayAnimInternal(AnimName, 0, TweenTime, false);
	}

	virtual void AnimStopLooping(/*int Channel */) = NULL;
	virtual void FreezeAnimAt(float Time /*, int Channel = 0*/) = NULL;

	// animation state
	virtual void GetAnimParams(/*int Channel,*/ const char *&AnimName,
		float &Frame, float &NumFrames, float &Rate) const = NULL;
	virtual bool IsTweening(/*int Channel*/) = NULL;

	// animation enumeration
	virtual int GetAnimCount() const = NULL;
	virtual const char *GetAnimName(int Index) const = NULL;

private:
	virtual void PlayAnimInternal(const char *AnimName, float Rate, float TweenTime /*, int Channel*/, bool Looped) = NULL;
};


/*-----------------------------------------------------------------------------
	CVertMeshInstance class
-----------------------------------------------------------------------------*/

class CVertMeshInstance : public CMeshInstance
{
public:
	CVertMeshInstance(UVertMesh *Mesh, CVertMeshViewer *Viewer)
	:	CMeshInstance(Mesh, Viewer)
	,	AnimIndex(-1)
	,	AnimTime(0)
	{}
	virtual void Draw();

	// animation control
	virtual void AnimStopLooping(/*int Channel */)
	{
		AnimLooped = false;
	}
	virtual void FreezeAnimAt(float Time /*, int Channel = 0*/);

	// animation state
	virtual void GetAnimParams(/*int Channel,*/ const char *&AnimName,
		float &Frame, float &NumFrames, float &Rate) const;
	virtual bool IsTweening(/*int Channel*/)
	{
		return false;
	}

	// animation enumeration
	virtual int GetAnimCount() const
	{
		const UVertMesh *Mesh = GetMesh();
		return Mesh->AnimSeqs.Num();
	}
	virtual const char *GetAnimName(int Index) const
	{
		guard(CVertMeshInstance::GetAnimName);
		if (Index < 0) return "None";
		return GetMesh()->AnimSeqs[Index].Name;
		unguard;
	}

	virtual void UpdateAnimation(float TimeDelta);

private:
	// animation state
	int			AnimIndex;			// current animation sequence
	float		AnimTime;			// current animation frame
	float		AnimRate;			// animation rate, frames per seconds
	bool		AnimLooped;

	const UVertMesh *GetMesh() const
	{
		return static_cast<UVertMesh*>(pMesh);
	}
	int FindAnim(const char *AnimName) const;
	virtual void PlayAnimInternal(const char *AnimName, float Rate, float TweenTime /*, int Channel*/, bool Looped);
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

	// animation control
	virtual void AnimStopLooping(/*int Channel */)
	{
		AnimLooped = false;
	}
	virtual void FreezeAnimAt(float Time /*, int Channel = 0*/);

	// animation state

	// get current animation information:
	// Frame     = 0..NumFrames
	// NumFrames = animation length, frames
	// Rate      = frames per seconds
	virtual void GetAnimParams(/*int Channel,*/ const char *&AnimName,
		float &Frame, float &NumFrames, float &Rate) const;

	bool HasAnim(const char *AnimName) const
	{
		return FindAnim(AnimName) >= 0;
	}
	virtual bool IsTweening(/*int Channel*/)
	{
		return AnimTweenTime > 0;
	}

	// animation enumeration
	virtual int GetAnimCount() const
	{
		const USkeletalMesh *Mesh = GetMesh();
		if (!Mesh->Animation) return 0;
		return Mesh->Animation->AnimSeqs.Num();
	}
	virtual const char *GetAnimName(int Index) const
	{
		guard(CSkelMeshInstance::GetAnimName);
		if (Index < 0) return "None";
		const USkeletalMesh *Mesh = GetMesh();
		assert(Mesh->Animation);
		return Mesh->Animation->AnimSeqs[Index].Name;
		unguard;
	}

	virtual void UpdateAnimation(float TimeDelta);

private:
	// mesh data
	struct CMeshBoneData *BoneData;
	CVec3		*MeshVerts;
	// animation state
	//!! make multi-channel
	int			AnimIndex;			// current animation sequence; -1 for default pose
	float		AnimTime;			// current animation frame
	float		AnimRate;			// animation rate, frames per seconds
	float		AnimTweenTime;		// time to stop tweening; 0 when no tweening at all
	float		AnimTweenStep;		// fraction between current pose and desired pose; updated in UpdateAnimation()
	bool		AnimLooped;

	const USkeletalMesh *GetMesh() const
	{
		return static_cast<USkeletalMesh*>(pMesh);
	}
	int FindBone(const char *BoneName) const;
	int FindAnim(const char *AnimName) const;
	virtual void PlayAnimInternal(const char *AnimName, float Rate, float TweenTime /*, int Channel*/, bool Looped);
	void UpdateSkeleton();
};


#endif // __MESHINSTANCE_H__
