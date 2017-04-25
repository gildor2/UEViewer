#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"
#include "UnMesh2.h"

#include "Exporters/Exporters.h"
#include "TypeConvert.h"


CVertMeshViewer::CVertMeshViewer(UVertMesh* Mesh, CApplication* Window)
:	CMeshViewer(Mesh, Window)
,	AnimIndex(-1)
{
	CVertMeshInstance *VertInst = new CVertMeshInstance();
	VertInst->SetMesh(Mesh);
	Inst = VertInst;

	CVertMeshInstance *MeshInst = static_cast<CVertMeshInstance*>(Inst);
	// compute model center by Z-axis (vertical)
	CVec3 offset;
	const FBox &B = Mesh->BoundingBox;
#if 1
	VectorAdd(CVT(B.Min), CVT(B.Max), offset);
	offset.Scale(0.5f);
	MeshInst->BaseTransformScaled.UnTransformPoint(offset, offset);
#else
	// scale/translate origin
	float z = (B.Max.Z + B.Min.Z) / 2;
	z = (z - Mesh->MeshOrigin.Z) * Mesh->MeshScale.Z;	//!! bad formula
	offset.Set(0, 0, z);
#endif
	offset[2] += Mesh->BoundingSphere.R / 20;			// offset a bit up
	// offset view
	SetViewOffset(offset);
	// automatically scale view distance depending on model size
	float Radius = Mesh->BoundingSphere.R;
	if (Radius < 10) Radius = 10;
	SetDistScale(Mesh->MeshScale.X * Radius / 150);
}


#if TEST_FILES
void CVertMeshViewer::Test()
{
	CMeshViewer::Test();

	const UVertMesh *Mesh = static_cast<UVertMesh*>(Object);
	// verify some assumptions
// (macro broken)	VERIFY2(VertexCount, VertexCount);
	VERIFY_NOT_NULL(VertexCount);
	VERIFY_NULL(Verts2.Num());
	VERIFY(Verts.Num(), Normals.Num());
	assert(Mesh->Verts.Num() == Mesh->VertexCount * Mesh->FrameCount);
	VERIFY_NULL(f150.Num());
	VERIFY(BoundingBoxes.Num(), BoundingSpheres.Num());
	VERIFY(BoundingBoxes.Num(), FrameCount);
	VERIFY_NULL(AnimMeshVerts.Num());
	VERIFY_NULL(StreamVersion);
	// ULodMesh fields
	VERIFY_NOT_NULL(Faces.Num());
	VERIFY_NOT_NULL(Verts.Num());
//	VERIFY(FaceLevel.Num(), Faces.Num()); -- no for UMesh
	VERIFY_NOT_NULL(Wedges.Num());
}
#endif


void CVertMeshViewer::Dump()
{
	CMeshViewer::Dump();

	const UVertMesh *Mesh = static_cast<UVertMesh*>(Object);	// ULodMesh
	appPrintf(
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
		Mesh->HasImpostor ? "true" : "false", Mesh->SpriteMaterial ? MATERIAL_CAST(Mesh->SpriteMaterial)->Name : "none",
		Mesh->SkinTesselationFactor
	);
	int i;
	appPrintf("Textures: %d\n", Mesh->Textures.Num());
	for (i = 0; i < Mesh->Textures.Num(); i++)
	{
		const UUnrealMaterial *Tex = MATERIAL_CAST(Mesh->Textures[i]);
		if (Tex)
			appPrintf("  %d: %s (%s)\n", i, Tex->Name, Tex->GetClassName());
		else
			appPrintf("  %d: null\n", i);
	}
	appPrintf("Materials: %d\n", Mesh->Materials.Num());
	for (i = 0; i < Mesh->Materials.Num(); i++)
	{
		const FMeshMaterial &Mat = Mesh->Materials[i];
		appPrintf("  %d: tex=%d  flags=%08X\n", i, Mat.TextureIndex, Mat.PolyFlags);
	}

//	const UVertMesh *Mesh = static_cast<UVertMesh*>(Object);
	appPrintf(
		"\nVertMesh info:\n==============\n"
		"Verts # %d  Normals # %d\n"
		"f150 #         %d\n"
		"AnimSeqs #     %d\n"
		"Bounding: Boxes # %d  Spheres # %d\n"
		"VertexCount %d  FrameCount %d\n"
		"AnimMeshVerts   # %d\n"
		"StreamVersion  %d\n",
		Mesh->Verts2.Num(),
		Mesh->Normals.Num(),
		Mesh->f150.Num(),
		Mesh->AnimSeqs.Num(),
		Mesh->BoundingBoxes.Num(),
		Mesh->BoundingSpheres.Num(),
		Mesh->VertexCount,
		Mesh->FrameCount,
		Mesh->AnimMeshVerts.Num(),
		Mesh->StreamVersion
	);
}


void CVertMeshViewer::Export()
{
	CMeshViewer::Export();
	const UVertMesh *Mesh = static_cast<UVertMesh*>(Object);
	assert(Mesh);
	for (int i = 0; i < Mesh->Textures.Num(); i++)
	{
		const UUnrealMaterial *Tex = MATERIAL_CAST(Mesh->Textures[i]);
		ExportObject(Tex);
	}
}


void CVertMeshViewer::Draw2D()
{
	guard(CVertMeshViewer::Draw2D);

	CMeshViewer::Draw2D();

	CVertMeshInstance *MeshInst = static_cast<CVertMeshInstance*>(Inst);
	const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);

	// mesh
	DrawTextLeft(S_GREEN "Verts   : " S_WHITE "%d\n"
				 S_GREEN "Tris    : " S_WHITE "%d",
				 Mesh->Wedges.Num(),
				 Mesh->Faces.Num());

	// materials
	if (Inst->bColorMaterials)
	{
		for (int i = 0; i < Mesh->Materials.Num(); i++)
		{
			int TexIndex = Mesh->Materials[i].TextureIndex;
			if (TexIndex >= 0 && TexIndex < Mesh->Textures.Num())
				PrintMaterialInfo(i, MATERIAL_CAST(Mesh->Textures[TexIndex]), 0);
		}
		DrawTextLeft("");
	}

	// animation
	const char *AnimName;
	float Frame, NumFrames, Rate;
	MeshInst->GetAnimParams(AnimName, Frame, NumFrames, Rate);

	DrawTextBottomLeft(S_GREEN "Anim:" S_WHITE " %d/%d (%s) rate: %g frames: %g",
		AnimIndex+1, MeshInst->GetAnimCount(), AnimName, Rate, NumFrames);
	DrawTextBottomLeft(S_GREEN "Time:" S_WHITE " %.1f/%g", Frame, NumFrames);

	unguard;
}


void CVertMeshViewer::Draw3D(float TimeDelta)
{
	guard(CVertMeshViewer::Draw3D);
	assert(Inst);

	CVertMeshInstance *MeshInst = static_cast<CVertMeshInstance*>(Inst);

	// tick animations
	MeshInst->UpdateAnimation(TimeDelta);

	CMeshViewer::Draw3D(TimeDelta);

	unguard;
}


void CVertMeshViewer::ShowHelp()
{
	CMeshViewer::ShowHelp();
	DrawKeyHelp("[]",    "prev/next animation");
	DrawKeyHelp("<>",    "prev/next frame");
	DrawKeyHelp("Space", "play animation");
	DrawKeyHelp("X",     "play looped animation");
}


void CVertMeshViewer::ProcessKey(int key)
{
	guard(CVertMeshViewer::ProcessKey);

	CVertMeshInstance *MeshInst = static_cast<CVertMeshInstance*>(Inst);
	int NumAnims = MeshInst->GetAnimCount();

	const char *AnimName;
	float		Frame;
	float		NumFrames;
	float		Rate;
	MeshInst->GetAnimParams(AnimName, Frame, NumFrames, Rate);

	switch (key)
	{
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
			AnimName = MeshInst->GetAnimName(AnimIndex);
			MeshInst->TweenAnim(AnimName);	//?? no tweening for VertMesh
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
		MeshInst->FreezeAnimAt(Frame);
		break;

	case ' ':
		if (AnimIndex >= 0)
		{
			MeshInst->PlayAnim(AnimName);
		}
		break;
	case 'x':
		if (AnimIndex >= 0)
		{
			MeshInst->LoopAnim(AnimName);
		}
		break;

	default:
		CMeshViewer::ProcessKey(key);
	}

	unguard;
}


#endif // RENDERING
