#include "Core.h"
#include "UnCore.h"

#if RENDERING

#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"
#include "Mesh/MeshCommon.h"

#if HAS_UI
#include "BaseDialog.h"
#endif

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
	float angle = 15;
	// Before UE4, all objects are looking at X direction (red axis). For UE4
	// default direction is changed to Y axis (green).
	if (Object && Object->GetGame() >= GAME_UE4_BASE)
		angle += 90;
	MoveCamera(angle, 20);
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

	// Reset highlighted material
	Inst->HighlightMaterialIndex = -1;

	unguard;
}


void CMeshViewer::DrawMesh(CMeshInstance *Inst)
{
	// Default DrawMesh implementation, not necessarily to be calles
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
	DrawKeyHelp("C", "show vertex colors");
	DrawKeyHelp("N", "show normals");
	DrawKeyHelp("W", "toggle wireframe");
	DrawKeyHelp("M", "colorize materials");
}


#if HAS_UI

UIMenuItem* CMeshViewer::GetObjectMenu(UIMenuItem* menu)
{
	if (!menu)
	{
		menu = &NewSubmenu("Mesh");
	}
	else
	{
		menu->Add(NewMenuSeparator());
	}

	(*menu)
	[
		NewMenuCheckbox("Show normals\tN", &DrawFlags, DF_SHOW_NORMALS)
		+NewMenuCheckbox("Wireframe\tW", &Wireframe)
		+NewMenuCheckbox("Material IDs\tM", &Inst->bColorMaterials)
		+NewMenuCheckbox("Vertex colors\tC", &Inst->bVertexColors)
	];

	return menu;
}

#endif // HAS_UI

void CMeshViewer::ProcessKey(unsigned key)
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

	case 'c':
		Inst->bVertexColors = !Inst->bVertexColors;
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
	bool bHighlight = false;
	unsigned color = CMeshInstance::GetMaterialDebugColor(Index);
	if (Material)
	{
		bool bClicked = DrawTextH(ETextAnchor::TopLeft, &bHighlight, color, "  %d: " S_HYPERLINK("%s (%s)") ", %d tris", Index, Material->Name, Material->GetClassName(), NumFaces);
		if (bClicked)
			JumpTo(Material);
	}
	else
	{
		DrawTextH(ETextAnchor::TopLeft, &bHighlight, color, "  %d: null, %d tris", Index, NumFaces);
	}
	if (bHighlight)
	{
		// HighlightMaterialIndex will be reset after mesh drawing. Can't reset it here because
		// if mat_0 = highlight, mat_1 = no_highlight will cause mat_1 to overwrite mat_0 'true' value.
		Inst->HighlightMaterialIndex = Index;
	}
}


#endif // RENDERING
