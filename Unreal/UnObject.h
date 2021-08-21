#ifndef __UNOBJECT_H__
#define __UNOBJECT_H__

#include "TypeInfo.h"


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

class UObject
{
#if 0
	DECLARE_BASE(UObject, CNullType)
#else
public:
	// This declaration doesn't have indirection to CNullType::StaticGetProps and therefore is completely 'const' and inlined.
	// DECLARE_BASE constructs CTypeInfo with reference to CNullType, what makes compiler impossible to optimize it.
	static const CTypeInfo* StaticGetTypeinfo()
	{
		static const CTypeInfo type("UObject", NULL, sizeof(UObject), NULL, 0, TYPE_None, NULL, NULL);
		return &type;
	}
#endif

public:
	// internal storage
	UnPackage*		Package;
	const char*		Name;
	UObject*		Outer;			// UObject containing this UObject (e.g. UAnimSet holding UAnimSequence). Not really used here.
	int				PackageIndex;	// index in package export table; INDEX_NONE for non-packaged (transient) object
#if UNREAL3
	int32			NetIndex;
#endif

//	unsigned	ObjectFlags;

	UObject();
	virtual ~UObject();
	virtual void Serialize(FArchive &Ar);
	virtual void PostLoad()			// called after serialization of all objects
	{}

	// RTTI support
	inline bool IsA(const char* ClassName) const
	{
		return GetTypeinfo()->IsA(ClassName);
	}
	inline const char* GetClassName() const
	{
		return GetTypeinfo()->Name + 1;
	}
	const char* GetRealClassName() const;		// class name from the package export table
	const char* GetPackageName() const;
	const char* GetUncookedPackageName() const;
	// Get full object path in following format: "OutermostPackage.Package1...PackageN.ObjectName".
	// IncludeCookedPackageName - use "uncooked" package name for UE3 game
	// ForcePackageName - append package name even if it's missing in export table
	void GetFullName(char *buf, int bufSize, bool IncludeObjectName = true, bool IncludeCookedPackageName = true, bool ForcePackageName = false) const;

	// Function which collects metadata from the object into FArchive, it might be useful for quick comparison of 2 objects
	// during export. It's relevant only for UE3 packages in "uncook" mode, when 2 objects with different quality may match
	// the same name.
	virtual void GetMetadata(FArchive& Ar) const
	{
	}

	static const uint32 LocalTypeFlags = TYPE_None;

	// Empty property table
	enum { PropLevel = -1 };
	static constexpr FORCEINLINE const CPropInfo* StaticGetProps(int &numProps)
	{
		numProps = 0;
		return NULL;
	}
	virtual const CTypeInfo* GetTypeinfo() const
	{
		return StaticGetTypeinfo();
	}

//private: -- not private to allow object browser ...
	// static data and methods
	static int				GObjBeginLoadCount;
	static TArray<UObject*>	GObjLoaded;
	static TArray<UObject*> GObjObjects;
	static UObject*			GLoadingObj;

	static void BeginLoad();
	static void EndLoad();

	// accessing object's package properties (here just to exclude UnPackage.h whenever possible)
	const FArchive* GetPackageArchive() const;
	int GetGame() const;
	int GetArVer() const;
	int GetLicenseeVer() const;

	void* operator new(size_t Size)
	{
		return appMalloc(Size);
	}
	void* operator new(size_t Size, void *Mem)	// for inplace constructor
	{
		return Mem;
	}
	void operator delete(void* Object)
	{
		appFree(Object);
	}
};


UObject *CreateClass(const char *Name);
void RegisterCoreClasses();

// This callback is called before placing the object into serialization queue (GObjLoaded). If it returns
// false, serialization function will not be called.
extern bool (*GBeforeLoadObjectCallback)(UObject*);

#endif // __UNOBJECT_H__
