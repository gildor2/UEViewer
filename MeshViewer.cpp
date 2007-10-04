#include "ObjectViewer.h"


#if TEST_FILES
void CMeshViewer::Test()
{
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
		"Textures #     %d\n"
		"MeshScale      %g %g %g\n"
		"MeshOrigin     %g %g %g\n"
		"RotOrigin      %d %d %d\n"
		"FaceLevel #    %d\n"
		"Faces #        %d\n"
		"CollapseWedgeThus # %d\n"
		"Wedges #       %d\n"
		"Materials #    %d\n"
		"HasImpostor    %d\n"
		"fF8            %X ??\n"
		"fFC            %g %g %g\n"
		"f108           %d %d %d\n"
		"f114           %g %g %g\n"
		"f120..123      %d %d %d %d\n"
		"f124           %d %d %d\n"
		"f130           %g\n",
		Mesh->Version,
		Mesh->VertexCount,
		Mesh->Verts.Num(),
		Mesh->Textures.Num(),
		FVECTOR_ARG(Mesh->MeshScale),
		FVECTOR_ARG(Mesh->MeshOrigin),
		ROTATOR_ARG(Mesh->RotOrigin),
		Mesh->FaceLevel.Num(),
		Mesh->Faces.Num(),
		Mesh->CollapseWedgeThus.Num(),
		Mesh->Wedges.Num(),
		Mesh->Materials.Num(),
		Mesh->HasImpostor,
		Mesh->fF8,
		FVECTOR_ARG(Mesh->fFC),
		ROTATOR_ARG(Mesh->f108),
		FVECTOR_ARG(Mesh->f114),
		Mesh->f120, Mesh->f121, Mesh->f122, Mesh->f123,
		ROTATOR_ARG(Mesh->f124),
		Mesh->f130
	);
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
}
