/****************************************************************************************
 
   Copyright (C) 2017 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxvideo.h
#ifndef _FBXSDK_SCENE_AUDIO_H_
#define _FBXSDK_SCENE_AUDIO_H_

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/scene/fbxmediaclip.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

/**	FBX SDK audio class.
  * \nosubgrouping
  */
class FBXSDK_DLL FbxAudio : public FbxMediaClip
{
	FBXSDK_OBJECT_DECLARE(FbxAudio, FbxMediaClip);

public:
    /**
	  * \name Audio object creation.
	  *
	  */
	//@{
		/** Create a FbxAnimCurve.
		  * \param pContainer Scene to which the created audio clip belongs.
		  * \param pName Name of the audio clip.
		  * \return Newly created audio clip
		  */
		static FbxAudio* Create(FbxScene* pContainer, const char* pName);
	//@}
	/**
	  *\name Reset audio
	  */
	//@{
		//! Reset the audio to default values.
		void Reset();
	//@}

    /**
      * \name Audio attributes Management
      */
    //@{

		//! Audio file bit rate value (bit/s).
		FbxPropertyT<FbxInt> 		BitRate;

		//! Audio file sample rate value (Hz).
		FbxPropertyT<FbxInt> 	SampleRate;

		//! Audio file number of channels.
		FbxPropertyT<FbxUChar>		Channels;

		//! Audio file length.
		FbxPropertyT<FbxTime>		Duration;

		/** Compound property to be used if animation data needs to be connected.
			* In this case, specific properties should be added to this one with the Animatable flag set.
			* \remarks By default, the Volume property is always created first and, unless explicitily
			*          removed by a "client", will always exist.
			*/
		FbxProperty					AnimFX;
    //@}
    
	/**
	  *\name Utility section
	  */
	//@{
		/** Access the Volume child property of the AnimFX.
		  * \return The volume property if it exists or an invalid property.
		  */
		FbxProperty Volume();
	//@}
/*****************************************************************************************************************************
** WARNING! Anything beyond these lines is for internal use, may not be documented and is subject to change without notice! **
*****************************************************************************************************************************/
#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
	void InitializeAnimFX();

protected:
    virtual void Construct(const FbxObject* pFrom);
    virtual void ConstructProperties(bool pForceSet);

public:
	virtual FbxObject& Copy(const FbxObject& pObject);

#endif /* !DOXYGEN_SHOULD_SKIP_THIS *****************************************************************************************/
};

#include <fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_SCENE_AUDIO_H_ */
