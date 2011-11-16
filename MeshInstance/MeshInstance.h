#ifndef __MESHINSTANCE_H__
#define __MESHINSTANCE_H__

// forwards
class UVertMesh;

class CSkeletalMesh;
class CAnimSet;
class CStaticMesh;

enum EAnimRotationOnly;


/*-----------------------------------------------------------------------------
	Basic CMeshInstance class
-----------------------------------------------------------------------------*/

class CMeshInstance
{
public:
	// common properties
	CCoords			BaseTransform;			// rotation for mesh; have identity axis
	CCoords			BaseTransformScaled;	// rotation for mesh with scaled axis
	// debugging
	bool			bShowNormals;
	bool			bColorMaterials;
	bool			bWireframe;

	CMeshInstance()
	:	bShowNormals(false)
	,	bColorMaterials(false)
	,	bWireframe(false)
	{}

	virtual ~CMeshInstance()
	{}

	void SetMaterial(UUnrealMaterial *Mat, int Index, int PolyFlags);
	virtual void Draw() = 0;
};


/*-----------------------------------------------------------------------------
	CVertMeshInstance class
-----------------------------------------------------------------------------*/

class CVertMeshInstance : public CMeshInstance
{
public:
	// linked data
	const UVertMesh	*pMesh;

	CVertMeshInstance()
	:	AnimIndex(-1)
	,	AnimTime(0)
	{}

	void SetMesh(const UVertMesh *Mesh);
	UUnrealMaterial *GetMaterial(int Index, int *PolyFlags = NULL);
	void SetMaterial(int Index);

	virtual void Draw();

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
	int			AnimIndex;			// current animation sequence
	float		AnimTime;			// current animation frame
	float		AnimRate;			// animation rate, frames per seconds
	bool		AnimLooped;

	int FindAnim(const char *AnimName) const;
	void PlayAnimInternal(const char *AnimName, float Rate, bool Looped);
};


/*-----------------------------------------------------------------------------
	CSkelMeshInstance class
-----------------------------------------------------------------------------*/

#define MAX_SKELANIMCHANNELS	32

struct FVertInfluences;

class CSkelMeshInstance : public CMeshInstance
{
	struct CAnimChan
	{
		int			AnimIndex1;		// current animation sequence; -1 for default pose
		int			AnimIndex2;		// secondary animation for blending with; -1 = not used
		float		Time;			// current animation frame for primary animation
		float		SecondaryBlend;	// value = 0 (use primary animation) .. 1 (use secondary animation)
		float		BlendAlpha;		// blend with previous channels; 0 = not affected, 1 = fully affected
		int			RootBone;		// root animation bone
		float		Rate;			// animation rate multiplier for Anim.Rate
		float		TweenTime;		// time to stop tweening; 0 when no tweening at all
		float		TweenStep;		// fraction between current pose and desired pose; updated in UpdateAnimation()
		bool		Looped;
	};

public:
	// linked data
	CSkeletalMesh	*pMesh;

	// mesh state
	int			LodNum;
	int			UVIndex;
	EAnimRotationOnly RotationMode;		// EAnimRotationOnly
	// debugging
	int			ShowSkel;			// 0 - mesh, 1 - mesh+skel, 2 - skel only
	bool		ShowInfluences;
	bool		ShowLabels;
	bool		ShowAttach;

	CSkelMeshInstance();
	virtual ~CSkelMeshInstance();

	void SetMesh(CSkeletalMesh *Mesh);	// not 'const *mesh' because can call BuildTangents()
	void SetAnim(const CAnimSet *Anim);

	void ClearSkelAnims();
//??	void StopAnimating(bool ClearAllButBase);
	virtual void Draw();

	void DumpBones();
	void DrawSkeleton(bool ShowLabels);
	void DrawAttachments();
	void DrawMesh();

	// skeleton configuration
	void SetBoneScale(const char *BoneName, float scale = 1.0f);
	//!! SetBone[Direction|Location|Rotation]()

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
	void AnimStopLooping(int Channel);
	void FreezeAnimAt(float Time, int Channel = 0);

	// animation state

	// get current animation information:
	// Frame     = 0..NumFrames
	// NumFrames = animation length, frames
	// Rate      = frames per seconds
	void GetAnimParams(int Channel, const char *&AnimName,
		float &Frame, float &NumFrames, float &Rate) const;

	bool HasAnim(const char *AnimName) const
	{
		return FindAnim(AnimName) != INDEX_NONE;
	}
	bool IsTweening(int Channel = 0)
	{
		return GetStage(Channel).TweenTime > 0;
	}

	// animation blending
	void SetBlendParams(int Channel, float BlendAlpha, const char *BoneName = NULL);
	void SetBlendAlpha(int Channel, float BlendAlpha);
	void SetSecondaryAnim(int Channel, const char *AnimName = NULL);
	void SetSecondaryBlend(int Channel, float BlendAlpha);
	//?? -	AnimBlendToAlpha() - animate BlendAlpha coefficient
	//?? -	functions to smoothly replace current animation with another in a fixed time
	//??	(run new anim as secondary, ramp alpha from 0 to 1, and when blend becomes 1.0f
	//??	- replace 1st anim with 2nd, and clear 2nd

	// animation enumeration
	int GetAnimCount() const;
	const char *GetAnimName(int Index) const;
	void UpdateAnimation(float TimeDelta);

	const CAnimSet *GetAnim() const
	{
		return Animation;
	}

protected:
	const CAnimSet		 *Animation;
	// mesh data
	void				 *DataBlock;// all following data is resided here, aligned to 16 bytes
	struct CMeshBoneData *BoneData;
	struct CSkinVert     *Skinned;	// soft-skinned vertices
	CVec3		*InfColors;			// debug: color-by-influence for vertices
	int			LastLodNum;			// used to detect requirement to rebuild Wedges[]
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
	void TransformMesh();
	int FindBone(const char *BoneName) const;
	int FindAnim(const char *AnimName) const;
	void PlayAnimInternal(const char *AnimName, float Rate, float TweenTime, int Channel, bool Looped);
	void UpdateSkeleton();
	void BuildInfColors();
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

	// not 'const *mesh' because can call BuildTangents()
	void SetMesh(CStaticMesh *Mesh)
	{
		pMesh = Mesh;
	}

	virtual void Draw();

	// mesh state
	int			LodNum;
	int			UVIndex;

protected:
	CStaticMesh *pMesh;
};


#endif // __MESHINSTANCE_H__
