#ifndef __OBJECT_VIEWER_H__
#define __OBJECT_VIEWER_H__

#if RENDERING

#include "GlWindow.h"


class CMeshInstance;
class CVertMeshInstance;
class CSkelMeshInstance;

class CSkeletalMesh;
class CAnimSet;
class CStaticMesh;
struct CMeshVertex;
struct CBaseMeshLod;

class UUnrealMaterial;
class UVertMesh;
class USkeleton;

class UIMenuItem;


#define TEST_FILES		1		// comment line to disable some notifications


/*-----------------------------------------------------------------------------
	Basic object viewer
-----------------------------------------------------------------------------*/

class CObjectViewer
{
public:
	UObject*		Object;
	CApplication*	Window;
	const UObject*	JumpAfterFrame;

	CObjectViewer(UObject* Obj, CApplication* Win);

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
	virtual void ProcessKey(unsigned key);
	virtual void ProcessKeyUp(unsigned key)
	{}

#if HAS_UI
	virtual UIMenuItem* GetObjectMenu(UIMenuItem* menu);
#endif

	virtual void Draw3D(float TimeDelta)
	{}

	void JumpTo(const UObject* Object)
	{
		JumpAfterFrame = Object;
	}
};


/*-----------------------------------------------------------------------------
	Material viewer
-----------------------------------------------------------------------------*/

class CMaterialViewer : public CObjectViewer
{
public:
	bool			IsTexture;
	static bool		ShowOutline;
	static bool		ShowChannels;
	static int		ShapeIndex;

	CMaterialViewer(UUnrealMaterial* Material, CApplication* Window);
	virtual ~CMaterialViewer() override;

	virtual void ShowHelp() override;
	virtual void ProcessKey(unsigned key) override;

	virtual void Draw2D() override;
	virtual void Draw3D(float TimeDelta) override;

protected:
	void FlushProps();
	void OutlineMaterial(UObject *Obj, int indent = 0);
};


/*-----------------------------------------------------------------------------
	Basic mesh viewer (ULodMesh)
-----------------------------------------------------------------------------*/

class CMeshViewer : public CObjectViewer
{
public:
	// linked data
	CMeshInstance	*Inst;
	// debug rendering
	unsigned		DrawFlags;
	bool			Wireframe;

	CMeshViewer(UObject* Mesh, CApplication* Window)
	:	CObjectViewer(Mesh, Window)
	,	DrawFlags(0)
	,	Wireframe(false)
	{}
	virtual ~CMeshViewer() override;

	void InitViewerPosition(const CVec3 &Mins, const CVec3 &Maxs);	//?? CBox Bounds?

	virtual void ShowHelp() override;
	virtual void ProcessKey(unsigned key) override;
#if HAS_UI
	virtual UIMenuItem* GetObjectMenu(UIMenuItem* menu) override;
#endif

	virtual void Draw3D(float TimeDelta) override;

	virtual void DrawMesh(CMeshInstance *Inst);

	// Print a text about mesh material. Automatically highlights material if mouse points
	// at its text record on screen.
	void PrintMaterialInfo(int Index, UUnrealMaterial *Material, int NumFaces);

	void DisplayUV(const CMeshVertex* Verts, int VertexSize, const CBaseMeshLod* Mesh, int UVIndex);
};


/*-----------------------------------------------------------------------------
	Vertex mesh viewer (UVertMesh)
-----------------------------------------------------------------------------*/

class CVertMeshViewer : public CMeshViewer
{
public:
	int				AnimIndex;

	CVertMeshViewer(UVertMesh* Mesh, CApplication* Window);

	virtual void ShowHelp() override;
	virtual void ProcessKey(unsigned key) override;
	virtual void Dump() override;
	virtual void Export() override;
#if TEST_FILES
	virtual void Test() override;
#endif
	virtual void Draw2D() override;
	virtual void Draw3D(float TimeDelta) override;
};


/*-----------------------------------------------------------------------------
	Skeletal mesh viewer (USkeletalMesh)
-----------------------------------------------------------------------------*/

extern UObject *GForceAnimSet;

class CSkelMeshViewer : public CMeshViewer
{
public:
	int				AnimIndex;
	bool			IsFollowingMesh;
	int				ShowSkel;					// 0 - mesh, 1 - mesh+skel, 2 - skel only
	bool			ShowLabels;
	bool			ShowAttach;
	bool			ShowUV;

	CSkelMeshViewer(CSkeletalMesh* Mesh, CApplication* Window);

	void TagMesh(CSkelMeshInstance *Inst);
	static void UntagAllMeshes();

	virtual void ShowHelp() override;
#if HAS_UI
	virtual UIMenuItem* GetObjectMenu(UIMenuItem* menu) override;
#endif

	virtual void Dump() override;
	virtual void Export() override;
	virtual void Draw2D() override;
	virtual void Draw3D(float TimeDelta) override;
	virtual void ProcessKey(unsigned key) override;
	virtual void ProcessKeyUp(unsigned key) override;

	void SetAnim(const CAnimSet* AnimSet);

	void AttachAnimSet();
	void FindUE4Animations();

	virtual void DrawMesh(CMeshInstance *Inst) override;

	static TArray<CSkelMeshInstance*> TaggedMeshes;	// for displaying multipart meshes

private:
	CSkeletalMesh*	Mesh;
	const CAnimSet*	Anim;
	bool			bIsUE4Mesh;
	USkeleton*		Skeleton;
};


/*-----------------------------------------------------------------------------
	Static mesh viewer (CStaticMesh)
-----------------------------------------------------------------------------*/

class CStatMeshViewer : public CMeshViewer
{
public:
	CStatMeshViewer(CStaticMesh* Mesh, CApplication* Window);

	virtual void ShowHelp() override;
#if HAS_UI
	virtual UIMenuItem* GetObjectMenu(UIMenuItem* menu) override;
#endif

	virtual void Dump() override;
	virtual void Draw2D() override;
	virtual void ProcessKey(unsigned key) override;

private:
	CStaticMesh		*Mesh;
	bool			ShowUV;
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

#endif // RENDERING

#endif // __OBJECT_VIEWER_H__
