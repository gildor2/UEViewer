#ifndef __TYPEINFO_H__
#define __TYPEINFO_H__

// Comparing PropLevel with Super::PropLevel to detect whether we have property
// table in current class or not
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
		}

#define DECLARE_STRUCT(Class)					\
		DECLARE_BASE(Class, CNullType)			\
		const CTypeInfo *GetTypeinfo() const	\
		{										\
			return StaticGetTypeinfo();			\
		}

// structure derived from another structure with typeinfo
#define DECLARE_STRUCT2(Class,Base)				\
		DECLARE_BASE(Class, Base)				\
		const CTypeInfo *GetTypeinfo() const	\
		{										\
			return StaticGetTypeinfo();			\
		}

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
	const char	   *TypeName;	// name of the field type
	uint16			Offset;		// offset of this field from the class start
	int16			Count;		// number of array items
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
	FORCEINLINE CTypeInfo(const char *AName, const CTypeInfo *AParent, int DataSize,
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
		return (Name[0] == 'U') || (Name[0] == 'A');	// UE structure type names are started with 'F', classes with 'U' or 'A'
	}
	bool IsA(const char *TypeName) const;
	const CPropInfo *FindProperty(const char *Name) const;
	static void RemapProp(const char *Class, const char *OldName, const char *NewName);

	// Serialize Unreal engine UObject property block
	void SerializeUnrealProps(FArchive &Ar, void *ObjectData) const;

	void DumpProps(void *Data) const;
	void SaveProps(void *Data, FArchive& Ar) const;
	bool LoadProps(void *Data, FArchive& Ar) const;
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
#define PROP_STRUC(Field,Type)	_PROP_BASE(Field, Type)
#define PROP_DROP(Field)		{ #Field, NULL, 0, 0 },		// signal property, which should be dropped


// BEGIN_PROP_TABLE/END_PROP_TABLE declares property table inside class declaration
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
#define PROP_ENUM(Field)		{ #Field, "byte",   FIELD2OFS(ThisClass, Field), 1 },
#define PROP_ENUM2(Field,Type)	{ #Field, "#"#Type, FIELD2OFS(ThisClass, Field), 1 },
#define PROP_BYTE(Field)		_PROP_BASE(Field, byte     )
#define PROP_INT(Field)			_PROP_BASE(Field, int      )
#define PROP_BOOL(Field)		_PROP_BASE(Field, bool     )
#define PROP_FLOAT(Field)		_PROP_BASE(Field, float    )
#define PROP_NAME(Field)		_PROP_BASE(Field, FName    )
#define PROP_STRING(Field)		_PROP_BASE(Field, FString  )
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

#if DECLARE_VIEWER_PROPS
#define VPROP_ARRAY_COUNT(Field,Name)	{ #Name, "int", FIELD2OFS(ThisClass, Field) + ARRAY_COUNT_FIELD_OFFSET, 1 },
#endif // DECLARE_VIEWER_PROPS

// Mix of BEGIN_PROP_TABLE and DECLARE_BASE
// Allows to declare property table outside of class declaration
#define BEGIN_PROP_TABLE_EXTERNAL(Class)		\
static const CTypeInfo* Class##_StaticGetTypeinfo() \
{												\
	static const char ClassName[] = #Class;		\
	typedef Class ThisClass;					\
	static const CPropInfo props[] =			\
	{

// Mix of END_PROP_TABLE and DECLARE_BASE
#define END_PROP_TABLE_EXTERNAL					\
	};											\
	static const CTypeInfo type(				\
		ClassName,								\
		NULL,									\
		sizeof(ThisClass),						\
		props, ARRAY_COUNT(props),				\
		NULL									\
	);											\
	return &type;								\
}

struct CClassInfo
{
	const char		*Name;
	const CTypeInfo* (*TypeInfo)();
};


void RegisterClasses(const CClassInfo *Table, int Count);
void UnregisterClass(const char *Name, bool WholeTree = false);

const CTypeInfo *FindClassType(const char *Name, bool ClassType = true);

FORCEINLINE const CTypeInfo *FindStructType(const char *Name)
{
	return FindClassType(Name, false);
}

FORCEINLINE bool IsKnownClass(const char *Name)
{
	return (FindClassType(Name) != NULL);
}


#define BEGIN_CLASS_TABLE						\
	{											\
		static const CClassInfo Table[] = {
#define REGISTER_CLASS(Class)					\
			{ #Class, Class::StaticGetTypeinfo },
// Class with BEGIN_PROP_TABLE_EXTERNAL/END_PROP_TABLE_EXTERNAL property table
#define REGISTER_CLASS_EXTERNAL(Class)			\
			{ #Class, Class##_StaticGetTypeinfo },
#define REGISTER_CLASS_ALIAS(Class,ClassName)	\
			{ #ClassName, Class::StaticGetTypeinfo },
#define END_CLASS_TABLE							\
		};										\
		RegisterClasses(ARRAY_ARG(Table));		\
	}


struct enumToStr
{
	int			value;
	const char* name;
};

template<class T>
struct TEnumInfo
{
	FORCEINLINE static const char* GetName()
	{
	#ifndef __GNUC__ // this assertion always failed in gcc 4.9
		static_assert(0, "Working with unregistered enum");
	#endif
		return "UnregisteredEnum";
	}
};

#define _ENUM(name)						\
	template<> struct TEnumInfo<name>	\
	{									\
		FORCEINLINE static const char* GetName() { return #name; } \
	};									\
	const enumToStr name##Names[] =

#define _E(name)				{ name, #name }

#define REGISTER_ENUM(name)		RegisterEnum(#name, ARRAY_ARG(name##Names));

#define ENUM_UNKNOWN			255

void RegisterEnum(const char *EnumName, const enumToStr *Values, int Count);
// find name of enum item, NULL when not found
const char *EnumToName(const char *EnumName, int Value);
// find interer value by enum name, ENUM_UNKNOWN when not found
int NameToEnum(const char *EnumName, const char *Value);

template<class T>
FORCEINLINE const char* EnumToName(T Value)
{
	return EnumToName(TEnumInfo<T>::GetName(), Value);
}


#endif // __TYPEINFO_H__
