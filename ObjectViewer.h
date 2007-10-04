#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnMesh.h"
#include "UnPackage.h"

#include "GlWindow.h"


#define TEST_FILES		1		// comment line to disable some notifications


#if TEST_FILES
#	define TEST_OBJECT	virtual void Test()
#else
#	define TEST_OBJECT
#endif


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
	bool	bShowNormals;

	CMeshViewer(ULodMesh *Mesh)
	:	CObjectViewer(Mesh)
	,	bShowNormals(false)
	{
		// compute model center by Z-axis (vertical)
		CVec3 offset;
		offset.Set(0, 0, (Mesh->BoundingBox.Max.Z + Mesh->BoundingBox.Min.Z) / 2);
		GL::SetViewOffset(offset);
	}

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
	TEST_OBJECT;
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
	TEST_OBJECT;
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
	int		CurrAnim;
	float	AnimTime;

	CSkelMeshViewer(USkeletalMesh *Mesh)
	:	CMeshViewer(Mesh)
	,	LodNum(-1)
	,	CurrAnim(-1)
	,	AnimTime(0)
	{}

	virtual void ShowHelp()
	{
		CMeshViewer::ShowHelp();
		GL::text("L           cycle mesh LODs\n");
		GL::text("[]          prev/next animation\n");
		GL::text("<>          prev/next frame\n");
	}

	virtual void Dump();
	TEST_OBJECT;
	virtual void Draw2D();
	virtual void Draw3D();

	virtual void ProcessKey(unsigned char key)
	{
		USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
		int NumAnims = 0, NumFrames = 0;
		if (Mesh->Animation)
			NumAnims  = Mesh->Animation->AnimSeqs.Num();
		if (CurrAnim >= 0)
			NumFrames = Mesh->Animation->AnimSeqs[CurrAnim].NumFrames;

		switch (key)
		{
		case 'l':
			if (++LodNum >= Mesh->StaticLODModels.Num())
				LodNum = -1;
			break;
		case '[':
			if (--CurrAnim < -1)
				CurrAnim = NumAnims - 1;
			AnimTime = 0;
			break;
		case ']':
			if (++CurrAnim >= NumAnims)
				CurrAnim = -1;
			AnimTime = 0;
			break;
		case ',':		// '<'
			AnimTime -= 0.1;
			if (AnimTime < 0)
				AnimTime = 0;
			break;
		case '.':		// '>'
			if (NumFrames > 0)
			{
				AnimTime += 0.1;
				if (AnimTime > NumFrames - 1)
					AnimTime = NumFrames - 1;
			}
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
