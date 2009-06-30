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
		if (strcmp(GetClassName(), "Class") != 0)
			Super::Serialize(Ar);
		Ar << SuperField << Next;
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
		Ar << ScriptText << Children << FriendlyName;
#if UT2
		if (Ar.IsUT2 && Ar.ArLicenseeVer >= 25)	//?? other games too ?
			Ar << f60;
#endif
		Ar << Line << TextPos;
		assert(Ar.IsLoading);
		int ScriptSize;
		Ar << ScriptSize;
		if (ScriptSize)
		{
			Script.Empty(ScriptSize);
			Script.Add(ScriptSize);
			printf("script: %d, rest: %d\n", ScriptSize, Ar.GetStopper() - Ar.Tell());
			Ar.Serialize(&Script[0], ScriptSize);
		}
//		Ar.Seek(Ar.Tell() + ScriptSize);	// skip scripts
//		//?? following code: loop of UStruct::SerializeExpr(int &,FArchive &)
//		Ar.Seek(Ar.GetStopper());
		unguard;
	}
};


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
	FName			Category;
	word			f48;
	FString			f64;

	virtual void Serialize(FArchive &Ar)
	{
		guard(UProperty::Serialize);
		Super::Serialize(Ar);
		Ar << ArrayDim << PropertyFlags << Category;
		if (PropertyFlags & 0x20)
			Ar << f48;
		if (PropertyFlags & 0x2000000)
			Ar << f64;
//		printf("prop %s [%d] %X (%s)\n", Name, ArrayDim, PropertyFlags, *Category);
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
		Ar << Struct;
		unguard;
	}
};


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
