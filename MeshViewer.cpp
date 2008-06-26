#include "ObjectViewer.h"
#include "MeshInstance.h"

//!! move outside
void SetAxis(const FRotator &Rot, CAxis &Axis)
{
	CVec3 angles;
	angles[YAW]   = -Rot.Yaw   / 32768.0f * 180;
	angles[ROLL]  = -Rot.Pitch / 32768.0f * 180;
	angles[PITCH] = -Rot.Roll  / 32768.0f * 180;
	Axis.FromEuler(angles);
}


CMeshViewer::CMeshViewer(ULodMesh *Mesh)
:	CObjectViewer(Mesh)
,	AnimIndex(-1)
,	bShowNormals(false)
,	bWireframe(false)
,	CurrentTime(appMilliseconds())
{
	// compute model center by Z-axis (vertical)
	CVec3 offset;
	float z = (Mesh->BoundingBox.Max.Z + Mesh->BoundingBox.Min.Z) / 2;
	// scale/translate origin
	z = ((z - Mesh->MeshOrigin.Z) * Mesh->MeshScale.Z);
	// offset view
	offset.Set(0, 0, z);
	SetViewOffset(offset);
	// automatically scale view distance depending on model size
	SetDistScale(Mesh->MeshScale.X * Mesh->BoundingSphere.R / 150);
}


CMeshViewer::~CMeshViewer()
{
	delete Inst;
}


#if TEST_FILES
void CMeshViewer::Test()
{
	const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);
	// empty ...
	VERIFY_NOT_NULL(Textures.Num());
	VERIFY(CollapseWedgeThus.Num(), Wedges.Num());
}
#endif


void CMeshViewer::Dump()
{
	CObjectViewer::Dump();
	const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);
	printf(
		"\nLodMesh info:\n=============\n"
		"version        %d\n"
		"VertexCount    %d\n"
		"Verts #        %d\n"
		"MeshScale      %g %g %g\n"
		"MeshOrigin     %g %g %g\n"
		"RotOrigin      %d %d %d\n"
		"FaceLevel #    %d\n"
		"Faces #        %d\n"
		"CollapseWedgeThus # %d\n"
		"Wedges #       %d\n"
		"Impostor: %s  (%s)\n"
		"SkinTessFactor %g\n",
		Mesh->Version,
		Mesh->VertexCount,
		Mesh->Verts.Num(),
		FVECTOR_ARG(Mesh->MeshScale),
		FVECTOR_ARG(Mesh->MeshOrigin),
		FROTATOR_ARG(Mesh->RotOrigin),
		Mesh->FaceLevel.Num(),
		Mesh->Faces.Num(),
		Mesh->CollapseWedgeThus.Num(),
		Mesh->Wedges.Num(),
		Mesh->HasImpostor ? "true" : "false", Mesh->SpriteMaterial ? Mesh->SpriteMaterial->Name : "none",
		Mesh->SkinTesselationFactor
	);
	int i;
	printf("Textures: %d\n", Mesh->Textures.Num());
	for (i = 0; i < Mesh->Textures.Num(); i++)
	{
		const UMaterial *Tex = Mesh->Textures[i];
		printf("  %d: %s\n", i, Tex ? Tex->Name : "null");
	}
	printf("Materials: %d\n", Mesh->Materials.Num());
	for (i = 0; i < Mesh->Materials.Num(); i++)
	{
		const FMeshMaterial &Mat = Mesh->Materials[i];
		printf("  %d: tex=%d  flags=%08X\n", i, Mat.TextureIndex, Mat.PolyFlags);
	}
}


void CMeshViewer::Draw2D()
{
	guard(CMeshViewer::Draw2D);
	CObjectViewer::Draw2D();

	const char *AnimName;
	float Frame, NumFrames, Rate;
	Inst->GetAnimParams(0, AnimName, Frame, NumFrames, Rate);

	DrawTextLeft(S_GREEN"Anim:"S_WHITE" %d/%d (%s) rate: %g frames: %g%s",
		AnimIndex+1, Inst->GetAnimCount(), AnimName, Rate, NumFrames,
		Inst->IsTweening() ? " [tweening]" : "");
	DrawTextLeft(S_GREEN"Time:"S_WHITE" %.1f/%g", Frame, NumFrames);

	if (Inst->Viewport->bColorMaterials)
	{
		const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);
		DrawTextLeft(S_GREEN"Textures: %d", Mesh->Textures.Num());
		for (int i = 0; i < Mesh->Textures.Num(); i++)
		{
			const UMaterial *Tex = Mesh->Textures[i];
			DrawTextLeft("^%d  %d: %s",
				i < 7 ? i + 1 : 7,
				i, Tex ? Tex->Name : "null");
		}
		DrawTextLeft("");
	}

	unguard;
}


void CMeshViewer::Draw3D()
{
	guard(CMeshViewer::Draw3D);
	assert(Inst);

	// tick animations
	unsigned time = appMilliseconds();
	float TimeDelta = (time - CurrentTime) / 1000.0f;
	CurrentTime = time;
	Inst->UpdateAnimation(TimeDelta);

	// draw axis
	glDisable(GL_TEXTURE_2D);
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
	glPolygonMode(GL_FRONT_AND_BACK, bWireframe ? GL_LINE : GL_FILL);
	Inst->Draw();

	// restore draw state
	glColor3f(1, 1, 1);
	glDisable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	unguard;
}


void CMeshViewer::ShowHelp()
{
	CObjectViewer::ShowHelp();
	DrawTextLeft("N           show normals\n"
				 "W           toggle wireframe\n"
				 "M           colorize materials\n"
				 "[]          prev/next animation\n"
				 "<>          prev/next frame\n"
				 "Space       play animation\n"
				 "X           play looped animation");
}


void CMeshViewer::ProcessKey(int key)
{
	int NumAnims = Inst->GetAnimCount();

	const char *AnimName;
	float		Frame;
	float		NumFrames;
	float		Rate;
	Inst->GetAnimParams(0, AnimName, Frame, NumFrames, Rate);

	switch (key)
	{
	case 'n':
		bShowNormals = !bShowNormals;
		break;

	case 'm':
		bColorMaterials = !bColorMaterials;
		break;

	case 'w':
		bWireframe = !bWireframe;
		break;

	case '[':
	case ']':
		if (NumAnims)
		{
			if (key == '[')
			{
				if (--AnimIndex < -1)
					AnimIndex = NumAnims - 1;
			}
			else
			{
				if (++AnimIndex >= NumAnims)
					AnimIndex = -1;
			}
			// note: AnimIndex changed now
			AnimName = Inst->GetAnimName(AnimIndex);
			Inst->TweenAnim(AnimName, 0.25);	// change animation with tweening
		}
		break;

	case ',':		// '<'
	case '.':		// '>'
		if (key == ',')
		{
			Frame -= 0.2f;
			if (Frame < 0)
				Frame = 0;
		}
		else
		{
			Frame += 0.2f;
			if (Frame > NumFrames - 1)
				Frame = NumFrames - 1;
			if (Frame < 0)
				Frame = 0;
		}
		Inst->FreezeAnimAt(Frame);
		break;

	case ' ':
		if (AnimIndex >= 0)
			Inst->PlayAnim(AnimName);
		break;
	case 'x':
		if (AnimIndex >= 0)
			Inst->LoopAnim(AnimName);
		break;

	default:
		CObjectViewer::ProcessKey(key);
	}
}
