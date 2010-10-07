#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"


static TArray<CLodMeshInstance*> Meshes;


CLodMeshViewer::CLodMeshViewer(ULodMesh *Mesh)
:	CMeshViewer(Mesh)
,	AnimIndex(-1)
,	CurrentTime(appMilliseconds())
{
	// compute model center by Z-axis (vertical)
	CVec3 offset;
	float z = (Mesh->BoundingBox.Max.Z + Mesh->BoundingBox.Min.Z) / 2;
	// scale/translate origin
	z = (z - Mesh->MeshOrigin.Z) * Mesh->MeshScale.Z;	//!! bad formula
	// offset view
	offset.Set(0, 0, z);
	SetViewOffset(offset);
	// automatically scale view distance depending on model size
	float Radius = Mesh->BoundingSphere.R;
	if (Radius < 10) Radius = 10;
	SetDistScale(Mesh->MeshScale.X * Radius / 150);
}


#if TEST_FILES
void CLodMeshViewer::Test()
{
	const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);
	// empty ...
//	VERIFY_NOT_NULL(Textures.Num());
//	VERIFY(CollapseWedgeThus.Num(), Wedges.Num()); -- no for UMesh
}
#endif


void CLodMeshViewer::Dump()
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
		if (Tex)
			printf("  %d: %s (%s)\n", i, Tex->Name, Tex->GetClassName());
		else
			printf("  %d: null\n", i);
	}
	printf("Materials: %d\n", Mesh->Materials.Num());
	for (i = 0; i < Mesh->Materials.Num(); i++)
	{
		const FMeshMaterial &Mat = Mesh->Materials[i];
		printf("  %d: tex=%d  flags=%08X\n", i, Mat.TextureIndex, Mat.PolyFlags);
	}
}


void CLodMeshViewer::TagMesh(CLodMeshInstance *NewInst)
{
	for (int i = 0; i < Meshes.Num(); i++)
		if (Meshes[i]->pMesh == NewInst->pMesh)
			return;	// already tagged
	Meshes.AddItem(NewInst);
}


void CLodMeshViewer::Draw2D()
{
	guard(CLodMeshViewer::Draw2D);

	CMeshViewer::Draw2D();

	for (int i = 0; i < Meshes.Num(); i++)
		DrawTextLeft("%d: %s", i, Meshes[i]->pMesh->Name);

	const char *AnimName;
	float Frame, NumFrames, Rate;
	CLodMeshInstance *LodInst = static_cast<CLodMeshInstance*>(Inst);
	LodInst->GetAnimParams(0, AnimName, Frame, NumFrames, Rate);

	DrawTextLeft(S_GREEN"Anim:"S_WHITE" %d/%d (%s) rate: %g frames: %g%s",
		AnimIndex+1, LodInst->GetAnimCount(), AnimName, Rate, NumFrames,
		LodInst->IsTweening() ? " [tweening]" : "");
#if 0 //UNREAL3
	//!! REMOVE LATER
	if (Inst->pMesh->IsA("SkeletalMesh") && AnimIndex >= 0)
	{
		const USkeletalMesh *Mesh = static_cast<const USkeletalMesh*>(LodInst->pMesh);
		if (Mesh->Animation && Mesh->Animation->IsA("AnimSet"))
		{
			const UAnimSet *Anim = static_cast<const UAnimSet*>(Mesh->Animation);
			const UAnimSequence *Seq = Anim->Sequences[AnimIndex];
			DrawTextLeft("Anim: %s  Comp: %s", *Seq->SequenceName, *Seq->RotationCompressionFormat);
			for (int i = 0; i < Anim->TrackBoneNames.Num(); i++)
			{
				int TransKeys = Seq->CompressedTrackOffsets[i*4+1];
				int RotKeys   = Seq->CompressedTrackOffsets[i*4+3];
				DrawTextLeft("  Bone: %-15s PosKeys: %3d RotKeys: %3d", *Anim->TrackBoneNames[i], TransKeys, RotKeys);
			}
		}
	}
#endif
	DrawTextLeft(S_GREEN"Time:"S_WHITE" %.1f/%g", Frame, NumFrames);

	if (Inst->bColorMaterials)
	{
		const ULodMesh *Mesh = static_cast<ULodMesh*>(Object);
		DrawTextLeft(S_GREEN"Textures: %d", Mesh->Textures.Num());
		for (int i = 0; i < Mesh->Textures.Num(); i++)
		{
			const UMaterial *Tex = Mesh->Textures[i];
			int color = i < 7 ? i + 1 : 7;
			if (Tex)
				DrawTextLeft("^%d  %d: %s (%s)", color, i, Tex->Name, Tex->GetClassName());
			else
				DrawTextLeft("^%d  %d: null", color, i);

		}
		DrawTextLeft("");
	}

	unguard;
}


void CLodMeshViewer::Draw3D()
{
	guard(CLodMeshViewer::Draw3D);
	assert(Inst);

	CLodMeshInstance *LodInst = static_cast<CLodMeshInstance*>(Inst);

	// tick animations
	unsigned time = appMilliseconds();
	float TimeDelta = (time - CurrentTime) / 1000.0f;
	CurrentTime = time;
	LodInst->UpdateAnimation(TimeDelta);

	CMeshViewer::Draw3D();
	for (int i = 0; i < Meshes.Num(); i++)
	{
		CLodMeshInstance *mesh = Meshes[i];
		if (mesh->pMesh == LodInst->pMesh) continue;	// no duplicates
		mesh->UpdateAnimation(TimeDelta);
		mesh->Draw();
	}

	unguard;
}


void CLodMeshViewer::ShowHelp()
{
	CObjectViewer::ShowHelp();
	DrawTextLeft("[]          prev/next animation\n"
				 "<>          prev/next frame\n"
				 "Space       play animation\n"
				 "X           play looped animation");
}


void CLodMeshViewer::ProcessKey(int key)
{
	int i;

	CLodMeshInstance *LodInst = static_cast<CLodMeshInstance*>(Inst);
	int NumAnims = LodInst->GetAnimCount();	//?? use Meshes[] instead ...

	const char *AnimName;
	float		Frame;
	float		NumFrames;
	float		Rate;
	LodInst->GetAnimParams(0, AnimName, Frame, NumFrames, Rate);

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
			AnimName = LodInst->GetAnimName(AnimIndex);
			LodInst->TweenAnim(AnimName, 0.25);	// change animation with tweening
			for (i = 0; i < Meshes.Num(); i++)
				Meshes[i]->TweenAnim(AnimName, 0.25);
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
		LodInst->FreezeAnimAt(Frame);
		for (i = 0; i < Meshes.Num(); i++)
			Meshes[i]->FreezeAnimAt(Frame);
		break;

	case ' ':
		if (AnimIndex >= 0)
		{
			LodInst->PlayAnim(AnimName);
			for (i = 0; i < Meshes.Num(); i++)
				Meshes[i]->PlayAnim(AnimName);
		}
		break;
	case 'x':
		if (AnimIndex >= 0)
		{
			LodInst->LoopAnim(AnimName);
			for (i = 0; i < Meshes.Num(); i++)
				Meshes[i]->LoopAnim(AnimName);
		}
		break;
#if 0
	//!! REMOVE
	case 'a' + KEY_CTRL:
		{
			int idx = LodInst->pMesh->Package->FindExport("K_AnimHuman_BaseMale", "AnimSet");
			if (idx != INDEX_NONE)
			{
				USkeletalMesh *Mesh = (USkeletalMesh*)LodInst->pMesh;
				Mesh->Animation = (UAnimSet*)LodInst->pMesh->Package->CreateExport(idx);
				LodInst->SetMesh(Mesh);	// again, for recomputing animation linkup
				LodInst->PlayAnim("Rotate_Settle_Rif");
			}
		}
		break;
#endif

	default:
		CMeshViewer::ProcessKey(key);
	}
}

#endif // RENDERING
