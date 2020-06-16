#include "Core.h"
#include "UnHavok.h"

#if BIOSHOCK			//?? separate define for HAVOK

//#define DUMP_TYPEINFO	1

// Dump-only structures
//#define NEW_HAVOK		1	-- can make as "#if" in UnHavok.h instead of using OffsetPointer()


#if DUMP_TYPEINFO

static void DumpClass(const hkClass *Class)
{
	guard(DumpClass);

	int i;

	// header
	appPrintf("// sizeof=%d\n", Class->m_objectSize);
	appPrintf("struct %s", Class->m_name);
	if (Class->m_parent) appPrintf(" : %s", Class->m_parent->m_name);
	appPrintf("\n{\n");
	// enums
	const hkClassEnum *Enum = Class->m_declaredEnums;
	for (i = 0; i < Class->m_numDeclaredEnums; i++)
	{
		appPrintf("\tenum %s\n\t{\n", Enum->m_name);
		for (int j = 0; j < Enum->m_numItems; j++)
		{
			const hkClassEnum::Item *Item = Enum->m_items + j;
			appPrintf("\t\t%s = %d,\n", Item->m_name, Item->m_value);
		}
		appPrintf("\t};\n\n");
		Enum++;
#if NEW_HAVOK
		Enum = OffsetPointer(Enum, 8);
#endif
	}
	// contents
	const hkClassMember *Mem = Class->m_declaredMembers;
	for (i = 0; i < Class->m_numDeclaredMembers; i++)
	{
		static const char *TypeTable[] = {
			"void", "hkBool", "char", "hkInt8", "hkUint8",
			"hkInt16", "hkUint16", "hkInt32", "hkUint32", "hkInt64",
			"hkUint64", "hkReal", "hkVector4", "hkQuaternion", "hkMatrix3",
			"hkRotation", "hkQsTransform", "hkMatrix4", "hkTransform", "unknown",
			"pointer", "pfunction", "hkArray", "hkInplaceArray", "hkEnum",
			"Struct", "SimpleArray", "HomogenousArray", "hkVariant", "char*",
			"hkUlong", "hkFlags"
		};
		static_assert(ARRAY_COUNT(TypeTable) == hkClassMember::TYPE_MAX, "TypeTable is bad");

		// dump internal information
		appPrintf("\t// offset=%d type=%s", Mem->m_offset, TypeTable[Mem->m_type.m_storage]);
		if (Mem->m_subtype.m_storage != hkClassMember::TYPE_VOID)
			appPrintf("/%s", TypeTable[Mem->m_subtype.m_storage]);
		if (Mem->m_class)
			appPrintf(" class=%s", Mem->m_class->m_name);
		if (Mem->m_flags.m_storage)
			appPrintf(" flags=%X", Mem->m_flags.m_storage);
		appPrintf("\n");

		int TypeId = Mem->m_type.m_storage;
		const char *Ptr = "**" + 2;				// points to null char

		if (TypeId == hkClassMember::TYPE_SIMPLEARRAY)
		{
			Ptr--;
			TypeId = Mem->m_subtype.m_storage;
		}
		const char *Type = TypeTable[TypeId];
		if (TypeId == hkClassMember::TYPE_STRUCT)
			Type = Mem->m_class->m_name;
		if (TypeId == hkClassMember::TYPE_POINTER)
		{
			Ptr--;
			if (Mem->m_class)
				Type = Mem->m_class->m_name;
			else
				Type = TypeTable[Mem->m_subtype.m_storage];
		}
		if (TypeId == hkClassMember::TYPE_ENUM && Mem->m_enum)
		{
			static char EnumName[64];
			appSprintf(ARRAY_ARG(EnumName), "hkEnum<%s>", Mem->m_enum->m_name);
			Type = EnumName;
		}
		if (TypeId == hkClassMember::TYPE_ARRAY && Mem->m_class)
		{
			static char ArrayName[64];
			appSprintf(ARRAY_ARG(ArrayName), "hkArray<%s>", Mem->m_class->m_name);
			Type = ArrayName;
		}
		appPrintf("\t%s%s m_%s", Type, Ptr, Mem->m_name);
		if (Mem->m_cArraySize) appPrintf("[%d]", Mem->m_cArraySize);
		appPrintf(";\n");
		// array types may have extra fields
		if (Mem->m_type.m_storage == hkClassMember::TYPE_SIMPLEARRAY)
		{
			// create m_numVariableName field with uppercased 1st char
			appPrintf("\thkInt32 m_num%c%s;\n", toupper(Mem->m_name[0]), Mem->m_name+1);
		}

		Mem++;
#if NEW_HAVOK
		Mem = OffsetPointer(Mem, 4);
#endif
	}
	// footer
	appPrintf("};\n\n");

	unguard;
}

#endif // DUMP_TYPEINFO


//!! NOTE: not endian-friendly code
void FixupHavokPackfile(const char *Name, void *PackData)
{
	guard(FixupHavokPackfile);
	int i;

	byte *PackStart = (byte*)PackData;
	hkPackfileHeader *Hdr = (hkPackfileHeader*)PackStart;
//	appPrintf("Magic: %X %X Ver: %d (%s)\n", Hdr->m_magic[0], Hdr->m_magic[1], Hdr->m_fileVersion, Hdr->m_contentsVersion);

	// relocate all sections
	hkPackfileSectionHeader *Sections = (hkPackfileSectionHeader*)(Hdr + 1);
	for (i = 0; i < Hdr->m_numSections; i++)
	{
		hkPackfileSectionHeader *Sec = Sections + i;
		byte *SectionStart = PackStart + Sec->m_absoluteDataStart;
//		appPrintf("Sec[%d] = %s (%X)\n", i, Sec->m_sectionTag, Sec->m_absoluteDataStart);
		// process local fixups
		for (LocalFixup *LF = (LocalFixup*)(SectionStart + Sec->m_localFixupsOffset);
			 LF < (LocalFixup*)(SectionStart + Sec->m_globalFixupsOffset);
			 LF++)
		{
			if (LF->fromOffset == 0xFFFFFFFF) continue;		// padding
//			appPrintf("Lfix: %X -> %X\n", LF->fromOffset, LF->toOffset);
			// fixup
			*(byte**)(SectionStart + LF->fromOffset) = SectionStart + LF->toOffset;
		}
		// process global fixups
		for (GlobalFixup *GF = (GlobalFixup*)(SectionStart + Sec->m_globalFixupsOffset);
			 GF < (GlobalFixup*)(SectionStart + Sec->m_virtualFixupsOffset);
			 GF++)
		{
			if (GF->fromOffset == 0xFFFFFFFF) continue;		// padding
//			appPrintf("Gfix: %X -> %X / %X\n", GF->fromOffset, GF->toSec, GF->toOffset);
			// fixup
			byte *SectionStart2 = PackStart + Sections[GF->toSec].m_absoluteDataStart;
			*(byte**)(SectionStart + GF->fromOffset) = SectionStart2 + GF->toOffset;
		}
#if DUMP_TYPEINFO
		// process virtual fixups
		for (GlobalFixup *VF = (GlobalFixup*)(SectionStart + Sec->m_virtualFixupsOffset);
			 VF < (GlobalFixup*)(SectionStart + Sec->m_exportsOffset);
			 VF++)
		{
			if (VF->fromOffset == 0xFFFFFFFF) continue;		// padding
//			appPrintf("Vfix: %X -> %X / %X\n", VF->fromOffset, VF->toSec, VF->toOffset);
			// fixup
			const char *ClassName = (char*)PackStart + Sections[VF->toSec].m_absoluteDataStart + VF->toOffset;
//			appPrintf("Vfix: %X -> %s\n", VF->fromOffset, ClassName);
//			*(byte**)(SectionStart + VF->fromOffset) = SectionStart2 + VF->toOffset;
			if (!strcmp(ClassName, "hkClass"))
				DumpClass((hkClass*)(SectionStart + VF->fromOffset));
		}
#endif // DUMP_TYPEINFO
		if (Sec->m_exportsOffset != Sec->m_importsOffset) appNotify("%s, Sec[%s] has exports", Name, Sec->m_sectionTag);
		if (Sec->m_importsOffset != Sec->m_endOffset)     appNotify("%s, Sec[%s] has imports", Name, Sec->m_sectionTag);
	}
	unguard;
}


void GetHavokPackfileContents(const void *PackData, void **Object, const char **ClassName)
{
	guard(GetHavokPackfileContents);
	byte *PackStart = (byte*)PackData;
	hkPackfileHeader *Hdr = (hkPackfileHeader*)PackStart;
	hkPackfileSectionHeader *Sections = (hkPackfileSectionHeader*)(Hdr + 1);
	*Object    = (char*)PackStart + Sections[Hdr->m_contentsSectionIndex         ].m_absoluteDataStart + Hdr->m_contentsSectionOffset;
	*ClassName = (char*)PackStart + Sections[Hdr->m_contentsClassNameSectionIndex].m_absoluteDataStart + Hdr->m_contentsClassNameSectionOffset;
	unguard;
}


#endif // BIOSHOCK
