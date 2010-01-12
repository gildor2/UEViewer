#include "Core.h"

#if RENDERING

#include "UnrealClasses.h"
#include "MeshInstance.h"


#define SHOW_TANGENTS			1
#define SORT_BY_OPACITY			1


#define MAX_MESHMATERIALS		256


struct CTangent
{
	CVec3		Tangent;
	CVec3		Binormal;
};


CStatMeshInstance::~CStatMeshInstance()
{
	if (Tangents) delete Tangents;
}

void CStatMeshInstance::Draw()
{
	guard(CStatMeshInstance::Draw);
	int i;

	if (!Tangents) BuildTangents();

	const UStaticMesh* Mesh = pMesh;
	int NumSections = Mesh->Sections.Num();

	// copy of CSkelMeshInstance::Draw sorting code
#if SORT_BY_OPACITY
	// sort sections by material opacity
	int SectionMap[MAX_MESHMATERIALS];
	int secPlace = 0;
	for (int opacity = 0; opacity < 2; opacity++)
	{
		for (i = 0; i < NumSections; i++)
		{
			UMaterial *Mat = Mesh->Materials[i].Material;
			int op = 0;			// sort value
			if (Mat && Mat->IsTranslucent()) op = 1;
			if (op == opacity) SectionMap[secPlace++] = i;
		}
	}
	assert(secPlace == NumSections);
#endif // SORT_BY_OPACITY

	// draw mesh
	glEnable(GL_LIGHTING);
	for (i = 0; i < Mesh->Sections.Num(); i++)
	{
#if SORT_BY_OPACITY
		int MaterialIndex = SectionMap[i];
#else
		int MaterialIndex = i;
#endif
		SetMaterial(Mesh->Materials[MaterialIndex].Material, MaterialIndex, 0);

		// check tangent space
		GLint aTangent = -1, aBinormal = -1;
		bool hasTangent = false;
		const CShader *Sh = GCurrentShader;
		if (Sh)
		{
			aTangent   = Sh->GetAttrib("tangent");
			aBinormal  = Sh->GetAttrib("binormal");
			hasTangent = (aTangent >= 0 && aBinormal >= 0);
		}
		if (hasTangent)
		{
			glEnableVertexAttribArray(aTangent);
			glEnableVertexAttribArray(aBinormal);
			glVertexAttribPointer(aTangent,  3, GL_FLOAT, GL_FALSE, sizeof(CTangent), &Tangents[0].Tangent);
			glVertexAttribPointer(aBinormal, 3, GL_FLOAT, GL_FALSE, sizeof(CTangent), &Tangents[0].Binormal);
		}

		const FStaticMeshSection &Sec = Mesh->Sections[MaterialIndex];

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
		// disable tangents
		if (hasTangent)
		{
			glDisableVertexAttribArray(aTangent);
			glDisableVertexAttribArray(aBinormal);
		}
	}
	glDisable(GL_LIGHTING);
	BindDefaultMaterial(true);

	// draw mesh normals
	if (bShowNormals)
	{
		int NumVerts = Mesh->VertexStream.Vert.Num();
		glBegin(GL_LINES);
		glColor3f(0.5, 1, 0);
		for (i = 0; i < NumVerts; i++)
		{
			glVertex3fv(&Mesh->VertexStream.Vert[i].Pos.X);
			CVec3 tmp;
			VectorMA((CVec3&)Mesh->VertexStream.Vert[i].Pos, 2, (CVec3&)Mesh->VertexStream.Vert[i].Normal, tmp);
			glVertex3fv(tmp.v);
		}
#if SHOW_TANGENTS
		glColor3f(0, 0.5f, 1);
		for (i = 0; i < NumVerts; i++)
		{
			const CVec3 &v = (CVec3&)Mesh->VertexStream.Vert[i].Pos;
			glVertex3fv(v.v);
			CVec3 tmp;
			VectorMA(v, 2, Tangents[i].Tangent, tmp);
			glVertex3fv(tmp.v);
		}
		glColor3f(1, 0, 0.5f);
		for (i = 0; i < NumVerts; i++)
		{
			const CVec3 &v = (CVec3&)Mesh->VertexStream.Vert[i].Pos;
			glVertex3fv(v.v);
			CVec3 tmp;
			VectorMA(v, 2, Tangents[i].Binormal, tmp);
			glVertex3fv(tmp.v);
		}
#endif // SHOW_TANGENTS
		glEnd();
	}

	unguard;
}


// code of this function is similar to BuildNormals() from SkelMeshInstance.cpp
void CStatMeshInstance::BuildTangents()
{
	guard(CStatMeshInstance::BuildTangents);

	const UStaticMesh* Mesh = pMesh;
	int NumVerts = Mesh->VertexStream.Vert.Num();
	Tangents = new CTangent[NumVerts];

	int i, j;

	const TArray<word> &Indices = Mesh->IndexStream1.Indices;
	for (i = 0; i < Indices.Num() / 3; i++)
	{
		int IDX[3];
		const FStaticMeshVertex *V[3];
		const FStaticMeshUV *UV[3];
		for (j = 0; j < 3; j++)
		{
			int idx = Indices[i * 3 + j];
			IDX[j] = idx;
			V[j]   = &Mesh->VertexStream.Vert[idx];
			UV[j]  = &Mesh->UVStream[0].Data[idx];
		}

		// compute tangent
		CVec3 tang;
		if (UV[2]->V == UV[0]->V)
		{
			// W[0] -> W[2] -- axis for tangent
			VectorSubtract((CVec3&)V[2]->Pos, (CVec3&)V[0]->Pos, tang);
			// we should make tang to look in side of growing U
			if (UV[2]->U < UV[0]->U) tang.Negate();
		}
		else
		{
			float pos = (UV[1]->V - UV[0]->V) / (UV[2]->V - UV[0]->V);	// fraction, where W[1] is placed betwen W[0] and W[2] (may be < 0 or > 1)
			CVec3 tmp;
			Lerp((CVec3&)V[0]->Pos, (CVec3&)V[2]->Pos, pos, tmp);
			VectorSubtract(tmp, (CVec3&)V[1]->Pos, tang);
			// tang.V == W[1].V; but tang.U may be greater or smaller than W[1].U
			// we should make tang to look in side of growing U
			float tU = Lerp(UV[0]->U, UV[2]->U, pos);
			if (tU < UV[1]->U) tang.Negate();
		}
		// now, tang is on triangle plane
		// now we should place tangent orthogonal to normal, then normalize vector
		float binormalScale = 1.0f;
		for (j = 0; j < 3; j++)
		{
			CTangent &DW = Tangents[IDX[j]];
			const CVec3 &norm = (CVec3&)V[j]->Normal;
			float pos = dot(norm, tang);
			CVec3 tang2;
			VectorMA(tang, -pos, norm, tang2);
			tang2.Normalize();
			DW.Tangent = tang2;
			cross((CVec3&)V[j]->Normal, DW.Tangent, tang2);
			tang2.Normalize();
			if (j == 0)		// do this only once for triangle
			{
				// check binormal sign
				// find two points with different V
				int W1 = 0;
				int W2 = (UV[1]->V != UV[0]->V) ? 1 : 2;
				// check projections of these points to binormal
				float p1 = dot((CVec3&)V[W1]->Pos, tang2);
				float p2 = dot((CVec3&)V[W2]->Pos, tang2);
				if ((p1 - p2) * (UV[W1]->V - UV[W2]->V) < 0)
					binormalScale = -1.0f;
			}
			tang2.Scale(binormalScale);
			DW.Binormal = tang2;
		}
	}

	unguard;
}

#endif // RENDERING
