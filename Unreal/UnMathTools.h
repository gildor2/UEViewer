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
inline void ComputeBounds(const CVec3 *Data, int NumVerts, int Stride, CVec3 &Mins, CVec3 &Maxs)
{
	if (!NumVerts)
		return;

	Mins = Maxs = *Data;
	NumVerts--;

	while (NumVerts--)
	{
		Data = OffsetPointer(Data, Stride);
		CVec3 v = *Data;
		if (v[0] < Mins[0]) Mins[0] = v[0];
		if (v[0] > Maxs[0]) Maxs[0] = v[0];
		if (v[1] < Mins[1]) Mins[1] = v[1];
		if (v[1] > Maxs[1]) Maxs[1] = v[1];
		if (v[2] < Mins[2]) Mins[2] = v[2];
		if (v[2] > Maxs[2]) Maxs[2] = v[2];
	}
}


#endif // __UNMATHTOOLS_H__
