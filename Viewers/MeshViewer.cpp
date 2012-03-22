#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"


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
	unsigned flags = 0;
	Inst->Draw(DrawFlags);
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
