#include "Core.h"
#include "UnrealClasses.h"
#include "UnMesh2.h"
#include "UnMeshTypes.h"

#include "SkeletalMesh.h"
#include "TypeConvert.h"


/*-----------------------------------------------------------------------------
	UMeshAnimation class
-----------------------------------------------------------------------------*/

UMeshAnimation::~UMeshAnimation()
{
	delete ConvertedAnim;
}


void UMeshAnimation::ConvertAnims()
{
	guard(UMeshAnimation::ConvertAnims);

	int i, j;

	CAnimSet *AnimSet = new CAnimSet(this);
	ConvertedAnim = AnimSet;

	// TrackBoneNames
	int numBones = RefBones.Num();
	AnimSet->TrackBoneNames.AddUninitialized(numBones);
	for (i = 0; i < numBones; i++)
		AnimSet->TrackBoneNames[i] = RefBones[i].Name;

	// Sequences
	int numSeqs = AnimSeqs.Num();
	AnimSet->Sequences.Empty(numSeqs);
	for (i = 0; i < numSeqs; i++)
	{
		CAnimSequence &S = *new CAnimSequence;
		AnimSet->Sequences.Add(&S);

		const FMeshAnimSeq &Src = AnimSeqs[i];
		const MotionChunk  &M   = Moves[i];

		// attributes
		S.Name      = Src.Name;
		S.NumFrames = Src.NumFrames;
		S.Rate      = Src.Rate;

		// S.Tracks
		S.Tracks.AddZeroed(numBones);
		for (j = 0; j < numBones; j++)
		{
			CAnimTrack &T = S.Tracks[j];
			const AnalogTrack &A = M.AnimTracks[j];
			CopyArray(T.KeyPos,  CVT(A.KeyPos));
			CopyArray(T.KeyQuat, CVT(A.KeyQuat));
			CopyArray(T.KeyTime, A.KeyTime);
			// usually MotionChunk.TrackTime is identical to NumFrames, but some packages exists where
			// time channel should be adjusted
			if (M.TrackTime > 0)
			{
				float TimeScale = Src.NumFrames / M.TrackTime;
				for (int k = 0; k < T.KeyTime.Num(); k++)
					T.KeyTime[k] *= TimeScale;
			}
		}
	}

	unguard;
}


#if SPLINTER_CELL

// quaternion with 4 16-bit fixed point fields
struct FQuatComp
{
	int16			X, Y, Z, W;				// signed int16, corresponds to float*32767

	inline operator FQuat() const
	{
		FQuat r;
		r.X = X / 32767.0f;
		r.Y = Y / 32767.0f;
		r.Z = Z / 32767.0f;
		r.W = W / 32767.0f;
		return r;
	}
	inline operator FVector() const			// used for FixedPointTrack; may be separated to FVectorComp etc
	{
		FVector r;
		r.X = X / 64.0f;
		r.Y = Y / 64.0f;
		r.Z = Z / 64.0f;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatComp &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z << Q.W;
	}
};

SIMPLE_TYPE(FQuatComp, int16)

// normalized quaternion with 3 16-bit fixed point fields
struct FQuatComp2
{
	int16			X, Y, Z;				// signed int16, corresponds to float*32767

	inline operator FQuat() const
	{
		FQuat r;
		r.X = X / 32767.0f;
		r.Y = Y / 32767.0f;
		r.Z = Z / 32767.0f;
		// check FQuatFloat96NoW ...
		float wSq = 1.0f - (r.X*r.X + r.Y*r.Y + r.Z*r.Z);
		r.W = (wSq > 0) ? sqrt(wSq) : 0;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FQuatComp2 &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z;
	}
};

SIMPLE_TYPE(FQuatComp2, int16)

struct FVectorComp
{
	int16			X, Y, Z;

	inline operator FVector() const
	{
		FVector r;
		r.X = X / 64.0f;
		r.Y = Y / 64.0f;
		r.Z = Z / 64.0f;
		return r;
	}

	friend FArchive& operator<<(FArchive &Ar, FVectorComp &V)
	{
		return Ar << V.X << V.Y << V.Z;
	}
};

SIMPLE_TYPE(FVectorComp, int16)


#define SCELL_TRACK(Name,Quat,Pos,Time)						\
struct Name													\
{															\
	TArray<Quat>			KeyQuat;						\
	TArray<Pos>				KeyPos;							\
	TArray<Time>			KeyTime;						\
															\
	void Decompress(AnalogTrack &D)							\
	{														\
		D.Flags = 0;										\
		CopyArray(D.KeyQuat, KeyQuat);						\
		CopyArray(D.KeyPos,  KeyPos );						\
		CopyArray(D.KeyTime, KeyTime);						\
	}														\
															\
	friend FArchive& operator<<(FArchive &Ar, Name &A)		\
	{														\
		return Ar << A.KeyQuat << A.KeyPos << A.KeyTime;	\
	}														\
};

SCELL_TRACK(FixedPointTrack, FQuatComp,  FQuatComp,   uint16)
SCELL_TRACK(Quat16Track,     FQuatComp2, FVector,     uint16)	// all types are "large"
SCELL_TRACK(FixPosTrack,     FQuatComp2, FVectorComp, uint16)	// "small" KeyPos
SCELL_TRACK(FixTimeTrack,    FQuatComp2, FVector,     uint8)    // "small" KeyTime
SCELL_TRACK(FixPosTimeTrack, FQuatComp2, FVectorComp, uint8)    // "small" KeyPos and KeyTime


void AnalogTrack::SerializeSCell(FArchive &Ar)
{
	TArray<FQuatComp> KeyQuat2;
	TArray<uint16>      KeyTime2;
	Ar << KeyQuat2 << KeyPos << KeyTime2;
	// copy with conversion
	CopyArray(KeyQuat, KeyQuat2);
	CopyArray(KeyTime, KeyTime2);
}


struct MotionChunkFixedPoint
{
	FVector					RootSpeed3D;
	float					TrackTime;
	int						StartBone;
	unsigned				Flags;
	TArray<FixedPointTrack>	AnimTracks;

	friend FArchive& operator<<(FArchive &Ar, MotionChunkFixedPoint &M)
	{
		return Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags << M.AnimTracks;
	}
};

// Note: standard UE2 MotionChunk is equalent to MotionChunkCompress<AnalogTrack>
struct MotionChunkCompressBase
{
	FVector					RootSpeed3D;
	float					TrackTime;
	int						StartBone;
	unsigned				Flags;
	TArray<int>				BoneIndices;

	virtual void Decompress(MotionChunk &D)
	{
		D.RootSpeed3D = RootSpeed3D;
		D.TrackTime   = TrackTime;
		D.StartBone   = StartBone;
		D.Flags       = Flags;
	}
};

template<class T> struct MotionChunkCompress : public MotionChunkCompressBase
{
	TArray<T>				AnimTracks;
	AnalogTrack				RootTrack;		// standard track

	virtual void Decompress(MotionChunk &D)
	{
		guard(Decompress);
		MotionChunkCompressBase::Decompress(D);
		// copy/convert tracks
		CopyArray(D.BoneIndices, BoneIndices);
		int numAnims = AnimTracks.Num();
		D.AnimTracks.Empty(numAnims);
		D.AnimTracks.AddZeroed(numAnims);
		for (int i = 0; i < numAnims; i++)
			AnimTracks[i].Decompress(D.AnimTracks[i]);
		// RootTrack is unused ...
		unguard;
	}

	friend FArchive& operator<<(FArchive &Ar, MotionChunkCompress &M)
	{
		return Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags << M.BoneIndices << M.AnimTracks << M.RootTrack;
	}
};

void UMeshAnimation::SerializeSCell(FArchive &Ar)
{
	guard(SerializeSCell);

	// for logic of track decompression check UMeshAnimation::Moves() function
	int OldCompression = 0, CompressType = 0;
	TArray<MotionChunkFixedPoint>					T0;		// OldCompression!=0, CompressType=0
	TArray<MotionChunkCompress<Quat16Track> >		T1;		// CompressType=1
	TArray<MotionChunkCompress<FixPosTrack> >		T2;		// CompressType=2
	TArray<MotionChunkCompress<FixTimeTrack> >		T3;		// CompressType=3
	TArray<MotionChunkCompress<FixPosTimeTrack> >	T4;		// CompressType=4
	if (Version >= 1000)
	{
		Ar << OldCompression << T0;
		// note: this compression type is not supported (absent BoneIndices in MotionChunkFixedPoint)
	}
	if (Version >= 2000)
	{
		Ar << CompressType << T1 << T2 << T3 << T4;
		// decompress motion
		if (CompressType)
		{
			int i = 0, Count = 1;
			while (i < Count)
			{
				MotionChunkCompressBase *M = NULL;
				switch (CompressType)
				{
				case 1: Count = T1.Num(); M = &T1[i]; break;
				case 2: Count = T2.Num(); M = &T2[i]; break;
				case 3: Count = T3.Num(); M = &T3[i]; break;
				case 4: Count = T4.Num(); M = &T4[i]; break;
				default:
					appError("Unsupported CompressType: %d", CompressType);
				}
				if (!Count)
				{
					appNotify("CompressType=%d with no tracks", CompressType);
					break;
				}
				if (!i)
				{
					// 1st iteration, prepare Moves[] array
					Moves.Empty(Count);
					Moves.AddZeroed(Count);
				}
				// decompress current track
				M->Decompress(Moves[i]);
				// next track
				i++;
			}
		}
	}
//	if (OldCompression) appNotify("OldCompression=%d", OldCompression, CompressType);

	unguard;
}

#endif // SPLINTER_CELL


#if LINEAGE2

void UMeshAnimation::SerializeLineageMoves(FArchive &Ar)
{
	guard(UMeshAnimation::SerializeLineageMoves);
	if (Ar.ArVer < 123 || Ar.ArLicenseeVer < 0x19)
	{
		// standard UE2 format
		Ar << Moves;
		return;
	}
	assert(Ar.IsLoading);
	int pos, count;						// pos = global skip pos, count = data count
	Ar << pos << AR_INDEX(count);
	Moves.Empty(count);
	for (int i = 0; i < count; i++)
	{
		int localPos;
		Ar << localPos;
		MotionChunk *M = new(Moves) MotionChunk;
		Ar << *M;
		assert(Ar.Tell() == localPos);
	}
	assert(Ar.Tell() == pos);
	unguard;
}

#endif // LINEAGE2


#if SWRC

struct FVectorShortSWRC
{
	int16					X, Y, Z;

	friend FArchive& operator<<(FArchive &Ar, FVectorShortSWRC &V)
	{
		return Ar << V.X << V.Y << V.Z;
	}

	FVector ToFVector(float Scale) const
	{
		FVector r;
		float s = Scale / 32767.0f;
		r.X = X * s;
		r.Y = Y * s;
		r.Z = Z * s;
		return r;
	}

	FQuat ToFQuatOld() const							// for version older than 151
	{
		static const float s = 0.000095876726845745f;	// pi/32767
		float X2 = X * s;
		float Y2 = Y * s;
		float Z2 = Z * s;
		float tmp = sqrt(X2*X2 + Y2*Y2 + Z2*Z2);
		if (tmp > 0)
		{
			float scale = sin(tmp / 2) / tmp;			// strange code ...
			X2 *= scale;
			Y2 *= scale;
			Z2 *= scale;
		}
		float W2 = 1.0f - (X2*X2 + Y2*Y2 + Z2*Z2);
		if (W2 < 0) W2 = 0;
		else W2 = sqrt(W2);
		FQuat r;
		r.Set(X2, Y2, Z2, W2);
		return r;
	}

	FQuat ToFQuat() const
	{
		static const float s = 0.70710678118f / 32767;	// int16 -> range(sqrt(2))
		float A = int16(X & 0xFFFE) * s;
		float B = int16(Y & 0xFFFE) * s;
		float C = int16(Z & 0xFFFE) * s;
		float D = sqrt(1.0f - (A*A + B*B + C*C));
		if (Z & 1) D = -D;
		FQuat r;
		if (Y & 1)
		{
			if (X & 1)	r.Set(D, A, B, C);
			else		r.Set(C, D, A, B);
		}
		else
		{
			if (X & 1)	r.Set(B, C, D, A);
			else		r.Set(A, B, C, D);
		}
		return r;
	}
};

SIMPLE_TYPE(FVectorShortSWRC, int16)


void AnalogTrack::SerializeSWRC(FArchive &Ar)
{
	guard(AnalogTrack::SerializeSWRC);

	float					 PosScale;
	TArray<FVectorShortSWRC> PosTrack;		// scaled by PosScale
	TArray<FVectorShortSWRC> RotTrack;
	TArray<uint8>			 TimeTrack;		// frame duration

	Ar << PosScale << PosTrack << RotTrack << TimeTrack;

	// unpack data

	// time track
	int NumKeys, i;
	NumKeys = TimeTrack.Num();
	KeyTime.Empty(NumKeys);
	KeyTime.AddUninitialized(NumKeys);
	int Time = 0;
	for (i = 0; i < NumKeys; i++)
	{
		KeyTime[i] = Time;
		Time += TimeTrack[i];
	}

	// rotation track
	NumKeys = RotTrack.Num();
	KeyQuat.Empty(NumKeys);
	KeyQuat.AddUninitialized(NumKeys);
	for (i = 0; i < NumKeys; i++)
	{
		FQuat Q;
		if (Ar.ArVer >= 151) Q = RotTrack[i].ToFQuat();
		else				 Q = RotTrack[i].ToFQuatOld();
		// note: FMeshBone rotation is mirrored for ArVer >= 142
		Q.X *= -1;
		Q.Y *= -1;
		Q.Z *= -1;
		KeyQuat[i] = Q;
	}

	// translation track
	NumKeys = PosTrack.Num();
	KeyPos.Empty(NumKeys);
	KeyPos.AddUninitialized(NumKeys);
	for (i = 0; i < NumKeys; i++)
		KeyPos[i] = PosTrack[i].ToFVector(PosScale);

	unguard;
}


void UMeshAnimation::SerializeSWRCAnims(FArchive &Ar)
{
	guard(UMeshAnimation::SerializeSWRCAnims);

	// serialize TArray<FSkelAnimSeq>
	// FSkelAnimSeq is a combined (and modified) FMeshAnimSeq and MotionChunk
	// count
	int NumAnims;
	Ar << AR_INDEX(NumAnims);		// TArray.Num
	// prepare arrays
	Moves.Empty(NumAnims);
	Moves.AddZeroed(NumAnims);
	AnimSeqs.Empty(NumAnims);
	AnimSeqs.AddZeroed(NumAnims);
	// serialize items
	for (int i = 0; i < NumAnims; i++)
	{
		// serialize
		int						f50;
		int						f54;
		int						f58;
		guard(FSkelAnimSeq<<);
		Ar << AnimSeqs[i];
		int drop;
		if (Ar.ArVer < 143) Ar << drop;
		Ar << f50 << f54;
		if (Ar.ArVer >= 143) Ar << f58;
		Ar << Moves[i].AnimTracks;
		unguard;
	}

	unguard;
}

#endif // SWRC


#if UC1

struct FVectorShortUC1
{
	int16					X, Y, Z;

	friend FArchive& operator<<(FArchive &Ar, FVectorShortUC1 &V)
	{
		return Ar << V.X << V.Y << V.Z;
	}

	operator FVector() const
	{
		FVector r;
		float s = 1.0f / 100.0f;
		r.X = X * s;
		r.Y = Y * s;
		r.Z = Z * s;
		return r;
	}
};

SIMPLE_TYPE(FVectorShortUC1, int16)

struct FQuatShortUC1
{
	int16					X, Y, Z, W;

	friend FArchive& operator<<(FArchive &Ar, FQuatShortUC1 &Q)
	{
		return Ar << Q.X << Q.Y << Q.Z << Q.W;
	}

	operator FQuat() const
	{
		FQuat r;
		float s = 1.0f / 16383.0f;
		r.X = X * s;
		r.Y = Y * s;
		r.Z = Z * s;
		r.W = W * s;
		return r;
	}
};

SIMPLE_TYPE(FQuatShortUC1, int16)


void AnalogTrack::SerializeUC1(FArchive &Ar)
{
	guard(AnalogTrack::SerializeUC1);
	TArray<FQuatShortUC1>   PackedKeyQuat;
	TArray<FVectorShortUC1> PackedKeyPos;
	Ar << Flags << PackedKeyQuat << PackedKeyPos << KeyTime;
	CopyArray(KeyQuat, PackedKeyQuat);
	CopyArray(KeyPos, PackedKeyPos);
	unguard;
}

#endif // UC1


#if UC2

// Special array type ...
// This array holds data in a separate stream, these data are originally serialized with
// a single FArchive::Serialize() call and may be placed inside any memory block
// (multiple arrays may be stored in a single memory block) - so, this is a good memory
// fragmentation and loading speed optimization, plus ability to place data into GPU
// memory.
template<class T> class TRawArrayUC2 : public TArray<T>
{
	// We require "using TArray<T>::*" for gcc 3.4+ compilation
	// http://gcc.gnu.org/gcc-3.4/changes.html
	// - look for "unqualified names"
	// - "temp.dep/3" section of the C++ standard [ISO/IEC 14882:2003]
	using TArray<T>::DataCount;
	using TArray<T>::Empty;
	using TArray<T>::GetData;
public:
	void Serialize(FArchive &DataAr, FArchive &CountAr)
	{
		guard(TRawArrayUC2<<);

		assert(DataAr.IsLoading && CountAr.IsLoading);
		// serialize memory size from "CountAr"
		unsigned DataSize;
		CountAr << DataSize;
		// compute items count
		int Count = DataSize / sizeof(T);
		assert(Count * sizeof(T) == DataSize);
		// setup array
		Empty(Count);
		DataCount = Count;
		// serialize items from "DataAr"
		T* Item = GetData();
		while (Count > 0)
		{
			DataAr << *Item;
			Item++;
			Count--;
		}

		unguard;
	}
};

// helper function for RAW_ARRAY macro
template<class T> inline TRawArrayUC2<T>& ToRawArrayUC2(TArray<T> &Arr)
{
	return (TRawArrayUC2<T>&)Arr;
}

#define RAW_ARRAY_UC2(Arr)		ToRawArrayUC2(Arr)


#endif // UC2


#if UNREAL25

struct FlexTrackBase
{
	virtual ~FlexTrackBase()
	{}
	virtual void Serialize(FArchive &Ar) = 0;
#if UC2
	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		appError("FlexTrack::Serialize2() is not implemented");
	}
#endif // UC2
	virtual void Decompress(AnalogTrack &T) = 0;
};

struct FlexTrackStatic : public FlexTrackBase
{
	FQuatFloat96NoW		KeyQuat;
	TArray<FVector>		KeyPos;

	virtual void Serialize(FArchive &Ar)
	{
		Ar << KeyQuat;
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 130)
			Ar << KeyPos;
		else
#endif // UC2
		{
			FVector pos;
			Ar << pos;
			KeyPos.Add(pos);
		}
	}
#if UC2
	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		int Count;
		Ar2 << Count; assert(Count == 12);
		Ar << KeyQuat;
		RAW_ARRAY_UC2(KeyPos).Serialize(Ar, Ar2);
	}
#endif // UC2

	virtual void Decompress(AnalogTrack &T)
	{
		T.KeyQuat.Add(KeyQuat);
		CopyArray(T.KeyPos, KeyPos);
	}
};

struct FlexTrack48 : public FlexTrackBase
{
	TArray<FQuatFixed48NoW>	KeyQuat;
	TArray<int16>		KeyTime;
	TArray<FVector>		KeyPos;

	virtual void Serialize(FArchive &Ar)
	{
		Ar << KeyQuat << KeyPos << KeyTime;
	}
#if UC2
	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		RAW_ARRAY_UC2(KeyQuat).Serialize(Ar, Ar2);
		RAW_ARRAY_UC2(KeyPos).Serialize(Ar, Ar2);
		RAW_ARRAY_UC2(KeyTime).Serialize(Ar, Ar2);
	}
#endif // UC2

	virtual void Decompress(AnalogTrack &T)
	{
		CopyArray(T.KeyQuat, KeyQuat);
		CopyArray(T.KeyPos,  KeyPos );
		CopyArray(T.KeyTime, KeyTime);
	}
};

struct FlexTrack48RotOnly : public FlexTrackBase
{
	TArray<FQuatFixed48NoW>	KeyQuat;
	TArray<int16>		KeyTime;
	FVector				KeyPos;

	virtual void Serialize(FArchive &Ar)
	{
		Ar << KeyQuat << KeyTime << KeyPos;
	}

	virtual void Decompress(AnalogTrack &T)
	{
		CopyArray(T.KeyQuat, KeyQuat);
		CopyArray(T.KeyTime, KeyTime);
		T.KeyPos.Empty(1);
		T.KeyPos.Add(KeyPos);
	}
};

#if UC2

// Animation without translation (translation is from bind pose)
struct FlexTrack5 : public FlexTrackBase
{
	TArray<FQuatFixed48NoW>	KeyQuat;
	TArray<int16>		KeyTime;

	virtual void Serialize(FArchive &Ar)
	{}

	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		RAW_ARRAY_UC2(KeyQuat).Serialize(Ar, Ar2);
		RAW_ARRAY_UC2(KeyTime).Serialize(Ar, Ar2);
	}

	virtual void Decompress(AnalogTrack &T)
	{
		CopyArray(T.KeyQuat, KeyQuat);
		CopyArray(T.KeyTime, KeyTime);
	}
};

// static pose with packed quaternion
struct FlexTrack6 : public FlexTrackBase
{
	FQuatFixed48NoW		KeyQuat;
	FVector				KeyPos;

	virtual void Serialize(FArchive &Ar)
	{}

	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		int Count;
		Ar2 << Count; assert(Count == 6);
		Ar << KeyQuat;
		Ar2 << Count; assert(Count == 12);
		Ar << KeyPos;
	}

	virtual void Decompress(AnalogTrack &T)
	{
		T.KeyQuat.Add(KeyQuat);
		T.KeyPos.Add(KeyPos);
	}
};

// static pose without translation
struct FlexTrack7 : public FlexTrackBase
{
	FQuatFixed48NoW		KeyQuat;

	virtual void Serialize(FArchive &Ar)
	{}

	virtual void Serialize2(FArchive &Ar, FArchive &Ar2)
	{
		int Count;
		Ar2 << Count;
		assert(Count == 6);
		Ar << KeyQuat;
	}

	virtual void Decompress(AnalogTrack &T)
	{
		T.KeyQuat.Add(KeyQuat);
	}
};

#endif // UC2


FlexTrackBase *CreateFlexTrack(int TrackType)
{
	switch (TrackType)
	{
	case 0:
		return NULL;		// no track for this bone (bone should be in a bind pose)

	case 1:
		return new FlexTrackStatic;

//	case 2:
//		// This type uses structure with TArray<FVector>, TArray<FQuatFloat96NoW> and TArray<int16>.
//		// It's Footprint() method returns 0, GetRotPos() does nothing, but serializer is working.
//		appError("Unsupported FlexTrack type=2");

	case 3:
		return new FlexTrack48;

	case 4:
		return new FlexTrack48RotOnly;

#if UC2
	case 5:
		return new FlexTrack5;

	case 6:
		return new FlexTrack6;

	case 7:
		return new FlexTrack7;
#endif // UC2

	default:
		appError("Unknown FlexTrack type=%d", TrackType);
	}
	return NULL;
}

struct FlexTrackBasePtr
{
	FlexTrackBase* Track;

	~FlexTrackBasePtr()
	{
		if (Track) delete Track;
	}

	friend FArchive& operator<<(FArchive &Ar, FlexTrackBasePtr &T)
	{
		guard(SerializeFlexTrack);
		int TrackType;
		Ar << TrackType;
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 130 && TrackType == 4) TrackType = 3;	// replaced type: FlexTrack48RotOnly -> FlexTrack48
#endif
		T.Track = CreateFlexTrack(TrackType);
#if UC2
		if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 147) return Ar;
#endif
		if (T.Track) T.Track->Serialize(Ar);
		return Ar;
		unguard;
	}
};

// serialize TArray<FlexTrackBasePtr>
void SerializeFlexTracks(FArchive &Ar, MotionChunk &M)
{
	guard(SerializeFlexTracks);

	TArray<FlexTrackBasePtr> FT;
	Ar << FT;
	int numTracks = FT.Num();
	if (!numTracks) return;
	M.AnimTracks.Empty(numTracks);
	M.AnimTracks.AddZeroed(numTracks);
	for (int i = 0; i < numTracks; i++)
		FT[i].Track->Decompress(M.AnimTracks[i]);

	unguard;
}

#endif // UNREAL25

#if TRIBES3

void FixTribesMotionChunk(MotionChunk &M)
{
	int numBones = M.AnimTracks.Num();
	for (int i = 0; i < numBones; i++)
	{
		AnalogTrack &A = M.AnimTracks[i];
		if (A.Flags & 0x1000)
		{
			// bone overrided by Impersonator LipSinc
			// remove translation and rotation tracks (they are not correct anyway)
			A.KeyQuat.Empty();
			A.KeyPos.Empty();
			A.KeyTime.Empty();
		}
	}
}

#endif // TRIBES3


#if UC2

struct MotionChunkUC2 : public MotionChunk
{
	TArray<FlexTrackBasePtr> FlexTracks;

	friend FArchive& operator<<(FArchive &Ar, MotionChunkUC2 &M)
	{
		//?? note: can merge this structure into MotionChunk (will require FlexTrack declarations in h-file)
		guard(MotionChunkUC2<<);
		assert(Ar.ArVer >= 147);
		// start is the same, but arrays are serialized in a different way
		Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags;
		if (Ar.ArVer < 149) Ar << M.BoneIndices;
		Ar << M.AnimTracks << M.RootTrack;
		if (M.Flags >= 3)
			Ar << M.FlexTracks;
		assert(M.AnimTracks.Num() == 0 || M.FlexTracks.Num() == 0); // only one kind of tracks at a time
		assert(M.Flags != 3);				// Version == 3 has TLazyArray<FlexTrack2>

		return Ar;
		unguard;
	}
};


bool UMeshAnimation::SerializeUE2XMoves(FArchive &Ar)
{
	guard(UMeshAnimation::SerializeUE2XMoves);
	if (Ar.ArVer < 147)
	{
		// standard UE2 format
		Ar << Moves;
		return true;
	}
	assert(Ar.IsLoading);

	// read FByteBuffer
	uint8 *BufferData = NULL;
	int DataSize;
	int DataFlag;
	Ar << DataSize;
	DataFlag = 0;
	if (Ar.ArLicenseeVer == 1)
		Ar << DataFlag;
#if 0
	assert(DataFlag == 0);
#else
	if (DataFlag != 0)
	{
		guard(GetExternalAnim);
		// animation is stored in xpr file
		int Size;
		BufferData = FindXprData(va("%s_anim", Name), &Size);
		if (!BufferData)
		{
			appNotify("Missing external animations for %s", Name);
			return false;
		}
		assert(DataSize <= Size);
		appPrintf("Loading external animation for %s\n", Name);
		unguard;
	}
#endif
	if (!DataFlag && DataSize)
	{
		BufferData = (uint8*)appMalloc(DataSize);
		Ar.Serialize(BufferData, DataSize);
	}
	TArray<MotionChunkUC2> Moves2;
	Ar << Moves2;

	FMemReader Reader(BufferData, DataSize);

	// serialize Moves2 and copy Moves2 to Moves
	int numMoves = Moves2.Num();
	Moves.Empty(numMoves);
	Moves.AddZeroed(numMoves);
	for (int mi = 0; mi < numMoves; mi++)
	{
		MotionChunkUC2 &M = Moves2[mi];
		MotionChunk &DM = Moves[mi];

		// serialize AnimTracks
		int numATracks = M.AnimTracks.Num();
		if (numATracks)
		{
			DM.AnimTracks.AddZeroed(numATracks);
			for (int ti = 0; ti < numATracks; ti++)
			{
				AnalogTrack &A = DM.AnimTracks[ti];
				RAW_ARRAY_UC2(A.KeyQuat).Serialize(Reader, Ar);
				RAW_ARRAY_UC2(A.KeyPos).Serialize(Reader, Ar);
				RAW_ARRAY_UC2(A.KeyTime).Serialize(Reader, Ar);
			}
		}
		// "serialize" RootTrack
		int i1, i2, i3;
		Ar << i1 << i2 << i3;				// KeyQuat, KeyPos and KeyTime
		assert(i1 == 0 && i2 == 0 && i3 == 0);
		// serialize FlexTracks
		int numFTracks = M.FlexTracks.Num();
		if (numFTracks)
		{
			DM.AnimTracks.AddZeroed(numFTracks);
			for (int ti = 0; ti < numFTracks; ti++)
			{
				FlexTrackBase *Track = M.FlexTracks[ti].Track;
				if (Track)
				{
					Track->Serialize2(Reader, Ar);
					Track->Decompress(DM.AnimTracks[ti]);
				}
//				else -- keep empty track, will be ignored by animation system
			}
		}
	}
	assert(Reader.GetStopper() == Reader.Tell());

	// cleanup
	if (BufferData) appFree(BufferData);

	return true;

	unguard;
}

#endif // UC2
