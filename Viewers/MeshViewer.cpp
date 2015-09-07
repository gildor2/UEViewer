#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"
#include "MeshCommon.h"


CMeshViewer::~CMeshViewer()
{
	delete Inst;
}


void CMeshViewer::InitViewerPosition(const CVec3 &Mins, const CVec3 &Maxs)
{
	CVec3 tmp, Center;
	VectorAdd(Maxs, Mins, tmp);
	VectorScale(tmp, 0.5f, Center);
	VectorSubtract(Maxs, Center, tmp);
	float radius = tmp.GetLength();

	SetViewOffset(Center);
	SetDistScale((radius + 10) / 230);
	MoveCamera(20, 20);
}


void CMeshViewer::Draw3D(float TimeDelta)
{
	guard(CMeshViewer::Draw3D);
	assert(Inst);

	if (GShowDebugInfo)
	{
		// draw axis
		BindDefaultMaterial(true);
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
	}

	// draw mesh
	glPolygonMode(GL_FRONT_AND_BACK, Wireframe ? GL_LINE : GL_FILL);	//?? bWireframe is inside Inst, but used here only ?
	DrawMesh(Inst);

	unguard;
}


void CMeshViewer::DrawMesh(CMeshInstance *Inst)
{
	Inst->Draw(DrawFlags);
}


void CMeshViewer::DisplayUV(const CMeshVertex* Verts, int VertexSize, const CBaseMeshLod* Mesh, int UVIndex)
{
	guard(CMeshViewer::DisplayUV);

	int width, height;
	Window->GetWindowSize(width, height);
	int w = min(width, height);

	// add some border
	int x0 = width - w;
	int y0 = 10;
	w -= 20;

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (int MaterialIndex = 0; MaterialIndex < Mesh->Sections.Num(); MaterialIndex++)
	{
		const CMeshSection &Sec = Mesh->Sections[MaterialIndex];
		if (!Sec.NumFaces) continue;

		BindDefaultMaterial(true);
		glDisable(GL_CULL_FACE);
		unsigned color = CMeshInstance::GetMaterialDebugColor(MaterialIndex);
		glColor4ubv((GLubyte*)&color);

		CIndexBuffer::IndexAccessor_t Index = Mesh->Indices.GetAccessor();

		glBegin(GL_TRIANGLES);
		for (int i = Sec.FirstIndex; i < Sec.FirstIndex + Sec.NumFaces * 3; i++)
		{
			int VertIndex = Index(i);
			const CMeshVertex* V = OffsetPointer(Verts, VertIndex * VertexSize);
			const CMeshUVFloat& UV = (UVIndex == 0) ? V->UV : Mesh->ExtraUV[UVIndex-1][VertIndex];
			glVertex2f(UV.U * w + x0, UV.V * w + y0);
		}
		glEnd();
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	unguard;
}


void CMeshViewer::ShowHelp()
{
	CObjectViewer::ShowHelp();
	DrawKeyHelp("N", "show normals");
	DrawKeyHelp("W", "toggle wireframe");
	DrawKeyHelp("M", "colorize materials");
}


void CMeshViewer::ProcessKey(int key)
{
	guard(CMeshViewer::ProcessKey);
	switch (key)
	{
	case 'n':
		DrawFlags ^= DF_SHOW_NORMALS;
		break;

	case 'm':
		Inst->bColorMaterials = !Inst->bColorMaterials;
		break;

	case 'w':
		Wireframe = !Wireframe;
		break;

	default:
		CObjectViewer::ProcessKey(key);
	}
	unguard;
}


void CMeshViewer::PrintMaterialInfo(int Index, UUnrealMaterial *Material, int NumFaces)
{
	unsigned color = CMeshInstance::GetMaterialDebugColor(Index);
	if (Material)
		DrawText(TA_TopLeft, color, "  %d: %s (%s), %d tris", Index, Material->Name, Material->GetClassName(), NumFaces);
	else
		DrawText(TA_TopLeft, color, "  %d: null, %d tris", Index, NumFaces);
}


#endif // RENDERING
