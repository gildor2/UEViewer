#include "ObjectViewer.h"
#include "MeshInstance.h"

//!! move outside
void SetAxis(const FRotator &Rot, CAxis &Axis)
{
	CVec3 angles;
	angles[YAW]   = -Rot.Yaw   / 32768.0f * 180;
	angles[ROLL]  = -Rot.Pitch / 32768.0f * 180;
	angles[PITCH] =  Rot.Roll  / 32768.0f * 180;
	Axis.FromEuler(angles);
}


CMeshViewer::CMeshViewer(ULodMesh *Mesh)
:	CObjectViewer(Mesh)
,	bShowNormals(false)
,	bWireframe(false)
#if _WIN32
,	CurrentTime(GetTickCount())
#else
#error Port!  //!! WIN32
#endif
{
	// compute model center by Z-axis (vertical)
	CVec3 offset;
	float z = (Mesh->BoundingBox.Max.Z + Mesh->BoundingBox.Min.Z) / 2;
	// scale/translate origin
	z = ((z - Mesh->MeshOrigin.Z) * Mesh->MeshScale.Z);
	// offset view
	offset.Set(0, 0, z);
	GL::SetViewOffset(offset);
	// automatically scale view distance depending on model size
	GL::SetDistScale(Mesh->MeshScale.X * Mesh->BoundingSphere.R / 150);
}


CMeshViewer::~CMeshViewer()
{
	delete Inst;
}


#if TEST_FILES
void CMeshViewer::Test()
{
	const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);
	// empty ...
	VERIFY_NOT_NULL(Textures.Num());
	VERIFY(CollapseWedgeThus.Num(), Wedges.Num());
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
		FROTATOR_ARG(Mesh->RotOrigin),
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
	guard(CMeshViewer::Draw3D);
	assert(Inst);

	// tick animations
#if _WIN32
	unsigned time = GetTickCount();
#else
#error Port!		//!! WIN32
#endif
	float TimeDelta = (time - CurrentTime) / 1000.0f;
	CurrentTime = time;
	Inst->Tick(TimeDelta);

	// draw axis
	glBegin(GL_LINES);
	for (int i = 0; i < 3; i++)
	{
		CVec3 tmp = nullVec3;
		tmp[i] = 1;
		glColor3fv(tmp.v);
		tmp[i] = 70;
		glVertex3fv(tmp.v);
		glVertex3fv(nullVec3.v);
	}
	glEnd();
	glColor3f(1, 1, 1);

	// draw mesh
	glPolygonMode(GL_FRONT_AND_BACK, bWireframe ? GL_LINE : GL_FILL);
	Inst->Draw();

	// restore draw state
	glColor3f(1, 1, 1);
	glDisable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	unguard;
}
