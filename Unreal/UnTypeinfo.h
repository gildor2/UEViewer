/*-----------------------------------------------------------------------------
	Unreal Engine Typeinfo
-----------------------------------------------------------------------------*/


/*
	UField
		UConst
		UEnum
		UStruct
			UFunction
			UState
				UClass
		UProperty
			UByteProperty
			UIntProperty
			UBoolProperty
			UFloatProperty
			UObjectProperty
			UClassProperty
			UNameProperty
			UStrProperty
			UFixedArrayProperty		- not used, missing in UE3
			UArrayProperty
			UMapProperty
			UStructProperty
			// UE3 extra types
			UDelegateProperty
			UInterfaceProperty
*/

class UField : public UObject
{
	DECLARE_CLASS(UField, UObject);
public:
	UField			*SuperField;
	UField			*Next;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UField::Serialize);
		assert(Ar.IsLoading);
		Super::Serialize(Ar);
		Ar << SuperField << Next;
//		printf("super: %s next: %s\n", SuperField ? SuperField->Name : "-", Next ? Next->Name : "-");
		unguard;
	}
};


class UEnum : public UField
{
	DECLARE_CLASS(UEnum, UField);
public:
	TArray<FName>	Names;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UEnum::Serialize);
		Super::Serialize(Ar);
		Ar << Names;
		unguard;
	}
};


class UConst : public UField
{
	DECLARE_CLASS(UConst, UField);
public:
	FString			Value;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UConst::Serialize);
		Super::Serialize(Ar);
		Ar << Value;
		unguard;
	}
};


class UFunction : public UField
{
	DECLARE_CLASS(UFunction, UField);
public:
	//!! dummy class
	virtual void Serialize(FArchive &Ar)
	{
		guard(UFunction::Serialize);
		Super::Serialize(Ar);
		Ar.Seek(Ar.GetStopper());	//!!
		unguard;
	}
};


class UTextBuffer : public UObject
{
	DECLARE_CLASS(UTextBuffer, UObject);
public:
	int				Pos;
	int				Top;
	FString			Text;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UTextBuffer::Serialize);
		Super::Serialize(Ar);
		Ar << Pos << Top << Text;
		unguard;
	}
};


class UStruct : public UField
{
	DECLARE_CLASS(UStruct, UField);
public:
	UTextBuffer		*ScriptText;
#if UNREAL3
	UTextBuffer		*CppText;
#endif
	UField			*Children;
	FName			FriendlyName;
	int				TextPos;
	int				Line;
#if UT2
	int				f60;
#endif
	TArray<byte>	Script;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UStruct::Serialize);
		Super::Serialize(Ar);
		Ar << ScriptText << Children;
#if UNREAL3
		if (Ar.Game >= GAME_UE3)
			Ar << CppText;
#endif
//		printf("%s: cpp:%s scr:%s child:%s\n", Name, CppText ? CppText->Name : "-", ScriptText ? ScriptText->Name : "-", Children ? Children->Name : "-");
		if (Ar.Game < GAME_UE3)
			Ar << FriendlyName;	//?? UT2 ? or UE2 ?
#if UT2
		if (Ar.Game == GAME_UT2 && Ar.ArLicenseeVer >= 25)	//?? other games too ?
			Ar << f60;
#endif
		Ar << Line << TextPos;

#if BORDERLANDS
		if (Ar.Game == GAME_Borderlands) Exchange((UObject*&)ScriptText, (UObject*&)Children);	//?? strange ...
#endif

		assert(Ar.IsLoading);
		int ScriptSize;
		Ar << ScriptSize;
		if (ScriptSize)
		{
			printf("script: %d, rest: %d\n", ScriptSize, Ar.GetStopper() - Ar.Tell());
			Script.Empty(ScriptSize);
			Script.Add(ScriptSize);
			Ar.Serialize(&Script[0], ScriptSize);
		}
//		Ar.Seek(Ar.Tell() + ScriptSize);	// skip scripts
//		//?? following code: loop of UStruct::SerializeExpr(int &,FArchive &)
//		Ar.Seek(Ar.GetStopper());
		unguard;
	}
};

#if UNREAL3

class UScriptStruct : public UStruct
{
	DECLARE_CLASS(UScriptStruct, UStruct);
public:
	virtual void Serialize(FArchive &Ar)
	{
		Super::Serialize(Ar);
		// has properties here (struct defaults?)
		Ar.Seek(Ar.GetStopper());
	}
};

#endif // UNREAL3

class UState : public UStruct
{
	DECLARE_CLASS(UState, UStruct);
public:
	int64			ProbeMask;
	int64			IgnoreMask;
	int				StateFlags;
	word			LabelTableOffset;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UState::Serialize);
		Super::Serialize(Ar);
		Ar << ProbeMask << IgnoreMask << StateFlags << LabelTableOffset;
		unguard;
	}
};


class UClass : public UState
{
	DECLARE_CLASS(UClass, UState);
public:
	virtual void Serialize(FArchive &Ar)
	{
		guard(UClass::Serialize);
		Super::Serialize(Ar);
		//!! UStruct will drop remaining data
		Ar.Seek(Ar.GetStopper());
		unguard;
	}
};


class UProperty : public UField
{
	DECLARE_CLASS(UProperty, UField);
public:
	int				ArrayDim;
	unsigned		PropertyFlags;
#if UNREAL3
	unsigned		PropertyFlags2;	// uint64
#endif
	FName			Category;
	word			f48;
	FString			f64;
#if UNREAL3
	UObject			*unk60;			// UEnum, which constant is used to specify ArrayDim
#endif

	virtual void Serialize(FArchive &Ar)
	{
		guard(UProperty::Serialize);
		Super::Serialize(Ar);
		Ar << ArrayDim << PropertyFlags;
#if UNREAL3
		if (Ar.Game >= GAME_UE3)
			Ar << PropertyFlags2;
#endif
		Ar << Category;
#if UNREAL3
		if (Ar.Game >= GAME_UE3)
			Ar << unk60;
#endif
#if BORDERLANDS
		if (Ar.Game == GAME_Borderlands && Ar.ArLicenseeVer >= 2)
		{
			UObject *unk84, *unk88;
			Ar << unk84 << unk88;
		}
#endif // BORDERLANDS
		if (PropertyFlags & 0x20)
			Ar << f48;
		if (Ar.Game < GAME_UE3)
		{
			//?? UT2 only ?
			if (PropertyFlags & 0x2000000)
				Ar << f64;
		}
#if TNA_IMPACT
		if (Ar.Game == GAME_TNA && PropertyFlags2 & 0x20)
		{
			word unk58;
			Ar << unk58;
		}
#endif // TNA_IMPACT
#if A51
		//?? Area 51: if Flags2 & 0x80000000 -> serialize "FString f64"
		if (Ar.Game == GAME_A51 && (PropertyFlags2 & 0x80000000))
			Ar << f64;
#endif // A51
//		printf("... prop %s [%d] %X:%X (%s)\n", Name, ArrayDim, PropertyFlags, PropertyFlags2, *Category);
		unguard;
	}
};


class UByteProperty : public UProperty
{
	DECLARE_CLASS(UByteProperty, UProperty);
public:
	UEnum			*Enum;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UByteProperty::Serialize);
		Super::Serialize(Ar);
		Ar << Enum;
		unguard;
	}
};


class UIntProperty : public UProperty
{
	DECLARE_CLASS(UIntProperty, UProperty);
public:
/*	virtual void Serialize(FArchive &Ar)
	{
		guard(UIntProperty::Serialize);
		Super::Serialize(Ar);
		unguard;
	} */
};


class UBoolProperty : public UProperty
{
	DECLARE_CLASS(UBoolProperty, UProperty);
public:
	virtual void Serialize(FArchive &Ar)
	{
		guard(UBoolProperty::Serialize);
		Super::Serialize(Ar);
		unguard;
	}
};


class UFloatProperty : public UProperty
{
	DECLARE_CLASS(UFloatProperty, UProperty);
public:
/*	virtual void Serialize(FArchive &Ar)
	{
		guard(UFloatProperty::Serialize);
		Super::Serialize(Ar);
		unguard;
	} */
};


class UObjectProperty : public UProperty
{
	DECLARE_CLASS(UObjectProperty, UProperty);
public:
	UClass			*PropertyClass;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UObjectProperty::Serialize);
		Super::Serialize(Ar);
		Ar << PropertyClass;
		unguard;
	}
};


class UClassProperty : public UObjectProperty
{
	DECLARE_CLASS(UClassProperty, UObjectProperty);
public:
	UClass			*MetaClass;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UClassProperty::Serialize);
		Super::Serialize(Ar);
		Ar << MetaClass;
		unguard;
	}
};


class UNameProperty : public UProperty
{
	DECLARE_CLASS(UNameProperty, UProperty);
public:
/*	virtual void Serialize(FArchive &Ar)
	{
		guard(UNameProperty::Serialize);
		Super::Serialize(Ar);
		unguard;
	} */
};


class UStrProperty : public UProperty
{
	DECLARE_CLASS(UStrProperty, UProperty);
public:
/*	virtual void Serialize(FArchive &Ar)
	{
		guard(UStrProperty::Serialize);
		Super::Serialize(Ar);
		unguard;
	} */
};


class UArrayProperty : public UProperty
{
	DECLARE_CLASS(UArrayProperty, UProperty);
public:
	UProperty		*Inner;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UArrayProperty::Serialize);
		Super::Serialize(Ar);
		Ar << Inner;
		unguard;
	}
};


class UMapProperty : public UProperty
{
	DECLARE_CLASS(UMapProperty, UProperty);
public:
	UProperty		*Key;
	UProperty		*Value;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UMapProperty::Serialize);
		Super::Serialize(Ar);
		Ar << Key << Value;
		unguard;
	}
};


class UStructProperty : public UProperty
{
	DECLARE_CLASS(UStructProperty, UProperty);
public:
	UStruct			*Struct;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UStructProperty::Serialize);
		Super::Serialize(Ar);
printf("Struct(%s) rest: %X\n", Name, Ar.GetStopper() - Ar.Tell());	//!!!
		Ar << Struct;
		unguard;
	}
};


#if UNREAL3

class UComponentProperty : public UProperty
{
	DECLARE_CLASS(UComponentProperty, UProperty);
public:
	UObject			*SomeName;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UComponentProperty::Serialize);
		Super::Serialize(Ar);
		Ar << SomeName;
		unguard;
	}
};

#endif // UNREAL3


// UT2-specific?
class UPointerProperty : public UProperty
{
	DECLARE_CLASS(UPointerProperty, UProperty);
};


#define REGISTER_TYPEINFO_CLASSES		\
	REGISTER_CLASS(UClass)				\
	REGISTER_CLASS(UStruct)				\
	REGISTER_CLASS(UTextBuffer)			\
	REGISTER_CLASS(UEnum)				\
	REGISTER_CLASS(UConst)				\
	REGISTER_CLASS(UFunction)			\
	REGISTER_CLASS(UByteProperty)		\
	REGISTER_CLASS(UIntProperty)		\
	REGISTER_CLASS(UBoolProperty)		\
	REGISTER_CLASS(UFloatProperty)		\
	REGISTER_CLASS(UObjectProperty)		\
	REGISTER_CLASS(UClassProperty)		\
	REGISTER_CLASS(UNameProperty)		\
	REGISTER_CLASS(UStrProperty)		\
	REGISTER_CLASS(UArrayProperty)		\
	REGISTER_CLASS(UMapProperty)		\
	REGISTER_CLASS(UStructProperty)		\
	REGISTER_CLASS(UPointerProperty)

#define REGISTER_TYPEINFO_CLASSES_U3	\
	REGISTER_CLASS(UScriptStruct)		\
	REGISTER_CLASS(UComponentProperty)
