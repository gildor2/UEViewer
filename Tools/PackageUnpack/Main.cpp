#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"

#define DEF_UNP_DIR		"unpacked"
#define HOMEPAGE		"http://www.gildor.org/"


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
				"    -out=PATH          extract everything into PATH, default is \"" DEF_UNP_DIR "\"\n"
				"    -lzo|lzx|zlib      force compression method for fully-compressed packages\n"
				"\n"
				"Platform selection:\n"
				"    -ps3               override platform autodetection to PS3\n"
				"\n"
				"For details and updates please visit " HOMEPAGE "\n"
		);
		exit(0);
	}

	// parse command line
	char BaseDir[256];
	strcpy(BaseDir, DEF_UNP_DIR);

	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!strnicmp(opt, "out=", 4))
			{
				strcpy(BaseDir, opt+4);
			}
			else if (!stricmp(opt, "lzo"))
				GForceCompMethod = COMPRESS_LZO;
			else if (!stricmp(opt, "zlib"))
				GForceCompMethod = COMPRESS_ZLIB;
			else if (!stricmp(opt, "lzx"))
				GForceCompMethod = COMPRESS_LZX;
			else if (!stricmp(opt, "ps3"))
				GForcePlatform = PLATFORM_PS3;
			else
				goto help;
		}
		else
		{
			break;
		}
	}
	const char *argPkgName = argv[arg];
	if (!argPkgName) goto help;

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// load package
	UnPackage *Package;
	if (strchr(argPkgName, '.'))
		Package = new UnPackage(argPkgName);
	else
		Package = UnPackage::LoadPackage(argPkgName);
	if (!Package)
	{
		printf("ERROR: Unable to find/load package %s\n", argPkgName);
		exit(1);
	}

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

	byte *buffer = new byte[uncompressedSize];
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
		FILE *h = fopen(Package->Filename, "rb");
		fread(buffer, compressedStart, 1, h);
		fclose(h);

		FMemReader mem(buffer, compressedStart);
		mem.SetupFrom(*Package);

		int pos;
		bool found;

		// find package flags
		found = false;
		const FString &Group = Summary.PackageGroup;
		for (pos = 8; pos < 32; pos++)
		{
			mem.Seek(pos);
			int tmp;
			mem << tmp;
			if (tmp != Group.Num() && tmp != -Group.Num()) continue;	// ANSI or Unicode string (MassEffect3 has unicode here)
			mem.Seek(pos);
			FString tmp2;
			mem << tmp2;
			if (strcmp(tmp2, *Group) != 0) continue;
			int flagsPos = mem.Tell();
			mem << tmp;
			if (tmp != Summary.PackageFlags) continue;
			int *p = (int*)(buffer + flagsPos);
			*p &= (!Package->ReverseBytes) ? ~0x2000000 : ~0x2;	// remove PKG_Compressed flag
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
		int dstPos = pos + 8;	// skip CompressionFlags and CompressedChunks.Num
		int srcPos = pos + 8 + cut;	// skip CompressedChunks
		memcpy(buffer + dstPos, buffer + srcPos, compressedStart - srcPos);

		if (compressedStart - cut != uncompressedStart)
			appNotify("WARNING: wrong size of %s: differs in %d bytes", argPkgName, compressedStart - cut - uncompressedStart);

		// read package data
		Package->Seek(uncompressedStart);
		Package->Serialize(buffer + uncompressedStart, uncompressedSize - uncompressedStart);
	}
	else
	{
		// uncompressed package or fully compressed package
		guard(LoadFullyCompressedPackage);

		Package->Seek(0);
		Package->Serialize(buffer, uncompressedSize);

		unguard;
	}

	// write file
	if (fwrite(buffer, uncompressedSize, 1, out) != 1) appError("Write failed");

	// cleanup
	delete buffer;
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
