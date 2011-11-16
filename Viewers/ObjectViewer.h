#include "GlWindow.h"


class CMeshInstance;
class CVertMeshInstance;
class CSkelMeshInstance;

class CSkeletalMesh;
class CStaticMesh;


#define TEST_FILES		1		// comment line to disable some notifications


/*-----------------------------------------------------------------------------
	Basic object viewer
-----------------------------------------------------------------------------*/

class CObjectViewer
{
public:
	UObject*	Object;

	CObjectViewer(UObject *Obj);
	virtual ~CObjectViewer()
	{}

	virtual void Dump();

#if TEST_FILES
	virtual void Test()
	{}
#endif

	virtual void Export();
	virtual void ShowHelp();
	virtual void Draw2D();
	virtual void ProcessKey(int key);

	virtual void Draw3D()
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

	void InitViewerPosition(const CVec3 &Mins, const CVec3 &Maxs);	//?? CBox Bounds?

	virtual void ShowHelp();
	virtual void ProcessKey(int key);
	virtual void Draw3D();

	void PrintMaterialInfo(int Index, UUnrealMaterial *Material, int NumFaces);
};


/*-----------------------------------------------------------------------------
	Vertex mesh viewer (UVertMesh)
-----------------------------------------------------------------------------*/

class CVertMeshViewer : public CMeshViewer
{
public:
	int				AnimIndex;
	unsigned		CurrentTime;

	CVertMeshViewer(UVertMesh *Mesh);

	virtual void ShowHelp();
	virtual void ProcessKey(int key);
	virtual void Dump();
	virtual void Export();
#if TEST_FILES
	virtual void Test();
#endif
	virtual void Draw2D();
	virtual void Draw3D();
};


/*-----------------------------------------------------------------------------
	Skeletal mesh viewer (USkeletalMesh)
-----------------------------------------------------------------------------*/

class CSkelMeshViewer : public CMeshViewer
{
public:
	int				AnimIndex;
	unsigned		CurrentTime;

	CSkelMeshViewer(CSkeletalMesh *Mesh);

	static void TagMesh(CSkelMeshInstance *NewInst);

	virtual void ShowHelp();
	virtual void Dump();
	virtual void Export();
	virtual void Draw2D();
	virtual void Draw3D();
	virtual void ProcessKey(int key);

	static TArray<CSkelMeshInstance*> Meshes;	// for displaying multipart meshes

private:
	CSkeletalMesh	*Mesh;
};


/*-----------------------------------------------------------------------------
	Static mesh viewer (CStaticMesh)
-----------------------------------------------------------------------------*/

class CStatMeshViewer : public CMeshViewer
{
public:
	CStatMeshViewer(CStaticMesh *Mesh);
	virtual void ShowHelp();
	virtual void Dump();
	virtual void Draw2D();
	virtual void ProcessKey(int key);

private:
	CStaticMesh		*Mesh;
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
