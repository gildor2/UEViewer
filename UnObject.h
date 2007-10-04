#ifndef __UNOBJECT_H__
#define __UNOBJECT_H__


/*-----------------------------------------------------------------------------
	Simple RTTI
-----------------------------------------------------------------------------*/

// NOTE: DECLARE_CLASS and REGISTER_CLASS will skip 1st class name char

#define DECLARE_CLASS(Class)					\
	public:										\
		typedef Class ThisClass;				\
		static ThisClass* StaticConstructor()	\
		{ return new ThisClass; }				\
		virtual const char* GetClassName()		\
		{ return #Class+1; }					\
	private:									\
		friend FArchive& operator<<(FArchive &Ar, Class *&Res) \
		{										\
			return Ar << *(UObject**)&Res;		\
		}


struct CClassInfo
{
	const char *Name;
	UObject* (*Constructor)();
};


void RegisterClasses(CClassInfo *Table, int Count);
UObject *CreateClass(const char *Name);


#define BEGIN_CLASS_TABLE						\
	{											\
		static CClassInfo Table[] = {
#define REGISTER_CLASS(Class)					\
			{ #Class+1, (UObject* (*) ())Class::StaticConstructor },
#define END_CLASS_TABLE							\
		};										\
		RegisterClasses(ARRAY_ARG(Table));		\
	}


/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

class UObject
{
	friend class UnPackage;

public:
	// internal storage
	UnPackage		*Package;
	int				PackageIndex;	// index in package export table
	const char		*Name;

//	unsigned	ObjectFlags;

	virtual ~UObject();

	virtual const char *GetClassName()
	{
		return "Object";
	}

	virtual void Serialize(FArchive &Ar)
	{
		// stack frame
//		assert(!(ObjectFlags & RF_HasStack));
		// property list
		int tmp;					// really, FName
		Ar << AR_INDEX(tmp);
//		assert(tmp == 0);			// index of "None"
	}

private:
	// static data and methods
	static int				GObjBeginLoadCount;
	static TArray<UObject*>	GObjLoaded;
public: //!!!! PRIVATE
	static void BeginLoad();
	static void EndLoad();
};


#endif // __UNOBJECT_H__
