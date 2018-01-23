#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"
#include "GameDatabase.h"

#define DEF_UNP_DIR		"unpacked"
#define HOMEPAGE		"http://www.gildor.org/"


static void CopyStream(FArchive *Src, FILE *Dst, int Count)
{
	byte buffer[16384];

	while (Count > 0)
	{
		int Size = min(Count, sizeof(buffer));
		Src->Serialize(buffer, Size);
		if (fwrite(buffer, Size, 1, Dst) != 1) appError("Write failed");
		Count -= Size;
	}
}

#if UNREAL4

int UE4UnversionedPackage(int verMin, int verMax)
{
	appError("Unversioned UE4 packages are not supported. Please restart UModel and select UE4 version in range %d-%d using UI or command line.", verMin, verMax);
	return -1;
}

bool UE4EncryptedPak()
{
	return false;
}

#endif // UNREAL4


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
#if DO_GUARD
	TRY {
#endif

	guard(Main);

	// display usage
	if (argc < 2)
	{
	help:
		printf(	"Unreal Engine package decompressor\n"
				"Usage: decompress [options] <package filename>\n"
				"\n"
				"Options:\n"
				"    -path=PATH      path to game installation directory; if not specified,\n"
				"                    program will search for packages in current directory\n"
				"    -game=tag       override game autodetection (see -taglist for variants)\n"
				"    -out=PATH       extract everything into PATH, default is \"" DEF_UNP_DIR "\"\n"
				"    -lzo|lzx|zlib   force compression method for fully-compressed packages\n"
				"    -log=file       write log to the specified file\n"
				"    -taglist        list of tags to override game autodetection\n"
				"    -help           display this help page\n"
				"\n"
				"Platform selection:\n"
				"    -ps3            override platform autodetection to PS3\n"
				"\n"
				"For details and updates please visit " HOMEPAGE "\n"
		);
		exit(0);
	}

	// parse command line
	char BaseDir[256];
	strcpy(BaseDir, DEF_UNP_DIR);

	const char *argPkgName = NULL;

	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		const char *opt = argv[arg];
		if (opt[0] != '-')
		{
			if (argPkgName)
			{
				appPrintf("Package name already specified (passed \"%s\" and \"%s\")\n", argPkgName, opt);
				return 1;
			}
			argPkgName = opt;
			continue;
		}

		opt++;			// skip '-'

		if (!strnicmp(opt, "log=", 4))
		{
			appOpenLogFile(opt+4);
		}
		else if (!strnicmp(opt, "path=", 5))
		{
			appSetRootDirectory(opt+5);
		}
		else if (!strnicmp(opt, "out=", 4))
		{
			strcpy(BaseDir, opt+4);
		}
		else if (!strnicmp(opt, "game=", 5))
		{
			int tag = FindGameTag(opt+5);
			if (tag == -1)
			{
				appPrintf("ERROR: unknown game tag \"%s\". Use -taglist option to display available tags.\n", opt+5);
				exit(0);
			}
			GForceGame = tag;
		}
		else if (!stricmp(opt, "lzo"))
			GForceCompMethod = COMPRESS_LZO;
		else if (!stricmp(opt, "zlib"))
			GForceCompMethod = COMPRESS_ZLIB;
		else if (!stricmp(opt, "lzx"))
			GForceCompMethod = COMPRESS_LZX;
		else if (!stricmp(opt, "ps3"))
			GForcePlatform = PLATFORM_PS3;
		else if (!stricmp(opt, "taglist"))
		{
			PrintGameList(true);
			return 0;
		}
		else if (!stricmp(opt, "help"))
		{
			goto help;
		}
		else
		{
			appPrintf("decompress: invalid option: %s\n", opt);
			return 1;
		}
	}
	if (!argPkgName) goto help;

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// load a package
	UnPackage *Package = UnPackage::LoadPackage(argPkgName);
	if (!Package)
	{
		printf("ERROR: Unable to find/load package %s\n", argPkgName);
		exit(1);
	}
	// prepare package for reading
	Package->Open();

	guard(ProcessPackage);
	// extract package name, create directory for it
	char PkgName[256];
	const char *s = strrchr(argPkgName, '/');
	if (!s) s = strrchr(argPkgName, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = argPkgName;
	appStrncpyz(PkgName, s, ARRAY_COUNT(PkgName));

	char OutFile[256];
	appSprintf(ARRAY_ARG(OutFile), "%s/%s", BaseDir, PkgName);
	appMakeDirectoryForFile(OutFile);

	const FPackageFileSummary &Summary = Package->Summary;
	int uncompressedSize = Package->GetFileSize();
	if (uncompressedSize == 0) appError("GetFileSize for %s returned 0", argPkgName);
	printf("%s: uncompressed size %d\n", argPkgName, uncompressedSize);

	FILE *out = fopen(OutFile, "wb");

	/*!! Notes:
	 *	- GOW1 (XBox360 core.u) is not decompressed
	 *	- Bioshock core.u is not decompressed (because it has non-full compression, but
	 *	  CompressionFlags are 0) -- should place compressed chunks to Package.Summary
	 */
	if (Summary.CompressionFlags)
	{
		// compressed package, but header is not compressed

		// read header (raw)
		int compressedStart   = Summary.CompressedChunks[0].CompressedOffset;
		int uncompressedStart = Summary.CompressedChunks[0].UncompressedOffset;

		// use game file system to access file to avoid any troubles with locating file
		const CGameFileInfo* info = appFindGameFile(Package->Filename);
		FArchive* h;
		if (info)
		{
			// if file was loaded using search inside game path, use game filesystem, because
			// direct use of package name may not work
			h = appCreateFileReader(info);
		}
		else
		{
			// if file for some reason was not registered, open it using file name
			h = new FFileReader(argPkgName);
		}
		assert(h);
		byte *buffer = new byte[compressedStart];
		h->Serialize(buffer, compressedStart);
		delete h;

		FMemReader mem(buffer, compressedStart);
		mem.SetupFrom(*Package);

		int pos;
		bool found;

		// find package flags
		found = false;
		const FString &Group = Summary.PackageGroup;
		int DataArrayLen = Group.GetDataArray().Num();
		for (pos = 8; pos < 48; pos++)
		{
			mem.Seek(pos);
			int tmp;
			mem << tmp;
			if (tmp != DataArrayLen && tmp != -DataArrayLen) continue;	// ANSI or Unicode string (MassEffect3 has unicode here)
			mem.Seek(pos);
			FString tmp2;
			mem << tmp2;
			if (strcmp(*tmp2, *Group) != 0) continue;
			int flagsPos = mem.Tell();
			mem << tmp;
			if (tmp != Summary.PackageFlags) continue;
			int *p = (int*)(buffer + flagsPos);
			*p &= (!Package->ReverseBytes) ? ~0x2000000 : ~0x2;	// remove PKG_StoreCompressed flag (2 variants for different byte order)
			found = true;
			break;
		}
		if (!found) appError("Unable to find package flags");

		// find compression info in a header
		for (pos = 32; pos < compressedStart - Summary.CompressedChunks.Num() * 16; pos++)
		{
			mem.Seek(pos);
			int tmpCompressionFlags, tmpNumChunks;
			mem << tmpCompressionFlags << tmpNumChunks;
			if (tmpCompressionFlags != Summary.CompressionFlags || tmpNumChunks != Summary.CompressedChunks.Num())
				continue;
			// validate table
			bool valid = true;
			for (int i = 0; i < Summary.CompressedChunks.Num(); i++)
			{
				FCompressedChunk RC;
				const FCompressedChunk &C = Summary.CompressedChunks[i];
				mem << RC;
				if (C.UncompressedOffset != RC.UncompressedOffset ||
					C.UncompressedSize   != RC.UncompressedSize   ||
					C.CompressedOffset   != RC.CompressedOffset   ||
					C.CompressedSize     != RC.CompressedSize)
				{
					valid = false;
					break;
				}
			}
			if (!valid) continue;
			found = true;
			break;
		}
		if (!found) appError("Unable to find compression table");
//		printf("TABLE at %X\n", pos);

		// remove compression table
		int* p = (int*)(buffer + pos);
		p[0] = p[1] = 0;
		int cut = Summary.CompressedChunks.Num() * 16;
#if BULLETSTORM
		if (Package->Game == GAME_Bulletstorm)
			cut = Summary.CompressedChunks.Num() * 20;
#endif
#if MKVSDC
		if (Package->Game == GAME_MK && Package->ArVer >= 677) // MK X
			cut = Summary.CompressedChunks.Num() * 24;
#endif
		int dstPos = pos + 8;	// skip CompressionFlags and CompressedChunks.Num
		int srcPos = pos + 8 + cut;	// skip CompressedChunks
		memcpy(buffer + dstPos, buffer + srcPos, compressedStart - srcPos);

		if (compressedStart - cut != uncompressedStart)
			appNotify("WARNING: wrong size of %s: differs in %d bytes", argPkgName, compressedStart - cut - uncompressedStart);

		// write the header
		if (fwrite(buffer, uncompressedStart, 1, out) != 1) appError("Write failed");
		delete buffer;

		// copy remaining data
		Package->Seek(uncompressedStart);
		CopyStream(Package, out, uncompressedSize - uncompressedStart);
	}
	else
	{
		// uncompressed package or fully compressed package
		guard(LoadFullyCompressedPackage);

		Package->Seek(0);
		CopyStream(Package, out, uncompressedSize);

		unguard;
	}

	// cleanup
	fclose(out);

	unguard;

	unguard;

#if DO_GUARD
	} CATCH {
		if (GErrorHistory[0])
		{
//			printf("ERROR: %s\n", GErrorHistory);
			appNotify("ERROR: %s\n", GErrorHistory);
		}
		else
		{
//			printf("Unknown error\n");
			appNotify("Unknown error\n");
		}
		exit(1);
	}
#endif
	return 0;
}
