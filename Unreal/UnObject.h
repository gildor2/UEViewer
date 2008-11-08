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
		virtual const char* GetClassName() const\
		{ return #Class+1; }					\
		virtual bool IsA(const char *ClassName) const \
		{										\
			if (!strcmp(ClassName, #Class+1))	\
				return true;					\
			return Super::IsA(ClassName);		\
		}										\
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
#define PROP_DROP(Field)		{ #Field, NULL, 0, 0 },		// signal property, which should be dropped


#define BEGIN_PROP_TABLE						\
	virtual const CPropInfo *EnumProps(int index) const \
	{											\
		static const CPropInfo props[] =		\
		{
// simple types
// note: has special PROP_ENUM(), which is the same as PROP_BYTE(), but when using
// PROP_BYTE for enumeration value, _PROP_BASE macro will set 'Count' field of
// CPropInfo to 4 instead of 1 (enumeration values are serialized as byte, but
// compiler report it as 4-byte field)
#define PROP_ENUM(Field)		{ #Field, "byte", FIELD2OFS(ThisClass, Field), 1 },
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
		if (index >= ARRAY_COUNT(props))		\
			return Super::EnumProps(index - ARRAY_COUNT(props)); \
		return props + index;					\
	}											\


void RegisterClasses(CClassInfo *Table, int Count);
void UnregisterClass(const char *Name);
UObject *CreateClass(const char *Name);
bool IsKnownClass(const char *Name);


#define BEGIN_CLASS_TABLE						\
	{											\
		static CClassInfo Table[] = {
#define REGISTER_CLASS(Class)					\
			{ #Class+1, (UObject* (*) ())Class::StaticConstructor },
#define REGISTER_CLASS_ALIAS(Class,ClassName)	\
			{ #ClassName+1, (UObject* (*) ())Class::StaticConstructor },
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
	virtual const char *GetClassName() const
	{
		return "Object";
	}
	virtual bool IsA(const char *ClassName) const
	{
		return strcmp(ClassName, "Object") == 0;
	}
	virtual const CPropInfo *EnumProps(int index) const
	{
		return NULL;
	}
	const CPropInfo *FindProperty(const char *Name) const;
	void DumpProps() const;

//private: -- not private to allow object browser ...
	// static data and methods
	static int				GObjBeginLoadCount;
	static TArray<UObject*>	GObjLoaded;
	static TArray<UObject*> GObjObjects;

	static void BeginLoad();
	static void EndLoad();
};


#endif // __UNOBJECT_H__
