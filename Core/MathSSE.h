#ifndef __MATH_SSE_H__
#define __MATH_SSE_H__

#include <xmmintrin.h>

struct CVec4
{
	union
	{
		__m128		mm;
		float		v[4];
	};

	// access to data
	inline float& operator[](int index)
	{
		return v[index];
	}
	inline const float& operator[](int index) const
	{
		return v[index];
	}

	FORCEINLINE void Set(const CVec3 &src)
	{
		* (CVec3*)this = src;
		v[3] = 0;
	}

	FORCEINLINE CVec4& operator=(const CVec3 &src)
	{
		Set(src);
		return *this;
	}

	FORCEINLINE operator CVec3&()
	{
		return *(CVec3*)this;
	}

	FORCEINLINE operator const CVec3&() const
	{
		return *(CVec3*)this;
	}

	FORCEINLINE CVec3& ToVec3()
	{
		return (CVec3&)v;
	}

	FORCEINLINE const CVec3& ToVec3() const
	{
		return (CVec3&)v;
	}

	FORCEINLINE void Negate()
	{
#if 1
		__m128 zero = _mm_setzero_ps();
		mm = _mm_sub_ps(zero, mm);
#else
		ToVec3().Negate();
#endif
	}

	FORCEINLINE void Scale(float scale)
	{
#if 1
		__m128 s = _mm_load1_ps(&scale);
		mm = _mm_mul_ps(mm, s);
#else
		ToVec3().Scale(scale);
#endif
	}

	FORCEINLINE void Normalize()
	{
		ToVec3().Normalize();
	}
};


FORCEINLINE void VectorSubtract(const CVec4 &a, const CVec4 &b, CVec4 &d)
{
#if 1
	d.mm = _mm_sub_ps(a.mm, b.mm);
#else
	VectorSubtract(a.ToVec3(), b.ToVec3(), d.ToVec3());
#endif
}

FORCEINLINE void VectorSubtract(const CVec4 &a, const CVec4 &b, CVec3 &d)
{
#if 1
	CVec4 r;
	r.mm = _mm_sub_ps(a.mm, b.mm);
	d = r.ToVec3();
#else
	VectorSubtract(a.ToVec3(), b.ToVec3(), d);
#endif
}

FORCEINLINE void VectorMA(const CVec4 &a, float scale, const CVec4 &b, CVec4 &d)
{
#if 1
	__m128 s = _mm_load1_ps(&scale);
	s    = _mm_mul_ps(s, b.mm);
	d.mm = _mm_add_ps(s, a.mm);
#else
	VectorMA(a.ToVec3(), scale, b.ToVec3(), d.ToVec3());
#endif
}

FORCEINLINE void VectorMA(const CVec4 &a, float scale, const CVec4 &b, CVec3 &d)
{
#if 1
	CVec4 r;
	VectorMA(a, scale, b, r);
	d = r.ToVec3();
#else
	VectorMA(a.ToVec3(), scale, b.ToVec3(), d);
#endif
}

FORCEINLINE void Lerp(const CVec4 &A, const CVec4 &B, float Alpha, CVec4 &dst)
{
#if 1
	__m128 d = _mm_sub_ps(B.mm, A.mm);
	__m128 a = _mm_load1_ps(&Alpha);
	d        = _mm_mul_ps(a, d);
	dst.mm   = _mm_add_ps(d, A.mm);
#else
	Lerp(A.ToVec3(), B.ToVec3(), Alpha, dst.ToVec3());
#endif
}

FORCEINLINE float dot(const CVec4 &a, const CVec4 &b)
{
	return dot(a.ToVec3(), b.ToVec3());
}

FORCEINLINE void cross(const CVec4 &v1, const CVec4 &v2, CVec4 &result)
{
#if 1
	__m128 A_YZXW = _mm_shuffle_ps(v1.mm, v1.mm, _MM_SHUFFLE(3,0,2,1));
	__m128 B_ZXYW = _mm_shuffle_ps(v2.mm, v2.mm, _MM_SHUFFLE(3,1,0,2));
	__m128 A_ZXYW = _mm_shuffle_ps(v1.mm, v1.mm, _MM_SHUFFLE(3,1,0,2));
	__m128 B_YZXW = _mm_shuffle_ps(v2.mm, v2.mm, _MM_SHUFFLE(3,0,2,1));
	result.mm = _mm_sub_ps(_mm_mul_ps(A_YZXW,B_ZXYW), _mm_mul_ps(A_ZXYW, B_YZXW));
#else
	cross(v1.ToVec3(), v2.ToVec3(), result.ToVec3());
#endif
}

FORCEINLINE void cross(const CVec4 &v1, const CVec4 &v2, CVec3 &result)
{
	cross(v1.ToVec3(), v2.ToVec3(), result);
}


// identical to Matrix 4x4
struct CCoords4
{
	__m128		mm[4];

	void Set(const CCoords &src)
	{
		float *f = (float*)&(mm[0]);
		* (CVec3*)&mm[0] = src.axis[0];
		* (CVec3*)&mm[1] = src.axis[1];
		* (CVec3*)&mm[2] = src.axis[2];
		* (CVec3*)&mm[3] = src.origin;
		f[3] = f[7] = f[11] = f[15] = 0;
	}
};


// Byte unpacking functions.
// http://stackoverflow.com/questions/12121640/how-to-load-a-pixel-struct-into-an-sse-register

// Unpack char[4] (int32) in range -127..+127 -> float[4] (__m128) in range -1..+1
FORCEINLINE __m128 UnpackPackedChars(unsigned Packed)
{
	__m128i r = _mm_cvtsi32_si128(Packed);			// read 32-bit int to lower part of XMM register - ABCD.0000.0000.0000
	r = _mm_unpacklo_epi8(r, r);					// interleave bytes with themselves - AABB.CCDD.0000.0000
	r = _mm_unpacklo_epi16(r, r);					// interleave words with themselves - AAAA.BBBB.CCCC.DDDD
	r = _mm_srai_epi32(r, 24);						// arithmetical shift right by 24 bits, i.e. sign extend
	__m128 r2 = _mm_cvtepi32_ps(r);					// convert to floats
	static const __m128 scale = { 1.0f / 127, 1.0f / 127, 1.0f / 127, 1.0f / 127 };
	return _mm_mul_ps(r2, scale);
}

// Unpack byte[4] (int32) in range 0..255 -> float[4] (__m128) in range 0..+1
FORCEINLINE __m128 UnpackPackedBytes(unsigned Packed)
{
	__m128i r = _mm_cvtsi32_si128(Packed);			// read 32-bit int to lower part of XMM register - ABCD.0000.0000.0000
	r = _mm_unpacklo_epi8(r, _mm_setzero_si128());	// interleave bytes with zeros - 0A0B.0C0D.0000.0000
	r = _mm_unpacklo_epi16(r, _mm_setzero_si128());	// interleave words with themselves - 000A.000B.000C.000D
	__m128 r2 = _mm_cvtepi32_ps(r);					// convert to floats
	static const __m128 scale = { 1.0f / 255, 1.0f / 255, 1.0f / 255, 1.0f / 255 };
	return _mm_mul_ps(r2, scale);
}

#endif // __MATH_SSE_H__
