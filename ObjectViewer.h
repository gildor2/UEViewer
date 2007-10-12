#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMesh.h"
#include "UnPackage.h"

#include "GlWindow.h"

class CMeshInstance;
class CVertMeshInstance;
class CSkelMeshInstance;


#define TEST_FILES		1		// comment line to disable some notifications


#if TEST_FILES
#	define TEST_OBJECT	virtual void Test()
#else
#	define TEST_OBJECT
#endif


//?? move outside
UMaterial *BindDefaultMaterial();


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
	virtual ~CObjectViewer()
	{}

	virtual void Dump()
	{}
#if TEST_FILES
	virtual void Test()
	{}
#endif

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
	// view mode
	bool			bShowNormals;
	bool			bColorMaterials;
	// linked data
	CMeshInstance	*Inst;

	CMeshViewer(ULodMesh *Mesh)
	:	CObjectViewer(Mesh)
	,	bShowNormals(false)
	{
		// compute model center by Z-axis (vertical)
		CVec3 offset;
		offset.Set(0, 0, (Mesh->BoundingBox.Max.Z + Mesh->BoundingBox.Min.Z) / 2);
		GL::SetViewOffset(offset);
	}

	virtual ~CMeshViewer()
	{
		delete Inst;
	}

	virtual void ShowHelp()
	{
		GL::text("N           show normals\n");
		GL::text("M           colorize materials\n");
	}

	virtual void ProcessKey(unsigned char key)
	{
		switch (key)
		{
		case 'n':
			bShowNormals = !bShowNormals;
			break;
		case 'm':
			bColorMaterials = !bColorMaterials;
			break;
		default:
			CObjectViewer::ProcessKey(key);
		}
	}

	virtual void Dump();
	TEST_OBJECT;
	virtual void Draw3D();
};


/*-----------------------------------------------------------------------------
	Vertex mesh viewer (UVertMesh)
-----------------------------------------------------------------------------*/

class CVertMeshViewer : public CMeshViewer
{
public:
	CVertMeshViewer(UVertMesh *Mesh);
	virtual void Dump();
	TEST_OBJECT;
	virtual void ProcessKey(unsigned char key);
};


/*-----------------------------------------------------------------------------
	Skeletal mesh viewer (USkeletalMesh)
-----------------------------------------------------------------------------*/

class CSkelMeshViewer : public CMeshViewer
{
public:
	int		ShowSkel;		// 0 - mesh, 1 - mesh+skel, 2 - skel only

	CSkelMeshViewer(USkeletalMesh *Mesh);
	virtual void ShowHelp()
	{
		CMeshViewer::ShowHelp();
		GL::text("L           cycle mesh LODs\n");
		GL::text("S           show skeleton\n");
		GL::text("[]          prev/next animation\n");
		GL::text("<>          prev/next frame\n");
	}

	virtual void Dump();
	TEST_OBJECT;
	virtual void Draw2D();
	virtual void ProcessKey(unsigned char key);
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
