#include "Core.h"
#include "UnCore.h"
#include "UE4Version.h"

#if UNREAL4
#include "UnObject.h"
#include "UnPackage.h"			// for accessing FPackageFileSummary from FByteBulkData
#endif

#include <errno.h>				// not needed for VC

#if _WIN32
#include <io.h>					// for _filelengthi64
#endif

#if THREADING
#include "Parallel.h"
#endif

#define FILE_BUFFER_SIZE		4096


//#define DEBUG_BULK			1
//#define DEBUG_RAW_ARRAY		1

/*-----------------------------------------------------------------------------
	FCompactIndex
-----------------------------------------------------------------------------*/

static bool GameUsesFCompactIndex(FArchive &Ar)
{
#if UC2
	if (Ar.Engine() == GAME_UE2X && Ar.ArVer >= 145) return false;
#endif
#if VANGUARD
	if (Ar.Game == GAME_Vanguard && Ar.ArVer >= 128 && Ar.ArLicenseeVer >= 25) return false;
#endif
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) return false;
#endif
	return true;
}

FArchive& operator<<(FArchive &Ar, FCompactIndex &I)
{
#if UNREAL3
	if (Ar.Engine() >= GAME_UE3) appError("FCompactIndex is missing in UE3");
#endif
	if (Ar.IsLoading)
	{
		byte b;
		Ar << b;
		int sign  = b & 0x80;	// sign bit
		int shift = 6;
		int r     = b & 0x3F;
		if (b & 0x40)			// has 2nd byte
		{
			do
			{
				Ar << b;
				r |= (b & 0x7F) << shift;
				shift += 7;
			} while (b & 0x80);	// has more bytes
		}
		I.Value = sign ? -r : r;
	}
	else
	{
		int v = I.Value;
		byte b = 0;
		if (v < 0)
		{
			v = -v;
			b |= 0x80;			// sign
		}
		b |= v & 0x3F;
		if (v <= 0x3F)
		{
			Ar << b;
		}
		else
		{
			b |= 0x40;			// has 2nd byte
			v >>= 6;
			Ar << b;
			assert(v);
			while (v)
			{
				b = v & 0x7F;
				v >>= 7;
				if (v)
					b |= 0x80;	// has more bytes
				Ar << b;
			}
		}
	}
	return Ar;
}


/*-----------------------------------------------------------------------------
	TArray
-----------------------------------------------------------------------------*/

FArchive& FArray::Serialize(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize)
{
	int i = 0;

	guard(TArray::Serialize);

//-- if (Ar.IsLoading) Empty();	-- cleanup is done in TArray serializer (do not need
//								-- to pass array eraser/destructor to this function)
	// Here:
	// 1) when loading: 'this' array is empty (cleared from TArray's operator<<)
	// 2) when saving : data is not modified by this function

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	// perform serialization itself
	void *ptr;
	for (i = 0, ptr = DataPtr; i < Count; i++, ptr = OffsetPointer(ptr, elementSize))
		Serializer(Ar, ptr);
	return Ar;

	unguardf("%d/%d", i, DataCount);
}


struct DummyItem	// non-serializeable
{
	friend FArchive& operator<<(FArchive &Ar, DummyItem &Item)
	{
		return Ar;
	}
};

void SkipFixedArray(FArchive &Ar, int ItemSize)
{
	TArray<DummyItem> DummyArray;
	Ar << DummyArray;
	Ar.Seek(Ar.Tell() + DummyArray.Num() * ItemSize);
}

void SkipLazyArray(FArchive &Ar)
{
	guard(SkipLazyArray);
	assert(Ar.IsLoading);
	int pos;
	Ar << pos;
	assert(Ar.Tell() < pos);
	Ar.Seek(pos);
	unguard;
}

void appReverseBytes(void *Block, int NumItems, int ItemSize)
{
	byte *p1 = (byte*)Block;
	byte *p2 = p1 + ItemSize - 1;
	for (int i = 0; i < NumItems; i++, p1 += ItemSize, p2 += ItemSize)
	{
		byte *p1a = p1;
		byte *p2a = p2;
		while (p1a < p2a)
		{
			Exchange(*p1a, *p2a);
			p1a++;
			p2a--;
		}
	}
}


FArchive& FArray::SerializeRaw(FArchive &Ar, void (*Serializer)(FArchive&, void*), int elementSize)
{
	guard(TArray::SerializeRaw);

	if (Ar.ReverseBytes)	// reverse bytes -> cannot use fast serializer
		return Serialize(Ar, Serializer, elementSize);

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	if (!Count) return Ar;

	// perform serialization itself
	Ar.Serialize(DataPtr, elementSize * Count);
	return Ar;

	unguard;
}


FArchive& FArray::SerializeSimple(FArchive &Ar, int NumFields, int FieldSize)
{
	guard(TArray::SerializeSimple);

	//?? note: SerializeSimple() can reverse bytes on loading only, saving should
	//?? be done using generic serializer, or SerializeSimple should be
	//?? extended for this

	// serialize data count
	int Count = DataCount;
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(Count);
	else
		Ar << Count;

	int elementSize = NumFields * FieldSize;
	if (Ar.IsLoading)
	{
		// loading array items - should prepare array
		Empty(Count, elementSize);
		DataCount = Count;
	}
	if (!Count) return Ar;

	// perform serialization itself
	Ar.Serialize(DataPtr, elementSize * Count);
	// reverse bytes when needed
	if (FieldSize > 1 && Ar.ReverseBytes)
	{
		assert(Ar.IsLoading);
		appReverseBytes(DataPtr, Count * NumFields, FieldSize);
	}
	return Ar;

	unguard;
}


FArchive& SerializeLazyArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*))
{
	guard(TLazyArray<<);
	assert(Ar.IsLoading);
	int SkipPos = 0;								// ignored
	if (Ar.ArVer > 61)
		Ar << SkipPos;
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock && Ar.ArVer >= 131)
	{
		int f10, f8;
		Ar << f10 << f8;
//		printf("bio: pos=%08X skip=%08X f10=%08X f8=%08X\n", Ar.Tell(), SkipPos, f10, f8);
		if (SkipPos < Ar.Tell())
		{
//			appNotify("Bioshock: wrong SkipPos in array at %X", Ar.Tell());
			SkipPos = 0;		// have a few places with such bug ...
		}
	}
#endif // BIOSHOCK
	Serializer(Ar, &Array);
	return Ar;
	unguard;
}

#if UNREAL3

FArchive& SerializeBulkArray(FArchive &Ar, FArray &Array, FArchive& (*Serializer)(FArchive&, void*))
{
	guard(SerializeBulkArray);
	assert(Ar.IsLoading);
#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE) goto new_ver;
#endif
#if A51
	if (Ar.Game == GAME_A51 && Ar.ArVer >= 376) goto new_ver;	// partially upgraded old engine
#endif // A51
#if STRANGLE
	if (Ar.Game == GAME_Strangle) goto new_ver;					// also check package's MidwayTag ("WOO ") and MidwayVer (>= 369)
#endif
#if AVA
	if (Ar.Game == GAME_AVA && Ar.ArVer >= 436) goto new_ver;
#endif
#if DOH
	if (Ar.Game == GAME_DOH) goto old_ver;
#endif
#if MKVSDC
	if (Ar.Game == GAME_MK) goto old_ver;
#endif
	if (Ar.ArVer >= 453)
	{
	new_ver:
		int ElementSize;
		Ar << ElementSize;
		int SavePos = Ar.Tell();
		Serializer(Ar, &Array);
#if DEBUG_RAW_ARRAY
		appPrintf("savePos=%d count=%d elemSize=%d (real=%g) tell=%d\n", SavePos + 4, Array.Num(), ElementSize,
				Array.Num() ? float(Ar.Tell() - SavePos - 4) / Array.Num() : 0,
				Ar.Tell());
#endif
		if (Ar.Tell() != SavePos + 4 + Array.Num() * ElementSize)	// check position
			appError("RawArray item size mismatch: expected %d, serialized %d\n", ElementSize, (Ar.Tell() - SavePos) / Array.Num());
		return Ar;
	}
old_ver:
	// old version: no ElementSize property
	Serializer(Ar, &Array);
	return Ar;
	unguard;
}

void SkipBulkArrayData(FArchive &Ar, int Size)
{
	guard(SkipBulkArrayData);
	// Warning: this function has limited support for games, it works well only with
	// pure UE3 and UE4. If more games needed to be supported, should copy-paste code
	// from SerializeBulkArray(), or place it to separate function like
	// IsNewBulkArrayFormat(Ar).
#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE) goto new_ver;
#endif
	if (Ar.ArVer >= 453)
	{
	new_ver:
		int ElementSize, Count;
		Ar << ElementSize << Count;
		assert(Size == -1 || ElementSize == Size);
		Ar.Seek(Ar.Tell() + ElementSize * Count);
	}
	else
	{
		assert(Size > 0);
		int Count;
		Ar << Count;
		Ar.Seek(Ar.Tell() + Size * Count);
	}
	unguard;
}

#endif // UNREAL3


/*-----------------------------------------------------------------------------
	FString
-----------------------------------------------------------------------------*/

FArchive& operator<<(FArchive &Ar, FString &S)
{
	guard(FString<<);

	if (!Ar.IsLoading)
	{
		Ar << (TArray<char>&)S;
		return Ar;
	}

	// loading

	// serialize character count
	int32 len;
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		Ar << AR_INDEX(len);		// Bioshock serialized positive number, but it's string is always unicode
		len = -len;
	}
	else
#endif
#if VANGUARD
	if (Ar.Game == GAME_Vanguard)	// this game uses int for arrays, but FCompactIndex for strings
		Ar << AR_INDEX(len);
	else
#endif
	if (GameUsesFCompactIndex(Ar))
		Ar << AR_INDEX(len);
	else
		Ar << len;

	S.Empty((len >= 0) ? len : -len);

	// serialize the string
	if (!len)
	{
		// empty FString
		// original UE has array count == 0 and special handling when converting FString
		// to char*
		S.Data.AddZeroed(1);
		return Ar;
	}

	if (len > 0)
	{
		// ANSI string
		S.Data.AddUninitialized(len);
		Ar.Serialize(S.Data.GetData(), len);
	}
	else
	{
		// UNICODE string
		for (int i = 0; i < -len; i++)
		{
			uint16 c;
			Ar << c;
#if MASSEFF
			if (Ar.Game == GAME_MassEffect3 && Ar.ReverseBytes)	// uses little-endian strings for XBox360
				c = (c >> 8) | ((c & 0xFF) << 8);
#endif
			if (c & 0xFF00) c = '$';	//!! incorrect ...
			S.Data.Add(c & 255);		//!! incorrect ...
		}
	}
	if (S[abs(len)-1] != 0)
		appError("Serialized FString is not null-terminated");
	return Ar;

	unguard;
}


/*-----------------------------------------------------------------------------
	FArchive methods
-----------------------------------------------------------------------------*/

void FArchive::ByteOrderSerialize(void *data, int size)
{
	PROFILE_IF(size >= 1024);
	guard(FArchive::ByteOrderSerialize);

	Serialize(data, size);
	if (!ReverseBytes || size <= 1) return;

	assert(IsLoading);
	byte *p1 = (byte*)data;
	byte *p2 = p1 + size - 1;
	while (p1 < p2)
	{
		Exchange(*p1, *p2);
		p1++;
		p2--;
	}

	unguard;
}


void FArchive::Printf(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);
	Serialize(buf, len);
}


/*-----------------------------------------------------------------------------
	FFileArchive classes
-----------------------------------------------------------------------------*/

#define ArPos			SomethingBad		// guard to not use ArPos here, use ArPos64 instead

#if _WIN32

#define fopen64			fopen
#define fileno			_fileno

	#ifndef OLDCRT

	#define fseeko64		_fseeki64
	#define ftello64		_ftelli64

	#else

	// WinXP version of msvcrt.dll doesn't have _fseeki64 function
	inline int fseeko64(FILE* f, int64 offset, int whence)
	{
		assert(whence == SEEK_SET);
		fflush(f);
		return _lseeki64(fileno(f), offset, whence) == -1 ? -1 : 0;
	}

	#endif

#elif __APPLE__

	// On Darwin, all file APIs are 64-bit
	#define fopen64			fopen
	#define fseeko64		fseeko
	#define ftello64		ftell

#endif // _WIN32 / __APPLE__

FFileArchive::FFileArchive(const char *Filename, unsigned InOptions)
:	Options(InOptions)
,	f(NULL)
,	Buffer(NULL)
,	BufferSize(0)
,	BufferPos(0)
,	FilePos(0)
{
	// process the filename
	FullName = appStrdup(Filename);
	const char *s = strrchr(FullName, '/');
	if (!s)     s = strrchr(FullName, '\\');
	if (s) s++; else s = FullName;
	ShortName = s;
}

FFileArchive::~FFileArchive()
{
	appFree(const_cast<char*>(FullName));
}

int FFileArchive::GetFileSize() const
{
	int64 size = GetFileSize64();
	if (size >= (1LL << 31)) appError("GetFileSize returns 0x%llX", size); // 2Gb size restriction
	return (int)size;
}

// this function is useful only for FRO_NoOpenError mode
bool FFileArchive::IsOpen() const
{
	return (f != NULL);
}

void FFileArchive::Close()
{
	if (IsOpen())
	{
		fclose(f);
		f = NULL;
		appFree(Buffer);
		Buffer = NULL;
	}
}

bool FFileArchive::OpenFile()
{
	guard(FFileArchive::OpenFile);
	assert(!IsOpen());

	FilePos = 0;
	Buffer = (byte*)appMallocNoInit(FILE_BUFFER_SIZE);
	BufferPos = 0;
	BufferSize = 0;

	char Mode[4];
	char* s = Mode;
	*s++ = IsLoading ? 'r' : 'w';
	if (!(Options & FAO_TextFile))
	{
		*s++ = 'b';
	}
	*s++ = 0;

	f = fopen64(FullName, Mode);
	if (f) return true;			// success
	if (!(Options & FAO_NoOpenError))
	{
		appError("Can't open file (%s) %s", strerror(errno), FullName);
	}

	return false;
	unguard;
}

FFileReader::FFileReader(const char *Filename, unsigned InOptions)
:	FFileArchive(Filename, InOptions)
,	SeekPos(-1)
,	FileSize(-1)
,	BufferBytesLeft(0)
,	LocalReadPos(0)
{
	guard(FFileReader::FFileReader);
	IsLoading = true;
	Open();
	unguardf("%s", Filename);
}

FFileReader::~FFileReader()
{
	Close();
}

void FFileReader::Serialize(void *data, int size)
{
	PROFILE_IF(size >= 1024);
	guard(FFileReader::Serialize);

	assert(data);

	if (ArStopper > 0 && LocalReadPos + size > ArStopper - BufferPos)
		appError("Serializing behind stopper (%llX+%X > %X)", BufferPos + LocalReadPos, size, ArStopper);

	// The function is optimized for calling frequently with reading data from buffer
	while (size > 0)
	{
		if (BufferBytesLeft > 0)
		{
			// Use the buffer
			byte* BufferPtr = Buffer + LocalReadPos;
			int CanCopy = size > BufferBytesLeft ? BufferBytesLeft : size;
			// Copy data. If we're copying 1-2-4 bytes, "special" code works faster than the case with memcpy.
			switch (CanCopy)
			{
			case 1:
				*(byte*)data = *(BufferPtr);
				break;
			case 2:
				*(uint16*)data = *(uint16*)BufferPtr;
				break;
			case 4:
				*(uint32*)data = *(uint32*)BufferPtr;
				break;
			default:
				memcpy(data, BufferPtr, CanCopy);
			}
			// Advance pointers
			BufferBytesLeft -= CanCopy;
			data = OffsetPointer(data, CanCopy);
			size -= CanCopy;
			LocalReadPos += CanCopy;
		}
		else
		{
			// Buffer is empty
			if (SeekPos >= 0)
			{
				// Seek to desired position
				if (SeekPos != FilePos)
				{
					if (fseeko64(f, SeekPos, SEEK_SET) != 0)
						appError("Error seeking to position 0x%llX", SeekPos);
					FilePos = SeekPos;
				}
				SeekPos = -1;
			}
		#if MAX_DEBUG
			int tell = ftell(f); // msvcrt.dll doesn't have ftelli64, ftell() returns -1 when position is larger than 4Gb
			if (tell != -1 && tell != FilePos)
				appError("Bad FilePos!");
		#endif
			if (size >= FILE_BUFFER_SIZE / 2)
			{
				// Large block, read directly to destination skipping buffer
//				appPrintf("read2: %d+%d -> %d\n", (int)FilePos, size, (int)FilePos + size);
				int res = fread(data, size, 1, f);
				if (res != 1)
					appError("Unable to read %d bytes at pos=0x%llX", size, FilePos);
			#if PROFILE
				GNumSerialize++;
				GSerializeBytes += size;
			#endif
				FilePos += size;
				BufferPos = FilePos;
				// Invalidate buffer
				BufferSize = 0;
				BufferBytesLeft = 0;
				LocalReadPos = 0;
				return;
			}
			// Fill buffer
			int ReadBytes = fread(Buffer, 1, FILE_BUFFER_SIZE, f);
//			appPrintf("read: %d+%d -> %d\n", (int)FilePos, ReadBytes, (int)FilePos + ReadBytes);
			if (ReadBytes == 0)
				appError("Unable to read %d bytes at pos=0x%llX", 1, FilePos);
		#if PROFILE
			GNumSerialize++;
			GSerializeBytes += ReadBytes;
		#endif
			BufferPos = FilePos;
			BufferSize = ReadBytes;
			FilePos += ReadBytes;
			BufferBytesLeft = ReadBytes;
			LocalReadPos = 0;
		}
	}

	unguardf("File=%s", ShortName);
}

bool FFileReader::Open()
{
	return OpenFile();
}

void FFileReader::Seek(int Pos)
{
	Seek64(Pos);
}

void FFileReader::Seek64(int64 Pos)
{
//	appPrintf("seek: %d\n", (int)Pos);
	// Check for buffer validity
	int64 LocalPos64 = Pos - BufferPos;
	if (LocalPos64 < 0 || LocalPos64 >= BufferSize)
	{
		// Outside of the current buffer, invalidate it
		BufferSize = 0;
		BufferBytesLeft = 0;
		LocalReadPos = 0;
		// SeekPos will be reset to -1 after actual seek
		BufferPos = SeekPos = Pos;
	}
	else
	{
		// Inside of the buffer, recompute number of bytes to the end
		LocalReadPos = (int)LocalPos64;
		BufferBytesLeft = BufferSize - LocalReadPos;
	}
}

int FFileReader::Tell() const
{
	assert((BufferPos >> 32) == 0);
	return (int)BufferPos + LocalReadPos;
}

int64 FFileReader::Tell64() const
{
	return BufferPos + LocalReadPos;
}

int64 FFileReader::GetFileSize64() const
{
	// lazy file size computation
	if (FileSize < 0)
	{
		FFileReader* _this = const_cast<FFileReader*>(this);
#if _WIN32
		_this->FileSize = _filelengthi64(fileno(f));
#else
		fseeko64(f, 0, SEEK_END);
		_this->FileSize = ftello64(f);
		fseeko64(f, 0, FilePos);
#endif // _WIN32
	}
	return FileSize;
}

bool FFileReader::IsEof() const
{
	if (Options & FAO_TextFile)
	{
		// We're tracking file position as it returned by our read operations, however "text file" means
		// skipping "\r" characters, so position may not match.
		appError("FFileReader::IsEof is not suitable for text files (%s)", FullName);
	}
	return (BufferBytesLeft == 0) && (FilePos == GetFileSize64());
}

static TArray<FFileWriter*> GFileWriters;

#if THREADING
static CMutex GFileWritersMutex;
#endif

FFileWriter::FFileWriter(const char *Filename, unsigned InOptions)
:	FFileArchive(Filename, InOptions)
,	FileSize(0)
,	ArPos64(0)
{
	guard(FFileWriter::FFileWriter);
	IsLoading = false;
	Open();
#if THREADING
	CMutex::ScopedLock Lock(GFileWritersMutex);
#endif
	GFileWriters.Add(this);
	unguardf("%s", Filename);
}

FFileWriter::~FFileWriter()
{
#if THREADING
	CMutex::ScopedLock Lock(GFileWritersMutex);
#endif
	GFileWriters.RemoveSingle(this);
	Close();
}

void FFileWriter::CleanupOnError()
{
#if THREADING
	CMutex::ScopedLock Lock(GFileWritersMutex);
#endif
	for (int i = GFileWriters.Num() - 1; i >= 0; i--)
	{
		FFileWriter* Writer = GFileWriters[i];
		FString FileName(Writer->FullName);
		delete Writer;
		appPrintf("Deleting partially saved file %s\n", *FileName);
#if MAX_DEBUG
		char NewFileName[1024];
		appSprintf(ARRAY_ARG(NewFileName), "%s.crash", *FileName);
		rename(*FileName, NewFileName);
#else
		remove(*FileName);
#endif
	}
}

void FFileWriter::Serialize(void *data, int size)
{
	guard(FFileWriter::Serialize);

	assert(data);

	while (size > 0)
	{
		int LocalPos64 = int(ArPos64 - BufferPos);
		if (LocalPos64 < 0 || LocalPos64 >= FILE_BUFFER_SIZE || size >= FILE_BUFFER_SIZE)
		{
			// trying to write outside of buffer
			FlushBuffer();
			if (size >= FILE_BUFFER_SIZE)
			{
				// large block, write directly to file
				if (ArPos64 != FilePos)
				{
					if (fseeko64(f, ArPos64, SEEK_SET) != 0)
						appError("Error seeking to position 0x%llX", ArPos64);
//					int ret = fseeko64(f, ArPos64, SEEK_SET);
//					assert(ret == 0);
					FilePos = ArPos64;
				}
				int res = fwrite(data, size, 1, f);
				if (res != 1)
					appError("Unable to write %d bytes at pos=0x%llX", size, ArPos64);
			#if PROFILE
				GNumSerialize++;
				GSerializeBytes += size;
			#endif
				ArPos64 += size;
				FilePos += size;
				return;
			}
			BufferPos = ArPos64;
			BufferSize = 0;
			LocalPos64 = 0;
		}

		// here we have 32-bit position in buffer
		int LocalPos = (int)LocalPos64;

		// have something for buffer
		int CanCopy = FILE_BUFFER_SIZE - LocalPos;
		if (CanCopy > size) CanCopy = size;
		memcpy(Buffer + LocalPos, data, CanCopy);
		data = OffsetPointer(data, CanCopy);
		size -= CanCopy;
		ArPos64 += CanCopy;
		BufferSize = max(BufferSize, LocalPos + CanCopy);
	}

	unguardf("File=%s", ShortName);
}

bool FFileWriter::Open()
{
	assert(!IsOpen());
	Buffer = (byte*)appMallocNoInit(FILE_BUFFER_SIZE);
	BufferPos = 0;
	BufferSize = 0;
	ArPos64 = 0;
	return OpenFile();
}

void FFileWriter::Close()
{
	FlushBuffer();
	Super::Close();
}

void FFileWriter::FlushBuffer()
{
	if (BufferSize > 0)
	{
		if (BufferPos != FilePos)
		{
			int ret = fseeko64(f, BufferPos, SEEK_SET);
			assert(ret == 0);
			FilePos = BufferPos;
		}
		int res = fwrite(Buffer, BufferSize, 1, f);
		if (res != 1)
			appError("Unable to write %d bytes at pos=0x%llX", BufferSize, ArPos64);
#if PROFILE
		GNumSerialize++;
		GSerializeBytes += BufferSize;
#endif
		FilePos += BufferSize;
		BufferSize = 0;
		if (FilePos > FileSize) FileSize = FilePos;
	}
}

void FFileWriter::Seek(int Pos)
{
	ArPos64 = Pos;
}

void FFileWriter::Seek64(int64 Pos)
{
	ArPos64 = Pos;
}

int FFileWriter::Tell() const
{
	return (int)ArPos64;
}

int64 FFileWriter::Tell64() const
{
	return ArPos64;
}

int64 FFileWriter::GetFileSize64() const
{
	return max(FileSize, FilePos + BufferSize);
}

bool FFileWriter::IsEof() const
{
	return ArPos64 >= GetFileSize64();
}

#undef ArPos


/*-----------------------------------------------------------------------------
	FMemReader
-----------------------------------------------------------------------------*/

FArchive& FMemReader::operator<<(FName& N)
{
	FStaticString<256> NameString;
	*this << NameString;
	N.Str = appStrdupPool(*NameString);
	return *this;
}


/*-----------------------------------------------------------------------------
	FMemWriter
-----------------------------------------------------------------------------*/

FMemWriter::FMemWriter()
{
	IsLoading = false;
	Data = new TArray<byte>();
	Data->Empty(FILE_BUFFER_SIZE);
}

FMemWriter::~FMemWriter()
{
	delete Data;
}

void FMemWriter::Seek(int Pos)
{
	guard(FMemWriter::Seek);
	assert(Pos >= 0 && Pos <= Data->Num());
	ArPos = Pos;
	unguard;
}

bool FMemWriter::IsEof() const
{
	return ArPos >= Data->Num();
}

void FMemWriter::Serialize(void *data, int size)
{
	PROFILE_IF(size >= 1024);
	guard(FMemWriter::Serialize);
	if (ArPos + size > Data->Num())
	{
		Data->AddUninitialized(ArPos + size - Data->Num());
	}
	memcpy(Data->GetData() + ArPos, data, size);
	ArPos += size;
	unguard;
}
int FMemWriter::GetFileSize() const
{
	return Data->Num();
}


/*-----------------------------------------------------------------------------
	Reading UE3 bulk data and compressed chunks
-----------------------------------------------------------------------------*/

#if UNREAL3

FArchive& operator<<(FArchive &Ar, FCompressedChunkBlock &B)
{
#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)	// MK X
		goto int64_offsets;
#endif // MKVSDC

#if UNREAL4 || MKVSDC
	if (Ar.Game >= GAME_UE4_BASE)
	{
	int64_offsets:
		// UE4 has 64-bit values here
		int64 CompressedSize64, UncompressedSize64;
		Ar << CompressedSize64 << UncompressedSize64;
		assert((CompressedSize64 | UncompressedSize64) <= 0x7FFFFFFF); // we're using 32 bit values
		B.CompressedSize = (int)CompressedSize64;
		B.UncompressedSize = (int)UncompressedSize64;
		return Ar;
	}
#endif // UNREAL4
	return Ar << B.CompressedSize << B.UncompressedSize;
}

FArchive& operator<<(FArchive &Ar, FCompressedChunkHeader &H)
{
	guard(FCompressedChunkHeader<<);
	Ar << H.Tag;
	if (H.Tag == PACKAGE_FILE_TAG_REV)
		Ar.ReverseBytes = !Ar.ReverseBytes;

#if BERKANIX
	else if (Ar.Game == GAME_Berkanix && H.Tag == 0xF2BAC156) goto tag_ok;
#endif
#if HAWKEN
	else if (Ar.Game == GAME_Hawken && H.Tag == 0xEA31928C) goto tag_ok;
#endif
#if MMH7
	else if (/*Ar.Game == GAME_MMH7 && */ H.Tag == 0x4D4D4837) goto tag_ok;		// Might & Magic Heroes 7
#endif
#if SPECIAL_TAGS
	else if (H.Tag == 0x7E4A8BCA) goto tag_ok; // iStorm
#endif
	else
		assert(H.Tag == PACKAGE_FILE_TAG);

#if MKVSDC
	if (Ar.Game == GAME_MK && Ar.ArVer >= 677)	// MK X
		goto int64_offsets;
#endif // MKVSDC

#if UNREAL4 || MKVSDC
	if (Ar.Game >= GAME_UE4_BASE)
	{
	int64_offsets:
		// Tag and BlockSize are really FCompressedChunkBlock, which has 64-bit integers here.
		int Pad;
		int64 BlockSize64;
		Ar << Pad << BlockSize64;
		assert((Pad == 0) && (BlockSize64 <= 0x7FFFFFFF));
		H.BlockSize = (int)BlockSize64;
		goto summary;
	}
#endif // UNREAL4

tag_ok:
	Ar << H.BlockSize;

summary:
	Ar << H.Sum;
#if 0
	if (H.BlockSize == PACKAGE_FILE_TAG)
		H.BlockSize = (Ar.ArVer >= 369) ? 0x20000 : 0x8000;
	int BlockCount = (H.Sum.UncompressedSize + H.BlockSize - 1) / H.BlockSize;
	H.Blocks.Empty(BlockCount);
	H.Blocks.AddZeroed(BlockCount);
	for (int i = 0; i < BlockCount; i++)
		Ar << H.Blocks[i];
#else
	H.BlockSize = 0x20000;
	H.Blocks.Empty((H.Sum.UncompressedSize + 0x20000 - 1) / 0x20000);	// optimized for block size 0x20000
	int CompSize = 0, UncompSize = 0;
	while (CompSize < H.Sum.CompressedSize && UncompSize < H.Sum.UncompressedSize)
	{
		FCompressedChunkBlock *Block = new (H.Blocks) FCompressedChunkBlock;
		Ar << *Block;
		CompSize   += Block->CompressedSize;
		UncompSize += Block->UncompressedSize;
	}
	// check header; seen one package where sum(Block.CompressedSize) < H.CompressedSize,
	// but UncompressedSize is exact
	assert(/*CompSize == H.CompressedSize &&*/ UncompSize == H.Sum.UncompressedSize);
	if (H.Blocks.Num() > 1)
		H.BlockSize = H.Blocks[0].UncompressedSize;
#endif
	return Ar;
	unguardf("pos=%X", Ar.Tell());
}

// code is similar to FUE3ArchiveReader::PrepareBuffer()
void appReadCompressedChunk(FArchive &Ar, byte *Buffer, int Size, int CompressionFlags)
{
	guard(appReadCompressedChunk);

	// read header
	FCompressedChunkHeader ChunkHeader;
	Ar << ChunkHeader;
	// prepare buffer for reading compressed data
	int BufferSize = ChunkHeader.BlockSize * 16;
	byte *ReadBuffer = (byte*)appMallocNoInit(BufferSize);	// BlockSize is size of uncompressed data
	// read and decompress data
	for (int BlockIndex = 0; BlockIndex < ChunkHeader.Blocks.Num(); BlockIndex++)
	{
		const FCompressedChunkBlock *Block = &ChunkHeader.Blocks[BlockIndex];
		assert(Block->CompressedSize <= BufferSize);
		assert(Block->UncompressedSize <= Size);
		Ar.Serialize(ReadBuffer, Block->CompressedSize);
		appDecompress(ReadBuffer, Block->CompressedSize, Buffer, Block->UncompressedSize, CompressionFlags);
		Size   -= Block->UncompressedSize;
		Buffer += Block->UncompressedSize;
	}
	// finalize
	assert(Size == 0);			// should be comletely read
	appFree(ReadBuffer);

	unguard;
}


void FByteBulkData::SerializeHeader(FArchive &Ar)
{
	guard(FByteBulkData::SerializeHeader);

#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		guard(Bulk4);

		bIsUE4Data = true;

		Ar << BulkDataFlags;
		assert(!(BulkDataFlags & BULKDATA_Size64Bit));
		Ar << ElementCount;
		Ar << BulkDataSizeOnDisk;
		if (Ar.ArVer < VER_UE4_BULKDATA_AT_LARGE_OFFSETS)
		{
			Ar << (int&)BulkDataOffsetInFile;		// 32-bit
		}
		else
		{
			Ar << BulkDataOffsetInFile;				// 64-bit
		}
		UnPackage* Package = Ar.CastTo<UnPackage>();
		assert(Package);
	#if DEBUG_BULK
		appPrintf("pos: %X bulk %X*%d elements (flags=%X, pos=%llX+%X+pkg(%llX))\n",
			Ar.Tell(), ElementCount, GetElementSize(), BulkDataFlags, BulkDataOffsetInFile, Package->Summary.BulkDataStartOffset, BulkDataSizeOnDisk);
	#endif
		BulkDataOffsetInFile += Package->Summary.BulkDataStartOffset;
		return;

		unguard;
	}
#endif // UNREAL4

	if (Ar.ArVer < 266)
	{
		guard(OldBulkFormat);
		// old bulk format - evolution of TLazyArray
		// very old version: serialized EndPosition and ElementCount - exactly as TLazyArray
		assert(Ar.IsLoading);

		BulkDataFlags = 4;						// unknown
		BulkDataSizeOnDisk = INDEX_NONE;
		int32 EndPosition;
		Ar << EndPosition;
		if (Ar.ArVer >= 254)
			Ar << BulkDataSizeOnDisk;
		if (Ar.ArVer >= 251)
		{
			int LazyLoaderFlags;
			Ar << LazyLoaderFlags;
			assert((LazyLoaderFlags & 1) == 0);	// LLF_PayloadInSeparateFile
			if (LazyLoaderFlags & 2)
				BulkDataFlags |= BULKDATA_CompressedZlib;
		}
		if (Ar.ArVer >= 260)
		{
			FName unk;
			Ar << unk;
		}
		Ar << ElementCount;
		if (BulkDataSizeOnDisk == INDEX_NONE)
			BulkDataSizeOnDisk = ElementCount * GetElementSize();
		BulkDataOffsetInFile = Ar.Tell();
		BulkDataSizeOnDisk   = EndPosition - (int)BulkDataOffsetInFile;
		unguard;
	}
	else
	{
		// current bulk format
		// read header
		Ar << BulkDataFlags << ElementCount;
		assert(Ar.IsLoading);
		int32 tmpBulkDataOffsetInFile32;

#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 677)
		{
			// MK X has 64-bit offset and size fields
			int64 tmpBulkDataSizeOnDisk64;
			Ar << tmpBulkDataSizeOnDisk64 << BulkDataOffsetInFile;
			BulkDataSizeOnDisk = (int32)tmpBulkDataSizeOnDisk64;
			goto header_done;
		}
#endif // MKVSDC
#if BATMAN
		if (Ar.Game == GAME_Batman4 && Ar.ArLicenseeVer >= 153)
		{
			// 64-bit offset
			Ar << BulkDataSizeOnDisk << BulkDataOffsetInFile;
			goto header_done;
		}
#endif // BATMAN
#if ROCKET_LEAGUE
		if (Ar.Game == GAME_RocketLeague && Ar.ArLicenseeVer >= 20)
		{
			Ar << BulkDataSizeOnDisk;

			// Offset only serialized with BULKDATA_StoreInSeparateFile
			if (BulkDataFlags & BULKDATA_StoreInSeparateFile)
			{
				// 64-bit in LicenseeVer >= 22
				if (Ar.ArLicenseeVer >= 22)
				{
					Ar << BulkDataOffsetInFile;
				}
				else
				{
					Ar << tmpBulkDataOffsetInFile32;
					BulkDataOffsetInFile = tmpBulkDataOffsetInFile32;
				}
			}
			else
			{
				BulkDataOffsetInFile = Ar.Tell();
			}

			goto header_done;
		}
#endif // ROCKET_LEAGUE

		Ar << BulkDataSizeOnDisk << tmpBulkDataOffsetInFile32;
		BulkDataOffsetInFile = tmpBulkDataOffsetInFile32;		// sign extend to allow non-standard TFC systems which uses '-1' in this field

#if TRANSFORMERS
		if (Ar.Game == GAME_Transformers && Ar.ArLicenseeVer >= 128)
		{
			int32 BulkDataKey;
			Ar << BulkDataKey;
		}
#endif // TRANSFORMERS
	}

header_done: ;

#if MCARTA
	if (Ar.Game == GAME_MagnaCarta && (BulkDataFlags & 0x40))	// different flags
	{
		BulkDataFlags &= ~0x40;
		BulkDataFlags |= BULKDATA_CompressedLzx;
	}
#endif // MCARTA
#if APB
	if (Ar.Game == GAME_APB && (BulkDataFlags & 0x100))			// different flags
	{
		BulkDataFlags &= ~0x100;
		BulkDataFlags |= BULKDATA_SeparateData;
	}
#endif // APB

#if DEBUG_BULK
	appPrintf("pos: %X bulk %X*%d elements (flags=%X, pos=%llX+%X)\n",
		Ar.Tell(), ElementCount, GetElementSize(), BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif

	unguard;
}


void FByteBulkData::Serialize(FArchive &Ar)
{
	guard(FByteBulkData::Serialize);

	SerializeHeader(Ar);

	if (BulkDataFlags & BULKDATA_Unused || ElementCount == 0)	// skip serializing
	{
#if DEBUG_BULK
		appPrintf("bulk with no data\n");
#endif
		return;
	}

#if UNREAL4
	// Unreal Engine 4 code

	if (Ar.Game >= GAME_UE4_BASE)
	{
		if (BulkDataFlags & (BULKDATA_OptionalPayload|BULKDATA_PayloadInSeperateFile))
		{
#if DEBUG_BULK
			appPrintf("data in %s file (flags=%X, pos=%llX+%X)\n",
				(BulkDataFlags & BULKDATA_OptionalPayload) ? ".uptnl" : ".ubulk",
				BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif
			return;
		}
		if (BulkDataFlags & BULKDATA_PayloadAtEndOfFile)
		{
			if (BulkDataOffsetInFile + 16 >= Ar.GetFileSize64())
			{
				appPrintf("FByteBulkData::Serialize: position is outside of the file (%d bytes)\n", BulkDataSizeOnDisk);
				// Prevent any possible use of this bulk
				BulkDataFlags |= BULKDATA_Unused;
				return;
			}
			// stored in the same file, but at different position
			// save archive position
			int savePos, saveStopper;
			savePos     = Ar.Tell();
			saveStopper = Ar.GetStopper();
			// seek to data block and read data
			Ar.SetStopper(0);
			SerializeData(Ar);
			// restore archive position
			Ar.Seek(savePos);
			Ar.SetStopper(saveStopper);
			return;
		}
		if (BulkDataFlags & BULKDATA_ForceInlinePayload)
		{
			SerializeDataChunk(Ar);
			return;
		}
	}
#endif // UNREAL4

	// Unreal Engine 3 code

	if (BulkDataFlags & BULKDATA_StoreInSeparateFile)
	{
		// stored in a different file (TFC)
#if DEBUG_BULK
		appPrintf("bulk in separate file (flags=%X, pos=%llX+%X)\n", BulkDataFlags, BulkDataOffsetInFile, BulkDataSizeOnDisk);
#endif
		return;
	}

	if (BulkDataFlags & BULKDATA_SeparateData)
	{
		// stored in the same file, but at different position
		// save archive position
		int savePos, saveStopper;
		savePos     = Ar.Tell();
		saveStopper = Ar.GetStopper();
		// seek to data block and read data
		Ar.SetStopper(0);
		SerializeData(Ar);
		// restore archive position
		Ar.Seek(savePos);
		Ar.SetStopper(saveStopper);
		return;
	}

#if TRANSFORMERS
	// PS3 sounds in Transformers has alignment to 0x8000 with filling zeros
	if (Ar.Game == GAME_Transformers && Ar.Platform == PLATFORM_PS3)
		Ar.Seek64(BulkDataOffsetInFile);
#endif

	if (ElementCount > 0)
	{
//		assert(BulkDataOffsetInFile == Ar.Tell());
		SerializeData(Ar);
	}

	unguard;
}


// Serialize only header, and skip data block if it is inline
void FByteBulkData::Skip(FArchive &Ar)
{
	guard(FByteBulkData::Skip);

	SerializeHeader(Ar);

	if (BulkDataFlags & BULKDATA_Unused)
	{
		return;
	}

#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		if (BulkDataFlags & (BULKDATA_PayloadInSeperateFile | BULKDATA_PayloadAtEndOfFile))
		{
			return;
		}
		if (BulkDataFlags & BULKDATA_ForceInlinePayload)
		{
			Ar.Seek64(Ar.Tell64() + BulkDataSizeOnDisk);
			return;
		}
	}
#endif // UNREAL4

	if (BulkDataOffsetInFile == Ar.Tell64())
	{
		// really should check flags here, but checking position is simpler
		Ar.Seek64(Ar.Tell64() + BulkDataSizeOnDisk);
	}

	unguard;
}


void FByteBulkData::SerializeData(FArchive &Ar)
{
	guard(FByteBulkData::SerializeData);

	assert(!(BulkDataFlags & BULKDATA_Unused));

	// serialize data block
#if UNREAL4
	if (Ar.Game >= GAME_UE4_BASE)
	{
		if (!Ar.IsCompressed()) goto serialize_separate_data;

		// UE4 compressed packages use uncompressed position for bulk data
		/// reference: FUntypedBulkData::LoadDataIntoMemory

		// open new FArchive for the current file
		UnPackage* Package = Ar.CastTo<UnPackage>();
		assert(Package);
		//!! should make the following code as separate function
		const CGameFileInfo* info = Package->FileInfo;
		FArchive* loader = NULL;
		if (info)
		{
			loader = info->CreateReader();
			assert(loader);
		}
		else
		{
			loader = new FFileReader(*Package->GetFilename());
		}
		loader->Game = Ar.Game;

		loader->Seek64(BulkDataOffsetInFile);
		SerializeDataChunk(*loader);
		delete loader;
	}
	else
#endif // UNREAL4
	{
		if (BulkDataFlags & (BULKDATA_SeparateData | BULKDATA_StoreInSeparateFile))
		{
		serialize_separate_data:
			Ar.Seek64(BulkDataOffsetInFile);
			SerializeDataChunk(Ar);
			if (BulkDataOffsetInFile + BulkDataSizeOnDisk != Ar.Tell64())
			{
				// At least Special Force 2 has this situation with correct data - perhaps BulkDataSizeOnDisk is wrong there.
				// Let's spam, but don't crash.
				appNotify("Serialize bulk data: current position %llX, expected %llX", Ar.Tell64(), BulkDataOffsetInFile + BulkDataSizeOnDisk);
			}
		}
		else
		{
			// no seeks, so ignore any offset differences when BULKDATA_SeparateData is not set (i.e. no assertions)
			SerializeDataChunk(Ar);
		}
	}

	unguard;
}

void FByteBulkData::SerializeDataChunk(FArchive &Ar)
{
	guard(FByteBulkData::SerializeDataChunk);

	// allocate array
	if (BulkData) appFree(BulkData);
	BulkData = NULL;
	int DataSize = ElementCount * GetElementSize();
	if (!DataSize) return;		// nothing to serialize
	BulkData = (byte*)appMallocNoInit(DataSize);

	if (BulkDataFlags & (BULKDATA_CompressedLzo | BULKDATA_CompressedZlib | BULKDATA_CompressedLzx))
	{
		// compressed block
		int flags = 0;
		if (BulkDataFlags & BULKDATA_CompressedZlib) flags = COMPRESS_ZLIB;
		if (BulkDataFlags & BULKDATA_CompressedLzo)  flags = COMPRESS_LZO;
		if (BulkDataFlags & BULKDATA_CompressedLzx)  flags = COMPRESS_LZX;
		appReadCompressedChunk(Ar, BulkData, DataSize, flags);
	}
#if BLADENSOUL
	else if (Ar.Game == GAME_BladeNSoul && (BulkDataFlags & BULKDATA_CompressedLzoEncr))
	{
		appReadCompressedChunk(Ar, BulkData, DataSize, COMPRESS_LZO_ENC_BNS);
	}
#endif
	else
	{
		// uncompressed block
		Ar.Serialize(BulkData, DataSize);
	}

	unguard;
}

bool FByteBulkData::SerializeData(const UObject* MainObj) const
{
#if UNREAL4
	guard(FByteBulkData::SerializeData(UObject*));

	assert(bIsUE4Data); // the function is supported only for UE4 games

	if (!(BulkDataFlags & (BULKDATA_OptionalPayload|BULKDATA_PayloadInSeperateFile)))
	{
		// Already serialized, see FByteBulkData::Serialize()
		assert(CanReloadBulk() == false);
		return true;
	}

	assert(CanReloadBulk() == true);

	char bulkFileName[256];
	bulkFileName[0] = 0;

	const UnPackage* Package = MainObj->Package;

	strcpy(bulkFileName, *Package->GetFilename());
	//!! check for presence of BULKDATA_PayloadAtEndOfFile flag
	if (BulkDataFlags & (BULKDATA_OptionalPayload|BULKDATA_PayloadInSeperateFile))
	{
		// UE4.12+ store bulk payload in .ubulk file (BULKDATA_PayloadInSeperateFile)
		// UE4.20+ store bulk payload in .uptnl file (BULKDATA_OptionalPayload)
		// It seems UE4 may store both flags, but priority is to BULKDATA_OptionalPayload.
		char* s = strrchr(bulkFileName, '.');
		assert(s);
		strcpy(s, (BulkDataFlags & BULKDATA_OptionalPayload) ? ".uptnl" : ".ubulk");
	}

	const CGameFileInfo* bulkFile = CGameFileInfo::Find(bulkFileName);
	if (!bulkFile)
	{
		appPrintf("FByteBulkData %s: file %s is missing\n", MainObj->Name, bulkFileName);
		return false;
	}

	FArchive *Ar = bulkFile->CreateReader();
	Ar->SetupFrom(*Package);
#if DEBUG_BULK
	appPrintf("%s: Bulk %X %llX [%d] f=%X (%s)\n", MainObj->Name, this, this->BulkDataOffsetInFile, this->ElementCount, this->BulkDataFlags, bulkFileName);
#endif
	const_cast<FByteBulkData*>(this)->SerializeData(*Ar);
	delete Ar;
	return true;

	unguard;
#else
	appError("FByteBulkData::SerializeData(UObject*) call");
	return false;
#endif // UNREAL4
}


#endif // UNREAL3
