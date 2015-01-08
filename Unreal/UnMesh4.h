#ifndef __UNMESH4_H__
#define __UNMESH4_H__

#if UNREAL4

#include "UnMesh.h"			// common types

// forwards
class UMaterialInterface;

#define MAX_STATIC_UV_SETS_UE4			8
#define MAX_STATIC_LODS_UE4				4


/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

// forwards (structures are declared in cpp)
struct FStaticMeshLODModel4;

class UStaticMesh4 : public UObject
{
	DECLARE_CLASS(UStaticMesh4, UObject);
public:
	CStaticMesh				*ConvertedMesh;

	UObject					*BodySetup;			// UBodySetup
	UObject*				NavCollision;		// UNavCollision
	FGuid					LightingGuid;

	typedef UObject UStaticMeshSocket;			//!! remove when UStaticMeshSocket will be declared
	TArray<UStaticMeshSocket*> Sockets;

	// FStaticMeshRenderData fields
	FBoxSphereBounds		Bounds;
	float					ScreenSize[MAX_STATIC_LODS_UE4];
	float					StreamingTextureFactors[MAX_STATIC_UV_SETS_UE4];
	float					MaxStreamingTextureFactor;
	bool					bReducedBySimplygon;
	bool					bLODsShareStaticLighting;
	TArray<FStaticMeshLODModel4> Lods;

	BEGIN_PROP_TABLE
		//!! review table, add props
		PROP_OBJ(BodySetup)						// this property is serialized twice - as a property and in UStaticMesh::Serialize
	END_PROP_TABLE

	UStaticMesh4();
	virtual ~UStaticMesh4();

	virtual void Serialize(FArchive &Ar);

protected:
	void ConvertMesh();
};


#define REGISTER_MESH_CLASSES_U4 \
	REGISTER_CLASS_ALIAS(UStaticMesh4, UStaticMesh)


#endif // UNREAL4
#endif // __UNMESH4_H__
