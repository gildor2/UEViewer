#ifndef __UNHAVOK_H__
#define __UNHAVOK_H__

/*-----------------------------------------------------------------------------
	Basic types
	Source/Common/Base/Types/hkBaseTypes.h
-----------------------------------------------------------------------------*/

typedef uint8			hkUint8;
typedef int16			hkInt16;
typedef uint16			hkUint16;
typedef int32			hkInt32;
typedef uint32			hkUint32;
typedef int64			hkInt64;
typedef uint64			hkUint64;
typedef bool			hkBool;
typedef float			hkReal;


/// A wrapper to store an enum with explicit size.
template<typename ENUM, typename STORAGE>
class hkEnum
{
public:
	STORAGE				m_storage;
};


/// A wrapper to store bitfield with an with explicit size.
template<typename BITS, typename STORAGE>
class hkFlags
{
public:
	STORAGE				m_storage;
};


/*-----------------------------------------------------------------------------
	Binary packfile format
	Source/Common/Serialize/Packfile/Binary
-----------------------------------------------------------------------------*/

/// The header of a binary packfile.
// Unused structure fields are filled with 0xFF
class hkPackfileHeader
{
public:
	/// Magic file identifier. Values are 0x57e0e057, 0x10c0c010
	hkInt32				m_magic[2];

	/// This is a user settable tag.
	hkInt32				m_userTag;

	/// Binary file version. Currently 6 (major Havok version)
	hkInt32				m_fileVersion;

	/// The structure layout rules used by this file.
	hkUint8				m_layoutRules[4];

	/// Number of packfilesections following this header.
	hkInt32				m_numSections;

	/// Where the content's data structure is (section and offset within that section).
	hkInt32				m_contentsSectionIndex;
	hkInt32				m_contentsSectionOffset;

	/// Where the content's class name is (section and offset within that section).
	hkInt32				m_contentsClassNameSectionIndex;
	hkInt32				m_contentsClassNameSectionOffset;

	/// Future expansion
	char				m_contentsVersion[16];

	hkInt32				m_pad[2];
};


/// Packfiles are composed of several sections.
/// A section contains several areas
/// | data | local | global | finish | exports | imports |
/// data: the user usable data.
/// local: pointer patches within this section (src,dst).
/// global: pointer patches to locations within this packfile (src,(section,dst)).
/// finish: offset and typename of all objects for finish functions (src, typename).
/// exports: named objects (src,name).
/// imports: named pointer patches outside this packfile (src,name).
class hkPackfileSectionHeader
{
public:
	char				m_sectionTag[19];
	char				m_nullByte;

	/// Absolute file offset of where this sections data begins.
	hkInt32				m_absoluteDataStart;

	/// Offset of local fixups from absoluteDataStart.
	hkInt32				m_localFixupsOffset;

	/// Offset of global fixups from absoluteDataStart.
	hkInt32				m_globalFixupsOffset;

	/// Offset of virtual fixups from absoluteDataStart.
	hkInt32				m_virtualFixupsOffset;

	/// Offset of exports from absoluteDataStart.
	hkInt32				m_exportsOffset;

	/// Offset of imports from absoluteDataStart.
	hkInt32				m_importsOffset;

	/// Offset of the end of section. Also the section size.
	hkInt32				m_endOffset;
};

// Look here:
// Source/Common/Visualize/Serialize/hkObjectSerialize.h
// Source/Common/Serialize/Serialize/hkRelocationInfo.h
struct LocalFixup
{
	int					fromOffset;
	int					toOffset;
};

// Note: this structure was taken from hkPackfileSectionHeader comments
struct GlobalFixup
{
	int					fromOffset;
	int					toSec;
	int					toOffset;
};

// GlobalFixup structure is used for virtual (finish) fixups too; fromOffset is
// a pointer to object, toSec/toOffset points to a class name.


/*-----------------------------------------------------------------------------
	Base Havok object
	Source/Common/Base/Object
-----------------------------------------------------------------------------*/

/// Base class for all Havok classes that have virtual functions.
/// In gcc2 for instance, if the virtual base class has data in it the vtable
/// is placed after the data, whereas most other compilers always have the vtable
/// at the start. Thus we have an empty virtual base class to force the vtable
/// to always be at the start of the derived objects.
/// All Havok managed objects inherit from a sub class of this, hkReferencedObject
/// that stores the memory size and the reference count info (if used).
class hkBaseObject
{
public:
#if 0
	/// Virtual destructor for derived objects
	virtual ~hkBaseObject() {}
#else
	size_t				VtablePtr;
#endif
};


/// Base for all classes in the Havok SDK.
/// All core SDK objects that can be owned by multiple owners inherit from this class -
/// rigid bodies, constraints, and actions are all hkReferencedObjects
/// and any object that is memory managed by Havok also inherits from it.
class hkReferencedObject : public hkBaseObject
{
public:
	/// Stores the object's size for use by the memory manager.
	/// See the hkBase user guide to find out more about this.
	/// Top bit is used as a flag (no current internal usage)
	/// Bottom 15 bits are for the memory size.
	/// A memory size of 0 is a special case and indicates
	/// that the object should not be freed (as it has
	/// probably been read in from a packed file for instance)
	hkUint16			m_memSizeAndFlags; //+nosave

	/// Reference count. Note that if m_memSizeAndFlags == 0,
	/// reference counting is disabled for this object.
	mutable hkInt16		m_referenceCount; //+nosave
};


/*-----------------------------------------------------------------------------
	Reflection (type information) structures
	Source/Common/Base/Reflection
-----------------------------------------------------------------------------*/

/// Reflection information for any reflected type.
/// Reflection is based upon the Java object model where any
/// class has exactly zero or one parents and may implement
/// zero or more interfaces. An interface is a class which
/// has virtual methods but no data members.
class hkClass
{
public:
	enum FlagValues
	{
		FLAGS_NONE = 0,
		FLAGS_NOT_SERIALIZABLE = 1
	};
#if 1
	typedef hkFlags<FlagValues, hkUint32> Flags;
#else
	typedef hkUint32 Flags;
#endif


	/// Name of this type.
	const char*			m_name;

	/// Parent class.
	const hkClass*		m_parent;

	/// Size of the live instance.
	int					m_objectSize;

	/// Interfaces implemented by this class.
	//const hkClass** m_implementedInterfaces;

	/// Number of interfaces implemented by this class.
	int					m_numImplementedInterfaces;

	/// Declared enum members.
	const class hkClassEnum* m_declaredEnums;

	/// Number of enums declared in this class.
	int					m_numDeclaredEnums;

	/// Declared members.
	const class hkClassMember* m_declaredMembers;

	/// Number of members declared in this class.
	int					m_numDeclaredMembers;

	/// Default values for this class.
	const void*			m_defaults; //+nosave

	/// Default values for this class.
	const class hkCustomAttributes* m_attributes; //+serialized(false)

	/// Flag values.
	Flags				m_flags;

	/// Version of described object.
	int					m_describedVersion;
};


/// Reflection object for enumerated types.
class hkClassEnum
{
public:
	/// A single enumerated name and value pair.
	class Item
	{
	public:
		int				m_value;
		const char*		m_name;
	};
	typedef hkUint32 Flags;

	const char*			m_name;
	const class Item*	m_items;
	int					m_numItems;
//	class hkCustomAttributes* m_attributes; //+serialized(false)
//	Flags				m_flags;
	// Bioshock 1: hkCustomAttributes and Flags not serialized, Bioshock 2: serialized
};


/// Reflection information for a data member of a type.
class hkClassMember
{
public:
	/// An enumeration of all possible member types.
	/// There are three basic categories. Plain old data types
	/// (normal and c-style array variants), enums and hkArrays.
	enum Type
	{
		/// No type
		TYPE_VOID = 0,
		/// hkBool,  boolean type
		TYPE_BOOL,
		/// hkChar, signed char type
		TYPE_CHAR,
		/// hkInt8, 8 bit signed integer type
		TYPE_INT8,
		/// hkUint8, 8 bit unsigned integer type
		TYPE_UINT8,
		/// hkInt16, 16 bit signed integer type
		TYPE_INT16,
		/// hkUint16, 16 bit unsigned integer type
		TYPE_UINT16,
		/// hkInt32, 32 bit signed integer type
		TYPE_INT32,
		/// hkUint32, 32 bit unsigned integer type
		TYPE_UINT32,
		/// hkInt64, 64 bit signed integer type
		TYPE_INT64,
		/// hkUint64, 64 bit unsigned integer type
		TYPE_UINT64,
		/// hkReal, float type
		TYPE_REAL,
		/// hkVector4 type
		TYPE_VECTOR4,
		/// hkQuaternion type
		TYPE_QUATERNION,
		/// hkMatrix3 type
		TYPE_MATRIX3,
		/// hkRotation type
		TYPE_ROTATION,
		/// hkQsTransform type
		TYPE_QSTRANSFORM,
		/// hkMatrix4 type
		TYPE_MATRIX4,
		/// hkTransform type
		TYPE_TRANSFORM,
		/// Serialize as zero - deprecated.
		TYPE_ZERO,
		/// Generic pointer, see member flags for more info
		TYPE_POINTER,
		/// Function pointer
		TYPE_FUNCTIONPOINTER,
		/// hkArray<T>, array of items of type T
		TYPE_ARRAY,
		/// hkInplaceArray<T,N> or hkInplaceArrayAligned16<T,N>, array of N items of type T
		TYPE_INPLACEARRAY,
		/// hkEnum<ENUM,STORAGE> - enumerated values
		TYPE_ENUM,
		/// Object
		TYPE_STRUCT,
		/// Simple array (ptr(typed) and size only)
		TYPE_SIMPLEARRAY,
		/// Simple array of homogeneous types, so is a class id followed by a void* ptr and size
		TYPE_HOMOGENEOUSARRAY,
		/// hkVariant (void* and hkClass*) type
		TYPE_VARIANT,
		/// char*, null terminated string
		TYPE_CSTRING,
		/// hkUlong, unsigned long, defined to always be the same size as a pointer
		TYPE_ULONG,
		/// hkFlags<ENUM,STORAGE> - 8,16,32 bits of named values.
		TYPE_FLAGS,
		TYPE_MAX
	};

	/// Special member properties.
	enum FlagValues
	{
		FLAGS_NONE = 0,
		/// Member has forced 8 byte alignment.
		ALIGN_8 = 128,
		/// Member has forced 16 byte alignment.
		ALIGN_16 = 256,
		/// The members memory contents is not owned by this object
		NOT_OWNED = 512,
		/// This member should not be written when serializing
		SERIALIZE_IGNORED = 1024
	};
#if 1
	typedef hkFlags<FlagValues, hkUint16> Flags;
#else
	typedef hkUint16 Flags;
#endif

	/// The name of this member.
	const char*			m_name;

	const hkClass*		m_class;
	const hkClassEnum*	m_enum;				// Usually null except for enums
	hkEnum<Type,hkUint8> m_type;			// An hkMemberType.
	hkEnum<Type,hkUint8> m_subtype;			// An hkMemberType.
	hkInt16				m_cArraySize;		// Usually zero, nonzero for cstyle array..
	Flags				m_flags;			// Pointers:optional, voidstar, rawdata. Enums:sizeinbytes.
	hkUint16			m_offset;			// Address offset from start of struct.
//	const hkCustomAttributes* m_attributes;	//+serialized(false)
	// Bioshock 1: hkCustomAttributes not serialized
	// Bioshock 2: hkCustomAttributes are serialized
};


/*-----------------------------------------------------------------------------
	Havok math classes
	Source/Common/Base/Math
-----------------------------------------------------------------------------*/

/// Stores four hkReals in a SIMD friendly format.
/// Although an hkVector4 - as its name suggests - actually stores four hkReals,
/// it can be used to represent either a point or vector (x,y,z) in 3-space, ie. a 3-vector.
/// The final, or w, value of the hkVector4 defaults to zero.
class hkVector4
{
public:
	float				X, Y, Z, W;		// hkReal x, y, z, w
};


/// Stores a unit quaternion.
/// "Unitness" is not enforced, but certain functions (such as division
/// and inversion ) DO assume that the quaternions are unit quaternions.
class hkQuaternion
{
public:
	float				X, Y, Z, W;		// hkVector4 m_vec
};


/// An hkQsTransform represents a T*R*S transformation where translation is
/// represented with a vector4, rotation is represented with a quaternion and scale
/// is represented with a vector4.
/// When applied to a point (hkVector4::setTransformedPos()), the point is first scaled, then rotated and
/// finally translated.
/// Direct read/write access to the three components of the hkQsTransform is available through he public
/// members m_translation, m_rotation, m_scale. Const access can be done through get() methods.
/// It is important to notice that, due to the nature of the TRS representation, and since not all affine
/// transformations can be represented with an hkQsTransform, operations like multiplications have specific definitions
/// in order to keep the result inside the space of hkQsTransforms.
class hkQsTransform
{
public:
	/// Fourth component is not used.
	hkVector4			m_translation;
	hkQuaternion		m_rotation;
	/// The fourth component is not used.
	hkVector4			m_scale;
};


/*-----------------------------------------------------------------------------
	Helper functions
-----------------------------------------------------------------------------*/

void FixupHavokPackfile(const char *Name, void *PackData);
void GetHavokPackfileContents(const void *PackData, void **Object, const char **ClassName);


#endif // __UNHAVOK_H__
