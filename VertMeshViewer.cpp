#include "ObjectViewer.h"


#if TEST_FILES
void CVertMeshViewer::Test()
{
	CMeshViewer::Test();

	const UVertMesh *Mesh = static_cast<UVertMesh*>(Object);
	// verify some assumptions
// (macro broken)	VERIFY2(VertexCount, VertexCount);
	VERIFY_NOT_NULL(VertexCount);
	VERIFY_NOT_NULL(Textures.Num());
	VERIFY(FaceLevel.Num(), Faces.Num());
	VERIFY_NOT_NULL(Faces.Num());
	VERIFY_NOT_NULL(Wedges.Num());
	VERIFY(CollapseWedgeThus.Num(), Wedges.Num());
//	VERIFY(Materials.Num(), Textures.Num()); -- different
	VERIFY_NULL(fF8);
	VERIFY_NULL(f130);
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


void CVertMeshViewer::Draw3D()
{
	CMeshViewer::Draw3D();

	const UVertMesh *Mesh = static_cast<UVertMesh*>(Object);
	int i;

	int base = Mesh->VertexCount * FrameNum;
	//?? choose cull mode -- should be used from UFinalBlend
	glDisable(GL_CULL_FACE);
//	glCullFace(GL_BACK);

	CVec3 Vert[3];
	CVec3 Norm[3];

	for (i = 0; i < Mesh->Faces.Num(); i++)
	{
		int j;
		const FMeshFace &F = Mesh->Faces[i];
		for (j = 0; j < 3; j++)
		{
			const FMeshWedge &W = Mesh->Wedges[F.iWedge[j]];
			// vertex
			const FMeshVert &V = Mesh->Verts[base + W.iVertex];
			CVec3 &v = Vert[j];
			v[0] = V.X * Mesh->MeshScale.X /*+ Mesh->MeshOrigin.X*/;
			v[1] = V.Y * Mesh->MeshScale.Y /*+ Mesh->MeshOrigin.Y*/;
			v[2] = V.Z * Mesh->MeshScale.Z /*+ Mesh->MeshOrigin.Z*/;
			// normal
			const FMeshNorm &N = ((UVertMesh*)Mesh)->Normals[base + W.iVertex];
			CVec3 &n = Norm[j];
			n[0] = (N.X - 512.0f) / 512;
			n[1] = (N.Y - 512.0f) / 512;
			n[2] = (N.Z - 512.0f) / 512;
		}
		// draw mesh
		glEnable(GL_LIGHTING);
		glBegin(GL_TRIANGLES);
		for (j = 0; j < 3; j++)
		{
			glNormal3fv(Norm[j].v);
			glVertex3fv(Vert[j].v);
		}
		glEnd();
		glDisable(GL_LIGHTING);
		if (bShowNormals)
		{
			// draw normals
			glBegin(GL_LINES);
			glColor3f(1, 0.5, 0);
			for (j = 0; j < 3; j++)
			{
				glVertex3fv(Vert[j].v);
				CVec3 tmp;
				tmp = Vert[j];
				VectorMA(tmp, 3, Norm[j]);
				glVertex3fv(tmp.v);
			}
			glEnd();
			glColor3f(1, 1, 1);
		}
	}
	glEnd();
}
