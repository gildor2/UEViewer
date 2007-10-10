#ifndef __UNOBJECT_H__
#define __UNOBJECT_H__


/*-----------------------------------------------------------------------------
	Simple RTTI
-----------------------------------------------------------------------------*/

// NOTE: DECLARE_CLASS and REGISTER_CLASS will skip 1st class name char

#define DECLARE_CLASS(Class,Base)				\
	public:										\
		typedef Class	ThisClass;				\
		typedef Base	Super;					\
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


struct CPropInfo
{
	const char *Name;		// field name
	const char *TypeName;	// name of field type; simple type names used for verification only
	word		Offset;		// offset of field from class start
	word		Count;		// number of array items
};

#define _PROP_BASE(Field,Type)	{ #Field, #Type, FIELD2OFS(ThisClass, Field), sizeof(Field) / sizeof(Type) },


#define BEGIN_PROP_TABLE						\
	virtual const CPropInfo *FindProperty(const char *Name) const \
	{											\
		static const CPropInfo props[] =		\
		{
// simple types
#define PROP_BYTE(Field)		_PROP_BASE(Field, byte     )
#define PROP_INT(Field)			_PROP_BASE(Field, int      )
#define PROP_BOOL(Field)		_PROP_BASE(Field, bool     )
#define PROP_FLOAT(Field)		_PROP_BASE(Field, float    )
#define PROP_NAME(Field)		_PROP_BASE(Field, FName    )
#define PROP_OBJ(Field)			_PROP_BASE(Field, UObject* )
// structure types; note: structure names corresponds to F<StrucName> C++ struc
#define PROP_VECTOR(Field)		_PROP_BASE(Field, FVector  )
#define PROP_ROTATOR(Field)		_PROP_BASE(Field, FRotator )
#define PROP_COLOR(Field)		_PROP_BASE(Field, FColor   )

#define END_PROP_TABLE							\
		};										\
		const CPropInfo *Prop = UObject::FindProperty(ARRAY_ARG(props), Name); \
		if (!Prop) Prop = Super::FindProperty(Name); \
		return Prop;							\
	}


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
	virtual void Serialize(FArchive &Ar);

	// RTTI support
	virtual const char *GetClassName()
	{
		return "Object";
	}
	virtual const CPropInfo *FindProperty(const char *Name) const
	{
		return NULL;
	}
	static const CPropInfo *FindProperty(const CPropInfo *Table, int Count, const char *PropName);

private:
	// static data and methods
	static int				GObjBeginLoadCount;
	static TArray<UObject*>	GObjLoaded;

public: //!!!! PRIVATE
	static void BeginLoad();
	static void EndLoad();
};


#endif // __UNOBJECT_H__
