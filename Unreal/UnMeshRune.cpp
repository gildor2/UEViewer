#include "Core.h"
#include "UnrealClasses.h"
#include "UnMesh2.h"
#include "UnPackage.h"

#include "UnMaterial2.h"

#include "TypeConvert.h"


#if RUNE

#define NUM_POLYGROUPS		16
#define MAX_CHILD_JOINTS	4


struct RVertex
{
	FVector			point1;					// position of vertex relative to joint1
	FVector			point2;					// position of vertex relative to joint2
	int				joint1;
	int				joint2;
	float			weight1;

	friend FArchive& operator<<(FArchive &Ar, RVertex &V)
	{
		return Ar << V.point1 << V.point2 << V.joint1 << V.joint2 << V.weight1;
	}
};

SIMPLE_TYPE(RVertex, int)


struct RTriangle
{
	int16			vIndex[3];				// Vertex indices
	FMeshUV1		tex[3];					// Texture UV coordinates
	byte			polygroup;				// polygroup this tri belongs to

	friend FArchive& operator<<(FArchive &Ar, RTriangle &T)
	{
		Ar << T.vIndex[0] << T.vIndex[1] << T.vIndex[2];
		Ar << T.tex[0] << T.tex[1] << T.tex[2];
		Ar << T.polygroup;
		return Ar;
	}
};


struct RMesh
{
	int				numverts;
	int				numtris;

	TArray<RTriangle> tris;
	TArray<RVertex>	verts;
	int				GroupFlags[NUM_POLYGROUPS];
	FName			PolyGroupSkinNames[NUM_POLYGROUPS];

	int				decCount;
	TArray<byte>	decdata;

	friend FArchive& operator<<(FArchive &Ar, RMesh &M)
	{
		Ar << M.numverts << M.numtris;
		Ar << M.tris << M.verts;
		Ar << M.decCount << M.decdata;
		for (int i = 0; i < NUM_POLYGROUPS; i++)
			Ar << M.GroupFlags[i] << M.PolyGroupSkinNames[i];
		return Ar;
	}
};


struct RJoint
{
	// Skeletal structure implimentation
	int				parent;
	int				children[MAX_CHILD_JOINTS];
	FName			name;					// Name of joint
	int				jointgroup;				// Body group this belongs (allows us to animate groups seperately)
	int				flags;					// Default joint flags
	FRotator		baserot;				// Rotational delta to base orientation
	FPlane			planes[6];				// Local joint coordinate planes defining a collision box

	// Serializer.
	friend FArchive& operator<<(FArchive &Ar, RJoint &J)
	{
		int i;
		Ar << J.parent;
		for (i = 0; i < MAX_CHILD_JOINTS; i++)
			Ar << J.children[i];
		Ar << J.name << J.jointgroup << J.flags;
		Ar << J.baserot;
		for (i = 0; i < ARRAY_COUNT(J.planes); i++)
			Ar << J.planes[i];
		return Ar;
	}
};


struct FRSkelAnimSeq : public FMeshAnimSeq
{
	TArray<byte>	animdata;				// contains compressed animation data

	friend FArchive& operator<<(FArchive &Ar, FRSkelAnimSeq &S)
	{
		return Ar << (FMeshAnimSeq&)S << S.animdata;
	}
};


struct RJointState
{
	FVector			pos;
	FRotator		rot;
	FScale			scale;					//?? check this - possibly, require to extend animation with bone scales

	friend FArchive& operator<<(FArchive &Ar, RJointState &J)
	{
		return Ar << J.pos << J.rot << J.scale;
	}
};

SIMPLE_TYPE(RJointState, float)


struct RAnimFrame
{
	int16			sequenceID;				// Sequence this frame belongs to
	FName			event;					// Event to call during this frame
	FBox			bounds;					// Bounding box of frame
	TArray<RJointState> jointanim;			// O(numjoints) only used for preprocess (transient)

	friend FArchive& operator<<(FArchive &Ar, RAnimFrame &F)
	{
		return Ar << F.sequenceID << F.event << F.bounds << F.jointanim;
	}
};


static FQuat EulerToQuat(const FRotator &Rot)
{
	// signs was taken from experiments
	float yaw   =  Rot.Yaw   * (M_PI / 32768.0f);
	float pitch = -Rot.Pitch * (M_PI / 32768.0f);
	float roll  = -Rot.Roll  * (M_PI / 32768.0f);
	// derived from Doom3 idAngles::ToQuat()
	float sz = sin(yaw / 2);
	float cz = cos(yaw / 2);
	float sy = sin(pitch / 2);
	float cy = cos(pitch / 2);
	float sx = sin(roll / 2);
	float cx = cos(roll / 2);
	float sxcy = sx * cy;
	float cxcy = cx * cy;
	float sxsy = sx * sy;
	float cxsy = cx * sy;
	FQuat Q;
	Q.Set(cxsy*sz - sxcy*cz, -cxsy*cz - sxcy*sz, sxsy*cz - cxcy*sz, cxcy*cz + sxsy*sz);
	return Q;
}


static void ConvertRuneAnimations(UMeshAnimation &Anim, const TArray<RJoint> &Bones,
	const TArray<FRSkelAnimSeq> &Seqs)
{
	guard(ConvertRuneAnimations);

	int i, j;
	int numBones = Bones.Num();
	// create RefBones
	Anim.RefBones.Empty(Bones.Num());
	for (i = 0; i < Bones.Num(); i++)
	{
		const RJoint &SB = Bones[i];
		FNamedBone *B = new(Anim.RefBones) FNamedBone;
		B->Name        = SB.name;
		B->Flags       = 0;
		B->ParentIndex = SB.parent;
	}
	// create AnimSeqs
	Anim.AnimSeqs.Empty(Seqs.Num());
	Anim.Moves.Empty(Seqs.Num());
	for (i = 0; i < Seqs.Num(); i++)
	{
		// create FMeshAnimSeq
		const FRSkelAnimSeq &SS = Seqs[i];
		FMeshAnimSeq *S = new(Anim.AnimSeqs) FMeshAnimSeq;
		S->Name       = SS.Name;
		CopyArray(S->Groups, SS.Groups);
		S->StartFrame = 0;
		S->NumFrames  = SS.NumFrames;
		S->Rate       = SS.Rate;
		//?? S->Notifys
		// create MotionChunk
		MotionChunk *M = new(Anim.Moves) MotionChunk;
		M->TrackTime  = SS.NumFrames;
		// dummy bone remap
		M->AnimTracks.Empty(numBones);
		// convert animation data
		const byte *data = &SS.animdata[0];
		for (j = 0; j < numBones; j++)
		{
			// prepare AnalogTrack
			AnalogTrack *A = new(M->AnimTracks) AnalogTrack;
			A->KeyQuat.Empty(SS.NumFrames);
			A->KeyPos.Empty(SS.NumFrames);
			A->KeyTime.Empty(SS.NumFrames);
		}
		for (int frame = 0; frame < SS.NumFrames; frame++)
		{
			for (int joint = 0; joint < numBones; joint++)
			{
				AnalogTrack &A = M->AnimTracks[joint];

				FVector pos, scale;
				pos.Set(0, 0, 0);
				scale.Set(1, 1, 1);
				FRotator rot;
				rot.Set(0, 0, 0);

				byte f = *data++;
				int16 d;
#define GET		d = data[0] + (data[1] << 8); data += 2;
#define GETF(v)	{ GET; v = (float)d / 256.0f; }
#define GETI(v)	{ GET; v = d; }
				// decode position
				if (f & 1)    GETF(pos.X);
				if (f & 2)    GETF(pos.Y);
				if (f & 4)    GETF(pos.Z);
				// decode scale
				if (f & 8)  { GETF(scale.X); GETF(scale.Z); }
				if (f & 0x10) GETF(scale.Y);
				// decode rotation
				if (f & 0x20) GETI(rot.Pitch);
				if (f & 0x40) GETI(rot.Yaw);
				if (f & 0x80) GETI(rot.Roll);
#undef GET
#undef GETF
#undef GETI
				A.KeyQuat.Add(EulerToQuat(rot));
				A.KeyPos.Add(pos);
				//?? notify about scale!=(1,1,1)
			}
		}
		assert(data == &SS.animdata[0] + SS.animdata.Num());
	}

	unguard;
}


// function is similar to part of CSkelMeshInstance::SetMesh()
static void BuildSkeleton(TArray<CCoords> &Coords, const TArray<RJoint> &Bones, const TArray<AnalogTrack> &Anim)
{
	guard(BuildSkeleton);

	int numBones = Anim.Num();
	Coords.Empty(numBones);
	Coords.AddZeroed(numBones);

	for (int i = 0; i < numBones; i++)
	{
		const AnalogTrack &A = Anim[i];
		const RJoint &B = Bones[i];
		CCoords &BC = Coords[i];
		// compute reference bone coords
		CVec3 BP;
		CQuat BO;
		// get default pose
		BP = CVT(A.KeyPos[0]);
		BO = CVT(A.KeyQuat[0]);
		if (!i) BO.Conjugate();

		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
		{
			assert(B.parent >= 0);
			Coords[B.parent].UnTransformCoords(BC, BC);
		}
	}

	unguard;
}


void USkelModel::Serialize(FArchive &Ar)
{
	guard(USkelModel::Serialize);

	assert(Ar.IsLoading);					// no saving ...
	Super::Serialize(Ar);

	// USkelModel data
	int nummeshes;
	int numjoints;
	int numframes;
	int numsequences;
	int numskins;
	int rootjoint;

	FVector					PosOffset;		// Offset of creature relative to base
	FRotator				RotOffset;		// Offset of creatures rotation

	TArray<RMesh>			meshes;
	TArray<RJoint>			joints;
	TArray<FRSkelAnimSeq>	AnimSeqs;		// Compressed animation data for sequence
	TArray<RAnimFrame>		frames;

	Ar << nummeshes << numjoints << numframes << numsequences << numskins << rootjoint;
	Ar << meshes << joints << AnimSeqs << frames << PosOffset << RotOffset;

	int modelIdx;
	// create all meshes first, then fill them (for better view order)
	for (modelIdx = 0; modelIdx < meshes.Num(); modelIdx++)
	{
		// create new USkeletalMesh
		// use "CreateClass()" instead of "new USkeletalMesh" to allow this object to be
		// placed in GObjObjects array and be browsable in a viewer
		USkeletalMesh *sm = static_cast<USkeletalMesh*>(CreateClass("SkeletalMesh"));
		char nameBuf[256];
		appSprintf(ARRAY_ARG(nameBuf), "%s_%d", Name, modelIdx);
		const char *name = appStrdupPool(nameBuf);
		Meshes.Add(sm);
		// setup UOnject
		sm->Name         = name;
		sm->Package      = Package;
		sm->PackageIndex = INDEX_NONE;		// not really exported
		sm->Outer        = NULL;
	}
	// create animation
	Anim = static_cast<UMeshAnimation*>(CreateClass("MeshAnimation"));
	Anim->Name         = Name;
	Anim->Package      = Package;
	Anim->PackageIndex = INDEX_NONE;		// not really exported
	Anim->Outer        = NULL;
	ConvertRuneAnimations(*Anim, joints, AnimSeqs);
	Anim->ConvertAnims();					//?? second conversion
	// get baseframe
	assert(strcmp(Anim->AnimSeqs[0].Name, "baseframe") == 0);
	const TArray<AnalogTrack> &BaseAnim = Anim->Moves[0].AnimTracks;
	// compute bone coordinates
	TArray<CCoords> BoneCoords;
	BuildSkeleton(BoneCoords, joints, BaseAnim);

	// setup meshes
	for (modelIdx = 0; modelIdx < meshes.Num(); modelIdx++)
	{
		int i, j;
		const RMesh  &src = meshes[modelIdx];
		USkeletalMesh *sm = Meshes[modelIdx];
		sm->Animation = Anim;
		// setup ULodMesh
		sm->RotOrigin = RotOffset;
		sm->MeshScale.Set(1, 1, 1);
		sm->MeshOrigin = PosOffset;
		// copy skeleton
		sm->RefSkeleton.Empty(joints.Num());
		for (i = 0; i < joints.Num(); i++)
		{
			const RJoint &J = joints[i];
			FMeshBone *B = new(sm->RefSkeleton) FMeshBone;
			B->Name = J.name;
			B->Flags = 0;
			B->ParentIndex = (J.parent > 0) ? J.parent : 0;		// -1 -> 0
			// copy bone orientations from base animation frame
			B->BonePos.Orientation = BaseAnim[i].KeyQuat[0];
			B->BonePos.Position    = BaseAnim[i].KeyPos[0];
		}
		// copy vertices
		int VertexCount = sm->VertexCount = src.verts.Num();
		sm->Points.Empty(VertexCount);
		for (i = 0; i < VertexCount; i++)
		{
			const RVertex &v1 = src.verts[i];
			FVector *V = new(sm->Points) FVector;
			// transform point from local bone space to model space
			BoneCoords[v1.joint1].UnTransformPoint(CVT(v1.point1), CVT(*V));
		}
		// copy triangles and create wedges
		// here we create 3 wedges for each triangle.
		// it is possible to reduce number of wedges by finding duplicates, but we don't
		// need it here ...
		int TrisCount = src.tris.Num();
		sm->Triangles.Empty(TrisCount);
		sm->Wedges.Empty(TrisCount * 3);
		int numMaterials = 0;		// should detect real material count
		for (i = 0; i < TrisCount; i++)
		{
			const RTriangle &tri = src.tris[i];
			// create triangle
			VTriangle *T = new(sm->Triangles) VTriangle;
			T->MatIndex = tri.polygroup;
			if (numMaterials <= tri.polygroup)
				numMaterials = tri.polygroup+1;
			// create wedges
			for (j = 0; j < 3; j++)
			{
				T->WedgeIndex[j] = sm->Wedges.Num();
				FMeshWedge *W = new(sm->Wedges) FMeshWedge;
				W->iVertex = tri.vIndex[j];
				W->TexUV   = tri.tex[j];
			}
			// reverse order of triangle vertices
			Exchange(T->WedgeIndex[0], T->WedgeIndex[1]);
		}
		// build influences
		for (i = 0; i < VertexCount; i++)
		{
			const RVertex &v1 = src.verts[i];
			FVertInfluence *Inf = new(sm->VertInfluences) FVertInfluence;
			Inf->PointIndex = i;
			Inf->BoneIndex  = v1.joint1;
			Inf->Weight     = v1.weight1;
			if (Inf->Weight != 1.0f)
			{
				// influence for 2nd bone
				Inf = new(sm->VertInfluences) FVertInfluence;
				Inf->PointIndex = i;
				Inf->BoneIndex  = v1.joint2;
				Inf->Weight     = 1.0f - v1.weight1;
			}
		}
		// create materials
		for (i = 0; i < numMaterials; i++)
		{
			const char *texName = src.PolyGroupSkinNames[i];
			FMeshMaterial *M1 = new(sm->Materials) FMeshMaterial;
			M1->PolyFlags    = src.GroupFlags[i];
			M1->TextureIndex = sm->Textures.Num();
			if (strcmp(texName, "None") == 0)
			{
				// texture should be set from script
				sm->Textures.Add(NULL);
				continue;
			}
			// find texture in object's package
			int texExportIdx = Package->FindExport(texName);
			if (texExportIdx == INDEX_NONE)
			{
				appPrintf("ERROR: unable to find export \"%s\" for mesh \"%s\" (%d)\n",
					texName, Name, modelIdx);
				continue;
			}
			// load and remember texture
			UMaterial *Tex = static_cast<UMaterial*>(Package->CreateExport(texExportIdx));
			sm->Textures.Add(Tex);
		}
		// setup UPrimitive properties using 1st animation frame
		// note: this->BoundingBox and this->BoundingSphere are null
		const RAnimFrame &F = frames[0];
		assert(strcmp(AnimSeqs[0].Name, "baseframe") == 0 && AnimSeqs[0].StartFrame == 0);
		CVec3 mins, maxs;
		sm->BoundingBox = F.bounds;
		mins = CVT(F.bounds.Min);
		maxs = CVT(F.bounds.Max);
		CVec3 &center = CVT(sm->BoundingSphere);
		for (i = 0; i < 3; i++)
			center[i] = (mins[i] + maxs[i]) / 2;
		sm->BoundingSphere.R = VectorDistance(center, mins);

		// create CSkeletalMesh
		sm->ConvertMesh();
	}

	unguard;
}


USkelModel::~USkelModel()
{
	// Release holded data.
	// Note: children meshes are stored in GObjObjects array, so these meshes could be released separately
	// from parent USkelModel. Se we should be careful releasing generated children models.
	int i;
	for (i = 0; i < Meshes.Num(); i++)
	{
		USkeletalMesh* m = Meshes[i];
		// Check if this mesh were already released, i.e. pointer is not valid now.
		if (UObject::GObjObjects.FindItem(m) >= 0)
		{
			delete m;
		}
	}
	if (Anim && UObject::GObjObjects.FindItem(Anim) >= 0)
	{
		delete Anim;
	}
}


#endif // RUNE
