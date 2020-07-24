#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"

#include "UnArchivePak.h"

#if UNREAL4

#define PAK_FILE_MAGIC		0x5A6F12E1

FArchive& operator<<(FArchive& Ar, FPakInfo& P)
{
	// New FPakInfo fields.
	Ar << P.EncryptionKeyGuid;			// PakFile_Version_EncryptionKeyGuid
	Ar << P.bEncryptedIndex;			// PakFile_Version_IndexEncryption

	// Old FPakInfo fields.
	Ar << P.Magic;
	if (P.Magic != PAK_FILE_MAGIC)
	{
		// Stop immediately when magic is wrong
		return Ar;
	}
	Ar << P.Version << P.IndexOffset << P.IndexSize;
	Ar.Serialize(ARRAY_ARG(P.IndexHash));

#if SOD2
	if (GForceGame == GAME_StateOfDecay2)
		P.Version &= 0xffff;
#endif

	if (P.Version == PakFile_Version_FrozenIndex)
	{
		uint8 bIndexIsFrozen;
		Ar << bIndexIsFrozen;
		assert(!bIndexIsFrozen);	// used just for 4.25, so don't do any support unless it's really needed
	}

	if (P.Version >= PakFile_Version_FNameBasedCompressionMethod)
	{
		// For UE4.23, there are 5 compression methods, but we're ignoring last one.
		for (int i = 0; i < 4; i++)
		{
			char name[32+1];
			Ar.Serialize(name, 32);
			name[32] = 0;
			int32 CompressionMethod = 0;
			if (!stricmp(name, "zlib"))
			{
				CompressionMethod = COMPRESS_ZLIB;
			}
			else if (!stricmp(name, "oodle"))
			{
				CompressionMethod = COMPRESS_OODLE;
			}
			else if (!stricmp(name, "lz4"))
			{
				CompressionMethod = COMPRESS_LZ4;
			}
			else if (name[0])
			{
				appPrintf("Warning: unknown compression method for pak: %s\n", name);
			}
			P.CompressionMethods[i] = CompressionMethod;
		}
	}

	// Reset new fields to their default states when seralizing older pak format.
	if (P.Version < PakFile_Version_IndexEncryption)
	{
		P.bEncryptedIndex = false;
	}
	if (P.Version < PakFile_Version_EncryptionKeyGuid)
	{
		P.EncryptionKeyGuid = { 0, 0, 0, 0 };
	}
	return Ar;
}

void FPakEntry::Serialize(FArchive& Ar)
{
	guard(FPakEntry<<);

	// FPakEntry is duplicated before each stored file, without a filename. So,
	// remember the serialized size of this structure to avoid recomputation later.
	int64 StartOffset = Ar.Tell64();

#if GEARS4
	if (GForceGame == GAME_Gears4)
	{
		Ar << Pos << (int32&)Size << (int32&)UncompressedSize << (byte&)CompressionMethod;
		if (PakVer < PakFile_Version_NoTimestamps)
		{
			int64 timestamp;
			Ar << timestamp;
		}
		if (PakVer >= PakFile_Version_CompressionEncryption)
		{
			if (CompressionMethod != 0)
				Ar << CompressionBlocks;
			Ar << CompressionBlockSize;
			if (CompressionMethod == 4)
				CompressionMethod = COMPRESS_LZ4;
		}
		goto end;
	}
#endif // GEARS4

	Ar << Pos << Size << UncompressedSize;

	if (PakVer < PakFile_Version_FNameBasedCompressionMethod)
	{
		Ar << CompressionMethod;
	}
	else if (PakVer == PakFile_Version_FNameBasedCompressionMethod && PakSubver == 0)
	{
		// UE4.22
		uint8 CompressionMethodIndex;
		Ar << CompressionMethodIndex;
		CompressionMethod = CompressionMethodIndex;
	}
	else
	{
		// UE4.23+
		uint32 CompressionMethodIndex;
		Ar << CompressionMethodIndex;
		CompressionMethod = CompressionMethodIndex;
	}

	if (PakVer < PakFile_Version_NoTimestamps)
	{
		int64 timestamp;
		Ar << timestamp;
	}

	uint8 Hash[20];
	Ar.Serialize(ARRAY_ARG(Hash));

	if (PakVer >= PakFile_Version_CompressionEncryption)
	{
		if (CompressionMethod != 0)
			Ar << CompressionBlocks;
		Ar << bEncrypted << CompressionBlockSize;
	}
	if (CompressionMethod == COMPRESS_Custom)
	{
		CompressionMethod = COMPRESS_FIND;
	}

#if TEKKEN7
	if (GForceGame == GAME_Tekken7)
		bEncrypted = false;		// Tekken 7 has 'bEncrypted' flag set, but actually there's no encryption
#endif
#if SOD2
	if (GForceGame == GAME_StateOfDecay2 && CompressionMethod > 16)
		CompressionMethod = COMPRESS_LZ4;
#endif

	if (PakVer >= PakFile_Version_RelativeChunkOffsets)
	{
		// Convert relative compressed offsets to absolute
		for (int i = 0; i < CompressionBlocks.Num(); i++)
		{
			FPakCompressedBlock& B = CompressionBlocks[i];
			B.CompressedStart += Pos;
			B.CompressedEnd += Pos;
		}
	}

end:
	StructSize = Ar.Tell64() - StartOffset;

	unguard;
}

void FPakEntry::DecodeFrom(const uint8* Data)
{
	guard(FPakEntry::DecodeFrom);

	// UE4 reference: FPakFile::DecodePakEntry()
	uint32 Bitfield = *(uint32*)Data;
	Data += sizeof(uint32);

	CompressionMethod = (Bitfield >> 23) & 0x3f;

	// Offset follows - either 32 or 64 bit value
	if (Bitfield & 0x80000000)
	{
		Pos = *(uint32*)Data;
		Data += sizeof(uint32);
	}
	else
	{
		Pos = *(uint64*)Data;
		Data += sizeof(uint64);
	}

	// The same for UncompressedSize
	if (Bitfield & 0x40000000)
	{
		UncompressedSize = *(uint32*)Data;
		Data += sizeof(uint32);
	}
	else
	{
		UncompressedSize = *(uint64*)Data;
		Data += sizeof(uint64);
	}

	// Size field
	if (CompressionMethod)
	{
		if (Bitfield & 0x20000000)
		{
			Size = *(uint32*)Data;
			Data += sizeof(uint32);
		}
		else
		{
			Size = *(uint64*)Data;
			Data += sizeof(uint64);
		}
	}
	else
	{
		Size = UncompressedSize;
	}

	// bEncrypted
	bEncrypted = (Bitfield >> 22) & 1;

	// Compressed block count
	int BlockCount = (Bitfield >> 6) & 0xffff;

	// Compute StructSize: each file still have FPakEntry data prepended, and it should be skipped.
	StructSize = sizeof(int64) * 3 + sizeof(int32) * 2 + 1 + 20;
	// Take into account CompressionBlocks
	if (CompressionMethod)
	{
		StructSize += sizeof(int32) + BlockCount * 2 * sizeof(int64);
	}

	// Compression information
	CompressionBlocks.AddUninitialized(BlockCount);
	CompressionBlockSize = 0;
	if (BlockCount)
	{
		// CompressionBlockSize
		if (UncompressedSize < 65536)
			CompressionBlockSize = UncompressedSize;
		else
			CompressionBlockSize = (Bitfield & 0x3f) << 11;

		// CompressionBlocks
		if (BlockCount == 1)
		{
			FPakCompressedBlock& Block = CompressionBlocks[0];
			Block.CompressedStart = Pos + StructSize;
			Block.CompressedEnd = Block.CompressedStart + Size;
		}
		else
		{
			int64 CurrentOffset = Pos + StructSize;
			int64 Alignment = bEncrypted ? FPakFile::EncryptionAlign : 1;
			for (int BlockIndex = 0; BlockIndex < BlockCount; BlockIndex++)
			{
				int64 CurrentBlockSize = *(int64*)Data;
				Data += sizeof(int64);

				FPakCompressedBlock& Block = CompressionBlocks[BlockIndex];
				Block.CompressedStart = CurrentOffset;
				Block.CompressedEnd = Block.CompressedStart + CurrentBlockSize;
				CurrentOffset += Align(CurrentBlockSize, Alignment);
			}
		}
	}

	unguard;
}

void FPakFile::Serialize(void *data, int size)
{
	PROFILE_IF(size >= 1024);
	guard(FPakFile::Serialize);
	if (ArStopper > 0 && ArPos + size > ArStopper)
		appError("Serializing behind stopper (%X+%X > %X)", ArPos, size, ArStopper);

	if (Info->CompressionMethod)
	{
		guard(SerializeCompressed);

		while (size > 0)
		{
			if ((UncompressedBuffer == NULL) || (ArPos < UncompressedBufferPos) || (ArPos >= UncompressedBufferPos + Info->CompressionBlockSize))
			{
				// buffer is not ready
				if (UncompressedBuffer == NULL)
				{
					UncompressedBuffer = (byte*)appMallocNoInit((int)Info->CompressionBlockSize); // size of uncompressed block
				}
				// prepare buffer
				int BlockIndex = ArPos / Info->CompressionBlockSize;
				UncompressedBufferPos = Info->CompressionBlockSize * BlockIndex;

				const FPakCompressedBlock& Block = Info->CompressionBlocks[BlockIndex];
				int CompressedBlockSize = (int)(Block.CompressedEnd - Block.CompressedStart);
				int UncompressedBlockSize = min((int)Info->CompressionBlockSize, (int)Info->UncompressedSize - UncompressedBufferPos); // don't pass file end
				byte* CompressedData;
				if (!Info->bEncrypted)
				{
					CompressedData = (byte*)appMallocNoInit(CompressedBlockSize);
					Reader->Seek64(Block.CompressedStart);
					Reader->Serialize(CompressedData, CompressedBlockSize);
				}
				else
				{
					int EncryptedSize = Align(CompressedBlockSize, EncryptionAlign);
					CompressedData = (byte*)appMallocNoInit(EncryptedSize);
					Reader->Seek64(Block.CompressedStart);
					Reader->Serialize(CompressedData, EncryptedSize);
					PakRequireAesKey();
					appDecryptAES(CompressedData, EncryptedSize);
				}
				appDecompress(CompressedData, CompressedBlockSize, UncompressedBuffer, UncompressedBlockSize, Info->CompressionMethod);
				appFree(CompressedData);
			}

			// data is in buffer, copy it
			int BytesToCopy = UncompressedBufferPos + Info->CompressionBlockSize - ArPos; // number of bytes until end of the buffer
			if (BytesToCopy > size) BytesToCopy = size;
			assert(BytesToCopy > 0);

			// copy uncompressed data
			int OffsetInBuffer = ArPos - UncompressedBufferPos;
			memcpy(data, UncompressedBuffer + OffsetInBuffer, BytesToCopy);

			// advance pointers
			ArPos += BytesToCopy;
			size  -= BytesToCopy;
			data  = OffsetPointer(data, BytesToCopy);
		}

		unguard;
	}
	else if (Info->bEncrypted)
	{
		guard(SerializeEncrypted);

		// Uncompressed encrypted data. Reuse compression fields to handle decryption efficiently
		if (UncompressedBuffer == NULL)
		{
			UncompressedBuffer = (byte*)appMallocNoInit(EncryptedBufferSize);
			UncompressedBufferPos = 0x40000000; // some invalid value
		}
		while (size > 0)
		{
			if ((ArPos < UncompressedBufferPos) || (ArPos >= UncompressedBufferPos + EncryptedBufferSize))
			{
				// Should fetch block and decrypt it.
				// Note: AES is block encryption, so we should always align read requests for correct decryption.
				UncompressedBufferPos = ArPos & ~(EncryptionAlign - 1);
				Reader->Seek64(Info->Pos + Info->StructSize + UncompressedBufferPos);
				int RemainingSize = Info->Size - UncompressedBufferPos;
				if (RemainingSize > EncryptedBufferSize)
					RemainingSize = EncryptedBufferSize;
				RemainingSize = Align(RemainingSize, EncryptionAlign); // align for AES, pak contains aligned data
				Reader->Serialize(UncompressedBuffer, RemainingSize);
				PakRequireAesKey();
				appDecryptAES(UncompressedBuffer, RemainingSize);
			}

			// Now copy decrypted data from UncompressedBuffer (code is very similar to those used in decompression above)
			int BytesToCopy = UncompressedBufferPos + EncryptedBufferSize - ArPos; // number of bytes until end of the buffer
			if (BytesToCopy > size) BytesToCopy = size;
			assert(BytesToCopy > 0);

			// copy uncompressed data
			int OffsetInBuffer = ArPos - UncompressedBufferPos;
			memcpy(data, UncompressedBuffer + OffsetInBuffer, BytesToCopy);

			// advance pointers
			ArPos += BytesToCopy;
			size  -= BytesToCopy;
			data  = OffsetPointer(data, BytesToCopy);
		}

		unguard;
	}
	else
	{
		guard(SerializeUncompressed);

		// Pure data
		// seek every time in a case if the same 'Reader' was used by different FPakFile
		// (this is a lightweight operation for buffered FArchive)
		Reader->Seek64(Info->Pos + Info->StructSize + ArPos);
		Reader->Serialize(data, size);
		ArPos += size;

		unguard;
	}
	unguardf("file=%s", *Info->FileInfo->GetRelativeName());
}

void FPakVFS::CompactFilePath(FString& Path)
{
	guard(FPakVFS::CompactFilePath);

	if (Path.StartsWith("/Engine/Content"))	// -> /Engine
	{
		Path.RemoveAt(7, 8);
		return;
	}
	if (Path.StartsWith("/Engine/Plugins")) // -> /Plugins
	{
		Path.RemoveAt(0, 7);
		return;
	}

	if (Path[0] != '/')
		return;

	char* delim = strchr(&Path[1], '/');
	if (!delim)
		return;
	if (strncmp(delim, "/Content/", 9) != 0)
		return;

	int pos = delim - &Path[0];
	if (pos > 4)
	{
		// /GameName/Content -> /Game
		int toRemove = pos + 8 - 5;
		Path.RemoveAt(5, toRemove);
		memcpy(&Path[1], "Game", 4);
	}

	unguard;
}

bool FPakVFS::AttachReader(FArchive* reader, FString& error)
{
	int mainVer = 0, subVer = 0;

	guard(FPakVFS::ReadDirectory);
	PROFILE_LABEL(*Filename);

	// Pak file may have different header sizes, try them all
	static const int OffsetsToTry[] = { FPakInfo::Size, FPakInfo::Size8, FPakInfo::Size8a, FPakInfo::Size9 };
	FPakInfo info;

	for (int32 Offset : OffsetsToTry)
	{
		// Read pak header
		int64 HeaderOffset = reader->GetFileSize64() - Offset;
		if (HeaderOffset <= 0)
		{
			// The file is too small
			return false;
		}
		reader->Seek64(HeaderOffset);

		*reader << info;
		if (info.Magic == PAK_FILE_MAGIC)		// no endian checking here
		{
			if (Offset == FPakInfo::Size8a && info.Version == 8)
			{
				subVer = 1;
			}
			break;
		}
	}

	if (info.Magic != PAK_FILE_MAGIC)
	{
		// We didn't find a pak header
		return false;
	}

	if (info.Version > PakFile_Version_Latest)
	{
		appPrintf("WARNING: Pak file \"%s\" has unsupported version %d\n", *Filename, info.Version);
	}

	mainVer = info.Version;

	if (info.bEncryptedIndex)
	{
		if (!PakRequireAesKey(false))
		{
			char buf[1024];
			appSprintf(ARRAY_ARG(buf), "WARNING: Pak \"%s\" has encrypted index. Skipping.", *Filename);
			error = buf;
			return false;
		}
	}

	// Read pak index

	// Set PakVer
	reader->ArLicenseeVer = MakePakVer(mainVer, subVer);

	reader->Seek64(info.IndexOffset);

	bool result = false;
	if (info.Version < PakFile_Version_PathHashIndex)
	{
		result = LoadPakIndexLegacy(reader, info, error);
	}
	else
	{
		result = LoadPakIndex(reader, info, error);
	}

	if (result)
	{
		// Print statistics
		appPrintf("Pak %s: %d files", *Filename, FileInfos.Num());
		if (NumEncryptedFiles)
			appPrintf(" (%d encrypted)", NumEncryptedFiles);
		if (strcmp(*MountPoint, "/") != 0)
			appPrintf(", mount point: \"%s\"", *MountPoint);
		appPrintf(", version %d\n", info.Version);
	}

	return result;

	unguardf("PakVer=%d.%d", mainVer, subVer);
}

static bool ValidateString(FArchive& Ar)
{
	// We're operating with index data, which is definitely less than 2Gb of size, so use Tell instead of Tell64.
	int SavePos = Ar.Tell();

	// Try to validate the decrypted data. The first thing going here is MountPoint which is FString.
	int32 StringLen;
	Ar << StringLen;
	bool bFail = false;

	if (StringLen > 512 || StringLen < -512)
	{
		bFail = true;
	}
	if (!bFail)
	{
		// Seek to terminating zero character
		if (StringLen < 0)
		{
			Ar.Seek(Ar.Tell() - (StringLen - 1) * 2);
			uint16 c;
			Ar << c;
			bFail = (c != 0);
		}
		else
		{
			Ar.Seek(Ar.Tell() + StringLen - 1);
			char c;
			Ar << c;
			bFail = (c != 0);
		}
	}

	Ar.Seek(SavePos);
	return !bFail;
}

void FPakVFS::ValidateMountPoint(FString& MountPoint)
{
	bool badMountPoint = false;
	if (!MountPoint.RemoveFromStart("../../.."))
		badMountPoint = true;
	if (MountPoint[0] != '/' || ( (MountPoint.Len() > 1) && (MountPoint[1] == '.') ))
		badMountPoint = true;

	if (badMountPoint)
	{
		appPrintf("WARNING: Pak \"%s\" has strange mount point \"%s\", mounting to root\n", *Filename, *MountPoint);
		MountPoint = "/";
	}
}

bool FPakVFS::LoadPakIndexLegacy(FArchive* reader, const FPakInfo& info, FString& error)
{
	guard(FPakVFS::LoadPakIndexLegacy);

	// Always read index to memory block for faster serialization
	TArray<byte> InfoBlock;
	InfoBlock.SetNumUninitialized(info.IndexSize);
	reader->Serialize(InfoBlock.GetData(), info.IndexSize);
	FMemReader InfoReader(InfoBlock.GetData(), info.IndexSize);
	InfoReader.SetupFrom(*reader);

	// Manage pak files with encrypted index
	if (info.bEncryptedIndex)
	{
		guard(CheckEncryptedIndex);

		appDecryptAES(InfoBlock.GetData(), InfoBlock.Num());

		// Try to validate the decrypted data. The first thing going here is MountPoint which is FString.
		if (!ValidateString(InfoReader))
		{
			char buf[1024];
			appSprintf(ARRAY_ARG(buf), "WARNING: The provided encryption key doesn't work with \"%s\". Skipping.", *Filename);
			error = buf;
			return false;
		}

		// Data is ok, seek to data start.
		InfoReader.Seek(0);

		unguard;
	}

	// this file looks correct, store 'reader'
	Reader = reader;

	// Read pak index

	TRY {
		// Read MountPoint with catching error, to override error message.
		InfoReader << MountPoint;
	} CATCH {
		if (info.bEncryptedIndex)
		{
			// Display nice error message
			appErrorNoLog("Error during loading of encrypted pak file index. Probably the provided AES key is not correct.");
		}
		else
		{
			THROW_AGAIN;
		}
	}

	// Read number of files
	int32 count;
	InfoReader << count;
	if (!count)
	{
		appPrintf("Empty pak file \"%s\"\n", *Filename);
		return true;
	}

	// Process MountPoint
	ValidateMountPoint(MountPoint);

	// Read file information
	FileInfos.AddZeroed(count);
	Reserve(count);

	for (int i = 0; i < count; i++)
	{
		guard(ReadInfo);

		FPakEntry& E = FileInfos[i];
		// serialize name, combine with MountPoint
		FStaticString<MAX_PACKAGE_PATH> Filename;
		InfoReader << Filename;
		FStaticString<MAX_PACKAGE_PATH> CombinedPath;
		CombinedPath = MountPoint;
		CombinedPath += Filename;
		// compact file name
		CompactFilePath(CombinedPath);
		// serialize other fields
		E.Serialize(InfoReader);
		if (E.bEncrypted)
		{
//			appPrintf("Encrypted file: %s\n", *Filename);
			NumEncryptedFiles++;
		}
		if (info.Version >= PakFile_Version_FNameBasedCompressionMethod)
		{
			int32 CompressionMethodIndex = E.CompressionMethod;
			assert(CompressionMethodIndex >= 0 && CompressionMethodIndex <= 4);
			E.CompressionMethod = CompressionMethodIndex > 0 ? info.CompressionMethods[CompressionMethodIndex-1] : 0;
		}
		else if (E.CompressionMethod == COMPRESS_Custom)
		{
			// Custom compression method for UE4.20-UE4.21, use detection code.
			E.CompressionMethod = COMPRESS_FIND;
		}

		// Register the file
		CRegisterFileInfo reg;
		reg.Filename = *CombinedPath;
		reg.Size = E.UncompressedSize;
		reg.IndexInArchive = i;
		E.FileInfo = RegisterFile(reg);

		unguardf("Index=%d/%d", i, count);
	}

#if 0
	if (count >= MIN_PAK_SIZE_FOR_HASHING)
	{
		// Hash everything
		for (FPakEntry& E : FileInfos)
		{
			AddFileToHash(&E);
		}
	}
#endif

	return true;

	unguard;
}

bool FPakVFS::LoadPakIndex(FArchive* reader, const FPakInfo& info, FString& error)
{
	guard(FPakVFS::LoadPakIndex);

	// Read full index into InfoBlock and setup InfoReader
	TArray<byte> InfoBlock;
	InfoBlock.SetNumUninitialized(info.IndexSize);
	reader->Serialize(InfoBlock.GetData(), info.IndexSize);
	FMemReader InfoReader(InfoBlock.GetData(), info.IndexSize);
	InfoReader.SetupFrom(*reader);

	// Manage pak files with encrypted index
	if (info.bEncryptedIndex)
	{
		guard(CheckEncryptedIndex);

		appDecryptAES(InfoBlock.GetData(), InfoBlock.Num());

		// Try to validate the decrypted data. The first thing going here is MountPoint which is FString.
		if (!ValidateString(InfoReader))
		{
			char buf[1024];
			appSprintf(ARRAY_ARG(buf), "WARNING: The provided encryption key doesn't work with \"%s\". Skipping.", *Filename);
			error = buf;
			return false;
		}

		// Data is ok, seek to data start.
		InfoReader.Seek(0);

		unguard;
	}

	// this file looks correct, store 'reader'
	Reader = reader;

	// Read pak index

	TRY {
		// Read MountPoint with catching error, to override error message.
		InfoReader << MountPoint;
	} CATCH {
		if (info.bEncryptedIndex)
		{
			// Display nice error message
			appErrorNoLog("Error during loading of encrypted pak file index. Probably the provided AES key is not correct.");
		}
		else
		{
			THROW_AGAIN;
		}
	}

	// Read number of files
	int32 count;
	InfoReader << count;
	if (!count)
	{
		appPrintf("Empty pak file \"%s\"\n", *Filename);
		return true;
	}
	Reserve(count);

	// Process MountPoint
	ValidateMountPoint(MountPoint);

	uint64 PathHashSeed;
	InfoReader << PathHashSeed;

	// Read directory information

	bool bReaderHasPathHashIndex;
	int64 PathHashIndexOffset = -1;
	int64 PathHashIndexSize = 0;
	InfoReader << bReaderHasPathHashIndex;
	if (bReaderHasPathHashIndex)
	{
		InfoReader << PathHashIndexOffset << PathHashIndexSize;
		// Skip PathHashIndexHash
		InfoReader.Seek(InfoReader.Tell() + 20);
	}

	bool bReaderHasFullDirectoryIndex = false;
	int64 FullDirectoryIndexOffset = -1;
	int64 FullDirectoryIndexSize = 0;
	InfoReader << bReaderHasFullDirectoryIndex;
	if (bReaderHasFullDirectoryIndex)
	{
		InfoReader << FullDirectoryIndexOffset << FullDirectoryIndexSize;
		// Skip FullDirectoryIndexHash
		InfoReader.Seek(InfoReader.Tell() + 20);
	}

	if (!bReaderHasFullDirectoryIndex)
	{
		// todo: read PathHashIndex: PathHashIndexOffset + PathHashIndexSize
		// todo: structure: TMap<uint64, FPakEntryLocation> (i.e. not array), FPakEntryLocation = int32
		// todo: seems it maps hash to file index.
		char buf[1024];
		appSprintf(ARRAY_ARG(buf), "WARNING: Pak file \"%s\" doesn't have a full index. Skipping.", *Filename);
		error = buf;
		return false;
	}

	TArray<uint8> EncodedPakEntries;
	InfoReader << EncodedPakEntries;

	// Read 'Files' array. This one holds decoded file entries, without file names.
	TArray<FPakEntry> Files;
	InfoReader << Files;

	// Read the full index via the same InfoReader object
	assert(bReaderHasFullDirectoryIndex);

	guard(ReadFullDirectory);
	reader->Seek64(FullDirectoryIndexOffset);
	InfoBlock.Empty(); // avoid reallocation with memcpy
	InfoBlock.SetNumUninitialized(FullDirectoryIndexSize);
	reader->Serialize(InfoBlock.GetData(), FullDirectoryIndexSize);
	InfoReader = FMemReader(InfoBlock.GetData(), InfoBlock.Num());
	InfoReader.SetupFrom(*reader);

	if (info.bEncryptedIndex)
	{
		// Read encrypted data and decrypt
		appDecryptAES(InfoBlock.GetData(), InfoBlock.Num());
	}
	unguard;

	// Now InfoReader points to the full index data, either with use of 'reader' or 'InfoReaderProxy'.
	// Build "legacy" FileInfos array from new data format
	FileInfos.AddZeroed(count);

	guard(BuildFullDirectory);
	int FileIndex = 0;

	/*
		// We're unwrapping this complex structure serializer for faster performance (much less allocations)
		using FPakEntryLocation = int32;
		typedef TMap<FString, FPakEntryLocation> FPakDirectory;
		// Each directory has files, it maps clean filename to index
		TMap<FString, FPakDirectory> DirectoryIndex;
		InfoReader << DirectoryIndex;
	*/

	int32 DirectoryCount;
	InfoReader << DirectoryCount;

	for (int DirectoryIndex = 0; DirectoryIndex < DirectoryCount; DirectoryIndex++)
	{
		guard(Directory);
		// Read DirectoryIndex::Key
		FStaticString<MAX_PACKAGE_PATH> DirectoryName;
		InfoReader << DirectoryName;

		// Build folder name. MountPoint ends with '/', directory name too.
		FStaticString<MAX_PACKAGE_PATH> DirectoryPath;
		DirectoryPath = MountPoint;
		DirectoryPath += DirectoryName;
		CompactFilePath(DirectoryPath);
		if (DirectoryPath[DirectoryPath.Len()-1] == '/')
			DirectoryPath.RemoveAt(DirectoryPath.Len()-1, 1);

		int FolderIndex = RegisterGameFolder(*DirectoryPath);

		// Read size of FPakDirectory (DirectoryIndex::Value)
		int32 NumFilesInDirectory;
		InfoReader << NumFilesInDirectory;

		for (int DirectoryFileIndex = 0; DirectoryFileIndex < NumFilesInDirectory; DirectoryFileIndex++)
		{
			guard(File);
			// Read FPakDirectory entry Key
			FStaticString<MAX_PACKAGE_PATH> DirectoryFileName;
			InfoReader << DirectoryFileName;
			// Read FPakDirectory entry Value
			int32 PakEntryLocation;
			InfoReader << PakEntryLocation;

			FPakEntry& E = FileInfos[FileIndex];

			// PakEntryLocation is positive (offset in 'EncodedPakEntries') or negative (index in 'Files')
			// References in UE4:
			// FPakFile::DecodePakEntry <- FPakFile::GetPakEntry (decode or pick from 'Files') <- FPakFile::Find (name to index/location)
			if (PakEntryLocation < 0)
			{
				// Index in 'Files' array
				E.CopyFrom(Files[-(PakEntryLocation + 1)]);
			}
			else
			{
				// Pointer in 'EncodedPakEntries'
				E.DecodeFrom(&EncodedPakEntries[PakEntryLocation]);
			}

			if (E.bEncrypted)
			{
//				appPrintf("Encrypted file: %s\n", *Filename);
				NumEncryptedFiles++;
			}

			// Convert compression method
			int32 CompressionMethodIndex = E.CompressionMethod;
			assert(CompressionMethodIndex >= 0 && CompressionMethodIndex <= 4);
			E.CompressionMethod = CompressionMethodIndex > 0 ? info.CompressionMethods[CompressionMethodIndex-1] : 0;

			// Register the file
			CRegisterFileInfo reg;
			reg.Filename = *DirectoryFileName;
			reg.FolderIndex = FolderIndex;
			reg.Size = E.UncompressedSize;
			reg.IndexInArchive = FileIndex;
			E.FileInfo = RegisterFile(reg);

			FileIndex++;
			unguard;
		}
		unguard;
	}
	if (FileIndex != FileInfos.Num())
	{
		appError("Wrong pak file directory?");
	}
	unguard;

#if 0
	if (count >= MIN_PAK_SIZE_FOR_HASHING)
	{
		// Hash everything
		for (FPakEntry& E : FileInfos)
		{
			AddFileToHash(&E);
		}
	}
#endif

	return true;

	unguard;
}

#if 0
const FPakEntry* FPakVFS::FindFile(const char* name)
{
	if (LastInfo && !stricmp(LastInfo->Name, name))
		return LastInfo;

	FastNameComparer cmp(name);

	if (HashTable)
	{
		// Have a hash table, use it
		uint16 hash = GetHashForFileName(name);
		for (FPakEntry* info = HashTable[hash]; info; info = info->HashNext)
		{
			if (cmp(info->Name))
			{
				LastInfo = info;
				return info;
			}
		}
		return NULL;
	}

	// Linear search without a hash table
	for (int i = 0; i < FileInfos.Num(); i++)
	{
		FPakEntry* info = &FileInfos[i];
		if (!stricmp(info->Name, name))
		{
			LastInfo = info;
			return info;
		}
	}
	return NULL;
}
#endif

#endif // UNREAL4
