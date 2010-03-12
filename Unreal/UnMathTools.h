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

#endif // __UNMATHTOOLS_H__
