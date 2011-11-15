#ifndef __UNMATHTOOLS_H__
#define __UNMATHTOOLS_H__

inline void SetAxis(const FRotator &Rot, CAxis &Axis)
{
	CVec3 angles;
	//?? check: swapped pitch and roll ?
	angles[YAW]   = -Rot.Yaw   / 32768.0f * 180;
	angles[ROLL]  = -Rot.Pitch / 32768.0f * 180;
	angles[PITCH] = -Rot.Roll  / 32768.0f * 180;
	Axis.FromEuler(angles);
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


#endif // __UNMATHTOOLS_H__
