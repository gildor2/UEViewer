#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"

#include "UnArchivePak.h"

#if UNREAL4

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
#if TEKKEN7
	if (GForceGame == GAME_Tekken7)
		bEncrypted = false;		// Tekken 7 has 'bEncrypted' flag set, but actually there's no encryption
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

void FPakFile::Serialize(void *data, int size)
{
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
					UncompressedBuffer = (byte*)appMalloc((int)Info->CompressionBlockSize); // size of uncompressed block
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
					CompressedData = (byte*)appMalloc(CompressedBlockSize);
					Reader->Seek64(Block.CompressedStart);
					Reader->Serialize(CompressedData, CompressedBlockSize);
				}
				else
				{
					int EncryptedSize = Align(CompressedBlockSize, EncryptionAlign);
					CompressedData = (byte*)appMalloc(EncryptedSize);
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
			UncompressedBuffer = (byte*)appMalloc(EncryptedBufferSize);
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
	unguardf("file=%s", Info->Name);
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

	// Pak file may have different header sizes, try them all
	static const int OffsetsToTry[] = { FPakInfo::Size, FPakInfo::Size8, FPakInfo::Size8a };
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
			if (Offset == FPakInfo::Size8a)
			{
				assert(info.Version >= 8);
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

	// Manage pak files with encrypted index
	FMemReader* InfoReaderProxy = NULL;
	byte* InfoBlock = NULL;
	FArchive* InfoReader = reader;

	if (info.bEncryptedIndex)
	{
		guard(CheckEncryptedIndex);

		InfoBlock = new byte[info.IndexSize];
		reader->Serialize(InfoBlock, info.IndexSize);
		appDecryptAES(InfoBlock, info.IndexSize);
		InfoReaderProxy = new FMemReader(InfoBlock, info.IndexSize);
		InfoReaderProxy->SetupFrom(*reader);
		InfoReader = InfoReaderProxy;

		// Try to validate the decrypted data. The first thing going here is MountPoint which is FString.
		int32 StringLen;
		*InfoReader << StringLen;
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
				InfoReader->Seek(InfoReader->Tell() - (StringLen - 1) * 2);
				uint16 c;
				*InfoReader << c;
				bFail = (c != 0);
			}
			else
			{
				InfoReader->Seek(InfoReader->Tell() + StringLen - 1);
				char c;
				*InfoReader << c;
				bFail = (c != 0);
			}
		}
		if (bFail)
		{
			delete[] InfoBlock;
			delete InfoReaderProxy;
			char buf[1024];
			appSprintf(ARRAY_ARG(buf), "WARNING: The provided encryption key doesn't work with \"%s\". Skipping.", *Filename);
			error = buf;
			return false;
		}

		// Data is ok, seek to data start.
		InfoReader->Seek(0);

		unguard;
	}

	// this file looks correct, store 'reader'
	Reader = reader;

	// Read pak index

	FStaticString<MAX_PACKAGE_PATH> MountPoint;

	TRY {
		// Read MountPoint with catching error, to override error message.
		*InfoReader << MountPoint;
	} CATCH {
		if (info.bEncryptedIndex)
		{
			// Display nice error message
			appError("Error during loading of encrypted pak file index. Probably the provided AES key is not correct.");
		}
		else
		{
			THROW_AGAIN;
		}
	}

	// Read number of files
	int32 count;
	*InfoReader << count;
	if (!count)
	{
		appPrintf("Empty pak file \"%s\"\n", *Filename);
		return true;
	}

	// Process MountPoint
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

	FileInfos.AddZeroed(count);

	int numEncryptedFiles = 0;
	for (int i = 0; i < count; i++)
	{
		guard(ReadInfo);

		FPakEntry& E = FileInfos[i];
		// serialize name, combine with MountPoint
		FStaticString<MAX_PACKAGE_PATH> Filename;
		*InfoReader << Filename;
		FStaticString<MAX_PACKAGE_PATH> CombinedPath;
		CombinedPath = MountPoint;
		CombinedPath += Filename;
		// compact file name
		CompactFilePath(CombinedPath);
		// allocate file name in pool
		E.Name = appStrdupPool(*CombinedPath);
		// serialize other fields
		E.Serialize(*InfoReader);
		if (E.bEncrypted)
		{
//			appPrintf("Encrypted file: %s\n", *Filename);
			numEncryptedFiles++;
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

		unguardf("Index=%d/%d", i, count);
	}
	if (count >= MIN_PAK_SIZE_FOR_HASHING)
	{
		// Hash everything
		for (int i = 0; i < count; i++)
		{
			AddFileToHash(&FileInfos[i]);
		}
	}
	// Cleanup
	if (InfoBlock)
	{
		delete[] InfoBlock;
		delete InfoReaderProxy;
	}

	// Print statistics
	appPrintf("Pak %s: %d files", *Filename, count);
	if (numEncryptedFiles)
		appPrintf(" (%d encrypted)", numEncryptedFiles);
	if (strcmp(*MountPoint, "/") != 0)
		appPrintf(", mount point: \"%s\"", *MountPoint);
	appPrintf(", version %d\n", info.Version);

	return true;

	unguardf("PakVer=%d.%d", mainVer, subVer);
}

const FPakEntry* FPakVFS::FindFile(const char* name)
{
	if (LastInfo && !stricmp(LastInfo->Name, name))
		return LastInfo;

	if (HashTable)
	{
		// Have a hash table, use it
		uint16 hash = GetHashForFileName(name);
		for (FPakEntry* info = HashTable[hash]; info; info = info->HashNext)
		{
			if (!stricmp(info->Name, name))
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

#endif // UNREAL4
