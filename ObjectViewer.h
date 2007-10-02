#include "Core.h"
#include "UnCore.h"
#include "UnMesh.h"
#include "UnPackage.h"

#include "GlWindow.h"


#define TEST_FILES		1		// comment line to disable some notifications


/*-----------------------------------------------------------------------------
	Basic object viewer
-----------------------------------------------------------------------------*/

class CObjectViewer
{
public:
	UObject*	Object;

	CObjectViewer(UObject *Obj)
	:	Object(Obj)
	{}

	virtual void Dump()
	{}

	virtual void ShowHelp()
	{}

	virtual void Draw2D()
	{
		GL::textf("Package: %s\n"
				  "Class  : %s\n"
				  "Object : %s\n\n",
				  Object->Package->SelfName, Object->GetClassName(), Object->Name);
	}

	virtual void Draw3D()
	{}
	virtual void ProcessKey(unsigned char key)
	{}
};


/*-----------------------------------------------------------------------------
	Basic mesh viewer (ULodMesh)
-----------------------------------------------------------------------------*/

class CMeshViewer : public CObjectViewer
{
public:
	bool	bShowNormals;

	CMeshViewer(ULodMesh *Mesh)
	:	CObjectViewer(Mesh)
	,	bShowNormals(false)
	{}

	virtual void ShowHelp()
	{
		GL::text("N           show normals\n");
	}

	virtual void ProcessKey(unsigned char key)
	{
		switch (key)
		{
		case 'n':
			bShowNormals = !bShowNormals;
			break;
		default:
			CObjectViewer::ProcessKey(key);
		}
	}

	virtual void Dump();
	virtual void Draw3D();
};


/*-----------------------------------------------------------------------------
	Vertex mesh viewer (UVertMesh)
-----------------------------------------------------------------------------*/

class CVertMeshViewer : public CMeshViewer
{
public:
	int		FrameNum;

	CVertMeshViewer(UVertMesh *Mesh)
	:	CMeshViewer(Mesh)
	,	FrameNum(0)
	{}

	virtual void Dump();
	virtual void Draw3D();

	virtual void ProcessKey(unsigned char key)
	{
		int FrameCount = (static_cast<UVertMesh*>(Object))->FrameCount;
		switch (key)
		{
		case '1':
			if (++FrameNum >= FrameCount)
				FrameNum = 0;
			break;
		case '2':
			if (--FrameNum < 0)
				FrameNum = FrameCount-1;
			break;
		default:
			CMeshViewer::ProcessKey(key);
		}
	}
};


/*-----------------------------------------------------------------------------
	Skeletal mesh viewer (USkeletalMesh)
-----------------------------------------------------------------------------*/

class CSkelMeshViewer : public CMeshViewer
{
public:
	int		LodNum;

	CSkelMeshViewer(USkeletalMesh *Mesh)
	:	CMeshViewer(Mesh)
	,	LodNum(-1)
	{}

	virtual void ShowHelp()
	{
		CMeshViewer::ShowHelp();
		GL::text("L           cycle mesh LODs\n");
	}

	virtual void Draw2D()
	{
		CMeshViewer::Draw2D();
		if (LodNum < 0)
			GL::textf("LOD: base mesh\n");
		else
			GL::textf("LOD: %d\n", LodNum);
	}

	virtual void Dump();
	virtual void Draw3D();

	virtual void ProcessKey(unsigned char key)
	{
		switch (key)
		{
		case 'l':
			if (++LodNum >= static_cast<USkeletalMesh*>(Object)->StaticLODModels.Num())
				LodNum = -1;
			break;
		default:
			CMeshViewer::ProcessKey(key);
		}
	}
};


/*-----------------------------------------------------------------------------
	Macros for easier object format verification
-----------------------------------------------------------------------------*/

#if TEST_FILES

#define VERIFY(MeshField, MField) \
	if (Mesh->MeshField != Mesh->MField) appNotify(#MeshField" "#MField);
//#define VERIFY2(MeshField, MField) \
//	if (M->MeshField != Mesh->MField) appNotify(#MeshField" "#MField);
#define VERIFY_NULL(Field) \
	if (Mesh->Field != 0) appNotify("%s != 0", #Field);
#define VERIFY_NOT_NULL(Field) \
	if (Mesh->Field == 0) appNotify("%s == 0", #Field);


#define TEST_ARRAY(array)							\
	{												\
		int min = 0x40000000, max = -0x40000000;	\
		for (int i = 0; i < array.Num(); i++)		\
		{											\
			if (array[i] < min) min = array[i];		\
			if (array[i] > max) max = array[i];		\
		}											\
		if (array.Num())							\
			printf(#array": [%d .. %d]\n", min, max); \
	}

#define TEST_ARRAY2(array,field)					\
	{												\
		int min = 0x40000000, max = -0x40000000;	\
		for (int i = 0; i < array.Num(); i++)		\
		{											\
			if (array[i].field < min) min = array[i].field; \
			if (array[i].field > max) max = array[i].field; \
		}											\
		if (array.Num())							\
			printf(#array"."#field": [%d .. %d]\n", min, max); \
	}

#endif
