#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"
#include "Mesh/StaticMesh.h"
#include "Exporters.h"


void ExportStaticMeshOBJ(const CStaticMesh* Mesh)
{
	guard(ExportStaticMeshOBJ);

	UObject* OriginalMesh = Mesh->OriginalMesh;
	if (!Mesh->Lods.Num())
	{
		appNotify("Mesh %s has 0 lods", OriginalMesh->Name);
		return;
	}

	int MaxLod = GExportLods ? Mesh->Lods.Num() : 1;
	for (int Lod = 0; Lod < MaxLod; Lod++)
	{
		guard(Lod);

		const CStaticMeshLod& MeshLod = Mesh->Lods[Lod];
		if (MeshLod.NumVerts == 0)
		{
			appPrintf("WARNING: skipping LOD %d with 0 vertices\n", Lod);
			continue;
		}
		if (MeshLod.Sections.Num() == 0)
		{
			appNotify("Mesh %s Lod %d has no sections", OriginalMesh->Name, Lod);
			continue;
		}

		char objName[512];
		char mtlName[512];
		if (Lod == 0)
		{
			appSprintf(ARRAY_ARG(objName), "%s.obj", OriginalMesh->Name);
			appSprintf(ARRAY_ARG(mtlName), "%s.mtl", OriginalMesh->Name);
		}
		else
		{
			appSprintf(ARRAY_ARG(objName), "%s_Lod%d.obj", OriginalMesh->Name, Lod);
			appSprintf(ARRAY_ARG(mtlName), "%s_Lod%d.mtl", OriginalMesh->Name, Lod);
		}

		FArchive* Ar = CreateExportArchive(OriginalMesh, EFileArchiveOptions::TextFile, "%s", objName);
		if (!Ar)
		{
			if (Lod == 0) return;
			continue;
		}

		// Header
		Ar->Printf("# Exported by UEViewer\nmtllib %s\no %s\n\n", mtlName, OriginalMesh->Name);

		// Vertex positions (raw UE coordinate space)
		for (int i = 0; i < MeshLod.NumVerts; i++)
		{
			const CVecT& P = MeshLod.Verts[i].Position;
			Ar->Printf("v %g %g %g\n", P[0], P[1], P[2]);
		}
		Ar->Printf("\n");

		// UV coordinates
		for (int i = 0; i < MeshLod.NumVerts; i++)
		{
			const CMeshUVFloat& UV = MeshLod.Verts[i].UV;
			Ar->Printf("vt %g %g\n", UV.U, UV.V);
		}
		Ar->Printf("\n");

		// Normals
		for (int i = 0; i < MeshLod.NumVerts; i++)
		{
			CVec3 N;
			Unpack(N, MeshLod.Verts[i].Normal);
			Ar->Printf("vn %g %g %g\n", N[0], N[1], N[2]);
		}
		Ar->Printf("\n");

		// Faces grouped by section (material)
		CIndexBuffer::IndexAccessor_t GetIndex = MeshLod.Indices.GetAccessor();
		for (int SecIdx = 0; SecIdx < MeshLod.Sections.Num(); SecIdx++)
		{
			const CMeshSection& S = MeshLod.Sections[SecIdx];

			char matBuf[256];
			const char* MatName;
			if (S.Material)
				MatName = S.Material->Name;
			else
			{
				appSprintf(ARRAY_ARG(matBuf), "Material_%d", SecIdx);
				MatName = matBuf;
			}

			Ar->Printf("g %s\nusemtl %s\n", MatName, MatName);

			for (int f = 0; f < S.NumFaces; f++)
			{
				int base = S.FirstIndex + f * 3;
				// OBJ indices are 1-based; vertex/uv/normal share the same index
				int i0 = GetIndex(base + 0) + 1;
				int i1 = GetIndex(base + 1) + 1;
				int i2 = GetIndex(base + 2) + 1;
				Ar->Printf("f %d/%d/%d %d/%d/%d %d/%d/%d\n",
					i0, i0, i0,
					i1, i1, i1,
					i2, i2, i2);
			}
			Ar->Printf("\n");
		}

		delete Ar;

		// MTL file — one newmtl entry per section
		FArchive* MtlAr = CreateExportArchive(OriginalMesh, EFileArchiveOptions::TextFile, "%s", mtlName);
		if (MtlAr)
		{
			for (int SecIdx = 0; SecIdx < MeshLod.Sections.Num(); SecIdx++)
			{
				const CMeshSection& S = MeshLod.Sections[SecIdx];
				char matBuf[256];
				const char* MatName;
				if (S.Material)
					MatName = S.Material->Name;
				else
				{
					appSprintf(ARRAY_ARG(matBuf), "Material_%d", SecIdx);
					MatName = matBuf;
				}
				MtlAr->Printf("newmtl %s\n\n", MatName);
			}
			delete MtlAr;
		}

		unguardf("%d", Lod);
	}

	unguard;
}
