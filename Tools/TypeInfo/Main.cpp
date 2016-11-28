#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"
#include "UnTypeinfo.h"

//!! NOTE: UE3 has comments for properties, stored in package.UMetaData'MetaData'

#define HOMEPAGE		"http://www.gildor.org/"


/*-----------------------------------------------------------------------------
	Table of known Unreal classes
-----------------------------------------------------------------------------*/

static void RegisterUnrealClasses()
{
	// classes and structures
BEGIN_CLASS_TABLE
	REGISTER_TYPEINFO_CLASSES
#if UNREAL3
	REGISTER_TYPEINFO_CLASSES_U3
#endif
#if MKVSDC
	REGISTER_TYPEINFO_CLASSES_MK
#endif
END_CLASS_TABLE
}


/*-----------------------------------------------------------------------------
	Display class
-----------------------------------------------------------------------------*/

void DumpProps(FArchive &Ar, const UStruct *Struct);

static const char indentBase[] = "\t\t\t\t\t\t\t\t";
static const char *fieldIndent = indentBase + ARRAY_COUNT(indentBase) - 1;

static void Dump(FArchive &Ar, const UFunction *Func)
{
	Ar.Printf("%sfunction %s();", fieldIndent, Func->Name);
}

static void Dump(FArchive &Ar, const UEnum *Enum)
{
	Ar.Printf("%senum %s\n%s{\n", fieldIndent, Enum->Name, fieldIndent);
	for (int i = 0; i < Enum->Names.Num(); i++)
		Ar.Printf("%s\t%s,\n", fieldIndent, *Enum->Names[i]);
	Ar.Printf("%s};\n", fieldIndent);
}

static void Dump(FArchive &Ar, const UConst *Const)
{
	Ar.Printf("%sconst %s = %s;", fieldIndent, Const->Name, *Const->Value);
}

static void Dump(FArchive &Ar, const UStruct *Struct)
{
	Ar.Printf("%sstruct %s\n{", fieldIndent, Struct->Name);
	fieldIndent--;
	DumpProps(Ar, Struct);
	fieldIndent++;
	Ar.Printf("\n%s};\n", fieldIndent);
}

static void DumpProperty(FArchive &Ar, const UProperty *Prop, const char *Type, const char *StructName)
{
	Ar.Printf("%svar", fieldIndent);
	if (Prop->PropertyFlags & 1)
	{
		Ar.Printf("(");
		if (strcmp(*Prop->Category, "None") != 0 && strcmp(*Prop->Category, StructName) != 0)
			Ar.Printf("%s", *Prop->Category);
		Ar.Printf(")");
	}
#define FLAG(v,name)	if (Prop->PropertyFlags & v) Ar.Printf(" "#name);
	FLAG(0x0001000, native    )
	FLAG(0x0000002, const     )
	FLAG(0x0020000, editconst )
	FLAG(0x4000000, editinline)
	FLAG(0x0002000, transient )		//?? sometimes may be "private"
	FLAG(0x0800000, noexport  )
	FLAG(0x0004000, config    )
#undef FLAG
	Ar.Printf(" %s %s", Type, Prop->Name);
	if (Prop->ArrayDim > 1)
		Ar.Printf("[%d]", Prop->ArrayDim);
	Ar.Printf(";");
	if (Prop->PropertyFlags || Prop->PropertyFlags2)
	{
		Ar.Printf("\t// ");
		if (Prop->PropertyFlags2)	// UE3's int64
			Ar.Printf("%08X:", Prop->PropertyFlags2);
		Ar.Printf("%08X", Prop->PropertyFlags);
	}
}

void DumpProps(FArchive &Ar, const UStruct *Struct)
{
	guard(DumpProps);

	const UField *Next = NULL;
	for (const UField *F = Struct->Children; F; F = Next)
	{
//		appPrintf("field: %s (%s)\n", F->Name, F->GetClassName());
		Next = F->Next;
		Ar.Printf("\n");
		const char *ClassName = F->GetClassName();
#define IS(Type)		strcmp(ClassName, #Type+1)==0
#define CVT(Type)		const Type *Prop = static_cast<const Type*>(F);
#define DUMP(Type)					\
			if (IS(Type))			\
			{						\
				CVT(Type);			\
				Dump(Ar, Prop);		\
				continue;			\
			}

		// special types
		DUMP(UFunction);
		DUMP(UEnum);
		DUMP(UConst);
		DUMP(UStruct);
#if UNREAL3
		DUMP(UScriptStruct);	// as UStruct
#endif
		// properties
		if (!F->IsA("Property"))
		{
			appNotify("DumpProps for %s'%s' (%s): object is not a Property", F->GetClassName(), F->Name, Struct->Name);
			continue;
		}

		bool isArray = false;
		if (IS(UArrayProperty))
		{
			const UArrayProperty *Arr = static_cast<const UArrayProperty*>(F);
			F  = Arr->Inner;
			if (!F)
			{
				appNotify("ArrayProperty %s.%s has no inner field", Struct->Name, Arr->Name);
				DumpProperty(Ar, Arr, "array<unknown>", Struct->Name);
				continue;
			}
			else
				ClassName = F->GetClassName();
			isArray = true;
		}
		const char *TypeName = NULL;
		if (IS(UByteProperty))
		{
			CVT(UByteProperty);
			TypeName = (Prop->Enum) ? Prop->Enum->Name : "byte";		//?? #IMPORTS#
		}
		else if (IS(UMapProperty))
		{
			TypeName = "map<>";		//!! implement
		}
		else if (IS(UIntProperty))
			TypeName = "int";
		else if (IS(UBoolProperty))
			TypeName = "bool";
		else if (IS(UFloatProperty))
			TypeName = "float";
		else if (IS(UObjectProperty))
		{
			CVT(UObjectProperty);
			TypeName = (Prop->PropertyClass) ? Prop->PropertyClass->Name : "object";	//?? #IMPORTS#
		}
		else if (IS(UClassProperty))
			TypeName = "class";
		else if (IS(UNameProperty))
			TypeName = "name";
		else if (IS(UStrProperty))
			TypeName = "string";
		else if (IS(UPointerProperty))
			TypeName = "pointer";
		else if (IS(UArrayProperty))
			appError("nested arrays for field %s", F->Name);
		else if (IS(UStructProperty))
		{
			CVT(UStructProperty);
			TypeName = (Prop->Struct) ? Prop->Struct->Name : "struct";	//?? #IMPORTS#
		}
		else if (IS(UComponentProperty))
		{
			TypeName = "component";	//???
		}
#if MKVSDC
		else if (IS(UNativeTypeProperty))
		{
			CVT(UNativeTypeProperty);
			TypeName = Prop->TypeName;
		}
#endif // MKVSDC
		// other: UDelegateProperty, UInterfaceProperty
		if (!TypeName) appError("Unknown type %s for field %s", ClassName, F->Name);
		char FullType[256];
		if (isArray)
			appSprintf(ARRAY_ARG(FullType), "array<%s>", TypeName);
		else
			appStrncpyz(FullType, TypeName, ARRAY_COUNT(FullType));
		CVT(UProperty);
		DumpProperty(Ar, Prop, FullType, Struct->Name);
#undef IS
#undef CVT
#undef DUMP
	}

	unguardf("Struct=%s", Struct->Name);
}

void DumpClass(const UClass *Class)
{
	//!! NOTE: UProperty going in correct format, other UField data in reverse format
	char Filename[256];
	appSprintf(ARRAY_ARG(Filename), "%s/%s.uc", Class->Package->Name, Class->Name);
	FFileWriter Ar(Filename);
	Ar.Printf("class %s", Class->Name);
	//?? note: import may be failed when placed in a different package - so, SuperField may be NULL
	//?? when parsing Engine class, derived from Core.Object
	//?? other places with same issue marked as "#IMPORTS#"
	if (Class->SuperField)
		Ar.Printf(" extends %s", Class->SuperField->Name);
	Ar.Printf(";\n");
	DumpProps(Ar, Class);
	Ar.Printf("\n");
}

bool DumpTextBuffer(const UTextBuffer *Text)
{
	if (!Text->Text.Len()) return false;		// empty

	// get class name (UTextBuffer's outer UPackage)
	const FObjectExport &Exp = Text->Package->GetExport(Text->PackageIndex);
	const char *ClassName = Text->Package->GetExport(Exp.PackageIndex-1).ObjectName;

	char Filename[256];
	appSprintf(ARRAY_ARG(Filename), "%s/%s.uc", Text->Package->Name, ClassName);
	FFileWriter Ar(Filename);
	Ar.Serialize((void*)*Text->Text, Text->Text.Len());

	return true;
}


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
#if DO_GUARD
	TRY {
#endif

	guard(Main);

	// display usage
	if (argc < 2)
	{
	help:
		appPrintf("Unreal typeinfo dumper\n"
				  "Usage: typeinfo [options] <package filename>\n"
				  "\n"
				  "Options:\n"
				  "    -text              use TextBuffer object instead of decompilation\n"
				  "    -lzo|lzx|zlib      force compression method for fully-compressed packages\n"
				  "\n"
				  "For details and updates please visit " HOMEPAGE "\n"
		);
		exit(0);
	}

	// parse command line
	int arg = 1;
	bool UseTextBuffer = false;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!stricmp(opt, "text"))
				UseTextBuffer = true;
			else if (!stricmp(opt, "lzo"))
				GForceCompMethod = COMPRESS_LZO;
			else if (!stricmp(opt, "zlib"))
				GForceCompMethod = COMPRESS_ZLIB;
			else if (!stricmp(opt, "lzx"))
				GForceCompMethod = COMPRESS_LZX;
			else
				goto help;
		}
		else
		{
			break;
		}
	}
	const char *argPkgName = argv[arg];
	if (!argPkgName) goto help;
	RegisterUnrealClasses();

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// setup root directory
	if (strchr(argPkgName, '/') || strchr(argPkgName, '\\'))
		appSetRootDirectory2(argPkgName);
	else
		appSetRootDirectory(".");
	// load a package
	UnPackage *Package = UnPackage::LoadPackage(argPkgName);
	if (!Package)
	{
		appPrintf("ERROR: Unable to find/load package %s\n", argPkgName);
		exit(1);
	}

	guard(ProcessPackage);
	// extract package name, create directory for it
	char PkgName[256];
	const char *s = strrchr(argPkgName, '/');
	if (!s) s = strrchr(argPkgName, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = argPkgName;
	appStrncpyz(PkgName, s, ARRAY_COUNT(PkgName));
	char *s2 = strchr(PkgName, '.');
	if (s2) *s2 = 0;
	appMakeDirectory(PkgName);

	if (UseTextBuffer)
	{
		bool dumped = true;
		for (int idx = 0; idx < Package->Summary.ExportCount; idx++)
		{
			const FObjectExport &Exp = Package->GetExport(idx);
			if (stricmp(Package->GetObjectName(Exp.ClassIndex), "TextBuffer") != 0)
				continue;		// not UTextBuffer
			if (stricmp(Exp.ObjectName, "ScriptText"))
				continue;
			UObject *Obj = Package->CreateExport(idx);
			assert(Obj);
			assert(Obj->IsA("TextBuffer"));
			if (!DumpTextBuffer(static_cast<UTextBuffer*>(Obj)))
			{
				dumped = false;
				appPrintf("Error: empty TextBuffer, switching to decompiler\n");
				break;
			}
		}
		if (dumped) return 0;
	}

	guard(LoadWholePackage);
	// load whole package
	for (int idx = 0; idx < Package->Summary.ExportCount; idx++)
	{
		const FObjectExport &Exp = Package->GetExport(idx);
		if (strcmp(Package->GetObjectName(Exp.ClassIndex), "Class") != 0)
			continue;		// not a class
		if (!strcmp(Exp.ObjectName, "None"))
			continue;		// Class'None'
		UObject *Obj = Package->CreateExport(idx);
		assert(Obj);
		assert(Obj->IsA("Class"));
		DumpClass(static_cast<UClass*>(Obj));
	}
	unguard;

	unguard;

	unguard;

#if DO_GUARD
	} CATCH {
		if (GErrorHistory[0])
		{
//			appPrintf("ERROR: %s\n", GErrorHistory);
			appNotify("ERROR: %s\n", GErrorHistory);
		}
		else
		{
//			appPrintf("Unknown error\n");
			appNotify("Unknown error\n");
		}
		exit(1);
	}
#endif
	return 0;
}
