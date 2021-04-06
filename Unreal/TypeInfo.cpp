#include "Core.h"
#include "UnCore.h"
#include "TypeInfo.h"

#include "UnObject.h"		// dumping UObject in a few places

#define MAX_CLASSES		256
#define MAX_ENUMS		32
#define MAX_SUPPRESSED_CLASSES 32
#define MAX_DUMP_LINE	48

//#define DEBUG_TYPES				1

/*-----------------------------------------------------------------------------
	CTypeInfo class table
-----------------------------------------------------------------------------*/

static CClassInfo GClasses[MAX_CLASSES];
static int GClassCount = 0;
static const char* GSuppressedClasses[MAX_SUPPRESSED_CLASSES];
static int GSuppressedClassCount = 0;

void RegisterClasses(const CClassInfo* Table, int Count)
{
	if (Count <= 0) return;
	assert(GClassCount + Count < ARRAY_COUNT(GClasses));
	for (int i = 0; i < Count; i++)
	{
		const char* ClassName = Table[i].Name;
		bool duplicate = false;
		for (int j = 0; j < GClassCount; j++)
		{
			if (!strcmp(GClasses[j].Name, ClassName))
			{
				// Overriding class with a different typeinfo (for example, overriding UE3 class with UE4 one)
				GClasses[j] = Table[i];
				duplicate = true;
				break;
			}
		}
		if (!duplicate)
		{
			GClasses[GClassCount++] = Table[i];
		}
	}
#if DEBUG_TYPES
	appPrintf("*** Register: %d classes ***\n", Count); //!! NOTE: printing will not work correctly when "duplicate" is "true" for one or more classes
	for (int i = GClassCount - Count; i < GClassCount; i++)
		appPrintf("[%d]:%s\n", i, GClasses[i].TypeInfo()->Name);
#endif
}


void UnregisterClass(const char* Name, bool WholeTree)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name + 1, Name) ||
			(WholeTree && (GClasses[i].TypeInfo()->IsA(Name))))
		{
#if DEBUG_TYPES
			appPrintf("Unregister %s\n", GClasses[i].Name);
#endif
			// class was found
			if (i == GClassCount-1)
			{
				// last table entry
				GClassCount--;
				return;
			}
			memcpy(GClasses+i, GClasses+i+1, (GClassCount-i-1) * sizeof(GClasses[0]));
			GClassCount--;
			i--;
		}
}


void SuppressUnknownClass(const char* ClassNameWildcard)
{
	assert(GSuppressedClassCount < ARRAY_COUNT(GSuppressedClasses));
#if DEBUG_TYPES
	appPrintf("Suppress %s\n", ClassNameWildcard);
#endif
	GSuppressedClasses[GSuppressedClassCount++] = ClassNameWildcard;
}


// todo: 'ClassType' probably should be dropped
const CTypeInfo* FindClassType(const char* Name, bool ClassType)
{
	guard(FindClassType);
#if DEBUG_TYPES
	appPrintf("--- find %s %s ... ", ClassType ? "class" : "struct", Name);
#endif
	for (int i = 0; i < GClassCount; i++)
	{
		// skip 1st char only for ClassType==true?
		if (ClassType)
		{
			if (stricmp(GClasses[i].Name + 1, Name) != 0) continue;
		}
		else
		{
			if (stricmp(GClasses[i].Name, Name) != 0) continue;
		}

		if (!GClasses[i].TypeInfo) appError("No typeinfo for class");
		const CTypeInfo *Type = GClasses[i].TypeInfo();
		// FindUnversionedProp() calls FindStructType for classes and structs, so disable the comparison for now
		// if (Type->IsClass() != ClassType) continue;
#if DEBUG_TYPES
		appPrintf("ok %s\n", Type->Name);
#endif
		return Type;
	}
#if DEBUG_TYPES
	appPrintf("failed!\n");
#endif
	return NULL;
	unguardf("%s", Name);
}


bool IsSuppressedClass(const char* Name)
{
	for (int i = 0; i < GSuppressedClassCount; i++)
	{
		if (appMatchWildcard(Name, GSuppressedClasses[i] + 1))
			return true;
	}
	return false;
}


bool CTypeInfo::IsA(const char *TypeName) const
{
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
		if (!strcmp(TypeName, Type->Name + 1))
			return true;
	return false;
}


/*-----------------------------------------------------------------------------
	CTypeInfo enum table
-----------------------------------------------------------------------------*/

struct enumInfo
{
	const char       *Name;
	const enumToStr  *Values;
	int              NumValues;
};

static enumInfo RegisteredEnums[MAX_ENUMS];
static int NumEnums = 0;

void RegisterEnum(const char *EnumName, const enumToStr *Values, int Count)
{
	guard(RegisterEnum);

	assert(NumEnums < MAX_ENUMS);
	enumInfo &Info = RegisteredEnums[NumEnums++];
	Info.Name      = EnumName;
	Info.Values    = Values;
	Info.NumValues = Count;

	unguard;
}

const enumInfo *FindEnum(const char *EnumName)
{
	for (int i = 0; i < NumEnums; i++)
		if (!strcmp(RegisteredEnums[i].Name, EnumName))
			return &RegisteredEnums[i];
	return NULL;
}

const char *EnumToName(const char *EnumName, int Value)
{
	const enumInfo *Info = FindEnum(EnumName);
	if (!Info) return NULL;				// enum was not found
	for (int i = 0; i < Info->NumValues; i++)
	{
		const enumToStr &V = Info->Values[i];
		if (V.value == Value)
			return V.name;
	}
	return NULL;						// no such value
}

int NameToEnum(const char *EnumName, const char *Value)
{
	const enumInfo *Info = FindEnum(EnumName);
	if (!Info) return ENUM_UNKNOWN;		// enum was not found
	const char* s = strstr(Value, "::"); // UE4 may have "EnumName::Value" text
	if (s) Value = s+2;
	for (int i = 0; i < Info->NumValues; i++)
	{
		const enumToStr &V = Info->Values[i];
		if (!stricmp(V.name, Value))
			return V.value;
	}
	return ENUM_UNKNOWN;				// no such value
}


/*-----------------------------------------------------------------------------
	CTypeInfo property lookup
-----------------------------------------------------------------------------*/

struct PropPatch
{
	const char *ClassName;
	const char *OldName;
	const char *NewName;
};

static TArray<PropPatch> Patches;

/*static*/ void CTypeInfo::RemapProp(const char *ClassName, const char *OldName, const char *NewName)
{
	PropPatch *p = new (Patches) PropPatch;
	p->ClassName = ClassName;
	p->OldName   = OldName;
	p->NewName   = NewName;
}

const CPropInfo *CTypeInfo::FindProperty(const char *Name) const
{
	guard(CTypeInfo::FindProperty);
	// check for remap
	for (const PropPatch &p : Patches)
	{
		;
		if (!stricmp(p.ClassName, this->Name) && !stricmp(p.OldName, Name))
		{
			Name = p.NewName;
			break;
		}
	}
	// find property
	for (const CTypeInfo *Type = this; Type; Type = Type->Parent)
	{
		const char* AliasName = NULL;
		const CPropInfo* CurrentProp = Type->Props;
		for (int i = 0; i < Type->NumProps; i++, CurrentProp++)
		{
			if (!(stricmp(CurrentProp->Name, Name)))
			{
				if (CurrentProp->TypeName && CurrentProp->TypeName[0] == '>')
				{
					// Alias, redirection to another property
					AliasName = CurrentProp->TypeName + 1;
					break;
				}
				return CurrentProp;
			}
		}
		if (AliasName)
		{
			const CPropInfo* CurrentProp = Type->Props;
			for (int i = 0; i < Type->NumProps; i++, CurrentProp++)
			{
				if (!(stricmp(CurrentProp->Name, AliasName)))
				{
					assert(!CurrentProp->TypeName || CurrentProp->TypeName[0] != '>'); // no cyclic aliases
					return CurrentProp;
				}
			}
			appError("Alias %s for property %s::%s not found", AliasName, this->Name, Name);
		}
	}
	return NULL;
	unguard;
}


/*-----------------------------------------------------------------------------
	CTypeInfo dump functionality
-----------------------------------------------------------------------------*/

struct CPropDump
{
	FString				Name;
	FString				Value;
	TArray<CPropDump>	Nested;				// Value should be "" when Nested[] is not empty
	bool				bIsArrayItem;
	bool				bDiscard;

	CPropDump()
	: bIsArrayItem(false)
	, bDiscard(false)
	{}

	void PrintName(const char *fmt, ...)
	{
		va_list	argptr;
		va_start(argptr, fmt);
		PrintTo(Name, fmt, argptr);
		va_end(argptr);
	}

	void PrintValue(const char *fmt, ...)
	{
		va_list	argptr;
		va_start(argptr, fmt);
		PrintTo(Value, fmt, argptr);
		va_end(argptr);
	}

	void Discard()
	{
		bDiscard = true;
	}

private:
	void PrintTo(FString& Dst, const char *fmt, va_list argptr)
	{
		char buffer[1024];
		vsnprintf(ARRAY_ARG(buffer), fmt, argptr);
		Dst += buffer;
	}
};


static void CollectProps(const CTypeInfo *Type, const void *Data, CPropDump &Dump)
{
	for (/* empty */; Type; Type = Type->Parent)
	{
		if (!Type->NumProps) continue;

		for (int PropIndex = 0; PropIndex < Type->NumProps; PropIndex++)
		{
			const CPropInfo *Prop = Type->Props + PropIndex;
			if (!Prop->Count)
			{
//				appPrintf("  %3d: (dummy) %s\n", PropIndex, Prop->Name);
				continue;
			}
			CPropDump *PD = new (Dump.Nested) CPropDump;

			// name

#if DUMP_SHOW_PROP_INDEX
			PD->PrintName("(%d)", PropIndex);
#endif
#if DUMP_SHOW_PROP_TYPE
			PD->PrintName("%s ", (Prop->TypeName[0] != '#') ? Prop->TypeName : Prop->TypeName+1);	// skip enum marker
#endif
			PD->PrintName("%s", Prop->Name);

			// value

			byte *value = (byte*)Data + Prop->Offset;
			int PropCount = Prop->Count;

			bool IsArray = (PropCount > 1) || (PropCount == -1);
			if (PropCount == -1)
			{
				// TArray<> value
				FArray *Arr = (FArray*)value;
				value     = (byte*)Arr->GetData();
				PropCount = Arr->Num();
			}

			// find structure type
			const CTypeInfo *StrucType = FindStructType(Prop->TypeName);
			bool IsStruc = (StrucType != NULL);

			// formatting of property start
			if (IsArray)
			{
				PD->PrintName("[%d]", PropCount);
				if (!PropCount)
				{
					PD->PrintValue("{}");
					continue;
				}
			}

			// dump item(s)
			for (int ArrayIndex = 0; ArrayIndex < PropCount; ArrayIndex++)
			{
				CPropDump *PD2 = PD;
				if (IsArray)
				{
					// create nested CPropDump
					PD2 = new (PD->Nested) CPropDump;
					PD2->PrintName("%s[%d]", Prop->Name, ArrayIndex);
					PD2->bIsArrayItem = true;
				}

				// note: ArrayIndex is used inside PROP macro

#define PROP(type)	( ((type*)value)[ArrayIndex] )

#define IS(name) 	strcmp(Prop->TypeName, #name) == 0

#define PROCESS(type, format, value)	\
					if (IS(type))		\
					{					\
						PD2->PrintValue(format, value); \
					}

				PROCESS(byte,     "%d", PROP(byte));
				PROCESS(int,      "%d", PROP(int));
				PROCESS(bool,     "%s", PROP(bool) ? "true" : "false");
				PROCESS(float,    "%g", PROP(float));
				if (IS(UObject*))
				{
					UObject *obj = PROP(UObject*);
					if (obj)
					{
						if (!(obj->GetTypeinfo()->TypeFlags & TYPE_InlinePropDump))
						{
							char ObjName[256];
							obj->GetFullName(ARRAY_ARG(ObjName));
							PD2->PrintValue("%s'%s'", obj->GetClassName(), ObjName);
						}
						else
						{
							// Process this like a structure
							CollectProps(obj->GetTypeinfo(), obj, *PD2);
						}
					}
					else
					{
						if (IsArray)
						{
							PD2->Discard();
						}
						PD2->PrintValue("None");
					}
				}
				PROCESS(FName,    "%s", *PROP(FName));
				PROCESS(FString,  "\"%s\"", *PROP(FString));
				if (Prop->TypeName[0] == '#')
				{
					// enum value
					const char *v = EnumToName(Prop->TypeName+1, *value);		// skip enum marker
					PD2->PrintValue("%s (%d)", v ? v : "<unknown>", *value);
				}
				if (IsStruc)
				{
					// this is a structure type
					CollectProps(StrucType, value + ArrayIndex * StrucType->SizeOf, *PD2);
				}
			} // ArrayIndex loop
		} // PropIndex loop
	} // Type->Parent loop
}

#undef PROCESS

static void PrintIndent(FArchive& Ar, int Value)
{
	for (int i = 0; i < Value; i++)
		Ar.Printf("    ");
}

static void PrintProps(const CPropDump &Dump, FArchive& Ar, int Indent, bool TopLevel, int MaxLineWidth = 80)
{
	PrintIndent(Ar, Indent);

	const int NumNestedProps = Dump.Nested.Num();
	if (NumNestedProps)
	{
		// complex property
		bool bNamePrinted = false;
		if (!Dump.Name.IsEmpty())
		{
			Ar.Printf("%s =", *Dump.Name);	// root CPropDump will not have a name
			bNamePrinted = true;
		}

		bool IsSimple = true;
		int TotalLen = 0;
		int i;

		// check whether we can display all nested properties in a single line or not
		for (const CPropDump &Prop : Dump.Nested)
		{
			if (Prop.bDiscard) continue;

			if (Prop.Nested.Num())
			{
				IsSimple = false;
				break;
			}
			TotalLen += Prop.Value.Len() + 2;
			if (!Prop.bIsArrayItem)
				TotalLen += Prop.Name.Len();
			if (TotalLen >= MaxLineWidth)
			{
				IsSimple = false;
				break;
			}
		}

		if (IsSimple)
		{
			// single-line value display
			Ar.Printf(" { ");
			bool bFirst = true;
			for (const CPropDump &Prop : Dump.Nested)
			{
				if (Prop.bDiscard) continue;

				if (!bFirst)
				{
					Ar.Printf(", ");
				}
				bFirst = false;

				if (Prop.bIsArrayItem)
					Ar.Printf("%s", *Prop.Value);
				else
					Ar.Printf("%s=%s", *Prop.Name, *Prop.Value);
			}
			Ar.Printf(" }\n");
		}
		else
		{
			// complex value display
			if (bNamePrinted) Ar.Printf("\n");
			if (!TopLevel)
			{
				PrintIndent(Ar, Indent);
				Ar.Printf("{\n");
			}

			for (const CPropDump &Prop : Dump.Nested)
			{
				if (Prop.bDiscard) continue;
				PrintProps(Prop, Ar, Indent+1, false, MaxLineWidth);
			}

			if (!TopLevel)
			{
				PrintIndent(Ar, Indent);
				Ar.Printf("}\n");
			}
		}
	}
	else
	{
		// single property
		if (!Dump.Name.IsEmpty()) Ar.Printf("%s = %s\n", *Dump.Name, *Dump.Value);
	}
}


class FLineOutputArchive : public FArchive
{
	DECLARE_ARCHIVE(FLineOutputArchive, FArchive);
public:
	typedef void (*Callback_t)(const char*);

	FLineOutputArchive(Callback_t InCallback)
	: Callback(InCallback)
	{}
	~FLineOutputArchive()
	{
		// Flush the buffer if there's anything inside
		if (!Buffer.IsEmpty())
		{
			Callback(*Buffer);
		}
	}
	virtual void Seek(int Pos)
	{
		appError("FLineOutputArchive::Seek");
	}
	virtual void Serialize(void *data, int size)
	{
		const char* dataStart = (const char*) data;
		int length = 0;
		for (const char* s = (const char*)data; /* none */; s++)
		{
			char c = *s;
			if (c == '\n' || c == 0)
			{
				// Append data to buffer
				Buffer.AppendChars(dataStart, length);
				dataStart = s + 1;
				length = 0;
			}
			if (c == '\n')
			{
				// Flush only on newline character
				Callback(*Buffer);
				// Empty without reallocation
				Buffer.GetDataArray().Reset();
			}
			else if (c == 0)
			{
				// End of text
				break;
			}
			else
			{
				// Just count the character
				length++;
			}
		}
	}

	Callback_t Callback;
	FStaticString<1024> Buffer;
};


void CTypeInfo::DumpProps(const void *Data) const
{
	guard(CTypeInfo::DumpProps);
	CPropDump Dump;
	CollectProps(this, Data, Dump);

	// Indent = 0 will actually produce indent anyway, because we have parent CPropDump
	// object which owns everything else
	FPrintfArchive Ar;
	PrintProps(Dump, Ar, 0, true);

	unguard;
}

void CTypeInfo::DumpProps(const void* Data, void (*Callback)(const char*)) const
{
	guard(CTypeInfo::DumpProps);
	CPropDump Dump;
	CollectProps(this, Data, Dump);

	// Indent = 0 will actually produce indent anyway, because we have parent CPropDump
	// object which owns everything else
	FLineOutputArchive Ar(Callback);
	PrintProps(Dump, Ar, 0, true);

	unguard;
}

void CTypeInfo::SaveProps(const void *Data, FArchive& Ar) const
{
	guard(CTypeInfo::SaveProps);
	CPropDump Dump;
	CollectProps(this, Data, Dump);

	// Note: using indent -1 for better in-file formatting
	PrintProps(Dump, Ar, -1, true, MAX_DUMP_LINE);

	unguard;
}

static void SkipWhitespace(FArchive& Ar)
{
	while (!Ar.IsEof())
	{
		char c;
		Ar << c;
		if (!isspace(c))
		{
			Ar.Seek(Ar.Tell() - 1);
			break;
		}
	}
}

static void GetToken(FArchive& Ar, FString& Out)
{
	guard(GetToken);

	Out.Empty();

	SkipWhitespace(Ar);

	char buffer[1024];
	char* s = buffer;

	while (!Ar.IsEof())
	{
		char c;
		Ar << c;
		if (c == '"' && s == buffer)
		{
			// This is a string
			while (!Ar.IsEof())
			{
				Ar << c;
				if (c == '"')
				{
					break;
				}
				if (c == '\r' || c == '\n')
				{
					appPrintf("GetToken: mismatched quote at position %d\n", Ar.Tell());
					break;
				}
				*s++ = c;
			}
			break;
		}
		bool isDelimiter = (c == '=' || c == '{' || c == '}');
		if (s != buffer && (isspace(c) || isDelimiter))
		{
			// Found a delimiter character in the middle of string
			Ar.Seek(Ar.Tell() - 1);
			break;
		}
		*s++ = c;

		if (isDelimiter)
		{
			// Delimiters are single characters, and separate tokens
			break;
		}
	}

	// Return token
	assert(s < buffer + ARRAY_COUNT(buffer));
	*s = 0;
	Out = buffer;
//	printf("TOKEN: [%s]\n", *Out); //!!!!

	unguard;
}

static bool SkipProperty(FArchive& Ar)
{
	FStaticString<32> Token;
	GetToken(Ar, Token);
	if (Token != "=")
	{
		return false;
	}

	GetToken(Ar, Token);
	if (Token == "{")
	{
		// should skip structure block, not supported at the moment
		return false;
	}

	return true;
}

bool CTypeInfo::LoadProps(void *Data, FArchive& Ar) const
{
	guard(CTypeInfo::LoadProps);

	while (!Ar.IsEof())
	{
		int ArrayIndex = 0; //!! todo: use
		int LastPosition = Ar.Tell();

		FStaticString<256> Token;
		GetToken(Ar, Token);

		if (Token == ",")
		{
			// Property delimiter
			continue;
		}

		if (Token == "}" || Token.IsEmpty())
		{
			// End of structure or end of file
			break;
		}

		const CPropInfo* Prop = FindProperty(*Token);
		if (!Prop)
		{
			appPrintf("LoadProps: unknown property %s\n", *Token);
			// Try to recover
			if (!SkipProperty(Ar))
			{
				return false;
			}
			continue;
		}
		byte *value = (byte*)Data + Prop->Offset;
//		printf("Prop: %s %s\n", Prop->TypeName, Prop->Name); //!!!!

		LastPosition = Ar.Tell();
		FStaticString<32> Token2;
		GetToken(Ar, Token2);
		if (Token2 != "=")
		{
			appPrintf("LoadProps: \"=\" expected at position %d\n", LastPosition);
			return false;
		}

		const CTypeInfo *StrucType = FindStructType(Prop->TypeName);
		bool IsStruc = (StrucType != NULL);
		if (IsStruc)
		{
			LastPosition = Ar.Tell();
			GetToken(Ar, Token2);
			if (Token2 != "{")
			{
				appPrintf("LoadProps: \"{\" expected at position %d\n", LastPosition);
				return false;
			}
			if (!StrucType->LoadProps(value + ArrayIndex * StrucType->SizeOf, Ar))
			{
				return false;
			}
			// at this point, '}' should be already read
			continue;
		}

		GetToken(Ar, Token2);
		if (IS(FString))
		{
			PROP(FString) = Token2;
		}
		else if (IS(bool))
		{
			PROP(bool) = (stricmp(*Token2, "true") == 0 || Token2 == "1");
		}
		else if (IS(int))
		{
			PROP(int) = atoi(*Token2);
		}
		else
		{
			appPrintf("LoadProps: unknown type %s\n", Prop->TypeName);
			return false;
		}
	}

	return true;

	unguard;
}
