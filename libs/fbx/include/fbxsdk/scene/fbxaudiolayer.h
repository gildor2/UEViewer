/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxanimlayer.h
#ifndef _FBXSDK_SCENE_AUDIO_LAYER_H_
#define _FBXSDK_SCENE_AUDIO_LAYER_H_

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/scene/fbxcollection.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

class FbxAudio;

/** The audio layer is a collection of audio clips. Its purpose is to store
  * a variable number of FbxAudio objects representing audio clips. The class provides
  * a Mute, Solo and Lock states flags (bool properties), as well as a Clolor (double3 property).
  * There is no evaluation system for audio layers inside the FBX SDK and an audio layer can be
  * empty.
  * \nosubgrouping
  */
class FBXSDK_DLL FbxAudioLayer : public FbxCollection
{
    FBXSDK_OBJECT_DECLARE(FbxAudioLayer, FbxCollection);

public:
    //////////////////////////////////////////////////////////////////////////
    //
    // Properties
    //
    //////////////////////////////////////////////////////////////////////////

    /** This property stores the mute state.
      * The mute state indicates that this layer should be excluded from the evaluation.
      *
      * Default value is \c false
      */
    FbxPropertyT<FbxBool>         Mute;

    /** This property stores the solo state.
      * The solo state indicates that this layer is the only one that should be
      * processed during the evaluation.
      *
      * Default value is \c false
      */
    FbxPropertyT<FbxBool>         Solo;

	/** This property stores the volume increment value.
	  * This property can be animated.
	  * Default value is \c 0.0
	  */
	FbxPropertyT<FbxDouble>		  Volume;

    /** This property stores the lock state.
      * The lock state indicates that this layer has been "locked" from editing operations
      * and should no longer receive keyframes.
      *
      * Default value is \c false
      */
    FbxPropertyT<FbxBool>         Lock;

    /** This property stores the display color.
      * This color can be used by applications if they display a graphical representation
      * of the layer. The FBX SDK does not use it but guarantees that the value is saved to the FBX
      * file and retrieved from it.
      *
      * Default value is \c (0.8, 0.8, 0.8)
      */
    FbxPropertyT<FbxDouble3>       Color;

	//! Reset this object properties to their default value.
	void Reset();

/*****************************************************************************************************************************
** WARNING! Anything beyond these lines is for internal use, may not be documented and is subject to change without notice! **
*****************************************************************************************************************************/
#ifndef DOXYGEN_SHOULD_SKIP_THIS
protected:
	virtual void ConstructProperties(bool pForceSet);

#endif /* !DOXYGEN_SHOULD_SKIP_THIS *****************************************************************************************/
};

#include <fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_SCENE_AUDIO_LAYER_H_ */
