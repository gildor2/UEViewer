#ifndef __TYPEINFO_H__
#define __TYPEINFO_H__

// Bitfield flags
enum ETypeFlags
{
	TYPE_None = 0,
	TYPE_SilentLoad = 1,
	TYPE_InlinePropDump = 2,
};

namespace TypeinfoDetail
{
	// Default value for StaticSerialize(), could be changed with USE_NATIVE_SERIALIZER macro
	constexpr void (*StaticSerialize)(FArchive&, void*) = NULL;
};

// Comparing PropLevel with Super::PropLevel to detect whether we have property
// table in current class or not
#define DECLARE_BASE(Class,Base)				\
	public:										\
		typedef Class	ThisClass;				\
		typedef Base	Super;					\
		static void InternalConstructor(void *Mem) \
		{ new(Mem) ThisClass; }					\
		static const CTypeInfo* StaticGetTypeinfo() \
		{										\
			using namespace TypeinfoDetail;		\
			int numProps = 0;					\
			const CPropInfo *props = NULL;		\
			if ((int)PropLevel != (int)Super::PropLevel) \
				props = StaticGetProps(numProps); \
			static const CTypeInfo type(		\
				#Class,							\
				Super::StaticGetTypeinfo(),		\
				sizeof(ThisClass),				\
				props, numProps,				\
				LocalTypeFlags,					\
				InternalConstructor,			\
				StaticSerialize					\
			);									\
			return &type;						\
		}

#define DECLARE_STRUCT(Class)					\
		DECLARE_BASE(Class, CNullType)			\
		static const uint32 LocalTypeFlags = TYPE_None; \
		const CTypeInfo* GetTypeinfo() const	\
		{										\
			return StaticGetTypeinfo();			\
		}

// structure derived from another structure with typeinfo
#define DECLARE_STRUCT2(Class,Base)				\
		DECLARE_BASE(Class, Base)				\
		static const uint32 LocalTypeFlags = TYPE_None; \
		const CTypeInfo* GetTypeinfo() const	\
		{										\
			return StaticGetTypeinfo();			\
		}

//?? can replace virtual GetClassName() with GetTypeinfo()->Name
#define DECLARE_CLASS(Class,Base)				\
		DECLARE_BASE(Class, Base)				\
		virtual const CTypeInfo* GetTypeinfo() const \
		{										\
			return StaticGetTypeinfo();			\
		}										\
	private:									\
		friend FArchive& operator<<(FArchive &Ar, Class *&Res) \
		{										\
			return Ar << *(UObject**)&Res;		\
		}

#define TYPE_FLAGS(x)							\
	public:										\
		static const uint32 LocalTypeFlags = x;	\
	private:

// UE3 and UE4 has a possibility to use native serializer for a struct.
// In UE4 it is declared with ...
// The serializer is instantiated in UScriptStruct::TCppStructOps::Serialize()
// when TStructOpsTypeTraits<CPPSTRUCT>::WithSerializer is true.
//todo: use native serializer for FVector, FGuid etc, clean up UnObject.cpp from explicit handling of these structs
#define USE_NATIVE_SERIALIZER					\
		static void StaticSerialize(FArchive& Ar, void* Data) \
		{										\
			Ar << *(ThisClass*)Data;			\
		}

struct CPropInfo
{
	// Field name
	const char*	 	Name;
	// Name of the field type
	const char*		TypeName;
	// Offset of this field from the class start
	uint16			Offset;
	/* Number of array items:
	 *  1  for ordinary property
	 *  2+ for static arrays (Type Prop[COUNT])
	 * -1  for TArray (TArray<Type>)
	 *  0  for PROP_DROP - not linked to a read property
	 */
	int16			Count;

	constexpr CPropInfo(const char* InName, const char* InTypeName, uint16 InOffset, int16 InCount)
	: Name(InName)
	, TypeName(InTypeName)
	, Offset(InOffset)
	, Count(InCount)
	{}

	// Constructor for PROP_DROP
	constexpr CPropInfo(const char* InName, const char* InTypeName = NULL)
	: Name(InName)
	, TypeName(InTypeName)
	, Offset(0)
	, Count(0)
	{}
};


struct CTypeInfo
{
	const char*		Name;
	const CTypeInfo* Parent;
	int				SizeOf;
	const CPropInfo* Props;
	int				NumProps;
	uint32			TypeFlags;
	void (*Constructor)(void*);
	void (*NativeSerializer)(FArchive&, void*);
	// methods
	constexpr FORCEINLINE CTypeInfo(const char *AName, const CTypeInfo *AParent, int DataSize,
					 const CPropInfo *AProps, int PropCount, uint32 Flags,
					 void (*AConstructor)(void*), void (*ASerializer)(FArchive&, void*))
	:	Name(AName)
	,	Parent(AParent)
	,	SizeOf(DataSize)
	,	Props(AProps)
	,	NumProps(PropCount)
	,	TypeFlags(Flags)
	,	Constructor(AConstructor)
	,	NativeSerializer(ASerializer)
	{}
	inline bool IsClass() const
	{
		return (Name[0] == 'U') || (Name[0] == 'A');	// UE structure type names are started with 'F', classes with 'U' or 'A'
	}
	bool IsA(const char *TypeName) const;
	const CPropInfo *FindProperty(const char *Name) const;
	static void RemapProp(const char *Class, const char *OldName, const char *NewName);

	// Serialize Unreal engine UObject property block
	void SerializeUnrealProps(FArchive& Ar, void* ObjectData) const;

#if UNREAL4
	void SerializeUnversionedProperties4(FArchive& Ar, void* ObjectData) const;
	const char* FindUnversionedProp(int InPropIndex, int& OutArrayIndex, int InGame) const;
#endif

	void ReadUnrealProperty(FArchive& Ar, struct FPropertyTag& Tag, void* ObjectData, int PropTagPos) const;

#if BATMAN
	void SerializeBatmanProps(FArchive& Ar, void* ObjectData) const;
#endif

	void DumpProps(const void *Data) const;
	void DumpProps(const void* Data, void (*Callback)(const char*)) const;
	void SaveProps(const void *Data, FArchive& Ar) const;
	bool LoadProps(void *Data, FArchive& Ar) const;
};


// Helper class to simplify DECLARE_CLASS() macro group
// This class is used as Base for DECLARE_BASE()/DECLARE_CLASS() macros
struct CNullType
{
	enum { PropLevel = -1 };		// overridden in BEGIN_CLASS_TABLE
	static constexpr const CPropInfo* StaticGetProps(int& numProps)
	{
		numProps = 0;
		return NULL;
	}
	static constexpr const CTypeInfo* StaticGetTypeinfo()
	{
		return NULL;
	}
};

// Constants for declaring property types
namespace PropType
{
	constexpr const char* Bool = "bool";
	constexpr const char* Int = "int";
	constexpr const char* Byte = "byte";
	constexpr const char* Float = "float";
	constexpr const char* FName = "FName";
	constexpr const char* UObject = "UObject*";
	constexpr const char* FString = "FString";
	constexpr const char* FVector = "FVector";
	constexpr const char* FRotator = "FRotator";
	constexpr const char* FColor = "FColor";
	constexpr const char* FVector4 = "FVector4"; // FVector4, FQuat, FGuid etc
}

#define _PROP_BASE(Field,TypeName,BaseType)	{ #Field, TypeName, FIELD2OFS(ThisClass, Field), sizeof(ThisClass::Field) / sizeof(BaseType) },

// PROP_ARRAY is used for TArray. For static array (type[N]) use declaration for 'type'.
#define PROP_ARRAY(Field,TypeName)			{ #Field, TypeName, FIELD2OFS(ThisClass, Field), -1 }, //todo: should use PropType constants here!
// Structure property aggregated into the class
#define PROP_STRUC(Field,Type)				_PROP_BASE(Field, #Type, Type)
// Register a property which should be ignored
#if _MSC_VER
#define PROP_DROP(Field, ...)				CPropInfo(#Field, __VA_ARGS__),
#else
// gcc doesn't like  empty __VA_ARGS__
#define PROP_DROP(Field, ...)				CPropInfo(#Field __VA_OPT__(,) __VA_ARGS__),
#endif
// Register another name for the same property
#define PROP_ALIAS(Name,Alias)  			{ #Alias, ">" #Name, 0, 0 },


// BEGIN_PROP_TABLE/END_PROP_TABLE declares property table inside class declaration
#define BEGIN_PROP_TABLE						\
	enum { PropLevel = Super::PropLevel + 1 };	\
	static const CPropInfo* StaticGetProps(int& numProps) \
	{											\
		static const CPropInfo props[] =		\
		{

// Simple types
// note: has special PROP_ENUM(), which is the same as PROP_BYTE(), but when using
// PROP_BYTE for enumeration value, _PROP_BASE macro will set 'Count' field of
// CPropInfo to 4 instead of 1 (enumeration values are serialized as byte, but
// compiler report it as 4-byte field).
#define PROP_ENUM(Field)		{ #Field, PropType::Byte, FIELD2OFS(ThisClass, Field), 1 },
#define PROP_ENUM2(Field,Type)	{ #Field, "#"#Type, FIELD2OFS(ThisClass, Field), 1 },
#define PROP_BYTE(Field)		_PROP_BASE(Field, PropType::Byte, byte )
#define PROP_INT(Field)			_PROP_BASE(Field, PropType::Int, int32  )
#define PROP_BOOL(Field)		_PROP_BASE(Field, PropType::Bool, bool )
#define PROP_FLOAT(Field)		_PROP_BASE(Field, PropType::Float, float)
#define PROP_NAME(Field)		_PROP_BASE(Field, PropType::FName, FName)
#define PROP_STRING(Field)		_PROP_BASE(Field, PropType::FString, FString)
#define PROP_OBJ(Field)			_PROP_BASE(Field, PropType::UObject, UObject*)
// Structure types; note: structure names corresponds to F<StrucName> C++ struc
#define PROP_VECTOR(Field)		_PROP_BASE(Field, PropType::FVector, FVector)
#define PROP_ROTATOR(Field)		_PROP_BASE(Field, PropType::FRotator, FRotator)
#define PROP_COLOR(Field)		_PROP_BASE(Field, PropType::FColor, FColor)

#define END_PROP_TABLE							\
		};										\
		numProps = ARRAY_COUNT(props);			\
		return props;							\
	}

#define DUMMY_PROP_TABLE						\
	BEGIN_PROP_TABLE							\
		PROP_DROP(Dummy)						\
	END_PROP_TABLE

#if DECLARE_VIEWER_PROPS
#define VPROP_ARRAY_COUNT(Field,Name)	{ #Name, PropType::Int, FIELD2OFS(ThisClass, Field) + ARRAY_COUNT_FIELD_OFFSET, 1 },
#endif // DECLARE_VIEWER_PROPS

// Mix of BEGIN_PROP_TABLE and DECLARE_BASE
// Allows to declare property table outside of class declaration
#define BEGIN_PROP_TABLE_EXTERNAL(Class)		\
static FORCEINLINE const CTypeInfo* Class##_StaticGetTypeinfo() \
{												\
	using namespace TypeinfoDetail;				\
	static const char ClassName[] = #Class;		\
	typedef Class ThisClass;					\
	static const CPropInfo props[] =			\
	{

#define BEGIN_PROP_TABLE_EXTERNAL_WITH_NATIVE_SERIALIZER(Class)	\
static FORCEINLINE const CTypeInfo* Class##_StaticGetTypeinfo() \
{												\
	static const char ClassName[] = #Class;		\
	typedef Class ThisClass;					\
	auto StaticSerialize=[](FArchive& Ar, void* Data) \
	{											\
		Ar << *(ThisClass*)Data;				\
	};											\
	static const CPropInfo props[] =			\
	{


// Mix of END_PROP_TABLE and DECLARE_BASE
//todo: there's no InternalConstructor for this type of object declaration?
#define END_PROP_TABLE_EXTERNAL					\
	};											\
	static const CTypeInfo type(				\
		ClassName,								\
		NULL,									\
		sizeof(ThisClass),						\
		props, ARRAY_COUNT(props),				\
		TYPE_None,								\
		NULL,									\
		StaticSerialize							\
	);											\
	return &type;								\
}

struct CClassInfo
{
	const char		*Name;
	const CTypeInfo* (*TypeInfo)();
};


void RegisterClasses(const CClassInfo* Table, int Count);
void UnregisterClass(const char* Name, bool WholeTree = false);
void SuppressUnknownClass(const char* ClassNameWildcard);

const CTypeInfo* FindClassType(const char* Name, bool ClassType = true);
bool IsSuppressedClass(const char* Name);

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
// find integer value by enum name, ENUM_UNKNOWN when not found
int NameToEnum(const char *EnumName, const char *Value);

template<class T>
FORCEINLINE const char* EnumToName(T Value)
{
	return EnumToName(TEnumInfo<T>::GetName(), Value);
}


#endif // __TYPEINFO_H__
