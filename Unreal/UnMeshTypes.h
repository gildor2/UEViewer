#ifndef __UNMESHTYPES_H__
#define __UNMESHTYPES_H__


/*-----------------------------------------------------------------------------
	Different compressed quaternion types
-----------------------------------------------------------------------------*/


struct FQuatFloat96NoW
{
	float			X, Y, Z;

	inline operator FQuat() const
	{
		FQuat r;
		r.X = X;
		r.Y = Y;
		r.Z = Z;
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;	// really have data, when wSq == -0.0f, and sqrt(wSq) was returned -INF
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatFloat96NoW &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z;
	}
};

SIMPLE_TYPE(FQuatFloat96NoW, float)


// normalized quaternion with 3 16-bit fixed point fields
struct FQuatFixed48NoW
{
	word			X, Y, Z;				// unsigned short, corresponds to (float+1)*32767

	inline operator FQuat() const
	{
		FQuat r;
		r.X = (X - 32767) / 32767.0f;
		r.Y = (Y - 32767) / 32767.0f;
		r.Z = (Z - 32767) / 32767.0f;
		// check FQuatFloat96NoW ...
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatFixed48NoW &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z;
	}
};

SIMPLE_TYPE(FQuatFixed48NoW, word)


// normalized quaternion with 11/11/10-bit fixed point fields
struct FQuatFixed32NoW
{
	unsigned		Z:10, Y:11, X:11;

	inline operator FQuat() const
	{
		FQuat r;
		r.X = X / 1023.0f - 1.0f;
		r.Y = Y / 1023.0f - 1.0f;
		r.Z = Z / 511.0f  - 1.0f;
		// check FQuatFloat96NoW ...
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatFixed32NoW &Q)
	{
		return Ar << GET_DWORD(Q);
	}
};

SIMPLE_TYPE(FQuatFixed32NoW, unsigned)


struct FQuatIntervalFixed32NoW
{
	unsigned		Z:10, Y:11, X:11;

	FQuat ToQuat(const FVector &Mins, const FVector &Ranges) const
	{
		FQuat r;
		r.X = (X / 1023.0f - 1.0f) * Ranges.X + Mins.X;
		r.Y = (Y / 1023.0f - 1.0f) * Ranges.Y + Mins.Y;
		r.Z = (Z / 511.0f  - 1.0f) * Ranges.Z + Mins.Z;
		// check FQuatFloat96NoW ...
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatIntervalFixed32NoW &Q)
	{
		return Ar << GET_DWORD(Q);
	}
};

SIMPLE_TYPE(FQuatIntervalFixed32NoW, unsigned)


#endif // __UNMESHTYPES_H__
