#ifndef __UNOBJECT_H__
#define __UNOBJECT_H__

#include "TypeInfo.h"


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

class UObject
{
	DECLARE_BASE(UObject, CNullType)

public:
	// internal storage
	UnPackage		*Package;
	int				PackageIndex;	// index in package export table; INDEX_NONE for non-packaged (transient) object
	const char		*Name;
	UObject			*Outer;
#if UNREAL3
	int				NetIndex;
#endif

//	unsigned	ObjectFlags;

	UObject();
	virtual ~UObject();
	virtual void Serialize(FArchive &Ar);
	virtual void PostLoad()			// called after serializing all objects
	{}

	// RTTI support
	inline bool IsA(const char *ClassName) const
	{
		return GetTypeinfo()->IsA(ClassName);
	}
	inline const char *GetClassName() const
	{
		return GetTypeinfo()->Name + 1;
	}
	const char *GetRealClassName() const;		// class name from the package export table
	const char* GetPackageName() const;
	const char *GetUncookedPackageName() const;
	void GetFullName(char *buf, int bufSize, bool IncludeObjectName = true, bool IncludeCookedPackageName = true, bool ForcePackageName = false) const;

	enum { PropLevel = 0 };
	static FORCEINLINE const CPropInfo *StaticGetProps(int &numProps)
	{
		numProps = 0;
		return NULL;
	}
	virtual const CTypeInfo *GetTypeinfo() const
	{
		return StaticGetTypeinfo();
	}

//private: -- not private to allow object browser ...
	// static data and methods
	static int				GObjBeginLoadCount;
	static TArray<UObject*>	GObjLoaded;
	static TArray<UObject*> GObjObjects;
	static UObject			*GLoadingObj;

	static void BeginLoad();
	static void EndLoad();

	// accessing object's package properties (here just to exclude UnPackage.h whenever possible)
	const FArchive* GetPackageArchive() const;
	int GetGame() const;
	int GetArVer() const;
	int GetLicenseeVer() const;

	void *operator new(size_t Size)
	{
		return appMalloc(Size);
	}
	void *operator new(size_t Size, void *Mem)	// for inplace constructor
	{
		return Mem;
	}
	void operator delete(void *Object)
	{
		appFree(Object);
	}
};


UObject *CreateClass(const char *Name);
void RegisterCoreClasses();

#endif // __UNOBJECT_H__
