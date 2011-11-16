#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"		// for typeinfo
#include "SkeletalMesh.h"


/*-----------------------------------------------------------------------------
	CSkeletalMesh
-----------------------------------------------------------------------------*/

//!! code of this function is similar to BuildNormals() from SkelMeshInstance.cpp
void CSkelMeshLod::BuildTangents()
{
	guard(CSkelMeshLod::BuildTangents);

	if (HasTangents) return;

	int i, j;

	CIndexBuffer::IndexAccessor_t Index = Indices.GetAccessor();
	for (i = 0; i < Indices.Num() / 3; i++)
	{
		CSkelMeshVertex *V[3];		//!!
		for (j = 0; j < 3; j++)
		{
			int idx = Index(Indices, i * 3 + j);
			V[j] = &Verts[idx];
		}

		// compute tangent
		CVecT tang;
		float U0 = V[0]->UV[0].U;
		float V0 = V[0]->UV[0].V;
		float U1 = V[1]->UV[0].U;
		float V1 = V[1]->UV[0].V;
		float U2 = V[2]->UV[0].U;
		float V2 = V[2]->UV[0].V;

		if (V0 == V2)
		{
			// W[0] -> W[2] -- axis for tangent
			VectorSubtract(V[2]->Position, V[0]->Position, tang);
			// we should make tang to look in side of growing U
			if (U2 < U0) tang.Negate();
		}
		else
		{
			float pos = (V1 - V0) / (V2 - V0);			// fraction, where W[1] is placed betwen W[0] and W[2] (may be < 0 or > 1)
			CVecT tmp;
			Lerp(V[0]->Position, V[2]->Position, pos, tmp);
			VectorSubtract(tmp, V[1]->Position, tang);
			// tang.V == W[1].V; but tang.U may be greater or smaller than W[1].U
			// we should make tang to look in side of growing U
			float tU = Lerp(U0, U2, pos);
			if (tU < U1) tang.Negate();
		}
		// now, tang is on triangle plane
		// now we should place tangent orthogonal to normal, then normalize vector
		float binormalScale = 1.0f;
		for (j = 0; j < 3; j++)
		{
			CSkelMeshVertex &DW = *V[j];	//!!
			const CVecT &norm = DW.Normal;
			float pos = dot(norm, tang);
			CVecT tang2;
			VectorMA(tang, -pos, norm, tang2);
			tang2.Normalize();
			DW.Tangent = tang2;
			cross(DW.Normal, DW.Tangent, tang2);
			tang2.Normalize();
			if (j == 0)		// do this only once for triangle
			{
				// check binormal sign
				// find two points with different V
				int W1 = 0;
				int W2 = (V1 != V0) ? 1 : 2;
				// check projections of these points to binormal
				float p1 = dot(V[W1]->Position, tang2);
				float p2 = dot(V[W2]->Position, tang2);
				if ((p1 - p2) * (V[W1]->UV[0].V - V[W2]->UV[0].V) < 0)
					binormalScale = -1.0f;
			}
			tang2.Scale(binormalScale);
			DW.Binormal = tang2;
		}
	}

	HasTangents = true;

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
