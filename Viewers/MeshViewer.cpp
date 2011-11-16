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


void CMeshViewer::Draw3D()
{
	guard(CMeshViewer::Draw3D);
	assert(Inst);

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

	// draw mesh
	glPolygonMode(GL_FRONT_AND_BACK, Inst->bWireframe ? GL_LINE : GL_FILL);	//?? bWireframe is inside Inst, but used here only ?
	Inst->Draw();

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
	switch (key)
	{
	case 'n':
		Inst->bShowNormals = !Inst->bShowNormals;
		break;

	case 'm':
		Inst->bColorMaterials = !Inst->bColorMaterials;
		break;

	case 'w':
		Inst->bWireframe = !Inst->bWireframe;
		break;

	default:
		CObjectViewer::ProcessKey(key);
	}
}


void CMeshViewer::PrintMaterialInfo(int Index, UUnrealMaterial *Material, int NumFaces)
{
	int color = Index < 7 ? Index + 1 : 7;
	if (Material)
		DrawTextLeft("^%d  %d: %s (%s), %d tris", color, Index, Material->Name, Material->GetClassName(), NumFaces);
	else
		DrawTextLeft("^%d  %d: null, %d tris", color, Index, NumFaces);
}


#endif // RENDERING
