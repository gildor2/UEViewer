#ifndef __UNMATH_TOOLS_H__
#define __UNMATH_TOOLS_H__

#include "MeshCommon.h"

inline void RotatorToAxis(const FRotator& Rot, CAxis& Axis)
{
	CVec3 angles;
	angles[YAW]   = Rot.Yaw   * (180 / 32768.0f);
	angles[PITCH] = Rot.Pitch * (180 / 32768.0f);
	angles[ROLL]  = Rot.Roll  * (180 / 32768.0f);
	Axis.FromEuler(angles);
	// X
	Axis[0][2] *= -1;
	// Y
	Axis[1][2] *= -1;
	// Z
	Axis[2][0] *= -1;
	Axis[2][1] *= -1;
//	printf("Rotator %d %d %d -> axis\n", Rot.Pitch, Rot.Yaw, Rot.Roll);
//	printf("%7.3f %7.3f %7.3f\n%7.3f %7.3f %7.3f\n%7.3f %7.3f %7.3f\n", VECTOR_ARG(Axis[0]), VECTOR_ARG(Axis[1]), VECTOR_ARG(Axis[2]));
}

inline void AxisToRotator(const CAxis& Axis, FRotator& Rot)
{
//	printf("src:\n");
//	printf("%7.3f %7.3f %7.3f\n%7.3f %7.3f %7.3f\n%7.3f %7.3f %7.3f\n", VECTOR_ARG(Axis[0]), VECTOR_ARG(Axis[1]), VECTOR_ARG(Axis[2]));

	CVec3 angles;
	Vec2Euler(Axis[0], angles);
	Rot.Yaw   =  angles[YAW]   / (180 / 32768.0f);
	Rot.Pitch = -angles[PITCH] / (180 / 32768.0f);
	Rot.Roll  =  0;
	// Now compute ROLL
	CAxis S;
	RotatorToAxis(Rot, S);
	Rot.Roll = atan2(dot(Axis[2], S[1]), dot(Axis[1], S[1])) / M_PI * 32768.0f;
	// Normalize value
	if (Rot.Yaw < 0)   Rot.Yaw   += 65536;
	if (Rot.Pitch < 0) Rot.Pitch += 65536;
	if (Rot.Roll < 0)  Rot.Roll  += 65536;
//	printf("r=%d %d %d\n", Rot.Pitch, Rot.Yaw, Rot.Roll);
}


//?? not inline
//?? pass CBox, place function into Core/Math32.h
//?? T = CVec3 or CVec4; CVec4 cound be SSE-optimized! make vmin(V1,V2) and vmax() functions for CVec3 and CVec4
//?? Another solution: use CVec3 only, cast CVec4 to CVec3 and pass correct Stride here - it would work
template<class T>
inline void ComputeBounds(const T *Data, int NumVerts, int Stride, CVec3 &Mins, CVec3 &Maxs, bool UpdateBounds = false)
{
	if (!NumVerts)
	{
		if (!UpdateBounds)
		{
			Mins.Set(0, 0, 0);
			Maxs.Set(0, 0, 0);
		}
		return;
	}

	if (!UpdateBounds)
	{
		Mins = Maxs = *Data;
		Data = OffsetPointer(Data, Stride);
		NumVerts--;
	}

	while (NumVerts--)
	{
		T v = *Data;
		Data = OffsetPointer(Data, Stride);
		if (v[0] < Mins[0]) Mins[0] = v[0];
		if (v[0] > Maxs[0]) Maxs[0] = v[0];
		if (v[1] < Mins[1]) Mins[1] = v[1];
		if (v[1] > Maxs[1]) Maxs[1] = v[1];
		if (v[2] < Mins[2]) Mins[2] = v[2];
		if (v[2] > Maxs[2]) Maxs[2] = v[2];
	}
}


#define USE_HASHING	1		// can disable this to compare performance

// structure which helps to share vertices between wedges
//?? rename to "CVertexWelder"?
struct CVertexShare
{
	TArray<CVec3>	Points;
	TArray<CPackedNormal> Normals;
	TArray<uint32>	ExtraInfos;
	TArray<int>		WedgeToVert;
	TArray<int>		VertToWedge;
	int				WedgeIndex;

#if USE_HASHING
	// hashing
	CVec3			Mins, Maxs;
	CVec3			Extents;
	int				Hash[16384];
	TArray<int>		HashNext;
#endif // USE_HASHING

	void Prepare(const CMeshVertex *Verts, int NumVerts, int VertexSize)
	{
		WedgeIndex = 0;
		Points.Empty(NumVerts);
		Normals.Empty(NumVerts);
		ExtraInfos.Empty(NumVerts);
		WedgeToVert.Empty(NumVerts);
		VertToWedge.Empty(NumVerts);
		VertToWedge.AddZeroed(NumVerts);
#if USE_HASHING
		// compute bounds for better hashing
		ComputeBounds(&Verts->Position, NumVerts, VertexSize, Mins, Maxs);
		VectorSubtract(Maxs, Mins, Extents);
		Extents[0] += 1; Extents[1] += 1; Extents[2] += 1; // avoid zero divide
		// initialize Hash and HashNext with -1
		HashNext.Init(-1, NumVerts);
		memset(Hash, -1, sizeof(Hash));
#endif // USE_HASHING
	}

	int AddVertex(const CVec3 &Pos, CPackedNormal Normal, uint32 ExtraInfo = 0)
	{
		int PointIndex = -1;

		Normal.Data &= 0xFFFFFF;		// clear W component which is used for binormal computation

#if USE_HASHING
		// compute hash
		int h = appFloor(
			( (Pos[0] - Mins[0]) / Extents[0] + (Pos[1] - Mins[1]) / Extents[1] + (Pos[2] - Mins[2]) / Extents[2] ) // 0..3
			* (ARRAY_COUNT(Hash) / 3.0f * 16)			// multiply to 16 for better spreading inside Hash array
		) % ARRAY_COUNT(Hash);
		// find point with the same position and normal
		for (PointIndex = Hash[h]; PointIndex >= 0; PointIndex = HashNext[PointIndex])
		{
			if (Points[PointIndex] == Pos && Normals[PointIndex] == Normal && ExtraInfos[PointIndex] == ExtraInfo)
				break;		// found it
		}
#else
		// find wedge with the same position and normal
		while (true)
		{
			PointIndex = Points.FindItem(Pos, PointIndex + 1);
			if (PointIndex == INDEX_NONE) break;
			if (Normals[PointIndex] == Normal && ExtraInfos[PointIndex] == ExtraInfo) break;
		}
#endif // USE_HASHING
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.Add(Pos);
			Normals.Add(Normal);
			ExtraInfos.Add(ExtraInfo);
#if USE_HASHING
			// add to Hash
			HashNext[PointIndex] = Hash[h];
			Hash[h] = PointIndex;
#endif // USE_HASHING
		}

		// remember vertex <-> wedge map
		WedgeToVert.Add(PointIndex);
		VertToWedge[PointIndex] = WedgeIndex++;

		return PointIndex;
	}
};

#endif // __UNMATH_TOOLS_H__
