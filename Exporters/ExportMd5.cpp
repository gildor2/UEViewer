#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"
#include "UnMesh.h"

#include "Exporters.h"						// for GetExportPath()
#include "TypeConvert.h"

#include "SkeletalMesh.h"

// MD5 uses right-hand coordinates, but unreal uses left-hand.
// When importing PSK into UnrealEd, it mirrors model.
// Here we performing reverse transformation.
#define MIRROR_MESH			1


// function is similar to part of CSkelMeshInstance::SetMesh() and Rune mesh loader
static void BuildSkeleton(TArray<CCoords> &Coords, const TArray<FMeshBone> &Bones)
{
	guard(BuildSkeleton);

	int numBones = Bones.Num();
	Coords.Empty(numBones);
	Coords.Add(numBones);

	for (int i = 0; i < numBones; i++)
	{
		const FMeshBone &B = Bones[i];
		CCoords &BC = Coords[i];
		// compute reference bone coords
		CVec3 BP;
		CQuat BO;
		// get default pose
		BP = CVT(B.BonePos.Position);
		BO = CVT(B.BonePos.Orientation);
#if MIRROR_MESH
		BP[1] *= -1;							// y
		BO.y  *= -1;
		BO.w  *= -1;
#endif
		if (!i) BO.Conjugate();					// root bone

		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
		{
			assert(B.ParentIndex < i);
			Coords[B.ParentIndex].UnTransformCoords(BC, BC);
		}
#if 0
	//!!
if (i == 32)
{
	appNotify("Bone %d (%8.3f %8.3f %8.3f) - (%8.3f %8.3f %8.3f %8.3f)", i, VECTOR_ARG(BP), QUAT_ARG(BO));
#define C BC
	appNotify("REF   : o=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.origin ));
	appNotify("        0=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[0]));
	appNotify("        1=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[1]));
	appNotify("        2=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[2]));
#undef C
}
#endif
	}

	unguard;
}

struct VertInfluences
{
	TArray<int> InfIndex;
};

void ExportMd5Mesh(const USkeletalMesh *Mesh, FArchive &Ar)
{
	guard(ExportMd5Mesh);

	int i;

	int NumMaterials = Mesh->Materials.Num();	// count USED materials
	for (i = 0; i < Mesh->Triangles.Num(); i++)
	{
		const VTriangle &T = Mesh->Triangles[i];
		int Mat = T.MatIndex;
		if (Mat >= NumMaterials) NumMaterials = Mat + 1;
	}

	Ar.Printf(
		"MD5Version 10\n"
		"commandline \"Created with UE Viewer\"\n"
		"\n"
		"numJoints %d\n"
		"numMeshes %d\n"
		"\n",
		Mesh->RefSkeleton.Num(),
		NumMaterials
	);

	// compute skeleton
	TArray<CCoords> BoneCoords;
	BuildSkeleton(BoneCoords, Mesh->RefSkeleton);

	// write joints
	Ar.Printf("joints {\n");
	UniqueNameList UsedBones;
	for (i = 0; i < Mesh->RefSkeleton.Num(); i++)
	{
		const FMeshBone &B = Mesh->RefSkeleton[i];

		const CCoords &BC = BoneCoords[i];
		CVec3 BP;
		CQuat BO;
		BP = BC.origin;
		BO.FromAxis(BC.axis);
		if (BO.w < 0) BO.Negate();				// W-component of quaternion will be removed ...

		int BoneSuffix = UsedBones.RegisterName(*B.Name);
		char BoneName[256];
		if (BoneSuffix < 2)
		{
			appStrncpyz(BoneName, *B.Name, ARRAY_COUNT(BoneName));
		}
		else
		{
			appSprintf(ARRAY_ARG(BoneName), "%s_%d", *B.Name, BoneSuffix);
			appPrintf("duplicate bone %s, renamed to %s\n", *B.Name, BoneName);
		}

		Ar.Printf(
			"\t\"%s\"\t%d ( %f %f %f ) ( %.10f %.10f %.10f )\n",
			BoneName, (i == 0) ? -1 : B.ParentIndex,
			VECTOR_ARG(BP),
			BO.x, BO.y, BO.z
		);
#if 0
	//!!
if (i == 32 || i == 34)
{
	CCoords BC;
	BC.origin = BP;
	BO.ToAxis(BC.axis);
	appNotify("Bone %d (%8.3f %8.3f %8.3f) - (%8.3f %8.3f %8.3f %8.3f)", i, VECTOR_ARG(BP), QUAT_ARG(BO));
#define C BC
	appNotify("INV   : o=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.origin ));
	appNotify("        0=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[0]));
	appNotify("        1=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[1]));
	appNotify("        2=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[2]));
#undef C
//	BO.Negate();
	BO.w *= -1;
	BO.ToAxis(BC.axis);
#define C BC
	appNotify("INV2  : o=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.origin ));
	appNotify("        0=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[0]));
	appNotify("        1=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[1]));
	appNotify("        2=%8.3f %8.3f %8.3f",    VECTOR_ARG(C.axis[2]));
#undef C
}
#endif
	}
	Ar.Printf("}\n\n");

	// collect weights information
	TArray<VertInfluences> Weights;				// Point -> Influences
	Weights.Add(Mesh->Points.Num());
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		const FVertInfluences &I = Mesh->VertInfluences[i];
		Weights[I.PointIndex].InfIndex.AddItem(i);
	}

	// write meshes
	for (int m = 0; m < NumMaterials; m++)
	{
		TArray<int>  MeshTris;					// surface triangle -> mesh face
		TArray<int>  MeshVerts;					// surface vertex -> mesh wedge
		TArray<bool> UsedVerts;					// mesh wedge -> surface: used of not
		TArray<int>  BackWedge;					// mesh wedge -> surface wedge
		TArray<bool> UsedPoints;				// mesh point -> surface: used or not
		TArray<int>  MeshWeights;				// mesh point -> weight index
		MeshTris.Empty(Mesh->Triangles.Num());
		MeshVerts.Empty(Mesh->Wedges.Num());
		UsedVerts.Add(Mesh->Wedges.Num());
		BackWedge.Add(Mesh->Wedges.Num());
		UsedPoints.Add(Mesh->Points.Num());
		MeshWeights.Add(Mesh->Wedges.Num());

		// find verts and triangles for current material
		for (i = 0; i < Mesh->Triangles.Num(); i++)
		{
			const VTriangle &T = Mesh->Triangles[i];
			if (T.MatIndex != m) continue;
			MeshTris.AddItem(i);
			for (int j = 0; j < 3; j++)
			{
				// register wedge
				int wedge = T.WedgeIndex[j];
				if (UsedVerts[wedge]) continue;
				UsedVerts[wedge] = true;
				int locWedge = MeshVerts.AddItem(wedge);
				BackWedge[wedge] = locWedge;
				// register point
				int point = Mesh->Wedges[wedge].iVertex;
				if (UsedPoints[point]) continue;
				UsedPoints[point] = true;
			}
		}
		// find influences
		int WeightIndex = 0;
		for (i = 0; i < Mesh->Points.Num(); i++)
		{
			if (!UsedPoints[i]) continue;
			MeshWeights[i] = WeightIndex;
			WeightIndex += Weights[i].InfIndex.Num();
		}

		// mesh header
		const UUnrealMaterial *Tex = NULL;
		if (m < Mesh->Materials.Num())
		{
			int texIdx = Mesh->Materials[m].TextureIndex;
			if (texIdx < Mesh->Textures.Num())
				Tex = MATERIAL_CAST(Mesh->Textures[texIdx]);
		}

		if (Tex && Tex->IsA("UnrealMaterial")) ExportMaterial(Tex);

		if (Tex)
		{
			Ar.Printf(
				"mesh {\n"
				"\tshader \"%s\"\n\n",
				Tex->Name
			);
		}
		else
		{
			Ar.Printf(
				"mesh {\n"
				"\tshader \"material_%d\"\n\n",
				m
			);
		}
		// verts
		Ar.Printf("\tnumverts %d\n", MeshVerts.Num());
		for (i = 0; i < MeshVerts.Num(); i++)
		{
			const FMeshWedge &W = Mesh->Wedges[MeshVerts[i]];
			int iPoint = W.iVertex;
			Ar.Printf("\tvert %d ( %f %f ) %d %d\n",
				i, W.TexUV.U, W.TexUV.V, MeshWeights[iPoint], Weights[iPoint].InfIndex.Num());
		}
		// triangles
		Ar.Printf("\n\tnumtris %d\n", MeshTris.Num());
		for (i = 0; i < MeshTris.Num(); i++)
		{
			const VTriangle &T = Mesh->Triangles[MeshTris[i]];
			Ar.Printf("\ttri %d", i);
#if MIRROR_MESH
			for (int j = 2; j >= 0; j--)
#else
			for (int j = 0; j < 3; j++)
#endif
				Ar.Printf(" %d", BackWedge[T.WedgeIndex[j]]);
			Ar.Printf("\n");
		}
		// weights
		Ar.Printf("\n\tnumweights %d\n", WeightIndex);
		int saveWeightIndex = WeightIndex;
		WeightIndex = 0;
		for (i = 0; i < Mesh->Points.Num(); i++)
		{
			if (!UsedPoints[i]) continue;
			for (int j = 0; j < Weights[i].InfIndex.Num(); j++)
			{
				const FVertInfluences &I = Mesh->VertInfluences[Weights[i].InfIndex[j]];
				CVec3 v;
				v = CVT(Mesh->Points[I.PointIndex]);
#if MIRROR_MESH
				v[1] *= -1;						// y
#endif
				BoneCoords[I.BoneIndex].TransformPoint(v, v);
				Ar.Printf(
					"\tweight %d %d %f ( %f %f %f )\n",
					WeightIndex, I.BoneIndex, I.Weight,
					VECTOR_ARG(v)
				);
				WeightIndex++;
			}
		}
		assert(saveWeightIndex == WeightIndex);

		// mesh footer
		Ar.Printf("}\n");
	}

	unguard;
}


void ExportMd5Anim(const CAnimSet *Anim, FArchive &Ar)
{
	guard(ExportMd5Anim);

	int numBones = Anim->TrackBoneNames.Num();
	UObject *OriginalAnim = Anim->OriginalAnim;

	char basename[512];
	appSprintf(ARRAY_ARG(basename), "%s/%s", GetExportPath(OriginalAnim), OriginalAnim->Name);
	appMakeDirectory(basename);

	for (int AnimIndex = 0; AnimIndex < Anim->Sequences.Num(); AnimIndex++)
	{
		int i;
		const CAnimSequence &S = Anim->Sequences[AnimIndex];

		char FileName[512];
		appSprintf(ARRAY_ARG(FileName), "%s/%s.md5anim", basename, *S.Name);

		FFileReader Ar(FileName, false);

		Ar.Printf(
			"MD5Version 10\n"
			"commandline \"Created with UE Viewer\"\n"
			"\n"
			"numFrames %d\n"
			"numJoints %d\n"
			"frameRate %g\n"
			"numAnimatedComponents %d\n"
			"\n",
			S.NumFrames,
			numBones,
			S.Rate,
			numBones * 6
		);

		// skeleton
		Ar.Printf("hierarchy {\n");
		for (i = 0; i < numBones; i++)
		{
			Ar.Printf("\t\"%s\" %d %d %d\n", *Anim->TrackBoneNames[i], (i == 0) ? -1 : 0, 63, i * 6);
				// ParentIndex is unknown for UAnimSet, so always write "0"
				// here: 6 is number of components per frame, 63 = (1<<6)-1 -- flags "all components are used"
		}

		// bounds
		Ar.Printf("}\n\nbounds {\n");
		for (i = 0; i < S.NumFrames; i++)
			Ar.Printf("\t( -100 -100 -100 ) ( 100 100 100 )\n");	//!! dummy
		Ar.Printf("}\n\n");

		// baseframe and frames
		for (int Frame = -1; Frame < S.NumFrames; Frame++)
		{
			int t = Frame;
			if (Frame == -1)
			{
				Ar.Printf("baseframe {\n");
				t = 0;
			}
			else
				Ar.Printf("frame %d {\n", Frame);

			for (int b = 0; b < numBones; b++)
			{
				CVec3 BP;
				CQuat BO;
				S.Tracks[b].GetBonePosition(t, S.NumFrames, false, BP, BO);
				if (!b) BO.Conjugate();			// root bone
#if MIRROR_MESH
				BO.y  *= -1;
				BO.w  *= -1;
				BP[1] *= -1;					// y
#endif
				if (BO.w < 0) BO.Negate();		// W-component of quaternion will be removed ...
				if (Frame < 0)
					Ar.Printf("\t( %f %f %f ) ( %.10f %.10f %.10f )\n", VECTOR_ARG(BP), BO.x, BO.y, BO.z);
				else
					Ar.Printf("\t%f %f %f %.10f %.10f %.10f\n", VECTOR_ARG(BP), BO.x, BO.y, BO.z);
			}
			Ar.Printf("}\n\n");
		}
	}

	unguard;
}
