#ifndef __UNOBJECT_H__
#define __UNOBJECT_H__


/*-----------------------------------------------------------------------------
	Simple RTTI
-----------------------------------------------------------------------------*/

// NOTE: DECLARE_CLASS and REGISTER_CLASS will skip 1st class name char

#define DECLARE_BASE(Class,Base)				\
	public:										\
		typedef Class	ThisClass;				\
		typedef Base	Super;					\
		static void InternalConstructor(void *Mem) \
		{ new(Mem) ThisClass; }					\
		static const CTypeInfo *StaticGetTypeinfo() \
		{										\
			int numProps = 0;					\
			const CPropInfo *props = NULL;		\
			if ((int)PropLevel != (int)Super::PropLevel) \
				props = StaticGetProps(numProps); \
			static const CTypeInfo type(		\
				#Class,							\
				Super::StaticGetTypeinfo(),		\
				sizeof(ThisClass),				\
				props, numProps,				\
				InternalConstructor				\
			);									\
			return &type;						\
			return StaticGetTypeinfo();			\
		}

//!! can easily derive one structure from another
#define DECLARE_STRUCT(Class)					\
		DECLARE_BASE(Class, CNullType)

//?? can replace virtual GetClassName() with GetTypeinfo()->Name
#define DECLARE_CLASS(Class,Base)				\
		DECLARE_BASE(Class, Base)				\
		virtual const CTypeInfo *GetTypeinfo() const \
		{										\
			return StaticGetTypeinfo();			\
		}										\
	private:									\
		friend FArchive& operator<<(FArchive &Ar, Class *&Res) \
		{										\
			return Ar << *(UObject**)&Res;		\
		}


struct CPropInfo
{
	const char	   *Name;		// field name
	const char	   *TypeName;	// name of field type; simple type names used for verification only
	word			Offset;		// offset of field from class start
	short			Count;		// number of array items
};


struct CTypeInfo
{
	const char		*Name;
	const CTypeInfo *Parent;
	int				SizeOf;
	const CPropInfo *Props;
	int				NumProps;
	void (*Constructor)(void*);
	// methods
	inline CTypeInfo(const char *AName, const CTypeInfo *AParent, int DataSize,
					 const CPropInfo *AProps, int PropCount, void (*AConstructor)(void*))
	:	Name(AName)
	,	Parent(AParent)
	,	SizeOf(DataSize)
	,	Props(AProps)
	,	NumProps(PropCount)
	,	Constructor(AConstructor)
	{}
	inline bool IsClass() const
	{
		return Name[0] != 'F';		// UE structure type names are started with 'F'
	}
	bool IsA(const char *TypeName) const;
	const CPropInfo *FindProperty(const char *Name) const;
	void SerializeProps(FArchive &Ar, void *ObjectData) const;
	void DumpProps(void *Data) const;
};


// Helper class to simplify DECLARE_CLASS() macro group
// This class is used as Base for DECLARE_BASE()/DECLARE_CLASS() macros
struct CNullType
{
	enum { PropLevel = -1 };		// overriden in BEGIN_CLASS_TABLE
	static FORCEINLINE const CPropInfo *StaticGetProps(int &numProps)
	{
		numProps = 0;
		return NULL;
	}
	static FORCEINLINE const CTypeInfo *StaticGetTypeinfo()
	{
		return NULL;
	}
};


#define _PROP_BASE(Field,Type)	{ #Field, #Type, FIELD2OFS(ThisClass, Field), sizeof(((ThisClass*)NULL)->Field) / sizeof(Type) },
#define PROP_ARRAY(Field,Type)	{ #Field, #Type, FIELD2OFS(ThisClass, Field), -1 },
#define PROP_DROP(Field)		{ #Field, NULL, 0, 0 },		// signal property, which should be dropped


#define BEGIN_PROP_TABLE						\
	enum { PropLevel = Super::PropLevel + 1 };	\
	static FORCEINLINE const CPropInfo *StaticGetProps(int &numProps) \
	{											\
		static const CPropInfo props[] =		\
		{
// simple types
// note: has special PROP_ENUM(), which is the same as PROP_BYTE(), but when using
// PROP_BYTE for enumeration value, _PROP_BASE macro will set 'Count' field of
// CPropInfo to 4 instead of 1 (enumeration values are serialized as byte, but
// compiler report it as 4-byte field)
#define PROP_ENUM(Field)		{ #Field, "byte", FIELD2OFS(ThisClass, Field), 1 },
#if UNREAL3
#define PROP_ENUM3(Field)		{ #Field, "enum3", FIELD2OFS(ThisClass, Field), 1 },	//!! should change this
#endif
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
		numProps = ARRAY_COUNT(props);			\
		return props;							\
	}


struct CClassInfo
{
	const char		*Name;
	const CTypeInfo* (*TypeInfo)();
};


void RegisterClasses(CClassInfo *Table, int Count);
void UnregisterClass(const char *Name, bool WholeTree = false);

const CTypeInfo *FindClassType(const char *Name, bool ClassType = true);

FORCEINLINE const CTypeInfo *FindStructType(const char *Name)
{
	return FindClassType(Name, false);
}

UObject *CreateClass(const char *Name);
bool IsKnownClass(const char *Name);


#define BEGIN_CLASS_TABLE						\
	{											\
		static CClassInfo Table[] = {
#define REGISTER_CLASS(Class)					\
			{ #Class+1, Class::StaticGetTypeinfo },
#define REGISTER_CLASS_ALIAS(Class,ClassName)	\
			{ #ClassName+1, Class::StaticGetTypeinfo },
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
	DECLARE_BASE(UObject, CNullType)

public:
	// internal storage
	UnPackage		*Package;
	int				PackageIndex;	// index in package export table
	const char		*Name;
#if UNREAL3
	int				NetIndex;
#endif

//	unsigned	ObjectFlags;

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

	static void BeginLoad();
	static void EndLoad();

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

#endif // __UNOBJECT_H__
