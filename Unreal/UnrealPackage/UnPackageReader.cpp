#include "Core.h"

#include "UnCore.h"
#include "UnObject.h"
#include "UnPackage.h"

#include "UnPackageUE3Reader.h"

byte GForceCompMethod = 0;		// COMPRESS_...

/*-----------------------------------------------------------------------------
	Lineage2 file reader
-----------------------------------------------------------------------------*/

#if LINEAGE2 || EXTEEL

#define LINEAGE_HEADER_SIZE		28

class FFileReaderLineage : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderLineage, FReaderWrapper);
public:
	FFileReaderLineage(FArchive *File, int Key)
	:	FReaderWrapper(File, LINEAGE_HEADER_SIZE)
	,	XorKey(Key)
	{
		Game = GAME_Lineage2;
		Seek(0);		// skip header
	}

	virtual void Serialize(void *data, int size)
	{
		Reader->Serialize(data, size);
		if (XorKey)
		{
			int i;
			byte *p;
			for (i = 0, p = (byte*)data; i < size; i++, p++)
				*p ^= XorKey;
		}
	}

protected:
	byte		XorKey;
};

#endif // LINEAGE2 || EXTEEL


/*-----------------------------------------------------------------------------
	Battle Territory Online
-----------------------------------------------------------------------------*/

#if BATTLE_TERR

class FFileReaderBattleTerr : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderBattleTerr, FReaderWrapper);
public:
	FFileReaderBattleTerr(FArchive *File)
	:	FReaderWrapper(File)
	{
		Game = GAME_BattleTerr;
	}

	virtual void Serialize(void *data, int size)
	{
		Reader->Serialize(data, size);

		int i;
		byte *p;
		for (i = 0, p = (byte*)data; i < size; i++, p++)
		{
			byte b = *p;
			int shift;
			byte v;
			for (shift = 1, v = b & (b - 1); v; v = v & (v - 1))	// shift = number of identity bits in 'v' (but b=0 -> shift=1)
				shift++;
			b = ROL8(b, shift);
			*p = b;
		}
	}
};

#endif // BATTLE_TERR


/*-----------------------------------------------------------------------------
	America's Army 2
-----------------------------------------------------------------------------*/

#if AA2

class FFileReaderAA2 : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderAA2, FReaderWrapper);
public:
	FFileReaderAA2(FArchive *File)
	:	FReaderWrapper(File)
	{}

	virtual void Serialize(void *data, int size)
	{
		int StartPos = Reader->Tell();
		Reader->Serialize(data, size);

		int i;
		byte *p;
		for (i = 0, p = (byte*)data; i < size; i++, p++)
		{
			byte b = *p;
		#if 0
			// used with ArraysAGPCount != 0
			int shift;
			byte v;
			for (shift = 1, v = b & (b - 1); v; v = v & (v - 1))	// shift = number of identity bits in 'v' (but b=0 -> shift=1)
				shift++;
			b = ROR8(b, shift);
		#else
			// used with ArraysAGPCount == 0
			int PosXor = StartPos + i;
			PosXor = (PosXor >> 8) ^ PosXor;
			b ^= (PosXor & 0xFF);
			if (PosXor & 2)
			{
				b = ROL8(b, 1);
			}
		#endif
			*p = b;
		}
	}
};

#endif // AA2


/*-----------------------------------------------------------------------------
	Blade & Soul
-----------------------------------------------------------------------------*/

#if BLADENSOUL

class FFileReaderBnS : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderBnS, FReaderWrapper);
public:
	FFileReaderBnS(FArchive *File)
	:	FReaderWrapper(File)
	{
		Game = GAME_BladeNSoul;
	}

	virtual void Serialize(void *data, int size)
	{
		int Pos = Reader->Tell();
		Reader->Serialize(data, size);

		// Note: similar code exists in DecryptBladeAndSoul()
		int i;
		byte *p;
		static const char *key = "qiffjdlerdoqymvketdcl0er2subioxq";
		for (i = 0, p = (byte*)data; i < size; i++, p++, Pos++)
		{
			*p ^= key[Pos % 32];
		}
	}
};


static void DecodeBnSPointer(int32 &Value, uint32 Code1, uint32 Code2, int32 Index)
{
	uint32 tmp1 = ROR32(Value, (Index + Code2) & 0x1F);
	uint32 tmp2 = ROR32(Code1, Index % 32);
	Value = tmp2 ^ tmp1;
}


void PatchBnSExports(FObjectExport *Exp, const FPackageFileSummary &Summary)
{
	unsigned Code1 = ((Summary.HeadersSize & 0xFF) << 24) |
					 ((Summary.NameCount   & 0xFF) << 16) |
					 ((Summary.NameOffset  & 0xFF) << 8)  |
					 ((Summary.ExportCount & 0xFF));
	unsigned Code2 = (Summary.ExportOffset + Summary.ImportCount + Summary.ImportOffset) & 0x1F;

	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
		DecodeBnSPointer(Exp->SerialSize,   Code1, Code2, i);
		DecodeBnSPointer(Exp->SerialOffset, Code1, Code2, i);
	}
}

#endif // BLADENSOUL

/*-----------------------------------------------------------------------------
	Dungeon Defenders
-----------------------------------------------------------------------------*/

#if DUNDEF

void PatchDunDefExports(FObjectExport *Exp, const FPackageFileSummary &Summary)
{
	// Dungeon Defenders has nullified ExportOffset entries starting from some version.
	// Let's recover them.
	int CurrentOffset = Summary.HeadersSize;
	for (int i = 0; i < Summary.ExportCount; i++, Exp++)
	{
		if (Exp->SerialOffset == 0)
			Exp->SerialOffset = CurrentOffset;
		CurrentOffset = Exp->SerialOffset + Exp->SerialSize;
	}
}

#endif // DUNDEF


/*-----------------------------------------------------------------------------
	Nurien
-----------------------------------------------------------------------------*/

#if NURIEN

class FFileReaderNurien : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderNurien, FReaderWrapper);
public:
	int			Threshold;

	FFileReaderNurien(FArchive *File)
	:	FReaderWrapper(File)
	,	Threshold(0x7FFFFFFF)
	{}

	virtual void Serialize(void *data, int size)
	{
		int Pos = Reader->Tell();
		Reader->Serialize(data, size);

		// only first Threshold bytes are compressed (package headers)
		if (Pos >= Threshold) return;

		int i;
		byte *p;
		static const byte key[] = {
			0xFE, 0xF2, 0x35, 0x2E, 0x12, 0xFF, 0x47, 0x8A,
			0xE1, 0x2D, 0x53, 0xE2, 0x21, 0xA3, 0x74, 0xA8
		};
		for (i = 0, p = (byte*)data; i < size; i++, p++, Pos++)
		{
			if (Pos >= Threshold) return;
			*p ^= key[Pos & 0xF];
		}
	}

	virtual void SetStartingPosition(int pos)
	{
		Threshold = pos;
	}
};

#endif // NURIEN

/*-----------------------------------------------------------------------------
	Rocket League
-----------------------------------------------------------------------------*/

#if ROCKET_LEAGUE

class FFileReaderRocketLeague : public FReaderWrapper
{
	DECLARE_ARCHIVE(FFileReaderRocketLeague, FReaderWrapper);
public:
	int			EncryptionStart;
	int			EncryptionEnd;

	FFileReaderRocketLeague(FArchive *File)
	:	FReaderWrapper(File)
	,	EncryptionStart(0)
	,	EncryptionEnd(0)
	{}

	virtual void Serialize(void *data, int size)
	{
		int Pos = Reader->Tell();
		Reader->Serialize(data, size);

		// Check if any of the data read was encrypted
		if (Pos + size <= EncryptionStart || Pos >= EncryptionEnd)
			return;

		// Determine what needs to be decrypted
		int StartOffset			= max(0, Pos - EncryptionStart);
		int EndOffset			= min(EncryptionEnd, Pos + size) - EncryptionStart;
		int CopySize			= EndOffset - StartOffset;
		int CopyOffset			= max(0, EncryptionStart - Pos);

		// Round to 16-byte AES blocks
		int BlockStartOffset	= StartOffset & ~15;
		int BlockEndOffset		= Align(EndOffset, 16);
		int EncryptedSize		= BlockEndOffset - BlockStartOffset;
		int EncryptedOffset		= StartOffset - BlockStartOffset;

		// Decrypt and copy
		static const byte key[] = {
			0xC7, 0xDF, 0x6B, 0x13, 0x25, 0x2A, 0xCC, 0x71,
			0x47, 0xBB, 0x51, 0xC9, 0x8A, 0xD7, 0xE3, 0x4B,
			0x7F, 0xE5, 0x00, 0xB7, 0x7F, 0xA5, 0xFA, 0xB2,
			0x93, 0xE2, 0xF2, 0x4E, 0x6B, 0x17, 0xE7, 0x79
		};

		byte *EncryptedBuffer = (byte*)(appMalloc(EncryptedSize));
		Reader->Seek(EncryptionStart + BlockStartOffset);
		Reader->Serialize(EncryptedBuffer, EncryptedSize);
		appDecryptAES(EncryptedBuffer, EncryptedSize, (char*)(key), ARRAY_COUNT(key));
		memcpy(OffsetPointer(data, CopyOffset), &EncryptedBuffer[EncryptedOffset], CopySize);
		appFree(EncryptedBuffer);

		// Note: this code is absolutely not optimal, because it will read 16 bytes block and fully decrypt
		// it many times for every small piece of serialized daya (for example if serializing array of bytes,
		// we'll have full code above executed for each byte, instead of once per block). However, it is assumed
		// that this reader is used only for decryption of package's header, so it is not so important to
		// optimize it.

		// Restore position
		Reader->Seek(Pos + size);
	}
};

#endif // ROCKET_LEAGUE

/*-----------------------------------------------------------------------------
	Top-level code
-----------------------------------------------------------------------------*/

/*static*/ FArchive* UnPackage::CreateLoader(const char* filename, FArchive* baseLoader)
{
	guard(UnPackage::CreateLoader);

	// setup FArchive
	FArchive* Loader = (baseLoader) ? baseLoader : new FFileReader(filename, EFileArchiveOptions::OpenWarning);
	if (!Loader)
	{
		return NULL;
	}

	// Verify the file size first, taking into account that it might be too large to open (requires 64-bit size support).
	int64 FileSize = Loader->GetFileSize64();
	if (FileSize < 16 || FileSize >= MAX_FILE_SIZE_32)
	{
		if (FileSize > 1024)
		{
			appPrintf("WARNING: package file %s is too large (%d Mb), ignoring\n", filename, int32(FileSize >> 20));
		}
		// The file is too small, possibly invalid one.
		if (!baseLoader)
			delete Loader;
		return NULL;
	}

	// Pick 32-bit integer from archive to determine its type
	uint32 checkDword;
	*Loader << checkDword;
	// Seek back to file start
	Loader->Seek(0);

#if LINEAGE2 || EXTEEL
	if (checkDword == ('L' | ('i' << 16)))	// unicode string "Lineage2Ver111"
	{
		// this is a Lineage2 package
		Loader->Seek(LINEAGE_HEADER_SIZE);
		// here is a encrypted by 'xor' standard FPackageFileSummary
		// to get encryption key, can check 1st byte
		byte b;
		*Loader << b;
		// for Ver111 XorKey==0xAC for Lineage or ==0x42 for Exteel, for Ver121 computed from filename
		byte XorKey = b ^ (PACKAGE_FILE_TAG & 0xFF);
		// replace Loader
		Loader = new FFileReaderLineage(Loader, XorKey);
		return Loader;
	}
#endif // LINEAGE2 || EXTEEL

#if BATTLE_TERR
	if (checkDword == 0x342B9CFC)
	{
		// replace Loader
		Loader = new FFileReaderBattleTerr(Loader);
		return Loader;
	}
#endif // BATTLE_TERR

#if NURIEN
	if (checkDword == 0xB01F713F)
	{
		// replace loader
		Loader = new FFileReaderNurien(Loader);
		return Loader;
	}
#endif // NURIEN

#if BLADENSOUL
	if (checkDword == 0xF84CEAB0)
	{
		if (!GForceGame) GForceGame = GAME_BladeNSoul;
		Loader = new FFileReaderBnS(Loader);
		return Loader;
	}
#endif // BLADENSOUL

#if UNREAL3
	// Code for loading UE3 "fully compressed packages"

	uint32 checkDword1, checkDword2;
	*Loader << checkDword1;
	if (checkDword1 == PACKAGE_FILE_TAG_REV)
	{
		Loader->ReverseBytes = true;
		if (GForcePlatform == PLATFORM_UNKNOWN)
			Loader->Platform = PLATFORM_XBOX360;			// default platform for "ReverseBytes" mode is PLATFORM_XBOX360
	}
	else if (checkDword1 != PACKAGE_FILE_TAG)
	{
		// fully compressed package always starts with package tag
		Loader->Seek(0);
		return Loader;
	}
	// Read 2nd dword after changing byte order in Loader
	*Loader << checkDword2;
	Loader->Seek(0);

	// Check if this is a fully compressed package. UE3 by itself checks if there's .uncompressed_size with text contents
	// file exists next to the package file.
	if (checkDword2 == PACKAGE_FILE_TAG || checkDword2 == 0x20000 || checkDword2 == 0x10000)	// seen 0x10000 in Enslaved/PS3
	{
		//!! NOTES:
		//!! - MKvsDC/X360 Core.u and Engine.u uses LZO instead of LZX (LZO and LZX are not auto-detected with COMPRESS_FIND)
		guard(ReadFullyCompressedHeader);
		// this is a fully compressed package
		FCompressedChunkHeader H;
		*Loader << H;
		TArray<FCompressedChunk> Chunks;
		FCompressedChunk *Chunk = new (Chunks) FCompressedChunk;
		Chunk->UncompressedOffset = 0;
		Chunk->UncompressedSize   = H.Sum.UncompressedSize;
		Chunk->CompressedOffset   = 0;
		Chunk->CompressedSize     = H.Sum.CompressedSize;
		byte CompMethod = GForceCompMethod;
		if (!CompMethod)
			CompMethod = (Loader->Platform == PLATFORM_XBOX360) ? COMPRESS_LZX : COMPRESS_FIND;
		FUE3ArchiveReader* UE3Loader = new FUE3ArchiveReader(Loader, CompMethod, Chunks);
		UE3Loader->IsFullyCompressed = true;
		Loader = UE3Loader;
		unguard;
	}
#endif // UNREAL3

	return Loader;

	unguardf("%s", filename);
}

void UnPackage::ReplaceLoader()
{
	guard(UnPackage::ReplaceLoader);

	// Current FArchive position is after FPackageFileSummary

#if BIOSHOCK
	if ((Game == GAME_Bioshock) && (Summary.PackageFlags & 0x20000))
	{
		// Bioshock has a special flag indicating compression. Compression table follows the package summary.
		// Read compression tables.
		int NumChunks, i;
		TArray<FCompressedChunk> Chunks;
		*this << NumChunks;
		Chunks.Empty(NumChunks);
		int UncompOffset = Tell() - 4;				//?? there should be a flag signalling presence of compression structures, because of "Tell()-4"
		for (i = 0; i < NumChunks; i++)
		{
			int Offset;
			*this << Offset;
			FCompressedChunk *Chunk = new (Chunks) FCompressedChunk;
			Chunk->UncompressedOffset = UncompOffset;
			Chunk->UncompressedSize   = 32768;
			Chunk->CompressedOffset   = Offset;
			Chunk->CompressedSize     = 0;			//?? not used
			UncompOffset             += 32768;
		}
		// Replace Loader for reading compressed Bioshock archives.
		Loader = new FUE3ArchiveReader(Loader, COMPRESS_ZLIB, Chunks);
		Loader->SetupFrom(*this);
		return;
	}
#endif // BIOSHOCK

#if AA2
	if (Game == GAME_AA2)
	{
		// America's Army 2 has encryption after FPackageFileSummary
		if (ArLicenseeVer >= 19)
		{
			int IsEncrypted;
			*this << IsEncrypted;
			if (IsEncrypted) Loader = new FFileReaderAA2(Loader);
		}
		return;
	}
#endif // AA2

#if ROCKET_LEAGUE
	if (Game == GAME_RocketLeague && (Summary.PackageFlags & PKG_Cooked))
	{
		// Rocket League has an encrypted header after FPackageFileSummary containing the name/import/export tables and a compression table.
		TArray<FString> AdditionalPackagesToCook;
		*this << AdditionalPackagesToCook;

		// Array of unknown structs
		int32 NumUnknownStructs;
		*this << NumUnknownStructs;
		for (int i = 0; i < NumUnknownStructs; i++)
		{
			this->Seek(this->Tell() + sizeof(int32)*5); // skip 5 int32 values
			TArray<int32> unknownArray;
			*this << unknownArray;
		}

		// Info related to encrypted buffer
		int32 GarbageSize, CompressedChunkInfoOffset, LastBlockSize;
		*this << GarbageSize << CompressedChunkInfoOffset << LastBlockSize;

		// Create a reader to decrypt the rest of Rocket League's header
		FFileReaderRocketLeague* RocketReader = new FFileReaderRocketLeague(Loader);
		RocketReader->SetupFrom(*this);
		RocketReader->EncryptionStart = Summary.NameOffset;
		RocketReader->EncryptionEnd = Summary.HeadersSize;

		// Create a UE3 compression reader with the chunk info contained in the encrypted RL header
		RocketReader->Seek(RocketReader->EncryptionStart + CompressedChunkInfoOffset);

		TArray<FCompressedChunk> Chunks;
		*RocketReader << Chunks;

		Loader = new FUE3ArchiveReader(RocketReader, COMPRESS_ZLIB, Chunks);
		Loader->SetupFrom(*this);

		// The decompressed chunks will overwrite past CompressedChunkInfoOffset, so don't decrypt past that anymore
		RocketReader->EncryptionEnd = RocketReader->EncryptionStart + CompressedChunkInfoOffset;
		return;
	}
#endif // ROCKET_LEAGUE

#if NURIEN
	// Nurien has encryption in header, and no encryption after
	FFileReaderNurien* NurienReader = Loader->CastTo<FFileReaderNurien>();
	if (NurienReader)
	{
		NurienReader->Threshold = Summary.HeadersSize;
		return;
	}
#endif // NURIEN

	unguard;
}
