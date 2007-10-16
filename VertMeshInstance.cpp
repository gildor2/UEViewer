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
			CVec3 tmp;
			// vertex
			const FMeshVert &V = Mesh->Verts[base + W.iVertex];
			tmp[0] = V.X * Mesh->MeshScale.X;
			tmp[1] = V.Y * Mesh->MeshScale.Y;
			tmp[2] = V.Z * Mesh->MeshScale.Z;
			BaseTransform.TransformPoint(tmp, Vert[j]);
			// normal
			const FMeshNorm &N = ((UVertMesh*)Mesh)->Normals[base + W.iVertex];
			tmp[0] = (N.X - 512.0f) / 512;
			tmp[1] = (N.Y - 512.0f) / 512;
			tmp[2] = (N.Z - 512.0f) / 512;
			BaseTransform.axis.TransformVector(tmp, Norm[j]);
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
		if (Viewport->bShowNormals)
		{
			glDisable(GL_LIGHTING);
			glDisable(GL_TEXTURE_2D);
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
