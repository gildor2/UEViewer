#include "ObjectViewer.h"


void CSkelMeshViewer::Dump()
{
	CMeshViewer::Dump();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	printf(
		"\nSkelMesh info:\n==============\n"
		"f1FC #         %d\n"
		"Bones  # %4d  Points    # %4d\n"
		"Wedges # %4d  Triangles # %4d\n"
		"CollapseWedge # %4d  f1C8      # %4d\n"
		"BoneDepth      %d\n"
		"WeightIds #    %d\n"
		"BoneInfs # %d 	VertInfs # %d\n"
		"Attachments #  %d\n"
		"StaticLODModels # %d\n",
		Mesh->f1FC.Num(),
		Mesh->Bones.Num(),
		Mesh->Points.Num(),
		Mesh->Wedges.Num(),Mesh->Triangles.Num(),
		Mesh->CollapseWedge.Num(), Mesh->f1C8.Num(),
		Mesh->BoneDepth,
		Mesh->WeightIndices.Num(),
		Mesh->BoneInfluences.Num(),
		Mesh->VertInfluences.Num(),
		Mesh->AttachBoneNames.Num(),
		Mesh->StaticLODModels.Num()
	);
//	TEST_ARRAY(Mesh->CollapseWedge);
//	TEST_ARRAY(Mesh->f1C8);
	VERIFY(AttachBoneNames.Num(), AttachAliases.Num());
	VERIFY(AttachBoneNames.Num(), AttachCoords.Num());

	int i;

	// check bone sort order (assumed, that child go after parent)
	for (i = 0; i < Mesh->Bones.Num(); i++)
	{
		const FMeshBone &B = Mesh->Bones[i];
		if (B.ParentIndex >= i + 1) appNotify("bone[%d] has parent %d", i+1, B.ParentIndex);
	}

	for (i = 0; i < Mesh->StaticLODModels.Num(); i++)
	{
		printf("model # %d\n", i);
		const FStaticLODModel &lod = Mesh->StaticLODModels[i];
		printf(
			"  f0=%d  SkinPoints=%d inf=%d  wedg=%d dynWedges=%d faces=%d  points=%d\n"
			"  DistanceFactor=%g  Hysteresis=%g  10C=%d  MaxInfluences=%d  114=%d  118=%d\n"
			"  smoothInds=%d  rigidInds=%d  vertStream.Size=%d\n",
			lod.f0.Num(),
			lod.SkinPoints.Num(),
			lod.VertInfluences.Num(),
			lod.Wedges.Num(), lod.NumDynWedges,
			lod.Faces.Num(),
			lod.Points.Num(),
			lod.LODDistanceFactor, lod.LODHysteresis, lod.f10C, lod.LODMaxInfluences, lod.f114, lod.f118,
			lod.SmoothIndices.Indices.Num(), lod.RigidIndices.Indices.Num(), lod.VertexStream.Verts.Num());
#if TEST_FILES
//?? (not always)	if (lod.NumDynWedges != lod.Wedges.Num()) appNotify("lod[%d]: NumDynWedges!=wedges.Num()", i);
		if (lod.SkinPoints.Num() != lod.Points.Num() && lod.RigidSections.Num() == 0)
			appNotify("[%d] skinPoints: %d", i,	lod.SkinPoints.Num());
//		if ((lod.f0.Num() != 0 || lod.NumDynWedges != 0) &&
//			(lod.f0.Num() != lod.NumDynWedges * 3 + 1)) appNotify("f0=%d  NumDynWedges=%d",lod.f0.Num(), lod.NumDynWedges);
		if ((lod.f0.Num() == 0) != (lod.NumDynWedges == 0)) appNotify("f0=%d  NumDynWedges=%d",lod.f0.Num(), lod.NumDynWedges);
// (may be empty)	if (lod.VertexStream.Verts.Num() != lod.Wedges.Num()) appNotify("lod%d: bad VertexStream size", i);
#endif
		const TArray<FSkelMeshSection> *sec[2];
		sec[0] = &lod.SmoothSections;
		sec[1] = &lod.RigidSections;
		static const char *secNames[] = { "smooth", "rigid" };
		for (int k = 0; k < 2; k++)
		{
			printf("  %s sections: %d\n", secNames[k], sec[k]->Num());
			for (int j = 0; j < sec[k]->Num(); j++)
			{
				const FSkelMeshSection &S = (*sec[k])[j];
				printf("    %d:  mat=%d %d [w=%d .. %d] %d b=%d %d [f=%d + %d]\n", j,
					S.MaterialIndex, S.MinStreamIndex, S.MinWedgeIndex, S.MaxWedgeIndex,
					S.NumStreamIndices, S.BoneIndex, S.fE, S.FirstFace, S.NumFaces);
#if TEST_FILES
				if (k == 1) // rigid sections
				{
					if (S.BoneIndex >= Mesh->Bones.Num())
						appNotify("rigid sec[%d,%d]: bad bone link (%d >= %d)", i, j, S.BoneIndex, Mesh->Bones.Num());
					if (S.MinStreamIndex + S.NumStreamIndices > lod.RigidIndices.Indices.Num())
						appNotify("rigid sec[%d,%d]: out of index buffer", i, j);
					if (S.NumFaces * 3 != S.NumStreamIndices)
						appNotify("rigid sec[%d,%d]: f8!=NumFaces*3", i, j);
					if (S.fE != 0)
						appNotify("rigid sec[%d,%d]: fE=%d", i, j, S.fE);
				}
#endif
			}
		}
	}
}


void DrawBaseSkeletalMesh(const USkeletalMesh *Mesh)
{
	int i;

	glBegin(GL_POINTS);
	for (i = 0; i < Mesh->VertexCount; i++)
	{
		const FVector &V = Mesh->Points[i];
		CVec3 v;
		v[0] = V.X * Mesh->MeshScale.X /*+ Mesh->MeshOrigin.X*/;
		v[1] = V.Y * Mesh->MeshScale.Y /*+ Mesh->MeshOrigin.Y*/;
		v[2] = V.Z * Mesh->MeshScale.Z /*+ Mesh->MeshOrigin.Z*/;
		glVertex3fv(&V.X);
	}
	glEnd();
}


void DrawLodSkeletalMesh(const USkeletalMesh *Mesh, const FStaticLODModel *lod)
{
	int i;
	glBegin(GL_POINTS);
	for (i = 0; i < lod->SkinPoints.Num(); i++)
	{
		const FVector &V = lod->SkinPoints[i].Point;
		glVertex3fv(&V.X);
	}
	glEnd();
	//!! wrong
	CCoords	BonePos[256];
	glBegin(GL_LINES);
	for (i = 0; i < Mesh->Bones.Num(); i++)
	{
		const FMeshBone &B = Mesh->Bones[i];
		const CVec3 &BP = (CVec3&)B.BonePos.Position;
//		const CQuat &BO = (CQuat&)B.BonePos.Orientation;
		CQuat BO; BO = (CQuat&)B.BonePos.Orientation;
		if (!i) BO.Conjugate();

		// convert VJointPos to CCoords
		CCoords &BC = BonePos[i];
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
			BonePos[B.ParentIndex].UnTransformCoords(BC, BC);

		CVec3 v2;
		v2.Set(10/*B.BonePos.Length*/, 0, 0);
		BC.UnTransformPoint(v2, v2);

		glColor3f(1,0,0);
		glVertex3fv(BC.origin.v);
		glVertex3fv(v2.v);

		if (i > 0)
		{
			glColor3f(1,1,1);
			glVertex3fv(BonePos[B.ParentIndex].origin.v);
		}
		else
		{
			glColor3f(1,0,1);
			glVertex3f(0, 0, 0);
		}
		glVertex3fv(BC.origin.v);
	}
	glColor3f(1,1,1);
	glEnd();
}


void CSkelMeshViewer::Draw3D()
{
	CMeshViewer::Draw3D();

	USkeletalMesh *Mesh = static_cast<USkeletalMesh*>(Object);
	if (LodNum < 0)
		DrawBaseSkeletalMesh(Mesh);
	else
		DrawLodSkeletalMesh(Mesh, &Mesh->StaticLODModels[LodNum]);
}
