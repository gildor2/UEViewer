#include "Core.h"
#include "UnCore.h"
#include "UnPackage.h"


void FPackageFileSummary::Serialize2(FArchive &Ar)
{
	guard(FPackageFileSummary::Serialize2);

#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell && Ar.ArLicenseeVer >= 83)
	{
		int32 unk8;
		Ar << unk8;
	}
#endif // SPLINTER_CELL

	Ar << PackageFlags;

#if LEAD
	if (Ar.Game == GAME_SplinterCellConv)
	{
		int32 unk10;
		Ar << unk10;		// some flags?
	}
#endif // LEAD

	Ar << NameCount << NameOffset << ExportCount << ExportOffset << ImportCount << ImportOffset;

#if LEAD
	if (Ar.Game == GAME_SplinterCellConv && Ar.ArLicenseeVer >= 48)
	{
		// this game has additional name table for some packages
		int32 ExtraNameCount, ExtraNameOffset;
		int32 unkC;
		Ar << ExtraNameCount << ExtraNameOffset;
		if (ExtraNameOffset < ImportOffset) ExtraNameCount = 0;
		if (Ar.ArLicenseeVer >= 85) Ar << unkC;
		goto generations;	// skip Guid
	}
#endif // LEAD
#if SPLINTER_CELL
	if (Ar.Game == GAME_SplinterCell)
	{
		int32 tmp1;
		TArray<byte> tmp2;
		Ar << tmp1;								// 0xFF0ADDE
		Ar << tmp2;
	}
#endif // SPLINTER_CELL
#if RAGNAROK2
	if ((GForceGame == 0) && (PackageFlags & 0x10000) && (Ar.ArVer >= 0x80 && Ar.ArVer < 0x88))	//?? unknown upper limit; known lower limit: 0x80
	{
		// encrypted Ragnarok Online archive header (data taken by archive analysis)
		Ar.Game = GAME_Ragnarok2;
		NameCount    ^= 0xD97790C7 ^ 0x1C;
		NameOffset   ^= 0xF208FB9F ^ 0x40;
		ExportCount  ^= 0xEBBDE077 ^ 0x04;
		ExportOffset ^= 0xE292EC62 ^ 0x03E9E1;
		ImportCount  ^= 0x201DA87A ^ 0x05;
		ImportOffset ^= 0xA9B999DF ^ 0x003E9BE;
		return;									// other data is useless for us, and they are encrypted too
	}
#endif // RAGNAROK2
#if EOS
	if (Ar.Game == GAME_EOS && Ar.ArLicenseeVer >= 49) goto generations;
#endif

	// Guid and generations
	if (Ar.ArVer < 68)
	{
		// old generations code
		int32 HeritageCount, HeritageOffset;
		Ar << HeritageCount << HeritageOffset;	// not used
		if (Ar.IsLoading)
		{
			Generations.Empty(1);
			FGenerationInfo gen;
			gen.ExportCount = ExportCount;
			gen.NameCount   = NameCount;
			Generations.Add(gen);
		}
	}
	else
	{
		Ar << Guid;
		// current generations code
	generations:
		// always used int for generation count (even for UE1-2)
		int32 Count;
		Ar << Count;
		Generations.Empty(Count);
		Generations.AddZeroed(Count);
		for (int i = 0; i < Count; i++)
			Ar << Generations[i];
	}

	unguard;
}


void FObjectExport::Serialize2(FArchive &Ar)
{
	guard(FObjectExport::Serialize2);

#if USE_COMPACT_PACKAGE_STRUCTS
	int32		SuperIndex;
	uint32		ObjectFlags;
#endif

#if PARIAH
	if (Ar.Game == GAME_Pariah)
	{
		Ar << ObjectName << AR_INDEX(SuperIndex) << PackageIndex << ObjectFlags;
		Ar << AR_INDEX(ClassIndex) << AR_INDEX(SerialSize);
		if (SerialSize)
			Ar << AR_INDEX(SerialOffset);
		return;
	}
#endif // PARIAH
#if BIOSHOCK
	if (Ar.Game == GAME_Bioshock)
	{
		int unkC, flags2, unk30;
		Ar << AR_INDEX(ClassIndex) << AR_INDEX(SuperIndex) << PackageIndex;
		if (Ar.ArVer >= 132) Ar << unkC;			// unknown
		Ar << ObjectName << ObjectFlags;
		if (Ar.ArLicenseeVer >= 40) Ar << flags2;	// qword flags
		Ar << AR_INDEX(SerialSize);
		if (SerialSize)
			Ar << AR_INDEX(SerialOffset);
		if (Ar.ArVer >= 130) Ar << unk30;			// unknown
		return;
	}
#endif // BIOSHOCK
#if SWRC
	if (Ar.Game == GAME_RepCommando && Ar.ArVer >= 151)
	{
		int unk0C;
		Ar << AR_INDEX(ClassIndex) << AR_INDEX(SuperIndex) << PackageIndex;
		if (Ar.ArVer >= 159) Ar << AR_INDEX(unk0C);
		Ar << ObjectName << ObjectFlags << SerialSize << SerialOffset;
		return;
	}
#endif // SWRC
#if AA2
	if (Ar.Game == GAME_AA2)
	{
		int rnd;	// random value
		Ar << AR_INDEX(SuperIndex) << rnd << AR_INDEX(ClassIndex) << PackageIndex;
		Ar << ObjectFlags << ObjectName << AR_INDEX(SerialSize);	// ObjectFlags are serialized in different order, ObjectFlags are negated
		if (SerialSize)
			Ar << AR_INDEX(SerialOffset);
		ObjectFlags = ~ObjectFlags;
		return;
	}
#endif // AA2

	// generic UE1/UE2 code
	Ar << AR_INDEX(ClassIndex) << AR_INDEX(SuperIndex) << PackageIndex;
	Ar << ObjectName << ObjectFlags << AR_INDEX(SerialSize);
	if (SerialSize)
		Ar << AR_INDEX(SerialOffset);

	unguard;
}


#if UC2

void FObjectExport::Serialize2X(FArchive &Ar)
{
	guard(FObjectExport::Serialize2X);

#if USE_COMPACT_PACKAGE_STRUCTS
	int32		SuperIndex;
	uint32		ObjectFlags;
#endif

	Ar << ClassIndex << SuperIndex;
	if (Ar.ArVer >= 150)
	{
		int16 idx = PackageIndex;
		Ar << idx;
		PackageIndex = idx;
	}
	else
		Ar << PackageIndex;
	Ar << ObjectName << ObjectFlags << SerialSize;
	if (SerialSize)
		Ar << SerialOffset;
	// UC2 has strange thing here: indices are serialized as 4-byte int (instead of AR_INDEX),
	// but stored into 2-byte shorts
	ClassIndex = int16(ClassIndex);
	SuperIndex = int16(SuperIndex);

	unguard;
}

#endif // UC2

void UnPackage::LoadNameTable2()
{
	guard(UnPackage::LoadNameTable2);

	// Unreal engine 1 and 2 code

	// Korean games sometimes uses Unicode strings, so use FString for serialization
	FStaticString<MAX_FNAME_LEN> nameStr;

	for (int i = 0; i < Summary.NameCount; i++)
	{
		guard(Name);

		// UE1
		if (ArVer < 64)
		{
			char buf[MAX_FNAME_LEN];
			int len;
			for (len = 0; len < ARRAY_COUNT(buf); len++)
			{
				char c;
				*this << c;
				buf[len] = c;
				if (!c) break;
			}
			assert(len < ARRAY_COUNT(buf));
			NameTable[i] = appStrdupPool(buf);
			goto dword_flags;
		}

#if UC1 || PARIAH
		if (Game == GAME_UC1 && ArLicenseeVer >= 28)
		{
		uc1_name:
			// used uint16 + char[] instead of FString
			char buf[MAX_FNAME_LEN];
			uint16 len;
			*this << len;
			assert(len < ARRAY_COUNT(buf));
			Serialize(buf, len+1);
			NameTable[i] = appStrdupPool(buf);
			goto dword_flags;
		}
	#if PARIAH
		if (Game == GAME_Pariah && ((ArLicenseeVer & 0x3F) >= 28)) goto uc1_name;
	#endif
#endif // UC1 || PARIAH

#if SPLINTER_CELL
		if (Game == GAME_SplinterCell && ArLicenseeVer >= 85)
		{
			char buf[256];
			byte len;
			*this << len;
			Serialize(buf, len+1);
			NameTable[i] = appStrdupPool(buf);
			goto dword_flags;
		}
#endif // SPLINTER_CELL

#if LEAD
		if (Game == GAME_SplinterCellConv && ArVer >= 68)
		{
			char buf[MAX_FNAME_LEN];
			int len;
			*this << AR_INDEX(len);
			assert(len < ARRAY_COUNT(buf));
			Serialize(buf, len);
			buf[len] = 0;
			NameTable[i] = appStrdupPool(buf);
			goto done;
		}
#endif // LEAD

#if AA2
		if (Game == GAME_AA2)
		{
			guard(AA2_FName);
			char buf[MAX_FNAME_LEN];
			int len;
			*this << AR_INDEX(len);
			// read as unicode string and decrypt
			assert(len <= 0);
			len = -len;
			assert(len < ARRAY_COUNT(buf));
			char* d = buf;
			byte shift = 5;
			for (int j = 0; j < len; j++, d++)
			{
				uint16 c;
				*this << c;
				uint16 c2 = ROR16(c, shift);
				assert(c2 < 256);
				*d = c2 & 0xFF;
				shift = (c - 5) & 15;
			}
			NameTable[i] = appStrdupPool(buf);
			int unk;
			*this << AR_INDEX(unk);
			unguard;
			goto dword_flags;
		}
#endif // AA2

		*this << nameStr;

		VerifyName(nameStr, i);

		// Remember the name
	#if 0
		NameTable[i] = new char[name.Num()];
		strcpy(NameTable[i], *name);
	#else
		NameTable[i] = appStrdupPool(*nameStr);
	#endif

	#if BIOSHOCK
		if (Game == GAME_Bioshock)
		{
			// 64-bit flags, like in UE3
			uint64 flags64;
			*this << flags64;
			goto done;
		}
	#endif // BIOSHOCK

	dword_flags:
		uint32 flags32;
		*this << flags32;

	done: ;
#if DEBUG_PACKAGE
		PKG_LOG("Name[%d]: \"%s\"\n", i, NameTable[i]);
#endif
		unguardf("%d", i);
	}

	unguard;
}
