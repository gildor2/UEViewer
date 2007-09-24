#include "Core.h"

#include "UnCore.h"
#include "UnMesh.h"

#include "GlWindow.h"


#define TEST_FILES		1
#define APP_CAPTION		"UT2004 Mesh Viewer"


enum
{
	MESH_INVALID,
	MESH_VERTEX,
	MESH_SKEL
};

static int         MeshType   = MESH_INVALID;
static ULodMesh   *Mesh       = NULL;
static const char* MeshName   = "";

#if TEST_FILES

#define VERIFY(MeshField, MField) \
	if (Mesh->MeshField != Mesh->MField) appNotify(#MeshField" "#MField);
//#define VERIFY2(MeshField, MField) \
//	if (M->MeshField != Mesh->MField) appNotify(#MeshField" "#MField);
#define VERIFY_NULL(Field) \
	if (Mesh->Field != 0) appNotify("%s != 0", #Field);
#define VERIFY_NOT_NULL(Field) \
	if (Mesh->Field == 0) appNotify("%s == 0", #Field);


#define TEST_ARRAY(array)							\
	{												\
		int min = 0x40000000, max = -0x40000000;	\
		for (int i = 0; i < array.Num(); i++)		\
		{											\
			if (array[i] < min) min = array[i];		\
			if (array[i] > max) max = array[i];		\
		}											\
		if (array.Num())							\
			printf(#array": [%d .. %d]\n", min, max); \
	}

#define TEST_ARRAY2(array,field)					\
	{												\
		int min = 0x40000000, max = -0x40000000;	\
		for (int i = 0; i < array.Num(); i++)		\
		{											\
			if (array[i].field < min) min = array[i].field; \
			if (array[i].field > max) max = array[i].field; \
		}											\
		if (array.Num())							\
			printf(#array"."#field": [%d .. %d]\n", min, max); \
	}

#endif


static void InfoLodMesh(const ULodMesh *Mesh)
{
	// show some mesh info
	printf(
		"\nLodMesh info:\n=============\n"
		"version        %d\n"
		"VertexCount    %d\n"
		"Verts #        %d\n"
		"Textures #     %d\n"
		"MeshScale      %g %g %g\n"
		"MeshOrigin     %g %g %g\n"
		"RotOrigin      %d %d %d\n"
		"FaceLevel #    %d\n"
		"Faces #        %d\n"
		"CollapseWedgeThus # %d\n"
		"Wedges #       %d\n"
		"Materials #    %d\n"
		"HasImpostor    %d\n"
		"fF8            %X ??\n"
		"fFC            %g %g %g\n"
		"f108           %d %d %d\n"
		"f114           %g %g %g\n"
		"f120..123      %d %d %d %d\n"
		"f124           %d %d %d\n"
		"f130           %g\n",
		Mesh->Version,
		Mesh->VertexCount,
		Mesh->Verts.Num(),
		Mesh->Textures.Num(),
		FVECTOR_ARG(Mesh->MeshScale),
		FVECTOR_ARG(Mesh->MeshOrigin),
		ROTATOR_ARG(Mesh->RotOrigin),
		Mesh->FaceLevel.Num(),
		Mesh->Faces.Num(),
		Mesh->CollapseWedgeThus.Num(),
		Mesh->Wedges.Num(),
		Mesh->Materials.Num(),
		Mesh->HasImpostor,
		Mesh->fF8,
		FVECTOR_ARG(Mesh->fFC),
		ROTATOR_ARG(Mesh->f108),
		FVECTOR_ARG(Mesh->f114),
		Mesh->f120, Mesh->f121, Mesh->f122, Mesh->f123,
		ROTATOR_ARG(Mesh->f124),
		Mesh->f130
	);
}


static void InfoVertMesh(const UVertMesh *Mesh)
{
	printf(
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
#if TEST_FILES
	// verify some assumptions
// (macro broken)	VERIFY2(VertexCount, VertexCount);
	VERIFY_NOT_NULL(VertexCount);
	VERIFY_NOT_NULL(Textures.Num());
	VERIFY(FaceLevel.Num(), Faces.Num());
	VERIFY_NOT_NULL(Faces.Num());
	VERIFY_NOT_NULL(Wedges.Num());
	VERIFY(CollapseWedgeThus.Num(), Wedges.Num());
//	VERIFY(Materials.Num(), Textures.Num()); -- different
	VERIFY_NULL(HasImpostor);
	VERIFY_NULL(fF8);
	VERIFY_NULL(f130);
	VERIFY_NULL(Verts2.Num());
	VERIFY(Verts.Num(), Normals.Num());
	VERIFY_NOT_NULL(Verts.Num());
	assert(Mesh->Verts.Num() == Mesh->VertexCount * Mesh->FrameCount);
	VERIFY_NULL(f150.Num());
	VERIFY(BoundingBoxes.Num(), BoundingSpheres.Num());
	VERIFY(BoundingBoxes.Num(), FrameCount);
	VERIFY_NULL(AnimMeshVerts.Num());
	VERIFY_NULL(StreamVersion);
#endif
}


static void InfoSkelMesh(const USkeletalMesh *Mesh)
{
	int i;

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
		"StaticLODModels # %d\n"
		"12345 = %d\n",
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
		Mesh->StaticLODModels.Num(),
		12345
	);
//	TEST_ARRAY(Mesh->CollapseWedge);
//	TEST_ARRAY(Mesh->f1C8);
	VERIFY(AttachBoneNames.Num(), AttachAliases.Num());
	VERIFY(AttachBoneNames.Num(), AttachCoords.Num());

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


void main(int argc, char **argv)
{
	if (argc < 3)
	{
	help:
		printf("Usage: UnLoader [-dump] <raw file> <class name>\n");
		exit(0);
	}

	bool dumpOnly = false;
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			if (!strcmp(argv[arg]+1, "dump"))
				dumpOnly = true;
			else
				goto help;
		}
		else
		{
			break;
		}
	}

	// open file
	GNotifyInfo = argv[arg];
	MeshName    = argv[arg];
	FILE *f;
	if (!(f = fopen(MeshName, "rb")))
		appError("Unable to open file %s\n", MeshName);

	// check mesh type
	if (!stricmp(argv[arg+1], "VertMesh"))
		MeshType = MESH_VERTEX;
	else if (!stricmp(argv[arg+1], "SkeletalMesh"))
		MeshType = MESH_SKEL;
	else
		appError("Unknown class: %s\n", argv[arg+1]);

	// load mesh
	if (MeshType == MESH_VERTEX)
		Mesh = new UVertMesh;
	else if (MeshType == MESH_SKEL)
		Mesh = new USkeletalMesh;
	else
		assert(0);

	printf("****** Loading %s \"%s\" ******\n", argv[arg+1], MeshName);

	// create archive wrapper
	FArchive Ar(f);
	// serialize mesh
	Mesh->Serialize(Ar);

	// check for unread bytes
//!!	if (!Ar.IsEof()) appError("extra bytes!");

	InfoLodMesh(Mesh);

	if (MeshType == MESH_VERTEX)
		InfoVertMesh(static_cast<UVertMesh*>(Mesh));
	else if (MeshType == MESH_SKEL)
		InfoSkelMesh(static_cast<USkeletalMesh*>(Mesh));
	else
		assert(0);

	if (!dumpOnly)
		VisualizerLoop(APP_CAPTION);
}


/*-----------------------------------------------------------------------------
	Draw meshes
-----------------------------------------------------------------------------*/

static void ShowBaseAxis()
{
	glBegin(GL_LINES);
	for (int i = 0; i < 3; i++)
	{
		CVec3 tmp = nullVec3;
		tmp[i] = 1;
		glColor3fv(tmp.v);
		tmp[i] = 100;
		glVertex3fv(tmp.v);
		glVertex3fv(nullVec3.v);
	}
	glEnd();
	glColor3f(1, 1, 1);
}


int frameNum = 0;
bool showNormals = false;

void DrawVertexMesh(const UVertMesh *Mesh)
{
	int i;

	int base = Mesh->VertexCount * frameNum;
	//?? choose cull mode
	glDisable(GL_CULL_FACE);
//	glCullFace(GL_BACK);

	CVec3 Vert[3];
	CVec3 Norm[3];

	for (i = 0; i < Mesh->Faces.Num(); i++)
	{
		int j;
		const FMeshFace &F = Mesh->Faces[i];
		for (j = 0; j < 3; j++)
		{
			const FMeshWedge &W = Mesh->Wedges[F.iWedge[j]];
			// vertex
			const FMeshVert &V = Mesh->Verts[base + W.iVertex];
			CVec3 &v = Vert[j];
			v[0] = V.X * Mesh->MeshScale.X /*+ Mesh->MeshOrigin.X*/;
			v[1] = V.Y * Mesh->MeshScale.Y /*+ Mesh->MeshOrigin.Y*/;
			v[2] = V.Z * Mesh->MeshScale.Z /*+ Mesh->MeshOrigin.Z*/;
			// normal
			const FMeshNorm &N = ((UVertMesh*)Mesh)->Normals[base + W.iVertex];
			CVec3 &n = Norm[j];
			n[0] = (N.X - 512.0f) / 512;
			n[1] = (N.Y - 512.0f) / 512;
			n[2] = (N.Z - 512.0f) / 512;
		}
		// draw mesh
		glEnable(GL_LIGHTING);
		glBegin(GL_TRIANGLES);
		for (j = 0; j < 3; j++)
		{
			glNormal3fv(Norm[j].v);
			glVertex3fv(Vert[j].v);
		}
		glEnd();
		glDisable(GL_LIGHTING);
		if (showNormals)
		{
			// draw normals
			glBegin(GL_LINES);
			glColor3f(1, 0.5, 0);
			for (j = 0; j < 3; j++)
			{
				glVertex3fv(Vert[j].v);
				CVec3 tmp;
				tmp = Vert[j];
				VectorMA(tmp, 3, Norm[j]);
				glVertex3fv(tmp.v);
			}
			glEnd();
			glColor3f(1, 1, 1);
		}
	}
	glEnd();
}


int skelLodNum = -1;

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
static bool show = true;

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
//if (show) printf("bone %d : p(%d)n(%d)", i, B.ParentIndex, B.Name);
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
//if (show) printf("  P=%g %g %g  Q=%g %g %g %g [%g %g %g]\n", VECTOR_ARG(BP), BO.x, BO.y, BO.z, BO.w, BC.axis[0].GetLength(),BC.axis[1].GetLength(),BC.axis[2].GetLength());
//if (show) printf(" Q=%g %g %g %g {%g,%g,%g} {%g,%g,%g} {%g,%g,%g} [%g %g %g]\n",BO.x, BO.y, BO.z, BO.w,
//VECTOR_ARG(BC.axis[0]),VECTOR_ARG(BC.axis[1]),VECTOR_ARG(BC.axis[2]),
//BC.axis[0].GetLength(),BC.axis[1].GetLength(),BC.axis[2].GetLength());
//if (show) printf("  (len=%g) %g %g %g -> %g %g %g\n", B.BonePos.Length, VECTOR_ARG(BC.origin), VECTOR_ARG(v2));
	}
	glColor3f(1,1,1);
	glEnd();
show = false;
}


void DrawSkeletalMesh(const USkeletalMesh *Mesh)
{
	if (skelLodNum < 0)
		DrawBaseSkeletalMesh(Mesh);
	else
		DrawLodSkeletalMesh(Mesh, &Mesh->StaticLODModels[skelLodNum]);
}


/*-----------------------------------------------------------------------------
	GlWindow callbacks
-----------------------------------------------------------------------------*/

void AppDrawFrame()
{
	ShowBaseAxis();
	if (MeshType == MESH_VERTEX)
		DrawVertexMesh(static_cast<UVertMesh*>(Mesh));
	else
		DrawSkeletalMesh(static_cast<USkeletalMesh*>(Mesh));
}


void AppKeyEvent(unsigned char key)
{
	if (key == 'n')
		showNormals = !showNormals;

	if (MeshType == MESH_VERTEX)
	{
		int FrameCount = ((UVertMesh*)Mesh)->FrameCount;
		if (key == '1')
		{
			if (++frameNum >= FrameCount)
				frameNum = 0;
		}
		else if (key == '2')
		{
			if (--frameNum < 0)
				frameNum = FrameCount-1;
		}
	}
	else if (MeshType == MESH_SKEL)
	{
		if (key == 'l')
		{
			if (++skelLodNum >= static_cast<USkeletalMesh*>(Mesh)->StaticLODModels.Num())
				skelLodNum = -1;
		}
	}
}


void AppDisplayTexts(bool helpVisible)
{
	if (helpVisible)
	{
		GL::text("N           show normals\n");
		if (MeshType == MESH_SKEL)
		{
			GL::text("L           cycle mesh LODs\n");
		}
		GL::textf("-----\n\n");		// divider
	}
	GL::textf("Mesh: %s\n", MeshName);
	if (MeshType == MESH_SKEL)
	{
		GL::textf("LOD : ");
		if (skelLodNum < 0)
			GL::textf("base mesh\n");
		else
			GL::textf("%d\n", skelLodNum);
	}
}
