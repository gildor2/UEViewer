#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"		// for typeinfo
#include "SkeletalMesh.h"


/*-----------------------------------------------------------------------------
	CSkeletalMesh
-----------------------------------------------------------------------------*/

void CSkelMeshLod::BuildNormals()
{
	guard(CSkelMeshLod::BuildNormals);

	if (HasNormals) return;

	int i, j;

	// Find vertices to share.
	// We are using very simple algorithm here: to share all vertices with the same position
	// independently on normals of faces which share this vertex.
	TArray<CVec3> tmpNorm, Points;
	TArray<int>   vertToPoint;						// Verts[] to Points[] map to share vertices
	tmpNorm.Add(NumVerts);							// really will use Points.Num() items, which value is smaller than NumVerts
	Points.Empty(NumVerts);							// ...
	for (i = 0; i < NumVerts; i++)
	{
		const CVec3 &Pos = Verts[i].Position;
		int PointIndex = Points.FindItem(Pos);
		if (PointIndex == INDEX_NONE)
		{
			// unique vertex (not appeared before)
			PointIndex = Points.AddItem(Pos);
		}
		vertToPoint.AddItem(PointIndex);
	}

	CIndexBuffer::IndexAccessor_t Index = Indices.GetAccessor();
	for (i = 0; i < Indices.Num() / 3; i++)
	{
		CSkelMeshVertex *V[3];
		CVec3 *N[3];
		for (j = 0; j < 3; j++)
		{
			int idx = Index(Indices, i * 3 + j);	// index in Verts[]
			V[j] = &Verts[idx];
			N[j] = &tmpNorm[vertToPoint[idx]];		// remap to shared verts
		}

		// compute edges
		CVec3 D[3];				// 0->1, 1->2, 2->0
		VectorSubtract(V[1]->Position, V[0]->Position, D[0]);
		VectorSubtract(V[2]->Position, V[1]->Position, D[1]);
		VectorSubtract(V[0]->Position, V[2]->Position, D[2]);
		// compute face normal
		CVec3 norm;
		cross(D[1], D[0], norm);
		norm.Normalize();
		// compute angles
		for (j = 0; j < 3; j++) D[j].Normalize();
		float angle[3];
		angle[0] = acos(-dot(D[0], D[2]));
		angle[1] = acos(-dot(D[0], D[1]));
		angle[2] = acos(-dot(D[1], D[2]));
		// add normals for triangle verts
		for (j = 0; j < 3; j++)
			VectorMA(*N[j], angle[j], norm);
	}

	// normalize shared normals ...
	for (i = 0; i < Points.Num(); i++)
		tmpNorm[i].Normalize();

	// ... then place ("unshare") normals to Verts
	for (i = 0; i < NumVerts; i++)
		Verts[i].Normal = tmpNorm[vertToPoint[i]];

	HasNormals = true;

	unguard;
}


/*-----------------------------------------------------------------------------
	CAnimSet
-----------------------------------------------------------------------------*/

#define MAX_LINEAR_KEYS		4

static int FindTimeKey(const TArray<float> &KeyTime, float Frame)
{
	guard(FindTimeKey);

	// find index in time key array
	int NumKeys = KeyTime.Num();
	// *** binary search ***
	int Low = 0, High = NumKeys-1;
	while (Low + MAX_LINEAR_KEYS < High)
	{
		int Mid = (Low + High) / 2;
		if (Frame < KeyTime[Mid])
			High = Mid-1;
		else
			Low = Mid;
	}
	// *** linear search ***
	int i;
	for (i = Low; i <= High; i++)
	{
		float CurrKeyTime = KeyTime[i];
		if (Frame == CurrKeyTime)
			return i;		// exact key
		if (Frame < CurrKeyTime)
			return (i > 0) ? i - 1 : 0;	// previous key
	}
	if (i > High)
		i = High;
	return i;

	unguard;
}


// In:  KeyTime, Frame, NumFrames, Loop
// Out: X - previous key index, Y - next key index, F - fraction between keys
static void GetKeyParams(const TArray<float> &KeyTime, float Frame, float NumFrames, bool Loop, int &X, int &Y, float &F)
{
	guard(GetKeyParams);
	X = FindTimeKey(KeyTime, Frame);
	Y = X + 1;
	int NumTimeKeys = KeyTime.Num();
	if (Y >= NumTimeKeys)
	{
		if (!Loop)
		{
			// clamp animation
			Y = NumTimeKeys - 1;
			assert(X == Y);
			F = 0;
		}
		else
		{
			// loop animation
			Y = 0;
			F = (Frame - KeyTime[X]) / (NumFrames - KeyTime[X]);
		}
	}
	else
	{
		F = (Frame - KeyTime[X]) / (KeyTime[Y] - KeyTime[X]);
	}
	unguard;
}


// not 'static', because used in ExportPsa()
void CAnimTrack::GetBonePosition(float Frame, float NumFrames, bool Loop, CVec3 &DstPos, CQuat &DstQuat) const
{
	guard(CAnimTrack::GetBonePosition);

	// fast case: 1 frame only
	if (KeyTime.Num() == 1 || NumFrames == 1 || Frame == 0)
	{
		if (KeyPos.Num())  DstPos  = KeyPos[0];
		if (KeyQuat.Num()) DstQuat = KeyQuat[0];
		return;
	}

	// data for lerping
	int posX, rotX;			// index of previous frame
	int posY, rotY;			// index of next frame
	float posF, rotF;		// fraction between X and Y for lerping

	int NumTimeKeys = KeyTime.Num();
	int NumPosKeys  = KeyPos.Num();
	int NumRotKeys  = KeyQuat.Num();

	if (NumTimeKeys)
	{
		// here: KeyPos and KeyQuat sizes either equals to 1 or equals to KeyTime size
		assert(NumPosKeys <= 1 || NumPosKeys == NumTimeKeys);
		assert(NumRotKeys == 1 || NumRotKeys == NumTimeKeys);

		GetKeyParams(KeyTime, Frame, NumFrames, Loop, posX, posY, posF);
		rotX = posX;
		rotY = posY;
		rotF = posF;

		if (NumPosKeys <= 1)
		{
			posX = posY = 0;
			posF = 0;
		}
		if (NumRotKeys == 1)
		{
			rotX = rotY = 0;
			rotF = 0;
		}
	}
	else
	{
		// empty KeyTime array - keys are evenly spaced on a time line
		// note: KeyPos and KeyQuat sizes can be different
		if (KeyPosTime.Num())
		{
			GetKeyParams(KeyPosTime, Frame, NumFrames, Loop, posX, posY, posF);
		}
		else if (NumPosKeys > 1)
		{
			float Position = Frame / NumFrames * NumPosKeys;
			posX = appFloor(Position);
			posF = Position - posX;
			posY = posX + 1;
			if (posY >= NumPosKeys)
			{
				if (!Loop)
				{
					posY = NumPosKeys - 1;
					posF = 0;
				}
				else
					posY = 0;
			}
		}
		else
		{
			posX = posY = 0;
			posF = 0;
		}

		if (KeyQuatTime.Num())
		{
			GetKeyParams(KeyQuatTime, Frame, NumFrames, Loop, rotX, rotY, rotF);
		}
		else if (NumRotKeys > 1)
		{
			float Position = Frame / NumFrames * NumRotKeys;
			rotX = appFloor(Position);
			rotF = Position - rotX;
			rotY = rotX + 1;
			if (rotY >= NumRotKeys)
			{
				if (!Loop)
				{
					rotY = NumRotKeys - 1;
					rotF = 0;
				}
				else
					rotY = 0;
			}
		}
		else
		{
			rotX = rotY = 0;
			rotF = 0;
		}
	}

	// get position
	if (posF > 0)
		Lerp(KeyPos[posX], KeyPos[posY], posF, DstPos);
	else if (NumPosKeys)		// do not change DstPos when no keys
		DstPos = KeyPos[posX];
	// get orientation
	if (rotF > 0)
		Slerp(KeyQuat[rotX], KeyQuat[rotY], rotF, DstQuat);
	else if (NumRotKeys)		// do not change DstQuat when no keys
		DstQuat = KeyQuat[rotX];

	unguard;
}
