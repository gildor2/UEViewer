#include "ObjectViewer.h"
#include "MeshInstance.h"


void CVertMeshInstance::Draw()
{
	guard(CVertMeshInstance::Draw);

	const UVertMesh *Mesh = static_cast<UVertMesh*>(pMesh);
	int i;

	int base = Mesh->VertexCount * FrameNum;

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
		SetMaterial(F.MaterialIndex);
		glEnable(GL_LIGHTING);
		glBegin(GL_TRIANGLES);
		for (j = 2; j >= 0; j--)	// skeletal mesh and vertex mesh has opposite triangle vertex order ??
		{
			const FMeshWedge &W = Mesh->Wedges[F.iWedge[j]];
			glTexCoord2f(W.TexUV.U, W.TexUV.V);
			glNormal3fv(Norm[j].v);
			glVertex3fv(Vert[j].v);
		}
		glEnd();
		glDisable(GL_LIGHTING);
		if (Viewport->bShowNormals)
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
	glDisable(GL_TEXTURE_2D);
	glEnd();

	unguard;
}
