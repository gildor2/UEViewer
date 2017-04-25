#include "Core.h"
#include "UnrealClasses.h"

//!! BUG: vertex meshes displayed too dark in GL2 renderer because of missing tangents

#if RENDERING

#include "MeshCommon.h"				// for CMeshSection
#include "MeshInstance.h"
#include "UnMesh2.h"
#include "UnMathTools.h"

#include "TypeConvert.h"


CVertMeshInstance::CVertMeshInstance()
:	AnimIndex(-1)
,	AnimTime(0)
,	Verts(NULL)
,	Normals(NULL)
,	Indices(NULL)
{}


CVertMeshInstance::~CVertMeshInstance()
{
	FreeRenderBuffers();
}


void CVertMeshInstance::FreeRenderBuffers()
{
	if (Verts) delete[] Verts;
	if (Normals) delete[] Normals;
	if (Indices) delete[] Indices;
	Verts   = NULL;
	Normals = NULL;
	Indices = NULL;
}


void CVertMeshInstance::SetMesh(const UVertMesh *Mesh)
{
	pMesh = Mesh;

	FRotator R;
	R = pMesh->RotOrigin;
	R.Roll = -R.Roll;		// difference from SkeletalMesh: Roll seems inverted
	RotatorToAxis(R, BaseTransformScaled.axis);
	BaseTransformScaled.axis[0].Scale(Mesh->MeshScale.X);
	BaseTransformScaled.axis[1].Scale(Mesh->MeshScale.Y);
	BaseTransformScaled.axis[2].Scale(Mesh->MeshScale.Z);
	CVec3 tmp;
	VectorNegate(CVT(Mesh->MeshOrigin), tmp);
	BaseTransformScaled.axis.UnTransformVector(tmp, BaseTransformScaled.origin);

	FreeRenderBuffers();
	if (!pMesh->Faces.Num()) return;

	// prepare vertex and index buffers, build sections

	Verts   = new CVec3[pMesh->Wedges.Num()];
	Normals = new CVec3[pMesh->Wedges.Num()];
	Indices = new uint16[pMesh->Faces.Num() * 3];

	const FMeshFace *F = &pMesh->Faces[0];
	uint16 *pIndex = Indices;
	int PrevMaterial = -2;
	CMeshSection *Sec = NULL;
	for (int i = 0; i < pMesh->Faces.Num(); i++, F++)
	{
		if (F->MaterialIndex != PrevMaterial)
		{
			// get material parameters
			int PolyFlags = 0;
			int TexIndex = 1000000;
			if (F->MaterialIndex < pMesh->Materials.Num())
			{
				const FMeshMaterial &M = pMesh->Materials[F->MaterialIndex];
				TexIndex  = M.TextureIndex;
				PolyFlags = M.PolyFlags;
			}
			// possible situation: Textures array is empty (mesh textured by script)
			UUnrealMaterial *Mat = (TexIndex < pMesh->Textures.Num()) ? MATERIAL_CAST(pMesh->Textures[TexIndex]) : NULL;

			// create new section
			Sec = new (Sections) CMeshSection;
			Sec->FirstIndex = pIndex - Indices;
			Sec->NumFaces   = 0;
			Sec->Material   = UMaterialWithPolyFlags::Create(Mat, PolyFlags);

			PrevMaterial = F->MaterialIndex;
		}
		// store indices
		// note: skeletal mesh and vertex mesh has opposite triangle vertex order
		*pIndex++ = F->iWedge[0];
		*pIndex++ = F->iWedge[2];
		*pIndex++ = F->iWedge[1];
		Sec->NumFaces++;
	}
}


int CVertMeshInstance::FindAnim(const char *AnimName) const
{
	if (!AnimName)
		return INDEX_NONE;
	for (int i = 0; i < pMesh->AnimSeqs.Num(); i++)
		if (!strcmp(pMesh->AnimSeqs[i].Name, AnimName))
			return i;
	return INDEX_NONE;
}


int CVertMeshInstance::GetAnimCount() const
{
	return pMesh->AnimSeqs.Num();
}


const char *CVertMeshInstance::GetAnimName(int Index) const
{
	guard(CVertMeshInstance::GetAnimName);
	if (Index < 0) return "None";
	return pMesh->AnimSeqs[Index].Name;
	unguard;
}


void CVertMeshInstance::Draw(unsigned flags)
{
	guard(CVertMeshInstance::Draw);

	if (!Sections.Num()) return;		// empty mesh

	int i;

	// get 2 frames for interpolation
	int FrameNum1, FrameNum2;
	float frac;
	if (AnimIndex != INDEX_NONE)
	{
		const FMeshAnimSeq &A = pMesh->AnimSeqs[AnimIndex];
		FrameNum1 = appFloor(AnimTime);
		FrameNum2 = FrameNum1 + 1;
		if (FrameNum2 >= A.NumFrames)
			FrameNum2 = 0;
		frac      = AnimTime - FrameNum1;
		FrameNum1 += A.StartFrame;
		FrameNum2 += A.StartFrame;
		// clamp frame numbers (has mesh with wrong frame count in last animation:
		// UT1/BotPack/cdgunmainM; this animation is shown in UnrealEd as lerping to null size)
		if (FrameNum1 >= pMesh->FrameCount)
			FrameNum1 = pMesh->FrameCount-1;
		if (FrameNum2 >= pMesh->FrameCount)
			FrameNum2 = pMesh->FrameCount-1;
	}
	else
	{
		FrameNum1 = 0;
		FrameNum2 = 0;
		frac      = 0;
	}

	int base1 = pMesh->VertexCount * FrameNum1;
	int base2 = pMesh->VertexCount * FrameNum2;

	float backLerp = 1 - frac;
//	CVec3 Scale1, Scale2;
//	Scale1 = Scale2 = CVT(pMesh->MeshScale);
//	Scale1.Scale(backLerp);
//	Scale2.Scale(frac);

	// compute deformed mesh
	const FMeshWedge *W = &pMesh->Wedges[0];
	CVec3 *pVec    = Verts;
	CVec3 *pNormal = Normals;
	for (i = 0; i < pMesh->Wedges.Num(); i++, pVec++, pNormal++, W++)
	{
		CVec3 tmp;
#if 0
		// path with no frame lerp
		// vertex
		const FMeshVert &V = pMesh->Verts[base1 + W->iVertex];
		tmp[0] = V.X;// * pMesh->MeshScale.X;
		tmp[1] = V.Y;// * pMesh->MeshScale.Y;
		tmp[2] = V.Z;// * pMesh->MeshScale.Z;
		BaseTransformScaled.UnTransformPoint(tmp, *pVec);
		// normal
		const FMeshNorm &N = pMesh->Normals[base1 + W->iVertex];
		tmp[0] = (N.X - 512.0f) / 512;
		tmp[1] = (N.Y - 512.0f) / 512;
		tmp[2] = (N.Z - 512.0f) / 512;
		BaseTransformScaled.axis.UnTransformVector(tmp, *pNormal);
#else
		// vertex
		const FMeshVert &V1 = pMesh->Verts[base1 + W->iVertex];
		const FMeshVert &V2 = pMesh->Verts[base2 + W->iVertex];
	#if 0
		tmp[0] = V1.X * Scale1[0] + V2.X * Scale2[0];
		tmp[1] = V1.Y * Scale1[1] + V2.Y * Scale2[1];
		tmp[2] = V1.Z * Scale1[2] + V2.Z * Scale2[2];
	#else
		tmp[0] = V1.X * backLerp + V2.X * frac;
		tmp[1] = V1.Y * backLerp + V2.Y * frac;
		tmp[2] = V1.Z * backLerp + V2.Z * frac;
	#endif
		BaseTransformScaled.UnTransformPoint(tmp, *pVec);
		// normal
		const FMeshNorm &N1 = pMesh->Normals[base1 + W->iVertex];
		const FMeshNorm &N2 = pMesh->Normals[base2 + W->iVertex];
		tmp[0] = (N1.X * backLerp + N2.X * frac - 512.0f) / 512;
		tmp[1] = (N1.Y * backLerp + N2.Y * frac - 512.0f) / 512;
		tmp[2] = (N1.Z * backLerp + N2.Z * frac - 512.0f) / 512;
		BaseTransformScaled.axis.UnTransformVector(tmp, *pNormal);
#endif
	}

#if 0
	glBegin(GL_POINTS);
	for (i = 0; i < pMesh->Wedges.Num(); i++)
	{
		glVertex3fv(Verts[i].v);
	}
	glEnd();
	return;
#endif

	// draw mesh
	glEnable(GL_LIGHTING);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	glVertexPointer(3, GL_FLOAT, sizeof(CVec3), Verts);
	glNormalPointer(GL_FLOAT, sizeof(CVec3), Normals);
	glTexCoordPointer(2, GL_FLOAT, sizeof(FMeshWedge), &pMesh->Wedges[0].TexUV.U);

	for (i = 0; i < Sections.Num(); i++)
	{
		const CMeshSection &Sec = Sections[i];
		if (!Sec.NumFaces) continue;
		SetMaterial(Sec.Material, i);
		glDrawElements(GL_TRIANGLES, Sec.NumFaces * 3, GL_UNSIGNED_SHORT, &Indices[Sec.FirstIndex]);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	glDisable(GL_LIGHTING);
	BindDefaultMaterial(true);

	// draw mesh normals
	if (flags & DF_SHOW_NORMALS)
	{
		glBegin(GL_LINES);
		glColor3f(0.5, 1, 0);
		for (i = 0; i < pMesh->Wedges.Num(); i++)
		{
			Normals[i].NormalizeFast();	// normals are scaled now with BaseTransformScaled, so normalize them for debug view
			glVertex3fv(Verts[i].v);
			CVec3 tmp;
			VectorMA(Verts[i], 2, Normals[i], tmp);
			glVertex3fv(tmp.v);
		}
		glEnd();
	}

	unguard;
}


void CVertMeshInstance::PlayAnimInternal(const char *AnimName, float Rate, bool Looped)
{
	guard(CVertMeshInstance::PlayAnimInternal);

	int NewAnimIndex = FindAnim(AnimName);
	if (NewAnimIndex == INDEX_NONE)
	{
		// show default pose
		AnimIndex  = INDEX_NONE;
		AnimTime   = 0;
		AnimRate   = 0;
		AnimLooped = false;
		return;
	}

	AnimRate   = (NewAnimIndex != INDEX_NONE) ? pMesh->AnimSeqs[NewAnimIndex].Rate * Rate : 0;
	AnimLooped = Looped;

	if (NewAnimIndex == AnimIndex && Looped)
	{
		// animation not changed, just set some flags (above)
		return;
	}

	AnimIndex = NewAnimIndex;
	AnimTime  = 0;

	unguard;
}


void CVertMeshInstance::FreezeAnimAt(float Time)
{
	guard(CVertMeshInstance::FreezeAnimAt);
	AnimTime = Time;
	AnimRate = 0;
	unguard;
}


void CVertMeshInstance::GetAnimParams(const char *&AnimName, float &Frame, float &NumFrames, float &Rate) const
{
	guard(CVertMeshInstance::GetAnimParams);

	if (AnimIndex < 0)
	{
		AnimName  = "None";
		Frame     = 0;
		NumFrames = 0;
		Rate      = 0;
		return;
	}
	const FMeshAnimSeq &AnimSeq = pMesh->AnimSeqs[AnimIndex];
	AnimName  = AnimSeq.Name;
	Frame     = AnimTime;
	NumFrames = AnimSeq.NumFrames;
	Rate      = AnimRate;

	unguard;
}


void CVertMeshInstance::UpdateAnimation(float TimeDelta)
{
	if (AnimIndex >= 0)
	{
		// update animation time
		AnimTime += TimeDelta * AnimRate;
		const FMeshAnimSeq &Seq = pMesh->AnimSeqs[AnimIndex];
		if (AnimLooped)
		{
			if (AnimTime >= Seq.NumFrames)
			{
				// wrap time
				int numSkip = appFloor(AnimTime / Seq.NumFrames);
				AnimTime -= numSkip * Seq.NumFrames;
			}
		}
		else
		{
			if (AnimTime >= Seq.NumFrames-1)
			{
				// clamp time
				AnimTime = Seq.NumFrames-1;
				if (AnimTime < 0)
					AnimTime = 0;
			}
		}
	}
}

#endif // RENDERING
