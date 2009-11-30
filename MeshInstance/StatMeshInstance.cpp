#include "Core.h"

#if RENDERING

#include "UnrealClasses.h"
#include "MeshInstance.h"


void CStatMeshInstance::Draw()
{
	guard(CStatMeshInstance::Draw);
	int i;

	// draw mesh
	glEnable(GL_LIGHTING);
	const UStaticMesh* Mesh = pMesh;
	for (i = 0; i < Mesh->Sections.Num(); i++)
	{
		SetMaterial(Mesh->Materials[i].Material, i, 0);

		const FStaticMeshSection &Sec = Mesh->Sections[i];

#if 0
		glBegin(GL_TRIANGLES);
		for (int j = 0; j < Sec.NumFaces * 3; j++)
		{
			int idx = Mesh->IndexStream1.Indices[j + Sec.FirstIndex];
			glTexCoord2fv(&Mesh->UVStream[0].Data[idx].U);
			glNormal3fv(&Mesh->Verts[idx].Normal.X);
			glVertex3fv(&Mesh->Verts[idx].Pos.X);
		}
		glEnd();
#else
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);

		glVertexPointer(3, GL_FLOAT, sizeof(FStaticMeshVertex), &Mesh->VertexStream.Vert[0].Pos.X);
		glNormalPointer(GL_FLOAT, sizeof(FStaticMeshVertex), &Mesh->VertexStream.Vert[0].Normal.X);
		glTexCoordPointer(2, GL_FLOAT, 0, &Mesh->UVStream[0].Data[0].U);
		glDrawElements(GL_TRIANGLES, Sec.NumFaces * 3, GL_UNSIGNED_SHORT, &Mesh->IndexStream1.Indices[Sec.FirstIndex]);
//		glDrawElements(GL_LINES, Mesh->IndexStream2.Indices.Num(), GL_UNSIGNED_SHORT, &Mesh->IndexStream2.Indices[0]);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
#endif
	}
	glDisable(GL_LIGHTING);
	BindDefaultMaterial(true);

	// draw mesh normals
	if (bShowNormals)
	{
		glBegin(GL_LINES);
		glColor3f(0.5, 1, 0);
		for (i = 0; i < Mesh->VertexStream.Vert.Num(); i++)
		{
			glVertex3fv(&Mesh->VertexStream.Vert[i].Pos.X);
			CVec3 tmp;
			VectorMA((CVec3&)Mesh->VertexStream.Vert[i].Pos, 2, (CVec3&)Mesh->VertexStream.Vert[i].Normal, tmp);
			glVertex3fv(tmp.v);
		}
		glEnd();
	}

	unguard;
}

#endif // RENDERING
