#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"

#include "StaticMesh.h"


CStatMeshViewer::CStatMeshViewer(CStaticMesh *Mesh0)
:	CMeshViewer(Mesh0->OriginalMesh)
,	Mesh(Mesh0)
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
	CMeshViewer::Dump();
	Mesh->GetTypeinfo()->DumpProps(Mesh);
}


void CStatMeshViewer::Draw2D()
{
	CObjectViewer::Draw2D();

	const CStatMeshInstance *MeshInst = static_cast<CStatMeshInstance*>(Inst);
	const CStaticMeshLod &Lod = Mesh->Lods[MeshInst->LodNum];

	DrawTextLeft(S_GREEN"LOD     : "S_WHITE"%d/%d\n"
				 S_GREEN"Verts   : "S_WHITE"%d\n"
				 S_GREEN"Tris    : "S_WHITE"%d\n"
				 S_GREEN"UV Sets : "S_WHITE"%d",
				 MeshInst->LodNum+1, Mesh->Lods.Num(),
				 Lod.Verts.Num(), Lod.Indices.Num() / 3, Lod.NumTexCoords);

	// code similar to CLodMeshViewer::Draw2D(), but using different fields
	DrawTextLeft(S_GREEN"Sections: "S_WHITE"%d", Lod.Sections.Num());
	if (Inst->bColorMaterials)
	{
		for (int i = 0; i < Lod.Sections.Num(); i++)
		{
			const UUnrealMaterial *Tex = Lod.Sections[i].Material;
			int color = i < 7 ? i + 1 : 7;
			int NumTris = Lod.Sections[i].NumFaces;
			if (Tex)
				DrawTextLeft("^%d  %d: %s (%s), %d tris", color, i, Tex->Name, Tex->GetClassName(), NumTris);
			else
				DrawTextLeft("^%d  %d: null, %d tris", color, i, NumTris);

		}
		DrawTextLeft("");
	}
}


void CStatMeshViewer::ShowHelp()
{
	CMeshViewer::ShowHelp();
	DrawTextLeft("L           cycle mesh LODs\n");
}


void CStatMeshViewer::ProcessKey(int key)
{
	guard(CStatMeshViewer::ProcessKey);

	CStatMeshInstance *MeshInst = static_cast<CStatMeshInstance*>(Inst);

	switch (key)
	{
	case 'l':
		if (++MeshInst->LodNum >= Mesh->Lods.Num())
			MeshInst->LodNum = 0;
		break;
	default:
		CMeshViewer::ProcessKey(key);
	}

	unguard;
}


#endif // RENDERING
