/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxscenecheckutility.h
#ifndef _FBXSDK_SCENE_CHECK_UTILITY_H_
#define _FBXSDK_SCENE_CHECK_UTILITY_H_

#include <fbxsdk/fbxsdk_def.h>
#include <fbxsdk/core/base/fbxarray.h>
#include <fbxsdk/core/base/fbxstring.h>
#include <fbxsdk/fbxsdk_nsbegin.h>

class FbxScene;
class FbxStatus;

/** \brief This class defines functions to check the received scene graph for issues.
  * remark This is still an experimental class and it is not expected to validate
           every single data object.
  */
class FBXSDK_DLL FbxSceneCheckUtility 
{
public:
	enum ECheckMode {
		eCheckCycles = 1,
		eCheckAnimationEmptyLayers = 2,
		eCheckAnimatioCurveData = 4,
		eCheckAnimationData = 6,
		eCheckGeometryData = 8,
		eCheckOtherData = 16,
		eCkeckData = 30 // includes Geometry,Animation&Other
	};

	/** Construct the object
	  * pScene  Input scene to check
	  * pStatus FbxStatus object to set error codes in (optional)
	  * pDetails Details messages of issues found (optional)
	  *
	  * remark The Details array and its content must be cleared by the caller
	  */
	FbxSceneCheckUtility(const FbxScene* pScene, FbxStatus* pStatus=NULL, FbxArray<FbxString*>* pDetails = NULL);
	~FbxSceneCheckUtility();

	/** Check for issues in the scene
	  * return	\false if any issue is found in the scene
	  * remark  Depending on the check mode settings, the processing time can increase dramatically.
	  * remark  If a status and/or details object is provided, the error code is set and, details info is
	            logged.
	  */
	bool Validate(ECheckMode pCheckMode=eCheckCycles);

/*****************************************************************************************************************************
** WARNING! Anything beyond these lines is for internal use, may not be documented and is subject to change without notice! **
*****************************************************************************************************************************/
#ifndef DOXYGEN_SHOULD_SKIP_THIS
protected:
	bool HaveCycles();
	bool HaveInvalidData(int pCheckMode);

private:
	FbxSceneCheckUtility();
	FbxSceneCheckUtility(const FbxSceneCheckUtility& pOther); 

	// Check functions return true if valid, false otherwise
	bool CheckMappingMode(FbxLayerElement::EMappingMode pMappingMode, const FbxString& pPrefix);
	bool CheckReferenceMode(FbxLayerElement::EReferenceMode pReferenceMode, const FbxString& pPrefix);
	bool CheckSurfaceMode(FbxGeometry::ESurfaceMode pSurfaceMode, const FbxString& pPrefix);
	bool CheckSurfaceType(FbxNurbs::EType pSurfaceType, const FbxString& pPrefix, const char* pDir);

	int  MaxCountLimit(FbxLayerElement::EMappingMode pMappingMode, 
						int pCtrlPointsCount, 
						int pVerticesCount,
						int pPolygonsCount,
						int pEdgesCount,
						int pElseCount);

	enum {
		eNoRestriction,
		eDirectOnly,
		eIndexOnly,
	};

	template <class T> 
	bool CheckLayerElement(FbxLayerElementTemplate<T>* pLET, 
						   int pMaxCount, 
						   const char* pId, 
						   const FbxString& pPrefix,
						   int pRestriction = eNoRestriction);

	bool MeshHaveInvalidData(FbxGeometry* pGeom, const FbxString& pName);
	bool NurbsHaveInvalidData(FbxGeometry* pGeom, const FbxString& pName);
	bool GeometryHaveInvalidData(FbxGeometry* pGeom, const FbxString& pBase);

	bool GlobalSettingsHaveInvalidData();

	bool AnimationHaveInvalidData(int pCheckMode);
	bool AnimationHaveEmptyLayers();
	bool AnimationHaveInvalidCurveData();

	const FbxScene* mScene;

	FbxStatus*            mStatus;
	FbxArray<FbxString*>* mDetails;
	FbxString             mBuffer;

#endif /* !DOXYGEN_SHOULD_SKIP_THIS *****************************************************************************************/
};

#include <fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_UTILS_ROOT_NODE_UTILITY_H_ */
