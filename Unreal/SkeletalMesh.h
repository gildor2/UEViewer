#ifndef __SKELETALMESH_H__
#define __SKELETALMESH_H__


/*!!
	Data exclusion:
	- Tribes
	  - uses MotionChunk.BoneIndices to exclude animation of particular bone in specific animation
	    * exclude individual CAnimTrack for one CAnimSequence
	    * can do this by empty KeyQuat and KeyPos
	    ! BoneIndices is set by MY CODE only depending on flags value (see FixTribesMotionChunk()), originally it is not
	      used at all
	- UC2
	  - may have empty AnalogTrack.KeyPos or AnalogTrack.KeyQuat, in this case use value from BindPose
	    * exclude KeyQuat and/or KeyPos from CAnimTrack for one CAnimSequence
	- UE3
	  - control from UAnimSet for all sequences by bAnimRotationOnly and UseTranslationBoneNames/ForceMeshTranslationBoneNames
	  - rotation is always taken from the animation
	  - root bone is always uses translation from the animation
	  - other bones uses translation from BindPose when
	    (bAnimRotationOnly && UseTranslationBoneNames[Bone]) || ForceMeshTranslationBoneNames[Bone]
	    * ForceMeshTranslationBoneNames tracks are removed from UAnimSequence (has single invalid translation key)
	* so, we require 2 kinds of track exclusion: removal of the per-track arrays, and removal of the per-sequence tracks
*/


/*!!
	CAnimSet conversion stages:
	- replace psax exporter with psa + text config file
	  - update ActorX Importer to support this
	- cleanup UnMesh.h header usage (not a time for this, require mesh refactoring?)

	VERIFY:
	* UE2:
	  ? export md5/psa (try to load somewhere)
	* UE3:
	  ? export md5/psa (try to load somewhere)
*/


struct CAnimTrack
{
	TArray<CQuat>			KeyQuat;
	TArray<CVec3>			KeyPos;
	// 3 time arrays; should be used either KeyTime or KeyQuatTime + KeyPosTime
	// When the corresponding array is empty, it will assume that Array[i] == i
	TArray<float>			KeyTime;
	TArray<float>			KeyQuatTime;
	TArray<float>			KeyPosTime;

	// DstPos and DstQuat will not be changed when KeyPos and KeyQuat are empty
	void GetBonePosition(float Frame, float NumFrames, bool Loop, CVec3 &DstPos, CQuat &DstQuat) const;
};


struct CAnimSequence
{
	FName					Name;					// sequence's name
	int						NumFrames;
	float					Rate;
	TArray<CAnimTrack>		Tracks;					// for each CAnimSet.TrackBoneNames
};


class CAnimSet
{
public:
	UObject					*OriginalAnim;			//?? make common for all mesh classes
	TArray<FName>			TrackBoneNames;
	TArray<CAnimSequence>	Sequences;

	bool					AnimRotationOnly;
	TArray<bool>			UseAnimTranslation;		// per bone; used with AnimRotationOnly mode
	TArray<bool>			ForceMeshTranslation;	// pre bone; used regardless of AnimRotationOnly

	CAnimSet(UObject *Original)
	:	OriginalAnim(Original)
	{}

	bool ShouldAnimateTranslation(int BoneIndex) const
	{
		if (BoneIndex == 0)							// root bone is always fully animated
			return true;
		if (ForceMeshTranslation.Num() && ForceMeshTranslation[BoneIndex])
			return false;
		if (!AnimRotationOnly)
			return true;
		if (UseAnimTranslation.Num() && UseAnimTranslation[BoneIndex])
			return true;
		return false;
	}
};


#endif // __SKELETALMESH_H__
