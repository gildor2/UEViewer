#ifndef __MESH_INSTANCE_H__
#define __MESH_INSTANCE_H__

// forwards
class UVertMesh;

class CSkeletalMesh;
class CAnimSet;
class CAnimSequence;
class CStaticMesh;

enum class EAnimRetargetingMode;

/*-----------------------------------------------------------------------------
	Basic CMeshInstance class
-----------------------------------------------------------------------------*/

// Draw flags
#define DF_SHOW_NORMALS			0x0001

class CMeshInstance
{
public:
	// common properties
	CCoords			BaseTransformScaled;	// rotation for mesh with scaled axis
	// debug
	static bool		bColorMaterials;		//?? replace with DF_COLOR_MATERIALS, but flags are stored outside of mesh and used in MeshInstance::SetMaterial()
	bool			bVertexColors;
	int				HighlightMaterialIndex;

	CMeshInstance()
	:	bVertexColors(false)
	,	HighlightMaterialIndex(-1)
	{}

	virtual ~CMeshInstance()
	{}

	static unsigned GetMaterialDebugColor(int Index);
	void SetMaterial(UUnrealMaterial *Mat, int Index);
	virtual void Draw(unsigned flags = 0) = 0;
};


/*-----------------------------------------------------------------------------
	CVertMeshInstance class
-----------------------------------------------------------------------------*/

struct CMeshSection;

class CVertMeshInstance : public CMeshInstance
{
public:
	// linked data
	const UVertMesh	*pMesh;

	CVertMeshInstance();
	virtual ~CVertMeshInstance();

	void FreeRenderBuffers();

	void SetMesh(const UVertMesh *Mesh);

	virtual void Draw(unsigned flags = 0);

	// animation control
	void PlayAnim(const char *AnimName, float Rate = 1)
	{
		PlayAnimInternal(AnimName, Rate, false);
	}
	void LoopAnim(const char *AnimName, float Rate = 1)
	{
		PlayAnimInternal(AnimName, Rate, true);
	}
	void TweenAnim(const char *AnimName)
	{
		PlayAnimInternal(AnimName, 0, false);
	}

	void AnimStopLooping()
	{
		AnimLooped = false;
	}
	void FreezeAnimAt(float Time);

	// animation state
	void GetAnimParams(const char *&AnimName, float &Frame, float &NumFrames, float &Rate) const;

	// animation enumeration
	int GetAnimCount() const;
	const char *GetAnimName(int Index) const;
	void UpdateAnimation(float TimeDelta);

protected:
	// animation state
	int						AnimIndex;			// current animation sequence
	float					AnimTime;			// current animation frame
	float					AnimRate;			// animation rate, frames per seconds
	bool					AnimLooped;

	// data for rendering
	TArray<CMeshSection>	Sections;
	CVec3					*Verts;				// deformed mesh, used in Draw() only
	CVec3					*Normals;
	uint16					*Indices;			// index buffer

	int FindAnim(const char *AnimName) const;
	void PlayAnimInternal(const char *AnimName, float Rate, bool Looped);
};


/*-----------------------------------------------------------------------------
	CSkelMeshInstance class
-----------------------------------------------------------------------------*/

#define MAX_SKELANIMCHANNELS	32

// Draw flags
#define DF_SHOW_INFLUENCES		0x0100

class CSkelMeshInstance : public CMeshInstance
{
	struct CAnimChan
	{
		const CAnimSequence	*Anim1;			// current animation sequence; NULL for default pose
		const CAnimSequence	*Anim2;			// secondary animation for blending with; NULL = not used
		float				CurrentFrame;	// current animation frame for primary animation
		float				SecondaryBlend;	// value = 0 (use primary animation) .. 1 (use secondary animation)
		float				BlendAlpha;		// blend with previous channels; 0 = not affected, 1 = fully affected
		int					RootBone;		// root animation bone
		float				Rate;			// animation rate multiplier for Anim.Rate
		float				TweenTime;		// time to stop tweening; 0 when no tweening at all
		float				TweenStep;		// fraction between current pose and desired pose; updated in UpdateAnimation()
		bool				bLooped;		// true when animation is looped
		bool				bReverse;		// true when animation is played in reverse direction
	};

public:
	// linked data
	CSkeletalMesh	*pMesh;

	// mesh state
	int			LodIndex;
	int			MorphIndex;
	int			UVIndex;
	EAnimRetargetingMode RetargetingModeOverride;

	CSkelMeshInstance();
	virtual ~CSkelMeshInstance();

	void SetMesh(CSkeletalMesh *Mesh);		// not 'const *mesh' because can call BuildTangents()
	void SetAnim(const CAnimSet *Anim);

	void ClearSkelAnims();
	virtual void Draw(unsigned flags = 0);

	void DumpBones();
	void DrawSkeleton(bool ShowLabels, bool ColorizeBones);
	void DrawAttachments();
	void DrawMesh(unsigned flags);

	// skeleton configuration
	void SetBoneScale(const char *BoneName, float scale = 1.0f);

	// animation control
	void PlayAnim(const char *AnimName, float Rate = 1, float TweenTime = 0, int Channel = 0)
	{
		PlayAnimInternal(AnimName, Rate, TweenTime, Channel, false);
	}
	void LoopAnim(const char *AnimName, float Rate = 1, float TweenTime = 0, int Channel = 0)
	{
		PlayAnimInternal(AnimName, Rate, TweenTime, Channel, true);
	}
	void TweenAnim(const char *AnimName, float TweenTime, int Channel = 0)
	{
		PlayAnimInternal(AnimName, 0, TweenTime, Channel, false);
	}
	void AnimStopLooping(int Channel)
	{
		GetStage(Channel).bLooped = false;
	}
	void FreezeAnimAt(float Frame, int Channel = 0);

	// animation state

	const CAnimSequence *GetAnim(int Channel) const
	{
		return GetStage(Channel).Anim1;
	}

	float GetAnimFrame(int Channel) const
	{
		return GetStage(Channel).CurrentFrame;
	}

	// get current animation information:
	// Frame     = 0..NumFrames
	// NumFrames = animation length, frames
	// Rate      = frames per seconds
	void GetAnimParams(int Channel, const char *&AnimName, float &Frame, float &NumFrames, float &Rate) const;

	bool HasAnim(const char *AnimName) const
	{
		return FindAnim(AnimName) != NULL;
	}
	bool IsTweening(int Channel = 0)
	{
		return GetStage(Channel).TweenTime > 0;
	}

	// animation blending
	void SetBlendParams(int Channel, float BlendAlpha, const char *BoneName = NULL);
	void SetBlendAlpha(int Channel, float BlendAlpha)
	{
		GetStage(Channel).BlendAlpha = BlendAlpha;
	}
	void SetSecondaryAnim(int Channel, const char *AnimName = NULL);
	void SetSecondaryBlend(int Channel, float BlendAlpha)
	{
		GetStage(Channel).SecondaryBlend = BlendAlpha;
	}

	// animation enumeration
	int GetAnimCount() const;
	const char *GetAnimName(int Index) const;
	void UpdateAnimation(float TimeDelta);

	const CAnimSet *GetAnim() const
	{
		return Animation;
	}

	CVec3 GetMeshOrigin() const;

protected:
	const CAnimSet*		Animation;
	// mesh data
	void*				DataBlock;

	// all of the following data is resides inside "DataBlock", aligned to 16 bytes
	struct CMeshBoneData* BoneData;
	struct CSkinVert*	Skinned;			// soft-skinned vertices
	struct CSkelMeshVertex* MorphedVerts;

	CVec3*				InfColors;			// debug: color-by-influence for vertices
	int					LastLodIndex;		// used to detect requirement to rebuild InfColors[]
	int					LastMorphIndex;		// used to detect requirement to rebuild MorphedVerts[]
	int					HighlightBoneIndex;	// index of the bone which will be highlighted while showing the skeleton
	bool				bLockBoneHighlight;	// ignore mouse hover events while locked, do not change the highlight
	int					LastHighlightBoneIndex; // used to determine if InfColors should be rebuilt
	// animation state
	CAnimChan	Channels[MAX_SKELANIMCHANNELS];
	int			MaxAnimChannel;

	CAnimChan &GetStage(int StageIndex)
	{
		assert(StageIndex >= 0 && StageIndex < MAX_SKELANIMCHANNELS);
		return Channels[StageIndex];
	}
	const CAnimChan &GetStage(int StageIndex) const
	{
		assert(StageIndex >= 0 && StageIndex < MAX_SKELANIMCHANNELS);
		return Channels[StageIndex];
	}
	void SkinMeshVerts();
	int FindBone(const char *BoneName) const;
	const CAnimSequence *FindAnim(const char *AnimName) const;
	void PlayAnimInternal(const char *AnimName, float Rate, float TweenTime, int Channel, bool Looped);
	void UpdateSkeleton();
	void ComputeMeshSpaceCoords();
	void BuildInfColors();
	bool BuildMorphVerts();
};


/*-----------------------------------------------------------------------------
	CStatMeshInstance class
-----------------------------------------------------------------------------*/

class CStatMeshInstance : public CMeshInstance
{
public:
	CStatMeshInstance()
	:	pMesh(NULL)
	,	LodNum(0)
	,	UVIndex(0)
	{}

	~CStatMeshInstance();

	// not 'const *mesh' because can call BuildTangents()
	void SetMesh(CStaticMesh *Mesh);

	virtual void Draw(unsigned flags = 0);

	// mesh state
	int			LodNum;
	int			UVIndex;

protected:
	CStaticMesh *pMesh;
};


#endif // __MESH_INSTANCE_H__
