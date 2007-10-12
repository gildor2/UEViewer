#include "ObjectViewer.h"
#include "MeshInstance.h"


CVertMeshViewer::CVertMeshViewer(UVertMesh *Mesh)
:	CMeshViewer(Mesh)
{
	Inst = new CVertMeshInstance(Mesh, this);
}


#if TEST_FILES
void CVertMeshViewer::Test()
{
	CMeshViewer::Test();

	const UVertMesh *Mesh = static_cast<UVertMesh*>(Object);
	// verify some assumptions
	//!! move some parts to CMeshViewer::Test()
// (macro broken)	VERIFY2(VertexCount, VertexCount);
	VERIFY_NOT_NULL(VertexCount);
	VERIFY_NOT_NULL(Textures.Num());
	VERIFY(FaceLevel.Num(), Faces.Num());
	VERIFY_NOT_NULL(Faces.Num());
	VERIFY_NOT_NULL(Wedges.Num());
	VERIFY(CollapseWedgeThus.Num(), Wedges.Num());
//	VERIFY(Materials.Num(), Textures.Num()); -- different
	VERIFY_NULL(Verts2.Num());
	VERIFY(Verts.Num(), Normals.Num());
	VERIFY_NOT_NULL(Verts.Num());
	assert(Mesh->Verts.Num() == Mesh->VertexCount * Mesh->FrameCount);
	VERIFY_NULL(f150.Num());
	VERIFY(BoundingBoxes.Num(), BoundingSpheres.Num());
	VERIFY(BoundingBoxes.Num(), FrameCount);
	VERIFY_NULL(AnimMeshVerts.Num());
	VERIFY_NULL(StreamVersion);
}
#endif


void CVertMeshViewer::Dump()
{
	CMeshViewer::Dump();

	const UVertMesh *Mesh = static_cast<UVertMesh*>(Object);
	printf(
		"\nVertMesh info:\n==============\n"
		"Verts # %d  Normals # %d\n"
		"f150 #         %d\n"
		"AnimSeqs #     %d\n"
		"Bounding: Boxes # %d  Spheres # %d\n"
		"VertexCount %d  FrameCount %d\n"
		"AnimMeshVerts   # %d\n"
		"StreamVersion  %d\n",
		Mesh->Verts2.Num(),
		Mesh->Normals.Num(),
		Mesh->f150.Num(),
		Mesh->AnimSeqs.Num(),
		Mesh->BoundingBoxes.Num(),
		Mesh->BoundingSpheres.Num(),
		Mesh->VertexCount,
		Mesh->FrameCount,
		Mesh->AnimMeshVerts.Num(),
		Mesh->StreamVersion
	);
}


void CVertMeshViewer::ProcessKey(unsigned char key)
{
	CVertMeshInstance *MeshInst = static_cast<CVertMeshInstance*>(Inst);
	int FrameCount = (static_cast<UVertMesh*>(Object))->FrameCount;
	switch (key)
	{
	case '1':
		if (++MeshInst->FrameNum >= FrameCount)
			MeshInst->FrameNum = 0;
		break;
	case '2':
		if (--MeshInst->FrameNum < 0)
			MeshInst->FrameNum = FrameCount-1;
		break;
	default:
		CMeshViewer::ProcessKey(key);
	}
}
