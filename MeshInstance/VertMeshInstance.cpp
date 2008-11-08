#include "Core.h"
#include "UnrealClasses.h"
#include "MeshInstance.h"


int CVertMeshInstance::FindAnim(const char *AnimName) const
{
	if (!AnimName)
		return INDEX_NONE;
	const UVertMesh *Mesh = GetMesh();
	for (int i = 0; i < Mesh->AnimSeqs.Num(); i++)
		if (!strcmp(Mesh->AnimSeqs[i].Name, AnimName))
			return i;
	return INDEX_NONE;
}


void CVertMeshInstance::Draw()
{
	guard(CVertMeshInstance::Draw);

	const UVertMesh *Mesh = GetMesh();
	int i;

	int FrameNum1, FrameNum2;
	float frac;
	if (AnimIndex != INDEX_NONE)
	{
		const FMeshAnimSeq &A = Mesh->AnimSeqs[AnimIndex];
		FrameNum1 = appFloor(AnimTime);
		FrameNum2 = FrameNum1 + 1;
		if (FrameNum2 >= A.NumFrames)
			FrameNum2 = 0;
		frac      = AnimTime - FrameNum1;
		FrameNum1 += A.StartFrame;
		FrameNum2 += A.StartFrame;
		// clamp frame numbers (has mesh with wrong frame count in last animation:
		// UT1/BotPack/cdgunmainM; this animation is shown in UnrealEd as lerping to null size)
		if (FrameNum1 >= Mesh->FrameCount)
			FrameNum1 = Mesh->FrameCount-1;
		if (FrameNum2 >= Mesh->FrameCount)
			FrameNum2 = Mesh->FrameCount-1;
	}
	else
	{
		FrameNum1 = 0;
		FrameNum2 = 0;
		frac      = 0;
	}

	int base1 = Mesh->VertexCount * FrameNum1;
	int base2 = Mesh->VertexCount * FrameNum2;

	CVec3 Vert[3];
	CVec3 Norm[3];

	float backLerp = 1 - frac;
	CVec3 Scale1, Scale2;
	Scale1 = Scale2 = (CVec3&)Mesh->MeshScale;
	Scale1.Scale(backLerp);
	Scale2.Scale(frac);

	for (i = 0; i < Mesh->Faces.Num(); i++)
	{
		int j;
		const FMeshFace &F = Mesh->Faces[i];
		for (j = 0; j < 3; j++)
		{
			const FMeshWedge &W = Mesh->Wedges[F.iWedge[j]];
			CVec3 tmp;
#if 0
			// path with no frame lerp
			// vertex
			const FMeshVert &V = Mesh->Verts[base1 + W.iVertex];
			tmp[0] = V.X * Mesh->MeshScale.X;
			tmp[1] = V.Y * Mesh->MeshScale.Y;
			tmp[2] = V.Z * Mesh->MeshScale.Z;
			BaseTransform.TransformPoint(tmp, Vert[j]);
			// normal
			const FMeshNorm &N = Mesh->Normals[base1 + W.iVertex];
			tmp[0] = (N.X - 512.0f) / 512;
			tmp[1] = (N.Y - 512.0f) / 512;
			tmp[2] = (N.Z - 512.0f) / 512;
			BaseTransform.axis.TransformVector(tmp, Norm[j]);
#else
			// vertex
			const FMeshVert &V1 = Mesh->Verts[base1 + W.iVertex];
			const FMeshVert &V2 = Mesh->Verts[base2 + W.iVertex];
			tmp[0] = V1.X * Scale1[0] + V2.X * Scale2[0];
			tmp[1] = V1.Y * Scale1[1] + V2.Y * Scale2[1];
			tmp[2] = V1.Z * Scale1[2] + V2.Z * Scale2[2];
			BaseTransform.TransformPoint(tmp, Vert[j]);
			// normal
			const FMeshNorm &N1 = Mesh->Normals[base1 + W.iVertex];
			const FMeshNorm &N2 = Mesh->Normals[base2 + W.iVertex];
			tmp[0] = (N1.X * backLerp + N2.X * frac - 512.0f) / 512;
			tmp[1] = (N1.Y * backLerp + N2.Y * frac - 512.0f) / 512;
			tmp[2] = (N1.Z * backLerp + N2.Z * frac - 512.0f) / 512;
			BaseTransform.axis.TransformVector(tmp, Norm[j]);
#endif
		}

		// draw mesh
		SetMaterial(F.MaterialIndex);
		glEnable(GL_LIGHTING);
		glBegin(GL_TRIANGLES);
		for (j = 2; j >= 0; j--)	// skeletal mesh and vertex mesh has opposite triangle vertex order ??
		{
			const FMeshWedge &W = Mesh->Wedges[F.iWedge[j]];
			glTexCoord2f(W.TexUV.U, W.TexUV.V);
			glNormal3fv(Norm[j].v);
			glVertex3fv(Vert[j].v);
		}
		glEnd();
		if (bShowNormals)
		{
			glDisable(GL_LIGHTING);
			glDisable(GL_TEXTURE_2D);
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
	glDisable(GL_TEXTURE_2D);
	glEnd();

	unguard;
}


void CVertMeshInstance::PlayAnimInternal(const char *AnimName, float Rate, float TweenTime, int Channel, bool Looped)
{
	guard(CVertMeshInstance::PlayAnimInternal);
	assert(Channel == 0);

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

	const UVertMesh *Mesh = GetMesh();
	AnimRate   = (NewAnimIndex != INDEX_NONE) ? Mesh->AnimSeqs[NewAnimIndex].Rate * Rate : 0;
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


void CVertMeshInstance::FreezeAnimAt(float Time, int Channel)
{
	guard(CVertMeshInstance::FreezeAnimAt);
	assert(Channel == 0);
	AnimTime = Time;
	AnimRate = 0;
	unguard;
}


void CVertMeshInstance::GetAnimParams(int Channel, const char *&AnimName,
	float &Frame, float &NumFrames, float &Rate) const
{
	guard(CVertMeshInstance::GetAnimParams);
	assert(Channel == 0);

	const UVertMesh *Mesh = GetMesh();
	if (AnimIndex < 0)
	{
		AnimName  = "None";
		Frame     = 0;
		NumFrames = 0;
		Rate      = 0;
		return;
	}
	const FMeshAnimSeq &AnimSeq = Mesh->AnimSeqs[AnimIndex];
	AnimName  = AnimSeq.Name;
	Frame     = AnimTime;
	NumFrames = AnimSeq.NumFrames;
	Rate      = AnimRate;

	unguard;
}


void CVertMeshInstance::UpdateAnimation(float TimeDelta)
{
	const UVertMesh *Mesh = GetMesh();

	if (AnimIndex >= 0)
	{
		// update animation time
		AnimTime += TimeDelta * AnimRate;
		const FMeshAnimSeq &Seq = Mesh->AnimSeqs[AnimIndex];
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
