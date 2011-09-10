#include "GlWindow.h"
#include "UnPackage.h"			// for CObjectViewer::Draw2D()


class CMeshInstance;
class CLodMeshInstance;
class CVertMeshInstance;
class CSkelMeshInstance;


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
	{
		SetDistScale(1);
		ResetView();
		SetViewOffset(nullVec3);
	}
	virtual ~CObjectViewer()
	{}

	virtual void Dump()
	{
		appPrintf("\nObject info:\n============\n");
		appPrintf("ClassName: %s ObjectName: %s\n", Object->GetClassName(), Object->Name);
		Object->GetTypeinfo()->DumpProps(Object);
	}
#if TEST_FILES
	virtual void Test()
	{}
#endif

	virtual void ShowHelp()
	{}

	virtual void Draw2D()
	{
		DrawTextLeft(S_GREEN"Package:"S_WHITE" %s\n"
					 S_GREEN"Class  :"S_WHITE" %s\n"
					 S_GREEN"Object :"S_WHITE" %s\n",
					 Object->Package->Filename, Object->GetClassName(), Object->Name);
	}

	virtual void Draw3D()
	{}
	virtual void ProcessKey(int key)
	{}
};


/*-----------------------------------------------------------------------------
	Material viewer
-----------------------------------------------------------------------------*/

class CMaterialViewer : public CObjectViewer
{
public:
	static bool		ShowOutline;

	CMaterialViewer(UUnrealMaterial *Material)
	:	CObjectViewer(Material)
	{}
	virtual ~CMaterialViewer();

	virtual void ShowHelp();
	virtual void ProcessKey(int key);

	virtual void Draw2D();
	virtual void Draw3D();
};


/*-----------------------------------------------------------------------------
	Basic mesh viewer (ULodMesh)
-----------------------------------------------------------------------------*/

class CMeshViewer : public CObjectViewer
{
public:
	// linked data
	CMeshInstance	*Inst;

	CMeshViewer(UObject *Mesh)
	:	CObjectViewer(Mesh)
	{}
	virtual ~CMeshViewer();

	virtual void ShowHelp();
	virtual void ProcessKey(int key);
//	virtual void Dump();
//	virtual void Draw2D();
	virtual void Draw3D();
};


class CLodMeshViewer : public CMeshViewer
{
public:
	int				AnimIndex;
	unsigned		CurrentTime;

	CLodMeshViewer(ULodMesh *Mesh);
	void Initialize();

	static void TagMesh(CLodMeshInstance *NewInst);

	virtual void ShowHelp();
	virtual void ProcessKey(int key);
	virtual void Dump();
	TEST_OBJECT;
	virtual void Draw2D();
	virtual void Draw3D();

	static TArray<CLodMeshInstance*> Meshes;	// for displaying multipart meshes
};


/*-----------------------------------------------------------------------------
	Vertex mesh viewer (UVertMesh)
-----------------------------------------------------------------------------*/

class CVertMeshViewer : public CLodMeshViewer
{
public:
	CVertMeshViewer(UVertMesh *Mesh);
	virtual void Dump();
	TEST_OBJECT;
};


/*-----------------------------------------------------------------------------
	Skeletal mesh viewer (USkeletalMesh)
-----------------------------------------------------------------------------*/

class CSkelMeshViewer : public CLodMeshViewer
{
public:
	CSkelMeshViewer(USkeletalMesh *Mesh);
	virtual void ShowHelp();
	virtual void Dump();
	TEST_OBJECT;
	virtual void Draw2D();
	virtual void ProcessKey(int key);
};


/*-----------------------------------------------------------------------------
	Static mesh viewer (UStaticMesh)
-----------------------------------------------------------------------------*/

class CStatMeshViewer : public CMeshViewer
{
public:
	CStatMeshViewer(UStaticMesh *Mesh);
	virtual void Dump();
	virtual void Draw2D();
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
			appPrintf(#array": [%d .. %d]\n", min, max); \
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
			appPrintf(#array"."#field": [%d .. %d]\n", min, max); \
	}

#endif
