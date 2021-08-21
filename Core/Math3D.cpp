#include "Core.h"

/*-----------------------------------------------------------------------------
	Constant geometry objects
-----------------------------------------------------------------------------*/

//const CBox	nullBox   = {{0,0,0}, {0,0,0}};
const CCoords identCoords = {{0, 0, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1}};

/*-----------------------------------------------------------------------------
	CVec3
-----------------------------------------------------------------------------*/

float CVec3::GetLength() const
{
	return sqrt(dot(*this, *this));
}

float CVec3::Normalize()
{
	float length = sqrt(dot(*this, *this));
	if (length)
		Scale(1.0f / length);
	return length;
}

//?? move
float VectorNormalize(const CVec3 &v, CVec3 &out)
{
	float length = sqrt(dot(v, v));
	if (length)
	{
		float ilength = 1.0f / length;
		VectorScale(v, ilength, out);
	}
	else
		out.Set(0, 0, 0);
	return length;
}

float CVec3::NormalizeFast()
{
	float len2 = dot(*this, *this);
	float denom = appRsqrt(len2);
	Scale(denom);
	return len2 * denom;
}

// source vector should be normalized
void CVec3::FindAxisVectors(CVec3 &right, CVec3 &up) const
{
	// create any vector, not collinear with "this"
	right.Set(v[2], -v[0], v[1]);
	// subtract projection on source (make vector orthogonal to source)
	VectorMA(right, -dot(right, *this), *this);
	right.Normalize();
	// generate third axis vector
	cross(right, *this, up);
}

void cross(const CVec3 &v1, const CVec3 &v2, CVec3 &result)
{
	result[0] = v1[1] * v2[2] - v1[2] * v2[1];
	result[1] = v1[2] * v2[0] - v1[0] * v2[2];
	result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

float VectorDistance(const CVec3 &vec1, const CVec3 &vec2)
{
	CVec3 vec;
	VectorSubtract(vec1, vec2, vec);
	return vec.GetLength();
}

/*-----------------------------------------------------------------------------
	CAxis
-----------------------------------------------------------------------------*/

void CAxis::FromEuler(const CVec3 &angles)
{
	CVec3 right;
	// Euler2Vecs() returns "right" instead of "y axis"
	Euler2Vecs(angles, &v[0], &right, &v[2]);
	VectorNegate(right, v[1]);
}

void CAxis::TransformVector(const CVec3 &src, CVec3 &dst) const
{
	CVec3 tmp;
	tmp[0] = dot(src, v[0]);
	tmp[1] = dot(src, v[1]);
	tmp[2] = dot(src, v[2]);
	dst = tmp;
}

void CAxis::TransformVectorSlow(const CVec3 &src, CVec3 &dst) const
{
	CVec3 tmp;
	tmp[0] = dot(src, v[0]) / v[0].GetLengthSq();
	tmp[1] = dot(src, v[1]) / v[1].GetLengthSq();
	tmp[2] = dot(src, v[2]) / v[2].GetLengthSq();
	dst = tmp;
}

void CAxis::UnTransformVector(const CVec3 &src, CVec3 &dst) const
{
	CVec3 tmp;
	VectorScale(v[0], src[0], tmp);
	VectorMA(tmp, src[1], v[1], tmp);
	VectorMA(tmp, src[2], v[2], dst);
}

void CAxis::TransformAxis(const CAxis &src, CAxis &dst) const
{
	CAxis tmp;
	TransformVector(src[0], tmp[0]);
	TransformVector(src[1], tmp[1]);
	TransformVector(src[2], tmp[2]);
	dst = tmp;
}

void CAxis::TransformAxisSlow(const CAxis &src, CAxis &dst) const
{
	//!! can speedup operation by pre-computing scales for axes (1/LenSq)
	//?? also, can speedup CCoords 'slow' functions
	CAxis tmp;
	TransformVectorSlow(src[0], tmp[0]);
	TransformVectorSlow(src[1], tmp[1]);
	TransformVectorSlow(src[2], tmp[2]);
	dst = tmp;
}

void CAxis::UnTransformAxis(const CAxis &src, CAxis &dst) const
{
	CAxis tmp;
	UnTransformVector(src[0], tmp[0]);
	UnTransformVector(src[1], tmp[1]);
	UnTransformVector(src[2], tmp[2]);
	dst = tmp;
}

void CAxis::PrescaleSource(const CVec3 &scale)
{
	v[0].Scale(scale);
	v[1].Scale(scale);
	v[2].Scale(scale);
}

/*-----------------------------------------------------------------------------
	CCoords
-----------------------------------------------------------------------------*/

void CCoords::TransformPoint(const CVec3 &src, CVec3 &dst) const
{
	CVec3 tmp;
	VectorSubtract(src, origin, tmp);
	dst[0] = dot(tmp, axis[0]);
	dst[1] = dot(tmp, axis[1]);
	dst[2] = dot(tmp, axis[2]);
}

void CCoords::TransformPointSlow(const CVec3 &src, CVec3 &dst) const
{
	CVec3 tmp;
	VectorSubtract(src, origin, tmp);
	dst[0] = dot(tmp, axis[0]) / axis[0].GetLengthSq();
	dst[1] = dot(tmp, axis[1]) / axis[1].GetLengthSq();
	dst[2] = dot(tmp, axis[2]) / axis[2].GetLengthSq();
}

void CCoords::UnTransformPoint(const CVec3 &src, CVec3 &dst) const
{
	CVec3 tmp;
	VectorMA(origin, src[0], axis[0], tmp);
	VectorMA(tmp, src[1], axis[1], tmp);
	VectorMA(tmp, src[2], axis[2], dst);
}

void CCoords::TransformCoords(const CCoords &src, CCoords &dst) const
{
	TransformPoint(src.origin, dst.origin);
	axis.TransformAxis(src.axis, dst.axis);
}

void CCoords::TransformCoordsSlow(const CCoords &src, CCoords &dst) const
{
	TransformPointSlow(src.origin, dst.origin);
	axis.TransformAxisSlow(src.axis, dst.axis);
}

void CCoords::UnTransformCoords(const CCoords &src, CCoords &dst) const
{
	UnTransformPoint(src.origin, dst.origin);
	axis.UnTransformAxis(src.axis, dst.axis);
}

void TransformPoint(const CVec3 &origin, const CAxis &axis, const CVec3 &src, CVec3 &dst)
{
	CVec3 tmp;
	VectorSubtract(src, origin, tmp);
	dst[0] = dot(tmp, axis[0]);
	dst[1] = dot(tmp, axis[1]);
	dst[2] = dot(tmp, axis[2]);
}

void UnTransformPoint(const CVec3 &origin, const CAxis &axis, const CVec3 &src, CVec3 &dst)
{
	CVec3 tmp;
	VectorMA(origin, src[0], axis[0], tmp);
	VectorMA(tmp, src[1], axis[1], tmp);
	VectorMA(tmp, src[2], axis[2], dst);
}

// orthonormal coords can be inverted fast
void InvertCoords(const CCoords &S, CCoords &D)
{
	CCoords T; // to allow inplace inversion (when &D==&S)
	// negate inverse rotated origin
	S.axis.TransformVector(S.origin, T.origin);
	T.origin.Negate();
	// transpose axis
	T.axis[0][0] = S.axis[0][0];
	T.axis[0][1] = S.axis[1][0];
	T.axis[0][2] = S.axis[2][0];
	T.axis[1][0] = S.axis[0][1];
	T.axis[1][1] = S.axis[1][1];
	T.axis[1][2] = S.axis[2][1];
	T.axis[2][0] = S.axis[0][2];
	T.axis[2][1] = S.axis[1][2];
	T.axis[2][2] = S.axis[2][2];
	D = T;
}

void InvertCoordsSlow(const CCoords &S, CCoords &D)
{
	S.TransformCoordsSlow(identCoords, D);
}

void CoordsMA(CCoords &a, float scale, const CCoords &b)
{
	VectorMA(a.axis[0], scale, b.axis[0]);
	VectorMA(a.axis[1], scale, b.axis[1]);
	VectorMA(a.axis[2], scale, b.axis[2]);
	VectorMA(a.origin, scale, b.origin);
}

/*-----------------------------------------------------------------------------
	Angle math
-----------------------------------------------------------------------------*/

// algles (0,0,0) => f(1,0,0) r(0,-1,0) u(0,0,1)
void Euler2Vecs(const CVec3 &angles, CVec3 *forward, CVec3 *right, CVec3 *up)
{
	float angle;
	float sr, sp, sy, cr, cp, cy; // roll/pitch/yaw sin/cos

	if (angles[YAW])
	{
		angle = angles[YAW] * M_PI / 180;
		sy = sin(angle);
		cy = cos(angle);
	}
	else
	{
		sy = 0;
		cy = 1;
	}

	if (angles[PITCH])
	{
		angle = angles[PITCH] * M_PI / 180;
		sp = sin(angle);
		cp = cos(angle);
	}
	else
	{
		sp = 0;
		cp = 1;
	}

	if (forward)
		forward->Set(cp * cy, cp * sy, -sp);

	if (!right && !up)
		return;

	if (angles[ROLL])
	{
		angle = angles[ROLL] * M_PI / 180;
		sr = sin(angle);
		cr = cos(angle);
	}
	else
	{
		sr = 0;
		cr = 1;
	}

	if (right)
		right->Set(-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp);

	if (up)
		up->Set(cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp);
}

void Vec2Euler(const CVec3 &vec, CVec3 &angles)
{
	angles[ROLL] = 0;
	if (vec[0] == 0 && vec[1] == 0)
	{
		angles[YAW] = 0;
		angles[PITCH] = (IsNegative(vec[2])) ? 90 : 270;
	}
	else
	{
		float yaw;
		if (vec[0])
		{
			yaw = atan2(vec[1], vec[0]) * 180 / M_PI;
			if (IsNegative(yaw))
				yaw += 360;
		}
		else
			yaw = (!IsNegative(vec[1])) ? 90 : 270;

		float forward = sqrt(vec[0] * vec[0] + vec[1] * vec[1]);
		float pitch = -atan2(vec[2], forward) * 180 / M_PI;
		if (IsNegative(pitch))
			pitch += 360;

		angles[PITCH] = pitch;
		angles[YAW] = yaw;
	}
}

// short version of Vec2Euler()
float Vec2Yaw(const CVec3 &vec)
{
	if (vec[0] == 0 && vec[1] == 0)
		return 0;

	if (vec[0])
	{
		float yaw = atan2(vec[1], vec[0]) * 180 / M_PI;
		if (IsNegative(yaw))
			yaw += 360;
		return yaw;
	}
	else
		return !IsNegative(vec[1]) ? 90 : 270;
}

/*-----------------------------------------------------------------------------
	Quaternion
-----------------------------------------------------------------------------*/

void CQuat::FromAxis(const CAxis &src)
{
	float trace = src[0][0] + src[1][1] + src[2][2];

	if (trace > 0)
	{
		float t = trace + 1.0f;
		float s = appRsqrt(t) * 0.5f;

		W = s * t;
		X = (src[2][1] - src[1][2]) * s;
		Y = (src[0][2] - src[2][0]) * s;
		Z = (src[1][0] - src[0][1]) * s;
	}
	else
	{
		static const int next[3] = {1, 2, 0};
		int i = 0;
		if (src[1][1] > src[0][0])
			i = 1;
		if (src[2][2] > src[i][i])
			i = 2;
		int j = next[i];
		int k = next[j];

		float t = (src[i][i] - (src[j][j] + src[k][k])) + 1.0f;
		float s = appRsqrt(t) * 0.5f;

		float *q = (float *)this;
		q[i] = s * t;
		q[3] = (src[k][j] - src[j][k]) * s;
		q[j] = (src[j][i] + src[i][j]) * s;
		q[k] = (src[k][i] + src[i][k]) * s;
	}
}

void CQuat::ToAxis(CAxis &dst) const
{
	float x2 = X * 2;
	float y2 = Y * 2;
	float z2 = Z * 2;

	float xx = X * x2;
	float xy = X * y2;
	float xz = X * z2;

	float yy = Y * y2;
	float yz = Y * z2;
	float zz = Z * z2;

	float wx = W * x2;
	float wy = W * y2;
	float wz = W * z2;

	dst[0][0] = 1.0f - (yy + zz);
	dst[0][1] = xy - wz;
	dst[0][2] = xz + wy;

	dst[1][0] = xy + wz;
	dst[1][1] = 1.0f - (xx + zz);
	dst[1][2] = yz - wx;

	dst[2][0] = xz - wy;
	dst[2][1] = yz + wx;
	dst[2][2] = 1.0f - (xx + yy);
}

float CQuat::GetLength() const
{
	return sqrt(X * X + Y * Y + Z * Z + W * W);
}

// References:
// - UE4: UnrealMath.cpp, FindBetween_Helper()
// - http://lolengine.net/blog/2014/02/24/quaternion-from-two-vectors-final
void CQuat::FromTwoVectors(const CVec3 &v1, const CVec3 &v2)
{
	float norm_u_norm_v = sqrt(dot(v1, v1) * dot(v2, v2));
	float w = norm_u_norm_v + dot(v1, v1);

	CQuat &result = *this;
	if (w < 1e-6f * norm_u_norm_v)
	{
		// If u and v are exactly opposite, rotate 180 degrees
		// around an arbitrary orthogonal axis. Axis normalisation
		// can happen later, when we normalise the quaternion.
		w = 0.0f;
		if (fabs(v1.X) > fabs(v1.Y))
			result.Set(-v1.Z, 0.0f, v1.X, w);
		else
			result.Set(0.0f, -v1.Z, v1.Y, w);
	}
	else
	{
		// Otherwise, build quaternion the standard way.
		cross(v1, v2, (CVec3 &)result.X);
		result.W = w;
	}

	result.Normalize();
}

void CQuat::RotateVector(const CVec3 &src, CVec3 &dst) const
{
	CAxis axis;
	ToAxis(axis);
	axis.TransformVector(src, dst);
}

void CQuat::Normalize()
{
	float len = GetLength();
	if (len)
	{
		len = 1.0f / len;
		X *= len;
		Y *= len;
		Z *= len;
		W *= len;
	}
}

void CQuat::Mul(const CQuat &Q)
{
	CQuat tmp;
	tmp.X = W * Q.X + X * Q.W + Y * Q.Z - Z * Q.Y;
	tmp.Y = W * Q.Y - X * Q.Z + Y * Q.W + Z * Q.X;
	tmp.Z = W * Q.Z + X * Q.Y - Y * Q.X + Z * Q.W;
	tmp.W = W * Q.W - X * Q.X - Y * Q.Y - Z * Q.Z;
	*this = tmp;
}

void Slerp(const CQuat &A, const CQuat &B, float Alpha, CQuat &dst)
{
	// Check "Hacking Quaternions" article for fast approximated slerp

	// silly optimizations
	if (Alpha <= 0)
	{
		dst = A;
		return;
	}
	if (Alpha >= 1)
	{
		dst = B;
		return;
	}
	// can also compare A and B and return A when equals ...

	// get cosine of angle between quaternions
	float cosom = A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W;
	float sign = 1;
	if (cosom < 0)
	{
		// rotation for more than 180 degree, inverse it for better result
		cosom = -cosom;
		sign = -1;
	}
	float scaleA, scaleB;
	if (1.0f - cosom > 1e-6f)
	{
#if 1
		float f = 1.0f - cosom * cosom;
		float sinomInv = appRsqrt(f);
		float omega = atan2(f * sinomInv, fabs(cosom)); //!! slow
#else
		float omega = acos(cosom);			//!! slow
		float sinomInv = 1.0f / sin(omega); //!! slow
#endif
		scaleA = sin((1.0f - Alpha) * omega) * sinomInv; //!! slow
		scaleB = sin(Alpha * omega) * sinomInv;			 //!! slow
	}
	else
	{
		scaleA = 1.0f - Alpha;
		scaleB = Alpha;
	}
	scaleB *= sign;
	// lerp result
	dst.X = scaleA * A.X + scaleB * B.X;
	dst.Y = scaleA * A.Y + scaleB * B.Y;
	dst.Z = scaleA * A.Z + scaleB * B.Z;
	dst.W = scaleA * A.W + scaleB * B.W;
}

// Convert angle to -180...+180 range
static float NormalizeAxis(float angle)
{
	angle = fmod(angle, 360.0f);
	if (angle > 180.0f)
		angle -= 360.0f;
	return angle;
}

void Quat2Euler(const CQuat &quat, CVec3 &angles)
{
	// Reference: FQuat::Rotator() in UE4
	const float SingularityTest = quat.Z * quat.X - quat.W * quat.Y;
	const float YawY = 2.0f * (quat.W * quat.Z + quat.X * quat.Y);
	const float YawX = 1.0f - 2.0f * (quat.Y * quat.Y + quat.Z * quat.Z);

	const float SINGULARITY_THRESHOLD = 0.4999995f;
	const float RAD_TO_DEG = (180.0f) / M_PI;

	if (SingularityTest < -SINGULARITY_THRESHOLD)
	{
		angles[PITCH] = -90.0f;
		angles[YAW] = atan2(YawY, YawX) * RAD_TO_DEG;
		angles[ROLL] = NormalizeAxis(-angles[YAW] - (2.0f * atan2(quat.X, quat.W) * RAD_TO_DEG));
	}
	else if (SingularityTest > SINGULARITY_THRESHOLD)
	{
		angles[PITCH] = 90.0f;
		angles[YAW] = atan2(YawY, YawX) * RAD_TO_DEG;
		angles[ROLL] = NormalizeAxis(angles[YAW] - (2.0f * atan2(quat.X, quat.W) * RAD_TO_DEG));
	}
	else
	{
		angles[PITCH] = asin(2.0f * SingularityTest) * RAD_TO_DEG;
		angles[YAW] = atan2(YawY, YawX) * RAD_TO_DEG;
		angles[ROLL] = atan2(-2.0f * (quat.W * quat.X + quat.Y * quat.Z), (1.0f - 2.0f * (quat.X * quat.X + quat.Y * quat.Y))) * RAD_TO_DEG;
	}
}
