#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "StaticMesh.h"
#include "MeshInstance.h"


#define SHOW_TANGENTS			1
#define SORT_BY_OPACITY			1


// Some huge editor assets (not optimized assets) could have very large number of sections
#define MAX_MESHMATERIALS		1024


CStatMeshInstance::~CStatMeshInstance()
{
	if (pMesh) pMesh->UnlockMaterials();
}

void CStatMeshInstance::SetMesh(CStaticMesh *Mesh)
{
	assert(pMesh == NULL);
	pMesh = Mesh;
	pMesh->LockMaterials();
}

void CStatMeshInstance::Draw(unsigned flags)
{
	guard(CStatMeshInstance::Draw);
	int i;

	if (!pMesh->Lods.Num()) return;

	/*const*/ CStaticMeshLod& Mesh = pMesh->Lods[LodNum];	//?? not 'const' because of BuildTangents(); change this?
	int NumSections = Mesh.Sections.Num();
	if (!NumSections || !Mesh.NumVerts) return;

//	if (!Mesh.HasNormals)  Mesh.BuildNormals();
	if (!Mesh.HasTangents) Mesh.BuildTangents();

	// copy of CSkelMeshInstance::Draw sorting code
#if SORT_BY_OPACITY
	// sort sections by material opacity
	int SectionMap[MAX_MESHMATERIALS];
	int secPlace = 0;
	for (int opacity = 0; opacity < 2; opacity++)
	{
		for (i = 0; i < NumSections; i++)
		{
			UUnrealMaterial *Mat = Mesh.Sections[i].Material;
			int op = 0;			// sort value
			if (Mat && Mat->IsTranslucent()) op = 1;
			if (op == opacity) SectionMap[secPlace++] = i;
		}
	}
	assert(secPlace == NumSections);
#endif // SORT_BY_OPACITY

	// draw mesh
	glEnable(GL_LIGHTING);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	glVertexPointer(3, GL_FLOAT, sizeof(CStaticMeshVertex), &Mesh.Verts[0].Position);
	glNormalPointer(GL_BYTE, sizeof(CStaticMeshVertex), &Mesh.Verts[0].Normal);
	if (UVIndex == 0)
	{
		glTexCoordPointer(2, GL_FLOAT, sizeof(CStaticMeshVertex), &Mesh.Verts[0].UV.U);
	}
	else
	{
		glTexCoordPointer(2, GL_FLOAT, sizeof(CMeshUVFloat), &Mesh.ExtraUV[UVIndex-1][0].U);
	}

	/*??
		Can move tangent/binormal setup here too, but this will require to force shader to use fixed attribute locations
		(use glBindAttribLocation before glLinkProgram instead of querying atttribute via glGetAtribLocation).
		In this case:
		- can remove GCurrentShader
		- can eliminate hasTangent checks below and always bind attributes (when supports GL2.0)
		- can share code between SkeletalMesh and StaticMesh:
		  - sort sections
		  - setup arrays; differences:
		    - position and normals are taken from different arrays in static and skeletal meshes
		    - ShowInfluences in SkeletalMeshInstance should disable material binding
		  - draw sections using glDrawElements()
		  - un-setup arrays
		  - use VAO+VBO for rendering
	*/

	for (i = 0; i < NumSections; i++)
	{
#if SORT_BY_OPACITY
		int MaterialIndex = SectionMap[i];
#else
		int MaterialIndex = i;
#endif
		const CMeshSection &Sec = Mesh.Sections[MaterialIndex];
		if (!Sec.NumFaces) continue;

		SetMaterial(Sec.Material, MaterialIndex);

		// check tangent space
		GLint aNormal = -1;
		GLint aTangent = -1;
//		GLint aBinormal = -1;
		const CShader *Sh = GCurrentShader;
		if (Sh)
		{
			aNormal    = Sh->GetAttrib("normal");
			aTangent   = Sh->GetAttrib("tangent");
//			aBinormal  = Sh->GetAttrib("binormal");
		}
		if (aNormal >= 0)
		{
			glEnableVertexAttribArray(aNormal);
			// send 4 components to decode binormal in shader
			glVertexAttribPointer(aNormal, 4, GL_BYTE, GL_FALSE, sizeof(CStaticMeshVertex), &Mesh.Verts[0].Normal);
		}
		if (aTangent >= 0)
		{
			glEnableVertexAttribArray(aTangent);
			glVertexAttribPointer(aTangent, 3, GL_BYTE, GL_FALSE, sizeof(CStaticMeshVertex), &Mesh.Verts[0].Tangent);
		}
/*		if (aBinormal >= 0)
		{
			glEnableVertexAttribArray(aBinormal);
			glVertexAttribPointer(aBinormal, 3, GL_BYTE, GL_FALSE, sizeof(CStaticMeshVertex), &Mesh.Verts[0].Binormal);
		} */
		// draw
		//?? place this code into CIndexBuffer?
		if (Mesh.Indices.Is32Bit())
			glDrawElements(GL_TRIANGLES, Sec.NumFaces * 3, GL_UNSIGNED_INT, &Mesh.Indices.Indices32[Sec.FirstIndex]);
		else
			glDrawElements(GL_TRIANGLES, Sec.NumFaces * 3, GL_UNSIGNED_SHORT, &Mesh.Indices.Indices16[Sec.FirstIndex]);

		// disable tangents
		if (aNormal >= 0)
			glDisableVertexAttribArray(aNormal);
		if (aTangent >= 0)
			glDisableVertexAttribArray(aTangent);
//		if (aBinormal >= 0)
//			glDisableVertexAttribArray(aBinormal);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	glDisable(GL_LIGHTING);
	BindDefaultMaterial(true);

	// draw mesh normals
	if (flags & DF_SHOW_NORMALS)
	{
		//!! TODO: performance issues when displaying a large mesh (1M+ triangles) with normals/tangents.
		//!! Possible solution:
		//!! 1. use vertex buffer for normals, use single draw call
		//!! 2. cache normal data between render frames
		//!! 3. DecodeTangents() will do Unpack() for normal and tangent too, so this work is duplicated here
		int NumVerts = Mesh.NumVerts;
		glBegin(GL_LINES);
		glColor3f(0.5, 1, 0);
		CVecT tmp, unpacked;
		const float VisualLength = 2.0f;
		for (i = 0; i < NumVerts; i++)
		{
			glVertex3fv(Mesh.Verts[i].Position.v);
			Unpack(unpacked, Mesh.Verts[i].Normal);
			VectorMA(Mesh.Verts[i].Position, VisualLength, unpacked, tmp);
			glVertex3fv(tmp.v);
		}
#if SHOW_TANGENTS
		glColor3f(0, 0.5f, 1);
		for (i = 0; i < NumVerts; i++)
		{
			const CVec3 &v = Mesh.Verts[i].Position;
			glVertex3fv(v.v);
			Unpack(unpacked, Mesh.Verts[i].Tangent);
			VectorMA(v, VisualLength, unpacked, tmp);
			glVertex3fv(tmp.v);
		}
		glColor3f(1, 0, 0.5f);
		for (i = 0; i < NumVerts; i++)
		{
			const CMeshVertex& vert = Mesh.Verts[i];
			// decode binormal
			CVecT normal, tangent, binormal;
			vert.DecodeTangents(normal, tangent, binormal);
			// render
			const CVecT &v = vert.Position;
			glVertex3fv(v.v);
			VectorMA(v, VisualLength, binormal, tmp);
			glVertex3fv(tmp.v);
		}
#endif // SHOW_TANGENTS
		glEnd();
	}

	unguard;
}


#endif // RENDERING
