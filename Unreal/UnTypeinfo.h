#ifndef __UNTYPEINFO_H__
#define __UNTYPEINFO_H__

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
	UField			*SuperField2;
	UField			*Next;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UField::Serialize);
		assert(Ar.IsLoading);
		Super::Serialize(Ar);
#if BIOSHOCK3
		if (Ar.Game == GAME_Bioshock3) goto new_ver;
#endif
		if (Ar.ArVer < 756)
			Ar << SuperField2;
	new_ver:
		Ar << Next;
//		appPrintf("super: %s next: %s\n", SuperField ? SuperField->Name : "-", Next ? Next->Name : "-");
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
//		for (int i = 0; i < Names.Num(); i++) appPrintf("enum %s: %d = %s\n", Name, i, Names[i]);
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
		DROP_REMAINING_DATA(Ar);	//!!
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
	UField			*SuperField;	// serialized in UField in older versions
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
#if UNREAL3
	#if BIOSHOCK3
		if (Ar.Game == GAME_Bioshock3) goto new_ver;
	#endif
		if (Ar.ArVer >= 756)
		{
		new_ver:
			Ar << SuperField;
		}
		else
			SuperField = SuperField2;	// serialized in parent
#endif // UNREAL3
#if MKVSDC || BATMAN
		if ((Ar.Game == GAME_MK && Ar.ArVer >= 472) || (Ar.Game == GAME_Batman4))
		{
			Ar << Children;
			return;				//!! remaining data will be dropped anyway
		}
#endif // MKVSDC
		Ar << ScriptText << Children;
#if UNREAL3
		if (Ar.Game >= GAME_UE3)
			Ar << CppText;
#endif
//		appPrintf("%s: cpp:%s scr:%s child:%s\n", Name, CppText ? CppText->Name : "-", ScriptText ? ScriptText->Name : "-", Children ? Children->Name : "-");
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
			int remaining = Ar.GetStopper() - Ar.Tell();
			appPrintf("script: %d, rest: %d\n", ScriptSize, remaining);
			if (remaining < ScriptSize)
			{
				appPrintf("WARNING: bad ScriptSize, dropping\n");
				return;
			}
			if (Ar.Game == GAME_Transformers) return;	//?? bad, did not understand the layout
			Script.Empty(ScriptSize);
			Script.Add(ScriptSize);
			Ar.Serialize(&Script[0], ScriptSize);
		}
//		Ar.Seek(Ar.Tell() + ScriptSize);	// skip scripts
//		//?? following code: loop of UStruct::SerializeExpr(int &,FArchive &)
//		DROP_REMAINING_DATA(Ar);
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
		DROP_REMAINING_DATA(Ar);
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
	uint16			LabelTableOffset;

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
		DROP_REMAINING_DATA(Ar);
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
	uint16			f48;
	FString			f64;
#if UNREAL3
	UObject			*unk60;			// UEnum, which constant is used to specify ArrayDim
#endif

	virtual void Serialize(FArchive &Ar)
	{
		guard(UProperty::Serialize);
		Super::Serialize(Ar);
#if LOST_PLANET3
		if (Ar.Game == GAME_LostPlanet3 && Ar.ArLicenseeVer >= 79)
		{
			int16 Dim2;
			Ar << Dim2;
			ArrayDim = Dim2;
			goto flags;
		}
#endif // LOST_PLANET3
		Ar << ArrayDim;
	flags:
		Ar << PropertyFlags;
#if UNREAL3
		if (Ar.Game >= GAME_UE3)
			Ar << PropertyFlags2;
#endif
#if MKVSDC
		if (Ar.Game == GAME_MK && Ar.ArVer >= 472) goto skip_1;
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
#if BATMAN
		if (Ar.Game >= GAME_Batman2 && Ar.Game <= GAME_Batman4)
		{
			// property flags constants were changed since ArLicenseeVer >= 101, performed conversion
			// we need correct 0x20 value only
			bool IsNet = (PropertyFlags & 0x4000000) != 0;
			PropertyFlags = IsNet ? 0x20 : 0;
		}
#endif // BATMAN
	skip_1:
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
			uint16 unk58;
			Ar << unk58;
		}
#endif // TNA_IMPACT
#if A51
		//?? Area 51: if Flags2 & 0x80000000 -> serialize "FString f64"
		if (Ar.Game == GAME_A51 && (PropertyFlags2 & 0x80000000))
			Ar << f64;
#endif // A51
//		appPrintf("... prop %s [%d] %X:%X (%s)\n", Name, ArrayDim, PropertyFlags, PropertyFlags2, *Category);
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
appPrintf("Struct(%s) rest: %X\n", Name, Ar.GetStopper() - Ar.Tell());	//!!!
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


#if MKVSDC

class UNativeTypeProperty : public UProperty
{
	DECLARE_CLASS(UNativeTypeProperty, UProperty);
public:
	FName			TypeName;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UNativeTypeProperty::Serialize);
		Super::Serialize(Ar);
		Ar << TypeName;
		unguard;
	}
};

#endif // MKVSDC


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

#define REGISTER_TYPEINFO_CLASSES_MK	\
	REGISTER_CLASS(UNativeTypeProperty)	\
	REGISTER_CLASS_ALIAS(UByteProperty, UResourceProperty)

#endif // __UNTYPEINFO_H__
