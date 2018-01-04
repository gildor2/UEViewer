#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"
#include "UnMathTools.h"

#include "UnMesh2.h"		// for UE2 USkeletalMesh and UMeshAnimation
#include "UnMesh3.h"		// for UAnimSet
#include "UnMesh4.h"
#include "SkeletalMesh.h"

#include "Exporters/Exporters.h"


#define TEST_ANIMS			1

//#define SHOW_BOUNDS		1
#define HIGHLIGHT_CURRENT	1

#define HIGHLIGHT_DURATION	0.5f		// seconds
#define HIGHLIGHT_STRENGTH	4.0f


#if HIGHLIGHT_CURRENT
static float TimeSinceCreate;
#endif

TArray<CSkelMeshInstance*> CSkelMeshViewer::TaggedMeshes;
UObject *GForceAnimSet = NULL;


static CAnimSet *GetAnimSet(const UObject *Obj)
{
	if (Obj->IsA("MeshAnimation"))		// UE1,UE2
		return static_cast<const UMeshAnimation*>(Obj)->ConvertedAnim;
#if UNREAL3
	if (Obj->IsA("AnimSet"))			// UE3
		return static_cast<const UAnimSet*>(Obj)->ConvertedAnim;
#endif
#if UNREAL4
	if (Obj->IsA("Skeleton"))
		return static_cast<const USkeleton*>(Obj)->ConvertedAnim;
#endif
	return NULL;
}


CSkelMeshViewer::CSkelMeshViewer(CSkeletalMesh* Mesh0, CApplication* Window)
:	CMeshViewer(Mesh0->OriginalMesh, Window)
,	Mesh(Mesh0)
,	AnimIndex(-1)
,	IsFollowingMesh(false)
,	ShowSkel(0)
,	ShowLabels(false)
,	ShowAttach(false)
,	ShowUV(false)
{
	CSkelMeshInstance *SkelInst = new CSkelMeshInstance();
	SkelInst->SetMesh(Mesh);
	if (GForceAnimSet)
	{
		CAnimSet *AttachAnim = GetAnimSet(GForceAnimSet);
		if (!AttachAnim)
		{
			appPrintf("WARNING: specified wrong AnimSet (%s class) object to attach\n", GForceAnimSet->GetClassName());
			GForceAnimSet = NULL;
		}
		if (AttachAnim)
			SkelInst->SetAnim(AttachAnim);
	}
	else if (Mesh->OriginalMesh->IsA("SkeletalMesh"))	// UE2 class
	{
		const USkeletalMesh *OriginalMesh = static_cast<USkeletalMesh*>(Mesh->OriginalMesh);
		if (OriginalMesh->Animation)
			SkelInst->SetAnim(OriginalMesh->Animation->ConvertedAnim);
	}
#if UNREAL4
	else if (Mesh->OriginalMesh->IsA("SkeletalMesh4"))
	{
		// UE4 SkeletalMesh has USkeleton reference, which collects all compatible animations in its PostLoad method
		const USkeletalMesh4* OriginalMesh = static_cast<USkeletalMesh4*>(Mesh->OriginalMesh);
		if (OriginalMesh->Skeleton)
			SkelInst->SetAnim(OriginalMesh->Skeleton->ConvertedAnim);
	}
#endif // UNREAL4
	Inst = SkelInst;
	// compute bounds for the current mesh
	CVec3 Mins, Maxs;
	if (Mesh0->Lods.Num())
	{
		const CSkelMeshLod &Lod = Mesh0->Lods[0];
		ComputeBounds(&Lod.Verts[0].Position, Lod.NumVerts, sizeof(CSkelMeshVertex), Mins, Maxs);
		// ... transform bounds
		SkelInst->BaseTransformScaled.UnTransformPoint(Mins, Mins);
		SkelInst->BaseTransformScaled.UnTransformPoint(Maxs, Maxs);
	}
	else
	{
		Mins = Maxs = nullVec3;
	}
	// extend bounds with additional meshes
	for (int i = 0; i < TaggedMeshes.Num(); i++)
	{
		CSkelMeshInstance* Inst = TaggedMeshes[i];
		if (Inst->pMesh != SkelInst->pMesh)
		{
			const CSkeletalMesh *Mesh2 = Inst->pMesh;
			// the same code for Inst
			CVec3 Bounds2[2];
			const CSkelMeshLod &Lod2 = Mesh2->Lods[0];
			ComputeBounds(&Lod2.Verts[0].Position, Lod2.NumVerts, sizeof(CSkelMeshVertex), Bounds2[0], Bounds2[1]);
			Inst->BaseTransformScaled.UnTransformPoint(Bounds2[0], Bounds2[0]);
			Inst->BaseTransformScaled.UnTransformPoint(Bounds2[1], Bounds2[1]);
			ComputeBounds(Bounds2, 2, sizeof(CVec3), Mins, Maxs, true);	// include Bounds2 into Mins/Maxs
		}
		// reset animation for all meshes
		TaggedMeshes[i]->TweenAnim(NULL, 0);
	}
	InitViewerPosition(Mins, Maxs);

#if HIGHLIGHT_CURRENT
	TimeSinceCreate = -2;		// ignore first 2 frames: 1st frame will be called after mesh changing, 2nd frame could load textures etc
#endif

#if SHOW_BOUNDS
	appPrintf("Bounds.min = %g %g %g\n", FVECTOR_ARG(Mesh->BoundingBox.Min));
	appPrintf("Bounds.max = %g %g %g\n", FVECTOR_ARG(Mesh->BoundingBox.Max));
	appPrintf("Origin     = %g %g %g\n", VECTOR_ARG(Mesh->MeshOrigin));
	appPrintf("Sphere     = %g %g %g R=%g\n", FVECTOR_ARG(Mesh->BoundingSphere), Mesh->BoundingSphere.R);
#endif // SHOW_BOUNDS
}


void CSkelMeshViewer::Dump()
{
	CMeshViewer::Dump();
	appPrintf("\n");
	Mesh->GetTypeinfo()->DumpProps(Mesh);

#if 0
	if (Mesh->OriginalMesh->IsA("SkeletalMesh"))	// UE2 class
	{
		const USkeletalMesh *OriginalMesh = static_cast<USkeletalMesh*>(Mesh->OriginalMesh);

		appPrintf(
			"\nSkelMesh info:\n==============\n"
			"Bones  # %4d  Points    # %4d  Points2  # %4d\n"
			"Wedges # %4d  Triangles # %4d\n"
			"CollapseWedge # %4d  f1C8      # %4d\n"
			"BoneDepth      %d\n"
			"WeightIds # %d  BoneInfs # %d  VertInfs # %d\n"
			"Attachments #  %d\n"
			"LODModels # %d\n"
			"Animations: %s\n",
			OriginalMesh->RefSkeleton.Num(),
			OriginalMesh->Points.Num(), OriginalMesh->Points2.Num(),
			OriginalMesh->Wedges.Num(), OriginalMesh->Triangles.Num(),
			OriginalMesh->CollapseWedge.Num(), OriginalMesh->f1C8.Num(),
			OriginalMesh->SkeletalDepth,
			OriginalMesh->WeightIndices.Num(), OriginalMesh->BoneInfluences.Num(), OriginalMesh->VertInfluences.Num(),
			OriginalMesh->AttachBoneNames.Num(),
			OriginalMesh->LODModels.Num(),
			OriginalMesh->Animation ? OriginalMesh->Animation->Name : "None"
		);

		int i;

		// check bone sort order (assumed, that child go after parent)
		for (i = 0; i < OriginalMesh->RefSkeleton.Num(); i++)
		{
			const FMeshBone &B = OriginalMesh->RefSkeleton[i];
			if (B.ParentIndex >= i + 1) appNotify("bone[%d] has parent %d", i+1, B.ParentIndex);
		}

		for (i = 0; i < OriginalMesh->LODModels.Num(); i++)
		{
			appPrintf("model # %d\n", i);
			const FStaticLODModel &lod = OriginalMesh->LODModels[i];
			appPrintf(
				"  SkinningData=%d  SkinPoints=%d inf=%d  wedg=%d dynWedges=%d faces=%d  points=%d\n"
				"  DistanceFactor=%g  Hysteresis=%g  SharedVerts=%d  MaxInfluences=%d  114=%d  118=%d\n"
				"  smoothInds=%d  rigidInds=%d  vertStream.Size=%d\n",
				lod.SkinningData.Num(),
				lod.SkinPoints.Num(),
				lod.VertInfluences.Num(),
				lod.Wedges.Num(), lod.NumDynWedges,
				lod.Faces.Num(),
				lod.Points.Num(),
				lod.LODDistanceFactor, lod.LODHysteresis, lod.NumSharedVerts, lod.LODMaxInfluences, lod.f114, lod.f118,
				lod.SmoothIndices.Indices.Num(), lod.RigidIndices.Indices.Num(), lod.VertexStream.Verts.Num());

			int i0 = 99999999, i1 = -99999999;
			int j;
			for (j = 0; j < lod.SmoothIndices.Indices.Num(); j++)
			{
				int x = lod.SmoothIndices.Indices[j];
				if (x < i0) i0 = x;
				if (x > i1) i1 = x;
			}
			appPrintf("  smoothIndices: [%d .. %d]\n", i0, i1);
			i0 = 99999999; i1 = -99999999;
			for (j = 0; j < lod.RigidIndices.Indices.Num(); j++)
			{
				int x = lod.RigidIndices.Indices[j];
				if (x < i0) i0 = x;
				if (x > i1) i1 = x;
			}
			appPrintf("  rigidIndices:  [%d .. %d]\n", i0, i1);

			const TArray<FSkelMeshSection> *sec[2];
			sec[0] = &lod.SmoothSections;
			sec[1] = &lod.RigidSections;
			static const char *secNames[] = { "smooth", "rigid" };
			for (int k = 0; k < 2; k++)
			{
				appPrintf("  %s sections: %d\n", secNames[k], sec[k]->Num());
				for (int j = 0; j < sec[k]->Num(); j++)
				{
					const FSkelMeshSection &S = (*sec[k])[j];
					appPrintf("    %d:  mat=%d %d [w=%d .. %d] %d b=%d %d [f=%d + %d]\n", j,
						S.MaterialIndex, S.MinStreamIndex, S.MinWedgeIndex, S.MaxWedgeIndex,
						S.NumStreamIndices, S.BoneIndex, S.fE, S.FirstFace, S.NumFaces);
				}
			}
		}
	}
#endif
}


void CSkelMeshViewer::Export()
{
	CMeshViewer::Export();

	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);
	const CAnimSet *Anim = MeshInst->GetAnim();
	if (Anim) ExportObject(Anim->OriginalAnim);

	for (int i = 0; i < TaggedMeshes.Num(); i++)
		ExportObject(TaggedMeshes[i]->pMesh->OriginalMesh);
}


void CSkelMeshViewer::TagMesh(CSkelMeshInstance *Inst)
{
	for (int i = 0; i < TaggedMeshes.Num(); i++)
	{
		if (TaggedMeshes[i]->pMesh == Inst->pMesh)
		{
			// already tagged, remove
			delete TaggedMeshes[i];
			TaggedMeshes.RemoveAt(i);
			return;
		}
	}
	// not tagget yet, create a copy of the mesh
	CSkelMeshInstance* NewInst = new CSkelMeshInstance();
	NewInst->SetMesh(Inst->pMesh);
	// remember animation which is linked to the object (UE2, UE4)
	NewInst->SetAnim(Inst->GetAnim());
	TaggedMeshes.Add(NewInst);
}


void CSkelMeshViewer::UntagAllMeshes()
{
	for (int i = 0; i < TaggedMeshes.Num(); i++)
		delete TaggedMeshes[i];
	TaggedMeshes.Empty();
}


void CSkelMeshViewer::Draw2D()
{
	CMeshViewer::Draw2D();

	if (!Mesh->Lods.Num())
	{
		DrawTextLeft(S_RED "Mesh has no LODs");
		return;
	}

	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);
	const CSkelMeshLod &Lod = Mesh->Lods[MeshInst->LodNum];

	if (ShowUV)
	{
		DisplayUV(Lod.Verts, sizeof(CSkelMeshVertex), &Lod, MeshInst->UVIndex);
	}

#if UNREAL4
	if (Mesh->OriginalMesh->IsA("SkeletalMesh4"))
	{
		USkeletalMesh4* Mesh4 = static_cast<USkeletalMesh4*>(Mesh->OriginalMesh);
		if (Mesh4->Skeleton)
			DrawTextLeft(S_GREEN"Skeleton: " S_WHITE "%s", Mesh4->Skeleton->Name);
		else
			DrawTextBottomLeft(S_RED"WARNING: no skeleton, animation will not work!");
	}
#endif // UNREAL4

	// mesh
	DrawTextLeft(S_GREEN "LOD     : " S_WHITE "%d/%d\n"
				 S_GREEN "Verts   : " S_WHITE "%d\n"
				 S_GREEN "Tris    : " S_WHITE "%d\n"
				 S_GREEN "UV Set  : " S_WHITE "%d/%d\n"
				 S_GREEN "Bones   : " S_WHITE "%d",
				 MeshInst->LodNum+1, Mesh->Lods.Num(),
				 Lod.NumVerts, Lod.Indices.Num() / 3,
				 MeshInst->UVIndex+1, Lod.NumTexCoords,
				 Mesh->RefSkeleton.Num());
	//!! show MaxInfluences etc

	// materials
	if (Inst->bColorMaterials)
	{
		for (int i = 0; i < Lod.Sections.Num(); i++)
			PrintMaterialInfo(i, Lod.Sections[i].Material, Lod.Sections[i].NumFaces);
		DrawTextLeft("");
	}

	// show extra meshes
	for (int i = 0; i < TaggedMeshes.Num(); i++)
		DrawTextLeft("%s%d: %s", (TaggedMeshes[i]->pMesh == MeshInst->pMesh) ? S_RED : S_WHITE, i, TaggedMeshes[i]->pMesh->OriginalMesh->Name);

	// show animation information
	const CAnimSet *AnimSet = MeshInst->GetAnim();
	if (AnimSet)
	{
		DrawTextBottomLeft("\n" S_GREEN "AnimSet : " S_WHITE "%s", AnimSet->OriginalAnim->Name);

		const char *OnOffStatus = NULL;
		switch (MeshInst->RotationMode)
		{
		case EARO_AnimSet:
			OnOffStatus = (AnimSet->AnimRotationOnly) ? "on" : "off";
			break;
		case EARO_ForceEnabled:
			OnOffStatus = S_RED "force on";
			break;
		case EARO_ForceDisabled:
			OnOffStatus = S_RED "force off";
			break;
		}
		DrawTextBottomLeft(S_GREEN "RotationOnly:" S_WHITE " %s", OnOffStatus);
		if (AnimSet->UseAnimTranslation.Num() || AnimSet->ForceMeshTranslation.Num())
		{
			DrawTextBottomLeft(S_GREEN "UseAnimBones:" S_WHITE " %d " S_GREEN "ForceMeshBones:" S_WHITE " %d",
				AnimSet->UseAnimTranslation.Num(), AnimSet->ForceMeshTranslation.Num());
		}

		const CAnimSequence *Seq = MeshInst->GetAnim(0);
		if (Seq)
		{
			DrawTextBottomLeft(S_GREEN "Anim:" S_WHITE " %d/%d (%s) " S_GREEN "Rate:" S_WHITE " %g " S_GREEN "Frames:" S_WHITE " %d",
				AnimIndex+1, MeshInst->GetAnimCount(), *Seq->Name, Seq->Rate, Seq->NumFrames);
			DrawTextBottomRight(S_GREEN "Time:" S_WHITE " %4.1f/%d", MeshInst->GetAnimTime(0), Seq->NumFrames);
#if ANIM_DEBUG_INFO
			const FString& DebugText = Seq->DebugInfo;
			if (DebugText.Num())
				DrawTextBottomLeft(S_RED"Info:" S_WHITE " %s", *DebugText);
#endif
		}
		else
		{
			DrawTextBottomLeft(S_GREEN "Anim:" S_WHITE " 0/%d (none)", MeshInst->GetAnimCount());
		}
	}
}


void CSkelMeshViewer::Draw3D(float TimeDelta)
{
	guard(CSkelMeshViewer::Draw3D);
	assert(Inst);

	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);

	// tick animations
	MeshInst->UpdateAnimation(TimeDelta);

#if HIGHLIGHT_CURRENT
	if (TimeSinceCreate < 0)
		TimeSinceCreate += 1.0f;			// ignore this frame for highlighting
	else
		TimeSinceCreate += TimeDelta;

	float lightAmbient[4];
	float boost = 0;
	float highlightTime = max(TimeSinceCreate, 0);

	if (TaggedMeshes.Num() && highlightTime < HIGHLIGHT_DURATION)
	{
		if (highlightTime > HIGHLIGHT_DURATION / 2)
			highlightTime = HIGHLIGHT_DURATION - highlightTime;	// fade
		boost = HIGHLIGHT_STRENGTH * highlightTime / (HIGHLIGHT_DURATION / 2);

		glGetMaterialfv(GL_FRONT, GL_AMBIENT, lightAmbient);
		lightAmbient[0] += boost;
		lightAmbient[1] += boost;
		lightAmbient[2] += boost;
		glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
		glMaterialfv(GL_FRONT, GL_AMBIENT, lightAmbient);
	}
#endif // HIGHLIGHT_CURRENT

	// draw main mesh
	CMeshViewer::Draw3D(TimeDelta);

#if HIGHLIGHT_CURRENT
	if (boost > 0)
	{
		lightAmbient[0] -= boost;
		lightAmbient[1] -= boost;
		lightAmbient[2] -= boost;
		glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
		glMaterialfv(GL_FRONT, GL_AMBIENT, lightAmbient);
	}
#endif // HIGHLIGHT_CURRENT

	int i;

#if SHOW_BOUNDS
	//?? separate function for drawing wireframe Box (pass CCoords, Mins and Maxs to function)
	BindDefaultMaterial(true);
	glBegin(GL_LINES);
	CVec3 verts[8];
	static const int inds[] =
	{
		0,1, 2,3, 0,2, 1,3,
		4,5, 6,7, 4,6, 5,7,
		0,4, 1,5, 2,6, 3,7
	};
	const FBox &B = MeshInst->pMesh->BoundingBox;
	for (i = 0; i < 8; i++)
	{
		CVec3 &v = verts[i];
		v[0] = (i & 1) ? B.Min.X : B.Max.X;
		v[1] = (i & 2) ? B.Min.Y : B.Max.Y;
		v[2] = (i & 4) ? B.Min.Z : B.Max.Z;
		MeshInst->BaseTransformScaled.UnTransformPoint(v, v);
	}
	// can use glDrawElements(), but this will require more GL setup
	glColor3f(0.5,0.5,1);
	for (i = 0; i < ARRAY_COUNT(inds) / 2; i++)
	{
		glVertex3fv(verts[inds[i*2  ]].v);
		glVertex3fv(verts[inds[i*2+1]].v);
	}
	glEnd();
	glColor3f(1, 1, 1);
#endif // SHOW_BOUNDS

	for (i = 0; i < TaggedMeshes.Num(); i++)
	{
		CSkelMeshInstance *mesh = TaggedMeshes[i];
		if (mesh->pMesh == MeshInst->pMesh) continue;	// avoid duplicates
		mesh->UpdateAnimation(TimeDelta);
		DrawMesh(mesh);
	}

	//?? make this common - place into DrawMesh() ?
	//?? problem: overdraw of skeleton when displaying multiple meshes
	//?? (especially when ShowInfluences is on, and meshes has different bone counts - the same bone
	//?? will be painted multiple times with different colors)
	if (ShowSkel)
		MeshInst->DrawSkeleton(ShowLabels, (DrawFlags & DF_SHOW_INFLUENCES) != 0);
	if (ShowAttach)
		MeshInst->DrawAttachments();

	if (IsFollowingMesh)
		FocusCameraOnPoint(MeshInst->GetMeshOrigin());

	unguard;
}


void CSkelMeshViewer::DrawMesh(CMeshInstance *Inst)
{
	if (ShowSkel == 2) return;	// no mesh should be painted in this mode

	CSkelMeshInstance *mesh = static_cast<CSkelMeshInstance*>(Inst);
	mesh->Draw(DrawFlags);
}


void CSkelMeshViewer::ShowHelp()
{
	CMeshViewer::ShowHelp();
	DrawKeyHelp("[]",     "prev/next animation");
	DrawKeyHelp("<>",     "prev/next frame");
	DrawKeyHelp("Space",  "play animation");
	DrawKeyHelp("X",      "play looped animation");
	DrawKeyHelp("L",      "cycle mesh LODs");
	DrawKeyHelp("U",      "cycle UV sets");
	DrawKeyHelp("S",      "show skeleton");
	DrawKeyHelp("B",      "show bone names");
	DrawKeyHelp("I",      "show influences");
	DrawKeyHelp("A",      "show attach sockets");
	DrawKeyHelp("F",      "focus camera on mesh");
	DrawKeyHelp("Ctrl+B", "dump skeleton to console");
	DrawKeyHelp("Ctrl+A", "cycle mesh animation sets");
	DrawKeyHelp("Ctrl+R", "toggle animation translaton mode");
	DrawKeyHelp("Ctrl+T", "tag/untag mesh");
	DrawKeyHelp("Ctrl+U", "display UV");
}


void CSkelMeshViewer::ProcessKey(int key)
{
	guard(CSkelMeshViewer::ProcessKey);
#if TEST_ANIMS
	static float Alpha = -1.0f; //!!!!
#endif
	int i;

	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);
	int NumAnims = MeshInst->GetAnimCount();	//?? use Meshes[] instead ...

	const char *AnimName;
	float		Frame;
	float		NumFrames;
	float		Rate;
	MeshInst->GetAnimParams(0, AnimName, Frame, NumFrames, Rate);

	switch (key)
	{
	// animation control
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
			MeshInst->TweenAnim(AnimName, 0.25);	// change animation with tweening
			for (i = 0; i < TaggedMeshes.Num(); i++)
				TaggedMeshes[i]->TweenAnim(AnimName, 0.25);
		}
		break;

	case ',':		// '<'
	case '.':		// '>'
		if (NumFrames)
		{
			if (key == ',')
				Frame -= 0.2f;
			else
				Frame += 0.2f;
			Frame = bound(Frame, 0, NumFrames-1);
			MeshInst->FreezeAnimAt(Frame);
			for (i = 0; i < TaggedMeshes.Num(); i++)
				TaggedMeshes[i]->FreezeAnimAt(Frame);
		}
		break;

	case ' ':
		if (AnimIndex >= 0)
		{
			MeshInst->PlayAnim(AnimName);
			for (i = 0; i < TaggedMeshes.Num(); i++)
				TaggedMeshes[i]->PlayAnim(AnimName);
		}
		break;
	case 'x':
		if (AnimIndex >= 0)
		{
			MeshInst->LoopAnim(AnimName);
			for (i = 0; i < TaggedMeshes.Num(); i++)
				TaggedMeshes[i]->LoopAnim(AnimName);
		}
		break;

	// mesh debug output
	case 'l':
		if (++MeshInst->LodNum >= Mesh->Lods.Num())
			MeshInst->LodNum = 0;
		break;
	case 'u':
		if (++MeshInst->UVIndex >= Mesh->Lods[MeshInst->LodNum].NumTexCoords)
			MeshInst->UVIndex = 0;
		break;
	case 's':
		if (++ShowSkel > 2)
			ShowSkel = 0;
		break;
	case 'b':
		ShowLabels = !ShowLabels;
		break;
	case 'a':
		ShowAttach = !ShowAttach;
		break;
	case 'i':
		DrawFlags ^= DF_SHOW_INFLUENCES;
		break;
	case 'b'|KEY_CTRL:
		MeshInst->DumpBones();
		break;

#if TEST_ANIMS
	//!! testing, remove later
	case 'y':
		if (Alpha >= 0)
		{
			Alpha = -1;
			MeshInst->ClearSkelAnims();
			return;
		}
		Alpha = 0;
		MeshInst->LoopAnim("CrouchF", 1, 0, 2);
//		MeshInst->LoopAnim("WalkF", 1, 0, 2);
//		MeshInst->LoopAnim("RunR", 1, 0, 2);
//		MeshInst->LoopAnim("SwimF", 1, 0, 2);
		MeshInst->SetSecondaryAnim(2, "RunF");
		break;
	case 'y'|KEY_CTRL:
		MeshInst->SetBlendParams(0, 1.0f, "b_MF_Forearm_R");
//		MeshInst->SetBlendParams(0, 1.0f, "b_MF_Hand_R");
//		MeshInst->SetBlendParams(0, 1.0f, "b_MF_ForeTwist2_R");
		break;
	case 'c':
		Alpha -= 0.02;
		if (Alpha < 0) Alpha = 0;
		MeshInst->SetSecondaryBlend(2, Alpha);
		break;
	case 'v':
		Alpha += 0.02;
		if (Alpha > 1) Alpha = 1;
		MeshInst->SetSecondaryBlend(2, Alpha);
		break;

/*	case 'u'|KEY_CTRL:
		MeshInst->LoopAnim("Gesture_Taunt02", 1, 0, 1);
		MeshInst->SetBlendParams(1, 1.0f, "Bip01 Spine1");
		MeshInst->LoopAnim("WalkF", 1, 0, 2);
		MeshInst->SetBlendParams(2, 1.0f, "Bip01 R Thigh");
		MeshInst->LoopAnim("AssSmack", 1, 0, 4);
		MeshInst->SetBlendParams(4, 1.0f, "Bip01 L UpperArm");
		break; */
#endif // TEST_ANIMS

	case 'a'|KEY_CTRL:
		{
			const CAnimSet *PrevAnim = MeshInst->GetAnim();
			// find next animation set (code is similar to PAGEDOWN handler)
			int looped = 0;
			int ObjIndex = -1;
			bool found = (PrevAnim == NULL);			// whether previous AnimSet was found; NULL -> any
			while (true)
			{
				ObjIndex++;
				if (ObjIndex >= UObject::GObjObjects.Num())
				{
					ObjIndex = 0;
					looped++;
					if (looped > 1) break;				// no other objects
				}
				const UObject *Obj = UObject::GObjObjects[ObjIndex];
				const CAnimSet *Anim = GetAnimSet(Obj);
				if (!Anim) continue;

				if (Anim == PrevAnim)
				{
					if (found) break;					// loop detected
					found = true;
					continue;
				}

				if (found && Anim)
				{
					// found desired animation set
					MeshInst->SetAnim(Anim);			// will rebind mesh to new animation set
					for (int i = 0; i < TaggedMeshes.Num(); i++)
						TaggedMeshes[i]->SetAnim(Anim);
					AnimIndex = -1;
					appPrintf("Bound %s'%s' to %s'%s'\n", Object->GetClassName(), Object->Name, Obj->GetClassName(), Obj->Name);
					break;
				}
			}
		}
		break;

	case 't'|KEY_CTRL:
		TagMesh(MeshInst);
		break;

	case 'r'|KEY_CTRL:
		{
			int mode = MeshInst->RotationMode + 1;
			if (mode > EARO_ForceDisabled) mode = 0;
			MeshInst->RotationMode = (EAnimRotationOnly)mode;
			for (int i = 0; i < TaggedMeshes.Num(); i++)
				TaggedMeshes[i]->RotationMode = (EAnimRotationOnly)mode;
		}
		break;

	case 'u'|KEY_CTRL:
		ShowUV = !ShowUV;
		break;

	case 'f':
		IsFollowingMesh = true;
		break;

	default:
		CMeshViewer::ProcessKey(key);
	}

	unguard;
}


void CSkelMeshViewer::ProcessKeyUp(int key)
{
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);
	switch (key)
	{
	case 'f':
		FocusCameraOnPoint(MeshInst->GetMeshOrigin());
		IsFollowingMesh = false;
		break;
	default:
		CMeshViewer::ProcessKeyUp(key);
	}
}


#endif // RENDERING
