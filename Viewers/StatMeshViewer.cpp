#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"


CStatMeshViewer::CStatMeshViewer(UStaticMesh *Mesh)
:	CMeshViewer(Mesh)
{
	CStatMeshInstance *StatInst = new CStatMeshInstance();
	StatInst->SetMesh(Mesh);
	Inst = StatInst;
	// compute model center
	CVec3 offset;
	offset[0] = (Mesh->BoundingBox.Max.X + Mesh->BoundingBox.Min.X) / 2;
	offset[1] = (Mesh->BoundingBox.Max.Y + Mesh->BoundingBox.Min.Y) / 2;
	offset[2] = (Mesh->BoundingBox.Max.Z + Mesh->BoundingBox.Min.Z) / 2;
	offset[2] += Mesh->BoundingSphere.R / 20;		// offset a bit up
	SetViewOffset(offset);
	// automatically scale view distance depending on model size
	SetDistScale(Mesh->BoundingSphere.R / 150);
}


void CStatMeshViewer::Dump()
{
	int i;

	CMeshViewer::Dump();

	const UStaticMesh* Mesh = static_cast<UStaticMesh*>(Object);

	printf(
		"\nStaticMesh info:\n================\n"
		"Version=%d  Trialgnes # %d Verts # %d (rev=%d)\n"
		"Colors1 # %d  Colors2 # %d  Indices1 # %d  Indices2 # %d\n"
		"f124=%d  f128=%d  f12C=%d\n"
		"f108=%s  f16C=%s\n"
		"f150 # %d  f15C=%08X\n",
		Mesh->Version, Mesh->Faces.Num(), Mesh->VertexStream.Vert.Num(), Mesh->VertexStream.Revision,
		Mesh->ColorStream1.Color.Num(), Mesh->ColorStream2.Color.Num(),
		Mesh->IndexStream1.Indices.Num(), Mesh->IndexStream2.Indices.Num(),
		Mesh->f124, Mesh->f128, Mesh->f12C,
		Mesh->f108 ? Mesh->f108->Name : "NULL", Mesh->f16C ? Mesh->f16C->Name : "NULL",
		Mesh->f150.Num(), Mesh->f15C
	);

	printf("UV Streams: %d\n", Mesh->UVStream.Num());
	for (i = 0; i < Mesh->UVStream.Num(); i++)
	{
		const FStaticMeshUVStream &S = Mesh->UVStream[i];
		printf("  %d: [%d] %d %d\n", i, S.Data.Num(), S.f10, S.f1C);
	}

	printf("Sections: %d\n", Mesh->Sections.Num());
	for (i = 0; i < Mesh->Sections.Num(); i++)
	{
		const FStaticMeshSection &Sec = Mesh->Sections[i];
		printf("  %d: %d idx0=%d v=[%d %d] f=%d %d\n", i, Sec.f4,
			Sec.FirstIndex, Sec.FirstVertex, Sec.LastVertex, Sec.NumFaces, Sec.fE);
	}

	printf("Materials: %d\n", Mesh->Materials.Num());
	for (i = 0; i < Mesh->Materials.Num(); i++)
	{
		const FStaticMeshMaterial &Mat = Mesh->Materials[i];
		if (Mat.Material)
			printf("  %d: %s'%s'\n", i, Mat.Material->GetClassName(), Mat.Material->Name);
		else
			printf("  %d: NULL\n", i);
	}
}


void CStatMeshViewer::Draw2D()
{
	CObjectViewer::Draw2D();

	const UStaticMesh* Mesh = static_cast<UStaticMesh*>(Object);
	DrawTextLeft(S_GREEN"Verts   : "S_WHITE"%d\n"
				 S_GREEN"Tris    : "S_WHITE"%d\n"
				 S_GREEN"Sections: "S_WHITE"%d\n"
				 S_GREEN"UV Sets : "S_WHITE"%d",
				 Mesh->VertexStream.Vert.Num(), Mesh->IndexStream1.Indices.Num() / 3, Mesh->Sections.Num(), Mesh->UVStream.Num());

	// code similar to CLodMeshViewer::Draw2D(), but using different fields
	if (Inst->bColorMaterials)
	{
		const UStaticMesh *Mesh = static_cast<UStaticMesh*>(Object);
		DrawTextLeft(S_GREEN"Textures: %d", Mesh->Materials.Num());
		for (int i = 0; i < Mesh->Materials.Num(); i++)
		{
			const UMaterial *Tex = Mesh->Materials[i].Material;
			int color = i < 7 ? i + 1 : 7;
			if (Tex)
				DrawTextLeft("^%d  %d: %s (%s)", color, i, Tex->Name, Tex->GetClassName());
			else
				DrawTextLeft("^%d  %d: null", color, i);

		}
		DrawTextLeft("");
	}
}

#endif // RENDERING
