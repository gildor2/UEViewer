#include "Core.h"
#include "UnCore.h"
#include "GameFileSystem.h"

#if GEARS4

struct FGears4AssetEntry
{
	FString AssetName;
	int32 p1, p2, p3;		// p1 -> large number, p2 -> Array1 index
	uint8 p4;

	friend FArchive& operator<<(FArchive& Ar, FGears4AssetEntry& E)
	{
		return Ar << E.AssetName << E.p1 << E.p2 << E.p3 << E.p4;
	}
};

struct FGears4BundleEntry
{
	int32 SomeNum;			// -> index at Assets (note: p1.X is the same, but array!)
	TArray<FIntPoint> p1;	// TArray { int1,int2 }: int1 -> index at Assets, int2 = Assets[index].p1
	TArray<int32> p2;

	friend FArchive& operator<<(FArchive& Ar, FGears4BundleEntry& E)
	{
		Ar << E.SomeNum;
		Ar << E.p1;
		Ar << E.p2;
		return Ar;
	}
};

struct FGears4Manifest
{
	// same size of arrays
	TArray<FGears4AssetEntry> Assets;
	TArray<FIntPoint> Array4;	// TArray<bool32,int32>: int32 -> Array3/Entries index

	// same size of arrays
	TArray<FGears4BundleEntry> Bundles;
	TArray<byte> Array3;

	TArray<int32> Array1;		// -> Assets index
	TArray<int32> Array2;		// ? -> Assets index

	int64 p1;
	int64 p2;

	int Serialize(FArchive& Ar)
	{
		guard(FGears4Manifest::Serialize);

		// Header
		uint32 Magic;
		int32 Version;
		Ar << Magic << Version;
		if (Magic != 0x4C444E42)
			appError("Wrong Gears4 manifest magic");
		if (Version != 3)
			return 1;

		// Asset list
		uint32 Magic2;
		int32 Version2;
		Ar << Magic2 << Version2;
		if (Magic2 != 0xABAD1DEA)
			return 2;
		if (Version2 != 3)
			return 3;

		Ar << Assets;
		Ar << Array1;
		Ar << Array2;

		Ar << Bundles;

		if (Version >= 1)
		{
			Ar << Array3;
		}
		if (Version >= 2)
		{
			Ar << p1 << p2;
		}

		Array4.AddUninitialized(Assets.Num());
		for (int i = 0; i < Array4.Num(); i++)
		{
			Ar << Array4[i];
		}

		return 0;

		unguard;
	}
};

void LoadGears4Manifest(const CGameFileInfo* info)
{
	guard(LoadGears4Manifest);

	appPrintf("Loading Gears4 manifest file %s\n", info->RelativeName);

	FArchive* loader = appCreateFileReader(info);
	assert(loader);
	loader->Game = GAME_Gears4;

	FGears4Manifest Manifest;
	int error = Manifest.Serialize(*loader);
	if (error != 0)
	{
		appError("Wrong Gears4 manifest format (error %d)", error);
	}

	delete loader;

	//!!!!!
	appPrintf("\n***\nAssets:%d A1:%d A2:%d Bundles:%d A3:%d A4:%d\n", Manifest.Assets.Num(),
		Manifest.Array1.Num(), Manifest.Array2.Num(), Manifest.Bundles.Num(), Manifest.Array3.Num(), Manifest.Array4.Num());

#define ANALYZE(arr,field) \
	{ \
		int vmin = 9999999, vmax = -9999999; \
		for (int i = 0; i < arr.Num(); i++) \
		{ \
			int n = arr[i] field; \
			if (n < vmin) vmin = n; \
			if (n > vmax) vmax = n; \
		} \
		appPrintf(STR(arr) STR(field)" = [%d,%d]\n", vmin, vmax); \
	}

#define ANALYZE2(arr,field1,field2) \
	{ \
		int vmin = 9999999, vmax = -9999999; \
		for (int i = 0; i < arr.Num(); i++) \
		{ \
			const auto& a2 = arr[i] field1; \
			for (int j = 0; j < a2.Num(); j++) \
			{ \
			    int n = a2[j] field2; \
				if (n < vmin) vmin = n; \
				if (n > vmax) vmax = n; \
			} \
		} \
		appPrintf(STR(arr) STR(field1) STR(field2) " = [%d,%d]\n", vmin, vmax); \
	}

	ANALYZE(Manifest.Assets, .p1);
	ANALYZE(Manifest.Assets, .p2);
	ANALYZE(Manifest.Assets, .p3);
	ANALYZE(Manifest.Assets, .p4);
	ANALYZE(Manifest.Array4, .X);
	ANALYZE(Manifest.Array4, .Y);
	ANALYZE(Manifest.Array1, );
	ANALYZE(Manifest.Array2, );
	ANALYZE(Manifest.Bundles, .SomeNum);
	ANALYZE2(Manifest.Bundles, .p1, .X);
	ANALYZE2(Manifest.Bundles, .p1, .Y);
	ANALYZE2(Manifest.Bundles, .p2, );

	for (int i = 0; i < Manifest.Bundles.Num(); i++)
	{
		const FGears4BundleEntry& E2 = Manifest.Bundles[i];
		for (int j = 0; j < E2.p1.Num(); j++)
		{
			if (E2.p1[j].X == 55430)
			{
				appPrintf("Found in %d (size=%d)\n", i, E2.p1[j].Y);
			}
		}
	}

#define DIR_REF(label,index) appPrintf("%s[%d] = %s\n", label, index, *Manifest.Assets[index].AssetName)

	const char* find[] =
	{
//		"/game/effects/sparta/global/fire/materials/burningember",
//		"/game/design/mp/overhead_maps/gridlock_overhead_map",
		"/game/interface/options/options_button_backing",
//		"/game/characters/locust/brumak/gp01/brumakmesh_skeleton",
	};
	for (int ii = 0; ii < ARRAY_COUNT(find); ii++)
	for (int i = 0; i < Manifest.Assets.Num(); i++)
	{
		const FGears4AssetEntry& E = Manifest.Assets[i];
		if (!stricmp(*E.AssetName, find[ii]))
		{
			// Assets
			appPrintf("\n***\nFound %s at index %d\n", *E.AssetName, i);
			appPrintf("p1=%d p2=%d p3=%d p4=%d\n", E.p1, E.p2, E.p3, E.p4);
			appPrintf("Array4[%d] = { %d, %d }\n", i, Manifest.Array4[i].X, Manifest.Array4[i].Y);
			// Array1
			int Array1Index = E.p2;
			DIR_REF("Array1", Manifest.Array1[Array1Index]);
			// Entry
			int EntryIndex = Manifest.Array4[i].Y;
			const FGears4BundleEntry& Bundle = Manifest.Bundles[EntryIndex];
			appPrintf("Bundle[%d] = %d, [%d], [%d]\n", EntryIndex, Bundle.SomeNum, Bundle.p1.Num(), Bundle.p2.Num());
			ANALYZE(Bundle.p1, .X);
			for (int i2 = 0; i2 < Bundle.p1.Num(); i2++)
				DIR_REF("Bundle.p1", Bundle.p1[i2].X);
			ANALYZE(Bundle.p1, .Y);
			ANALYZE(Bundle.p2, );
			break;
		}
	}

	// breakpoint
///	assert(0);

	unguard;
}

#endif // GEARS4
