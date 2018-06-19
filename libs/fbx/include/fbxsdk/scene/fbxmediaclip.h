/****************************************************************************************
 
   Copyright (C) 2017 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxmediaclip.h
#ifndef _FBXSDK_SCENE_MEDIACLIP_H_
#define _FBXSDK_SCENE_MEDIACLIP_H_

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/core/fbxobject.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

/**	FBX SDK media clip class.
  * \nosubgrouping
  */
class FBXSDK_DLL FbxMediaClip : public FbxObject
{
	FBXSDK_ABSTRACT_OBJECT_DECLARE(FbxMediaClip, FbxObject);

public:
	/**
	  *\name Media clip class Reset
	  */
	//@{
		//! Reset the clip to default values.
		virtual void Reset();
	//@}

    /**
      * \name Media clip attributes Management
      */
    //@{
		/** Specify the media full filename.
		  * \param pName     media full filename.
		  * \return          \c True,if update successfully, \c false otherwise.
		  */
		virtual bool SetFileName(const char* pName);

		/** Retrieve the media full filename.
		  * \return      Media full filename.
		  */
		FbxString GetFileName () const;

		/** Specify the media relative filename.
		  * \param pName     Media relative filename.
		  * \return          \c True, if update successfully, \c false otherwise.
		  */
		virtual bool SetRelativeFileName(const char* pName);

		/** Retrieve the media relative filename.
		  * \return      Media relative filename.
		  */
		FbxString GetRelativeFileName() const;

		/** Set the clip color.
		  * The color property can be used to display media clips on the user interface. It has no
		  * impact on the data itself and only uses the RGB components.
		  * \param pColor	New color of the clip.
		  */
		void SetColor(FbxColor pColor);

		/** Retrieve the clip color.
		  * return Clip color.
		  */
		FbxColor GetColor() const;

		/** Set the play speed of the media clip.
		  * \param pPlaySpeed    Playback speed of the clip.
		  * \remarks             The parameter value is not checked. It is the responsibility
		  *                      of the caller to deal with bad playback speed values.
		  */
		void SetPlaySpeed(double pPlaySpeed);

		/** Retrieve the play speed of the media clip.
		  * \return Playback speed.
		  */
		double GetPlaySpeed() const;

		/** Set the clip start time.
		  * \param pTime	Start time of the media file.
		  */
		void SetClipIn(FbxTime pTime);

		/** Retrieve the clip start time.
		  * \return		The current clip start time.
		  */
		FbxTime GetClipIn() const;

		/** Set the clip end time.
		  * \param pTime	End time of the media file.
		  */
		void SetClipOut(FbxTime pTime);

		/** Retrieve the clip start time.
		  * \return		The current clip start time.
		  */
		FbxTime GetClipOut() const;

		/** Set the time offset.
		  * The offset can be used to shift the playback start time of the clip.
		  * \param pTime     Time offset of the clip.
		  */
		void SetOffset(FbxTime pTime);

		/* Retrieve the time offset.
		 * \return     The current time shift.
		 */
		FbxTime GetOffset() const;

		/** Set the Free Running state of the media clip.
		  * The Free Running flag can be used by a client application to implement a
		  * playback scheme that is independent of the main timeline.
		  * \param pState     State of the Free running flag.
		  */
		void SetFreeRunning(bool pState);

		/** Retrieve the Free Running state.
		  * \return     Current free running flag.
		  */
		bool GetFreeRunning() const;


		/** Set the Loop state of the media clip.
		  * The Loop flag can be used by a client application to implement the loop
		  * playback of the media clip.
		  * \param pLoop     State of the loop flag.
		  */
		void SetLoop(bool pLoop);

		/** Retrieve the Loop state.
		  * \return     Current loop flag.
		  */
		bool GetLoop() const;

		/** Set the Mute state of the media clip.
		  * The Mute flag can be used by a client application to implement the muting
		  * of the media clip.
		  * \param pMute     State of the mute flag.
		  */
		void SetMute(bool pMute);

		/** Retrieve the Mute state.
		  * \return     Current mute flag.
		  */
		bool GetMute() const;

		/** Media clip access mode.
		  */
		enum EAccessMode
		{
			eDisk,
			eMemory,
			eDiskAsync
		};

		/** Set the clip Access Mode.
		  * \param pAccessMode     Clip access mode identifier.
		  */
		void SetAccessMode(EAccessMode pAccessMode);

		/** Retrieve the clip Access Mode.
		  * \return     Clip access mode identifier.
		  */
		EAccessMode GetAccessMode() const;
    //@}
    
/*****************************************************************************************************************************
** WARNING! Anything beyond these lines is for internal use, may not be documented and is subject to change without notice! **
*****************************************************************************************************************************/
#ifndef DOXYGEN_SHOULD_SKIP_THIS
protected:
    virtual void Construct(const FbxObject* pFrom);
    virtual void ConstructProperties(bool pForceSet);

public:
	virtual FbxObject& Copy(const FbxObject& pObject);

    virtual FbxStringList GetTypeFlags() const;

    void SetOriginalFormat(bool pState);
    bool GetOriginalFormat() const;
    void SetOriginalFilename(const char* pOriginalFilename);
    const char* GetOriginalFilename() const;

	FbxPropertyT<FbxDouble3>	Color;
	FbxPropertyT<FbxTime>		ClipIn;
	FbxPropertyT<FbxTime>		ClipOut;
    FbxPropertyT<FbxTime>		Offset;
    FbxPropertyT<FbxDouble>		PlaySpeed;
    FbxPropertyT<FbxBool>		FreeRunning;
    FbxPropertyT<FbxBool>		Loop;
	FbxPropertyT<FbxBool>		Mute;
    FbxPropertyT<EAccessMode>	AccessMode;

protected:
    void Init();
	FbxPropertyT<FbxString>		Path;
	FbxPropertyT<FbxString>     RelPath;

    bool		mOriginalFormat;
    FbxString	mOriginalFilename;

#endif /* !DOXYGEN_SHOULD_SKIP_THIS *****************************************************************************************/
};

inline EFbxType FbxTypeOf(const FbxMediaClip::EAccessMode&){ return eFbxEnum; }

#include <fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_SCENE_MEDIACLIP_H_ */
