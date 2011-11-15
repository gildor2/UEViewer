#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "MeshInstance.h"
#include "UnMesh.h"
#include "UnMathTools.h"

#include "TypeConvert.h"


void CVertMeshInstance::SetMesh(const UVertMesh *Mesh)
{
	pMesh = Mesh;
	SetAxis(Mesh->RotOrigin, BaseTransform.axis);
	BaseTransform.origin[0] = Mesh->MeshOrigin.X * Mesh->MeshScale.X;
	BaseTransform.origin[1] = Mesh->MeshOrigin.Y * Mesh->MeshScale.Y;
	BaseTransform.origin[2] = Mesh->MeshOrigin.Z * Mesh->MeshScale.Z;

	BaseTransformScaled.axis = BaseTransform.axis;
	CVec3 tmp;
	tmp[0] = 1.0f / Mesh->MeshScale.X;
	tmp[1] = 1.0f / Mesh->MeshScale.Y;
	tmp[2] = 1.0f / Mesh->MeshScale.Z;
	BaseTransformScaled.axis.PrescaleSource(tmp);
	BaseTransformScaled.origin = CVT(Mesh->MeshOrigin);
}

UUnrealMaterial *CVertMeshInstance::GetMaterial(int Index, int *PolyFlags)
{
	guard(CVertMeshInstance::GetMaterial);
	int TexIndex = 1000000;
	if (PolyFlags) *PolyFlags = 0;
	if (Index < pMesh->Materials.Num())
	{
		const FMeshMaterial &M = pMesh->Materials[Index];
		TexIndex  = M.TextureIndex;
		if (PolyFlags) *PolyFlags = M.PolyFlags;
	}
	// it is possible, that Textures array is empty (mesh textured by script)
	return (TexIndex < pMesh->Textures.Num()) ? MATERIAL_CAST(pMesh->Textures[TexIndex]) : NULL;
	unguard;
}

void CVertMeshInstance::SetMaterial(int Index)
{
	guard(CVertMeshInstance::SetMaterial);
	int PolyFlags;
	UUnrealMaterial *Mat = GetMaterial(Index, &PolyFlags);
	CMeshInstance::SetMaterial(Mat, Index, PolyFlags);
	unguard;
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


void CVertMeshInstance::Draw()
{
	guard(CVertMeshInstance::Draw);

	int i;

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

	CVec3 Vert[3];
	CVec3 Norm[3];

	float backLerp = 1 - frac;
	CVec3 Scale1, Scale2;
	Scale1 = Scale2 = CVT(pMesh->MeshScale);
	Scale1.Scale(backLerp);
	Scale2.Scale(frac);

	for (i = 0; i < pMesh->Faces.Num(); i++)
	{
		int j;
		const FMeshFace &F = pMesh->Faces[i];
		for (j = 0; j < 3; j++)
		{
			const FMeshWedge &W = pMesh->Wedges[F.iWedge[j]];
			CVec3 tmp;
#if 0
			// path with no frame lerp
			// vertex
			const FMeshVert &V = pMesh->Verts[base1 + W.iVertex];
			tmp[0] = V.X * pMesh->MeshScale.X;
			tmp[1] = V.Y * pMesh->MeshScale.Y;
			tmp[2] = V.Z * pMesh->MeshScale.Z;
			BaseTransform.TransformPoint(tmp, Vert[j]);
			// normal
			const FMeshNorm &N = pMesh->Normals[base1 + W.iVertex];
			tmp[0] = (N.X - 512.0f) / 512;
			tmp[1] = (N.Y - 512.0f) / 512;
			tmp[2] = (N.Z - 512.0f) / 512;
			BaseTransform.axis.TransformVector(tmp, Norm[j]);
#else
			// vertex
			const FMeshVert &V1 = pMesh->Verts[base1 + W.iVertex];
			const FMeshVert &V2 = pMesh->Verts[base2 + W.iVertex];
			tmp[0] = V1.X * Scale1[0] + V2.X * Scale2[0];
			tmp[1] = V1.Y * Scale1[1] + V2.Y * Scale2[1];
			tmp[2] = V1.Z * Scale1[2] + V2.Z * Scale2[2];
			BaseTransform.TransformPoint(tmp, Vert[j]);
			// normal
			const FMeshNorm &N1 = pMesh->Normals[base1 + W.iVertex];
			const FMeshNorm &N2 = pMesh->Normals[base2 + W.iVertex];
			tmp[0] = (N1.X * backLerp + N2.X * frac - 512.0f) / 512;
			tmp[1] = (N1.Y * backLerp + N2.Y * frac - 512.0f) / 512;
			tmp[2] = (N1.Z * backLerp + N2.Z * frac - 512.0f) / 512;
			BaseTransform.axis.TransformVector(tmp, Norm[j]);
#endif
		}

		// draw mesh
		//!! NOTE: very unoptimal drawing - should minimize state changes:
		//!! 1. use SetMaterial() as less as possible (when material is changed only)
		//!! 2. place glBegin/glEnd outsize of loop
		//!! 3. show normals is separate loop (will require to store morphed verts/normals somewhere)
		SetMaterial(F.MaterialIndex);
		glEnable(GL_LIGHTING);
		glBegin(GL_TRIANGLES);
		for (j = 2; j >= 0; j--)	// skeletal mesh and vertex mesh has opposite triangle vertex order ??
		{
			const FMeshWedge &W = pMesh->Wedges[F.iWedge[j]];
			glTexCoord2f(W.TexUV.U, W.TexUV.V);
			glNormal3fv(Norm[j].v);
			glVertex3fv(Vert[j].v);
		}
		glEnd();
		if (bShowNormals)
		{
			glDisable(GL_LIGHTING);
			BindDefaultMaterial(true);
			// draw normals
			glBegin(GL_LINES);
			glColor3f(1, 0.5, 0);
			for (j = 0; j < 3; j++)
			{
				glVertex3fv(Vert[j].v);
				CVec3 tmp;
				tmp = Vert[j];
				VectorMA(tmp, 2, Norm[j]);
				glVertex3fv(tmp.v);
			}
			glEnd();
			glColor3f(1, 1, 1);
		}
	}
	BindDefaultMaterial(true);

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
