#include "ObjectViewer.h"
#include "MeshInstance.h"


#if TEST_FILES
void CMeshViewer::Test()
{
	const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);
	// empty ...
}
#endif


void CMeshViewer::Dump()
{
	CObjectViewer::Dump();
	const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);
	printf(
		"\nLodMesh info:\n=============\n"
		"version        %d\n"
		"VertexCount    %d\n"
		"Verts #        %d\n"
		"MeshScale      %g %g %g\n"
		"MeshOrigin     %g %g %g\n"
		"RotOrigin      %d %d %d\n"
		"FaceLevel #    %d\n"
		"Faces #        %d\n"
		"CollapseWedgeThus # %d\n"
		"Wedges #       %d\n"
		"Impostor: %s  (%s)\n"
		"SkinTessFactor %g\n",
		Mesh->Version,
		Mesh->VertexCount,
		Mesh->Verts.Num(),
		FVECTOR_ARG(Mesh->MeshScale),
		FVECTOR_ARG(Mesh->MeshOrigin),
		ROTATOR_ARG(Mesh->RotOrigin),
		Mesh->FaceLevel.Num(),
		Mesh->Faces.Num(),
		Mesh->CollapseWedgeThus.Num(),
		Mesh->Wedges.Num(),
		Mesh->HasImpostor ? "true" : "false", Mesh->SpriteMaterial ? Mesh->SpriteMaterial->Name : "none",
		Mesh->SkinTesselationFactor
	);
	int i;
	printf("Textures: %d\n", Mesh->Textures.Num());
	for (i = 0; i < Mesh->Textures.Num(); i++)
	{
		const UMaterial *Tex = Mesh->Textures[i];
		printf("  %d: %s\n", i, Tex ? Tex->Name : "null");
	}
	printf("Materials: %d\n", Mesh->Materials.Num());
	for (i = 0; i < Mesh->Materials.Num(); i++)
	{
		const FMeshMaterial &Mat = Mesh->Materials[i];
		printf("  %d: tex=%d  flags=%08X\n", i, Mat.TextureIndex, Mat.PolyFlags);
	}
}


void CMeshViewer::Draw3D()
{
	glBegin(GL_LINES);
	for (int i = 0; i < 3; i++)
	{
		CVec3 tmp = nullVec3;
		tmp[i] = 1;
		glColor3fv(tmp.v);
		tmp[i] = 100;
		glVertex3fv(tmp.v);
		glVertex3fv(nullVec3.v);
	}
	glEnd();
	glColor3f(1, 1, 1);

	assert(Inst);
	Inst->Draw();
}
