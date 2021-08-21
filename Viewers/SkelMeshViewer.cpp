#include "Core.h"
#include "UnCore.h"

#if RENDERING

#include "UnObject.h"

#include "ObjectViewer.h"
#include "../MeshInstance/MeshInstance.h"
#include "UnrealMesh/UnMathTools.h"

#include "UnrealMesh/UnMesh2.h"		// for UE2 USkeletalMesh and UMeshAnimation
#include "UnrealMesh/UnMesh3.h"		// for UAnimSet
#include "UnrealMesh/UnMesh4.h"
#include "Mesh/SkeletalMesh.h"

#if HAS_UI
#include "BaseDialog.h"
#include "../UmodelTool/ProgressDialog.h"
#endif

#include "Exporters/Exporters.h"
#include "UnrealPackage/UnPackage.h"
#include "UnrealPackage/PackageUtils.h"


#define TEST_ANIMS			1

//#define SHOW_BOUNDS		1

#define HIGHLIGHT_DURATION	0.5f		// seconds
#define HIGHLIGHT_STRENGTH	2.0f


static float TimeSinceCreate;

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
,	Anim(NULL)
,	AnimIndex(-1)
,	IsFollowingMesh(false)
,	ShowSkel(0)
,	ShowLabels(false)
,	ShowAttach(false)
,	ShowUV(false)
,	bIsUE4Mesh(false)
,	HighlightMeshIndex(-1)
{
	guard(CSkelMeshViewer::CSkelMeshViewer);

	CSkelMeshInstance *SkelInst = new CSkelMeshInstance();
	Inst = SkelInst;
	SkelInst->SetMesh(Mesh);

	// Automatically attach animation if any
	CAnimSet* AttachAnim = NULL;
	if (GForceAnimSet)
	{
		AttachAnim = GetAnimSet(GForceAnimSet);
		if (!AttachAnim)
		{
			appPrintf("WARNING: specified wrong AnimSet (%s class) object to attach\n", GForceAnimSet->GetClassName());
			GForceAnimSet = NULL;
		}
	}
	else if (Mesh->OriginalMesh->IsA("SkeletalMesh"))	// UE2 class
	{
		const USkeletalMesh *OriginalMesh = static_cast<const USkeletalMesh*>(Mesh->OriginalMesh);
		if (OriginalMesh->Animation)
			AttachAnim = OriginalMesh->Animation->ConvertedAnim;
	}
#if UNREAL4
	else if (Mesh->OriginalMesh->IsA("SkeletalMesh4"))
	{
		// UE4 SkeletalMesh has USkeleton reference, which collects all compatible animations in its PostLoad method
		const USkeletalMesh4* OriginalMesh = static_cast<const USkeletalMesh4*>(Mesh->OriginalMesh);
		bIsUE4Mesh = true;
		Skeleton = OriginalMesh->Skeleton;
		if (Skeleton)
			AttachAnim = Skeleton->ConvertedAnim;
	}
#endif // UNREAL4
	if (AttachAnim)
		SetAnim(AttachAnim);

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
	for (CSkelMeshInstance* Inst : TaggedMeshes)
	{
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
		Inst->TweenAnim(NULL, 0);
	}
	InitViewerPosition(Mins, Maxs);

	TimeSinceCreate = -2;		// ignore first 2 frames: 1st frame will be called after mesh changing, 2nd frame could load textures etc

#if SHOW_BOUNDS
	appPrintf("Bounds.min = %g %g %g\n", VECTOR_ARG(Mesh->BoundingBox.Min));
	appPrintf("Bounds.max = %g %g %g\n", VECTOR_ARG(Mesh->BoundingBox.Max));
	appPrintf("Origin     = %g %g %g\n", VECTOR_ARG(Mesh->MeshOrigin));
	appPrintf("Sphere     = %g %g %g R=%g\n", VECTOR_ARG(Mesh->BoundingSphere), Mesh->BoundingSphere.R);
#endif // SHOW_BOUNDS

	unguard;
}


void CSkelMeshViewer::SetAnim(const CAnimSet* AnimSet)
{
	guard(CSkelMeshViewer::SetAnim);

	// Store animation reference locally in viewer class
	Anim = AnimSet;

	// Set animation for base mesh
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);
	MeshInst->SetAnim(AnimSet);

	// Set animation to all tagged meshes
	for (CSkelMeshInstance* MeshInst : TaggedMeshes)
		MeshInst->SetAnim(AnimSet);

	unguard;
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
	// Export base mesh and animation
	Mesh->Anim = Anim;
	CMeshViewer::Export();
	Mesh->Anim = NULL;

	// Export all tagged meshes
	for (CSkelMeshInstance* MeshInst : TaggedMeshes)
		ExportObject(MeshInst->pMesh->OriginalMesh);
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
	NewInst->SetAnim(Anim);
	TaggedMeshes.Add(NewInst);
}


void CSkelMeshViewer::UntagAllMeshes()
{
	for (CSkelMeshInstance* MeshInst : TaggedMeshes)
	{
		MeshInst->SetAnim(NULL);
		delete MeshInst;
	}
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
	const CSkelMeshLod &Lod = Mesh->Lods[MeshInst->LodIndex];

	if (ShowUV)
	{
		DisplayUV(Lod.Verts, sizeof(CSkelMeshVertex), &Lod, MeshInst->UVIndex);
	}

#if UNREAL4
	if (bIsUE4Mesh)
	{
		if (Skeleton)
		{
			bool bClicked = DrawTextH(ETextAnchor::TopLeft, NULL, S_GREEN "Skeleton: " S_WHITE S_HYPERLINK("%s"), Skeleton->Name);
			if (bClicked)
				JumpTo(Skeleton);
		}
		else
		{
			DrawTextBottomLeft(S_RED"WARNING: no skeleton, animation will not work!");
		}
	}
#endif // UNREAL4

	if (Mesh->Morphs.Num())
	{
		DrawTextLeft(S_GREEN "Moprhs  : " S_WHITE "%d", Mesh->Morphs.Num());
	}

	// mesh
	DrawTextLeft(S_GREEN "LOD     : " S_WHITE "%d/%d\n"
				 S_GREEN "Verts   : " S_WHITE "%d\n"
				 S_GREEN "Tris    : " S_WHITE "%d\n"
				 S_GREEN "UV Set  : " S_WHITE "%d/%d\n"
				 S_GREEN "Colors  : " S_WHITE "%s\n"
				 S_GREEN "Bones   : " S_WHITE "%d\n",
				 MeshInst->LodIndex+1, Mesh->Lods.Num(),
				 Lod.NumVerts, Lod.Indices.Num() / 3,
				 MeshInst->UVIndex+1, Lod.NumTexCoords,
				 Lod.VertexColors ? "present" : "none",
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
	HighlightMeshIndex = -1;
	for (int i = 0; i < TaggedMeshes.Num(); i++)
	{
		CSkeletalMesh* TaggedMesh = TaggedMeshes[i]->pMesh;
		bool bHighlight;
		bool bClicked = DrawTextH(ETextAnchor::TopLeft, &bHighlight, "%s%d: " S_HYPERLINK("%s"), (TaggedMesh == MeshInst->pMesh) ? S_RED : S_WHITE, i, TaggedMesh->OriginalMesh->Name);
		if (bHighlight)
		{
			TimeSinceCreate = 1000.0f; // disable another highlight
			HighlightMeshIndex = i;
		}
		if (bClicked && TaggedMesh != Mesh)
		{
			TimeSinceCreate = 1000.0f; // disable another highlight
			JumpTo(TaggedMesh->OriginalMesh);
		}
	}

	// show animation information
	if (Anim)
	{
		DrawTextLeft("\n"); // Add a newline. Hyperlinks can't start with '\n', it will fail to parse - so don't insert \n into the hypelink's text
		if (DrawTextH(ETextAnchor::BottomLeft, NULL, S_GREEN "AnimSet: " S_WHITE S_HYPERLINK("%s"), Anim->OriginalAnim->Name))
		{
			JumpTo(Anim->OriginalAnim);
		}

		const char *OnOffStatus = NULL;
		switch ((EAnimRetargetingMode)MeshInst->RetargetingModeOverride)
		{
		case EAnimRetargetingMode::AnimSet:
			OnOffStatus = "default";
			break;
		case EAnimRetargetingMode::AnimRotationOnly:
			OnOffStatus = S_YELLOW "force mesh translation";
			break;
		case EAnimRetargetingMode::NoRetargeting:
			OnOffStatus = S_RED "disabled";
			break;
		}
		if (DrawTextBottomLeftH(NULL, S_GREEN S_HYPERLINK("Retargeting:" S_WHITE " %s"), OnOffStatus))
		{
			// Allow retargeting toggle with a mouse click
			if (MeshInst->RetargetingModeOverride == EAnimRetargetingMode::AnimSet)
				SetRetargetingMode(EAnimRetargetingMode::NoRetargeting);
			else
				SetRetargetingMode(EAnimRetargetingMode::AnimSet);
		}

		const CAnimSequence* Seq = MeshInst->GetAnim(0);
		if (Seq)
		{
			if (Seq->OriginalSequence)
			{
				// Draw sequence name as hyperlink
				bool bClicked = DrawTextH(ETextAnchor::BottomLeft, NULL, S_GREEN "Anim:" S_WHITE " %d/%d "
					"(" S_HYPERLINK("%s") ") "
					S_GREEN " Rate:" S_WHITE " %g " S_GREEN "Frames:" S_WHITE " %d%s",
					AnimIndex+1, MeshInst->GetAnimCount(), *Seq->Name, Seq->Rate, Seq->NumFrames,
					Seq->bAdditive ? S_RED" (additive)" : "");
				if (bClicked)
					JumpTo(Seq->OriginalSequence);
			}
			else
			{
				DrawTextBottomLeft(S_GREEN "Anim:" S_WHITE " %d/%d (%s)"
					S_GREEN " Rate:" S_WHITE " %g " S_GREEN "Frames:" S_WHITE " %d%s",
					AnimIndex+1, MeshInst->GetAnimCount(), *Seq->Name, Seq->Rate, Seq->NumFrames,
					Seq->bAdditive ? S_RED" (additive)" : "");
			}
			DrawTextBottomRight(S_GREEN "Frame:" S_WHITE " %4.1f/%d", MeshInst->GetAnimFrame(0), Seq->NumFrames);
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

	if (Mesh->Morphs.Num())
	{
		int MorphIndex = MeshInst->MorphIndex;
		int MorphCount = Mesh->Morphs.Num();
		if (MeshInst->MorphIndex >= 0)
		{
			DrawTextBottomLeft(S_GREEN "Morph: " S_WHITE " %d/%d (%s)", MorphIndex+1, MorphCount, *Mesh->Morphs[MorphIndex]->Name);
		}
		else
		{
			DrawTextBottomLeft(S_GREEN "Morph: " S_WHITE " 0/%d (none)", MorphCount);
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

	CSkelMeshInstance* HighlightInstance = NULL;
	if (HighlightMeshIndex >= 0)
	{
		HighlightInstance = TaggedMeshes[HighlightMeshIndex];
	}

	if (TimeSinceCreate < 0)
		TimeSinceCreate += 1.0f;			// ignore this frame for highlighting
	else
		TimeSinceCreate += TimeDelta;

	// Support for highlighting. Using 'glMaterial' for fixed pipeline implementation.
	// Request current light parameters.
	float savedAmbient[4];
	float savedDiffuse[4];
	glGetLightfv(GL_LIGHT0, GL_AMBIENT, savedAmbient);
	glGetLightfv(GL_LIGHT0, GL_DIFFUSE, savedDiffuse);

	float highlightTime = max(TimeSinceCreate, 0);
	static const float dark[4] = { 0.01f, 0.01f, 0.01f, 1.0f };

	if (TaggedMeshes.Num() && (highlightTime < HIGHLIGHT_DURATION) && (HighlightInstance == NULL))
	{
		if (highlightTime > HIGHLIGHT_DURATION / 2)
			highlightTime = HIGHLIGHT_DURATION - highlightTime;	// fade
		float boost = HIGHLIGHT_STRENGTH * highlightTime / (HIGHLIGHT_DURATION / 2);

		float newAmbient[4];
		memcpy(newAmbient, savedAmbient, sizeof(newAmbient));
		newAmbient[0] += boost; newAmbient[1] += boost; newAmbient[2] += boost;
		glLightfv(GL_LIGHT0, GL_AMBIENT, newAmbient);
		glMaterialfv(GL_FRONT, GL_AMBIENT, newAmbient);
	}

	if (HighlightInstance != NULL && HighlightInstance->pMesh != MeshInst->pMesh)
	{
		// Darken the mesh
		glLightfv(GL_LIGHT0, GL_DIFFUSE, dark);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, dark);
	}

	// draw main mesh
	CMeshViewer::Draw3D(TimeDelta);

	// Restore light parameters
	glLightfv(GL_LIGHT0, GL_AMBIENT, savedAmbient);
	glMaterialfv(GL_FRONT, GL_AMBIENT, savedAmbient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, savedDiffuse);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, savedDiffuse);

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

	for (CSkelMeshInstance* mesh : TaggedMeshes)
	{
		if (mesh->pMesh == MeshInst->pMesh) continue;	// avoid duplicates
		mesh->UpdateAnimation(TimeDelta);

		if (HighlightInstance != mesh && HighlightInstance != NULL)
		{
			// Darken the mesh
			glLightfv(GL_LIGHT0, GL_AMBIENT, dark);
			glMaterialfv(GL_FRONT, GL_AMBIENT, dark);
			glLightfv(GL_LIGHT0, GL_DIFFUSE, dark);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, dark);
			// Draw the mesh
			DrawMesh(mesh);
			// Restore light parameters
			glLightfv(GL_LIGHT0, GL_AMBIENT, savedAmbient);
			glMaterialfv(GL_FRONT, GL_AMBIENT, savedAmbient);
			glLightfv(GL_LIGHT0, GL_DIFFUSE, savedDiffuse);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, savedDiffuse);
		}
		else
		{
			DrawMesh(mesh);
		}
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
	DrawKeyHelp("Ctrl+[]", "prev/next morph");
	DrawKeyHelp("U",      "cycle UV sets");
	DrawKeyHelp("S",      "show skeleton");
	DrawKeyHelp("B",      "show bone names");
	DrawKeyHelp("I",      "show influences");
	DrawKeyHelp("A",      "show attach sockets");
	DrawKeyHelp("F",      "focus camera on mesh");
	DrawKeyHelp("Ctrl+B", "dump skeleton to console");
	DrawKeyHelp("Ctrl+A", bIsUE4Mesh ? "find animations (UE4)" : "cycle mesh animation sets");
	DrawKeyHelp("Ctrl+R", "toggle animation retargeting mode");
	DrawKeyHelp("Ctrl+T", "tag/untag mesh");
	DrawKeyHelp("Ctrl+U", "display UV");
}


#if HAS_UI

UIMenuItem* CSkelMeshViewer::GetObjectMenu(UIMenuItem* menu)
{
	assert(!menu);
	menu = &NewSubmenu("SkeletalMesh");

	(*menu)
	[
		NewMenuCheckbox("Show bone names\tB", &ShowLabels)
		+NewMenuCheckbox("Show sockets\tA", &ShowAttach)
		+NewMenuCheckbox("Show influences\tI", &DrawFlags, DF_SHOW_INFLUENCES)
		+NewMenuCheckbox("Show mesh UVs\tCtrl+U", &ShowUV)
		+NewMenuSeparator()
		+NewMenuItem(bIsUE4Mesh ? "Find animations ...\tCtrl+A" : "Cycle AnimSets\tCtrl+A")
		.SetCallback(BIND_MEMBER(&CSkelMeshViewer::AttachAnimSet, this))
		+NewMenuItem("Cycle skeleton display\tS")
		.SetCallback(BIND_LAMBDA([this]() { ProcessKey('s'); })) // simulate keypress
		+NewMenuItem("Tag mesh\tCtrl+T")
		.SetCallback(BIND_LAMBDA([this]() { ProcessKey(KEY_CTRL|'t'); })) // simulate keypress
		+NewMenuItem("Untag all meshes")
		.SetCallback(BIND_LAMBDA([this]() { UntagAllMeshes(); }))
	];

	return CMeshViewer::GetObjectMenu(menu);
}

#endif // HAS_UI


void CSkelMeshViewer::ProcessKey(unsigned key)
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
			for (CSkelMeshInstance* mesh : TaggedMeshes)
				mesh->TweenAnim(AnimName, 0.25);
		}
		break;

	case '['|KEY_CTRL:
	case ']'|KEY_CTRL:
		if (Mesh->Morphs.Num())
		{
			int& MorphIndex = MeshInst->MorphIndex; // pointer
			int NumMorphs = Mesh->Morphs.Num();
			if (key == ('['|KEY_CTRL))
			{
				if (--MorphIndex < -1)
					MorphIndex = NumMorphs - 1;
			}
			else
			{
				if (++MorphIndex >= NumMorphs)
					MorphIndex = -1;
			}
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
			for (CSkelMeshInstance* mesh : TaggedMeshes)
				mesh->FreezeAnimAt(Frame);
		}
		break;

	case ' ':
		if (AnimIndex >= 0)
		{
			MeshInst->PlayAnim(AnimName);
			for (CSkelMeshInstance* mesh : TaggedMeshes)
				mesh->PlayAnim(AnimName);
		}
		break;
	case 'x':
		if (AnimIndex >= 0)
		{
			MeshInst->LoopAnim(AnimName);
			for (CSkelMeshInstance* mesh : TaggedMeshes)
				mesh->LoopAnim(AnimName);
		}
		break;

	// mesh debug output
	case 'l':
		if (++MeshInst->LodIndex >= Mesh->Lods.Num())
			MeshInst->LodIndex = 0;
		break;
	case 'u':
		if (++MeshInst->UVIndex >= Mesh->Lods[MeshInst->LodIndex].NumTexCoords)
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
	case 'c'|KEY_CTRL:
		Alpha -= 0.02;
		if (Alpha < 0) Alpha = 0;
		MeshInst->SetSecondaryBlend(2, Alpha);
		break;
	case 'v'|KEY_CTRL:
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
		AttachAnimSet();
		break;

	case 't'|KEY_CTRL:
		TagMesh(MeshInst);
		break;

	case 'r'|KEY_CTRL:
		{
			EAnimRetargetingMode mode = EAnimRetargetingMode((int)MeshInst->RetargetingModeOverride + 1);
			if (mode >= EAnimRetargetingMode::Count) mode = EAnimRetargetingMode::AnimSet;
			SetRetargetingMode(mode);
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

void CSkelMeshViewer::SetRetargetingMode(EAnimRetargetingMode NewMode)
{
	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);
	MeshInst->RetargetingModeOverride = NewMode;
	for (CSkelMeshInstance* mesh : TaggedMeshes)
		mesh->RetargetingModeOverride = NewMode;
}

void CSkelMeshViewer::AttachAnimSet()
{
	guard(CSkelMeshViewer::AttachAnimSet);

	FindUE4Animations();

	CSkelMeshInstance *MeshInst = static_cast<CSkelMeshInstance*>(Inst);

	const CAnimSet *PrevAnim = Anim;
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
		const CAnimSet *NewAnim = GetAnimSet(Obj);
		if (!NewAnim) continue;

		if (NewAnim == PrevAnim)
		{
			// Check for loop while iterating (found the same object twice)
			if (found) break;
			// 'NewAnim' is the current animation set, skip it - try to find something else
			found = true;
			continue;
		}

		if (found && NewAnim)
		{
			// Passed over current skeleton object
			if (NewAnim->Sequences.Num())
			{
				// found desired animation set
				SetAnim(NewAnim);
				AnimIndex = -1;
				appPrintf("Bound %s'%s' to %s'%s'\n", Object->GetClassName(), Object->Name, Obj->GetClassName(), Obj->Name);
				break;
			}
			else
			{
#if MAX_DEBUG
				appPrintf("Skeleton %s has no animations, continuing search\n", Obj->Name);
#endif
			}
		}
	}

	unguard;
}

#if !HAS_UI

class ConsoleProgress : public IProgressCallback
{
public:
	ConsoleProgress()
	: desc("")
	{}
	void Show(const char* title)
	{
		lastTick = 0;
		printf("%s:\n", title);
	}
	void SetDescription(const char* text)
	{
		desc = text;
	}
	~ConsoleProgress()
	{
		printf("\r%s: done %20s\n", desc, "");
	}
	virtual bool Progress(const char* package, int index, int total)
	{
		// do not update UI too often
		int tick = appMilliseconds();
		if (tick - lastTick < 100)
			return true;
		lastTick = tick;

		printf("\r%s: %d/%d %20s\r", desc, index+1, total, "");
		return true;
	}

protected:
	const char* desc;
	int lastTick;
};

#define UIProgressDialog ConsoleProgress

#endif // HAS_UI


// Find animations for currently selected skeleton
void CSkelMeshViewer::FindUE4Animations()
{
#if UNREAL4
	guard(CSkelMeshViewer::FindUE4Animations);

	if (!bIsUE4Mesh)
		return;

	if (!Skeleton)
	{
		appPrintf("No Skeleton object attached to the mesh, doing nothing\n");
		return;
	}

	// Find all packages
	TArray<const CGameFileInfo*> PackageInfos;
	appEnumGameFiles<TArray<const CGameFileInfo*> >( // won't compile with lambda without explicitly providing template argument
		[](const CGameFileInfo* file, TArray<const CGameFileInfo*>& param) -> bool
		{
			if (file->IsPackage())
				param.Add(file);
			return true;
		}, PackageInfos);

	UIProgressDialog progress;
	progress.Show("Finding animations");
	progress.SetDescription("Scanning package");

	// Perform full scan to be able to locate AnimSequence objects
	if (!ScanContent(PackageInfos, &progress))
	{
		appPrintf("Interrupted by user\n");
		return;
	}

	// Find potential uasset files with animations
	TArray<UnPackage*> packagesToLoad;
	packagesToLoad.Empty(256);

	const char* lookupSkeletonName = Skeleton->Name;
	for (int i = 0; i < PackageInfos.Num(); i++)
	{
		const CGameFileInfo* info = PackageInfos[i];
		UnPackage* package = info->Package;
		if (!package)
		{
			// Shouldn't happen, but happens (ScanPackage() should fill Package for all CGameFileInfo).
			// Example of appearance: ScanPackages may open a WRONG find in a case when game files are extracted from pak, and
			// user supplies wrong (shorter) game path, e.g. -path=Extported/Meshes, and multiple files with the same name
			// exists inside that folder. CGameFileInfo::Find has heuristic for finding files using partial path, but it may fail.
			// In this case, CGameFileInfo will say "it's package", but actually file won't be scanned and/or loaded.
			//appNotify("Strange package: IsPackage=true, Package is NULL: %s (size %d Kb)", *info->GetRelativeName(), info->SizeInKb + info->ExtraSizeInKb);
			continue;
		}

		bool found = false;
		for (int importIndex = 0; importIndex < package->Summary.ImportCount; importIndex++)
		{
			FObjectImport& imp = package->GetImport(importIndex);
			const char* ObjectClass = *imp.ClassName;
			const char* ObjectName = *imp.ObjectName;
			if (!stricmp(ObjectClass, "Skeleton") && !stricmp(ObjectName, lookupSkeletonName))
			{
				// This uasset refers to the Skeleton object with the same name, check if this
				// is an exactly the same Skeleton object as we're using
				const char* referencedFilename = package->GetObjectPackageName(imp.PackageIndex);
				const CGameFileInfo* referencedFile = CGameFileInfo::Find(referencedFilename);
				if (Skeleton->Package->FileInfo == referencedFile)
				{
					found = true;
					break;
				}
			}
		}

		if (!found) continue; // this package doesn't use our Skeleton

		// Now, if this package has animation sequence - enqueue it for loading
		if (PackageInfos[i]->NumAnimations)
		{
			packagesToLoad.Add(package);
		}
	}

	// Sort packages by name for easier navigation after loading
	packagesToLoad.Sort([](UnPackage* const& a, UnPackage* const& b) -> int
		{
			return stricmp(a->Name, b->Name);
		});

	// Load queued packages
	progress.SetDescription("Loading animations");
	for (int i = 0; i < packagesToLoad.Num(); i++)
	{
		guard(Load);
		UnPackage* package = packagesToLoad[i];
		if (!progress.Progress(*package->GetFilename(), i, packagesToLoad.Num()))
			break;
		LoadWholePackage(package);
		unguardf("%d/%d", i, packagesToLoad.Num());
	}

	unguard;
#endif // UNREAL4
}


void CSkelMeshViewer::ProcessKeyUp(unsigned key)
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
