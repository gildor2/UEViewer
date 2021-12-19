#ifndef __UE4_VERSION_H__
#define __UE4_VERSION_H__

#if UNREAL4

// Unreal engine 4 versions, declared as enum to be able to see all revisions in a single place
enum
{
	// Pre-release UE4 file versions
	VER_UE4_ASSET_REGISTRY_TAGS = 112,
	VER_UE4_TEXTURE_DERIVED_DATA2 = 124,
	VER_UE4_ADD_COOKED_TO_TEXTURE2D = 125,
	VER_UE4_REMOVED_STRIP_DATA = 130,
	VER_UE4_REMOVE_EXTRA_SKELMESH_VERTEX_INFLUENCES = 134,
	VER_UE4_TEXTURE_SOURCE_ART_REFACTOR = 143,
	VER_UE4_ADD_SKELMESH_MESHTOIMPORTVERTEXMAP = 152,
	VER_UE4_REMOVE_ARCHETYPE_INDEX_FROM_LINKER_TABLES = 163,
	VER_UE4_REMOVE_NET_INDEX = 196,
	VER_UE4_BULKDATA_AT_LARGE_OFFSETS = 198,
	VER_UE4_SUMMARY_HAS_BULKDATA_OFFSET = 212,
	VER_UE4_STATIC_MESH_STORE_NAV_COLLISION = 216,
	VER_UE4_DEPRECATED_STATIC_MESH_THUMBNAIL_PROPERTIES_REMOVED = 242,
	VER_UE4_APEX_CLOTH = 254,
	VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX = 269,
	VER_UE4_SUPPORT_32BIT_STATIC_MESH_INDICES = 277,
	VER_UE4_APEX_CLOTH_LOD = 280,
	VAR_UE4_ARRAY_PROPERTY_INNER_TAGS = 282, // note: here's a typo in UE4 code - "VAR_" instead of "VER_"
	VER_UE4_KEEP_SKEL_MESH_INDEX_DATA = 283,
	VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING = 302,
	VER_UE4_REFERENCE_SKELETON_REFACTOR = 310,
	VER_UE4_FIXUP_ROOTBONE_PARENT = 312,
	VER_UE4_FIX_ANIMATIONBASEPOSE_SERIALIZATION = 331,
	VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES = 332,
	VER_UE4_SUPPORT_GPUSKINNING_8_BONE_INFLUENCES = 334,
	VER_UE4_ANIM_SUPPORT_NONUNIFORM_SCALE_ANIMATION = 335,
	VER_UE4_ENGINE_VERSION_OBJECT = 336,
	VER_UE4_SKELETON_GUID_SERIALIZATION = 338,
	// UE4.0 source code was released on GitHub. Note: if we don't have any VER_UE4_...
	// values between two VER_UE4_xx constants, for instance, between VER_UE4_0 and VER_UE4_1,
	// it doesn't matter for this framework which version will be serialized serialized -
	// 4.0 or 4.1, because 4.1 has nothing new for supported object formats compared to 4.0.
	VER_UE4_0 = 342,
		VER_UE4_MORPHTARGET_CPU_TANGENTZDELTA_FORMATCHANGE = 348,
	VER_UE4_1 = 352,
	VER_UE4_2 = 363,
		VER_UE4_LOAD_FOR_EDITOR_GAME = 365,
		VER_UE4_FTEXT_HISTORY = 368,					// used for UStaticMesh versioning
		VER_UE4_STORE_BONE_EXPORT_NAMES = 370,
	VER_UE4_3 = 382,
		VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP = 384,
	VER_UE4_4 = 385,
		VER_UE4_SKELETON_ADD_SMARTNAMES = 388,
		VER_UE4_SOUND_COMPRESSION_TYPE_ADDED = 392,
		VER_UE4_RENAME_CROUCHMOVESCHARACTERDOWN = 394,	// used for UStaticMesh versioning
		VER_UE4_DEPRECATE_UMG_STYLE_ASSETS = 397,		// used for UStaticMesh versioning
	VER_UE4_5 = 401,
	VER_UE4_6 = 413,
		VER_UE4_RENAME_WIDGET_VISIBILITY = 416,			// used for UStaticMesh versioning
		VER_UE4_ANIMATION_ADD_TRACKCURVES = 417,
	VER_UE4_7 = 434,
		VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG = 441,
		VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION = 444,
	VER_UE4_8 = 451,
		VER_UE4_SERIALIZE_TEXT_IN_PACKAGES = 459,
	VER_UE4_9 = 482,
	VER_UE4_10 = VER_UE4_9,								// exactly the same file version for 4.9 and 4.10
		VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT = 485,
		VER_UE4_SOUND_CONCURRENCY_PACKAGE = 489,		// used for UStaticMesh versioning
	VER_UE4_11 = 498,
		VER_UE4_INNER_ARRAY_TAG_INFO = 500,
		VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG = 503,
		VER_UE4_NAME_HASHES_SERIALIZED = 504,
	VER_UE4_12 = 504,
	VER_UE4_13 = 505,
		VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS = 507,
		VER_UE4_TemplateIndex_IN_COOKED_EXPORTS = 508,
	VER_UE4_14 = 508,
		VER_UE4_PROPERTY_TAG_SET_MAP_SUPPORT = 509,
		VER_UE4_ADDED_SEARCHABLE_NAMES = 510,
	VER_UE4_15 = 510,
		VER_UE4_64BIT_EXPORTMAP_SERIALSIZES = 511,
	VER_UE4_16 = 513,
	VER_UE4_17 = 513,
		VER_UE4_ADDED_SOFT_OBJECT_PATH = 514,
	VER_UE4_18 = 514,
		VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID = 516,
	VER_UE4_19 = 516,
	VER_UE4_20 = 516,
	VER_UE4_21 = 517,
	VER_UE4_22 = 517,
	VER_UE4_23 = 517,
		VER_UE4_ADDED_PACKAGE_OWNER = 518,
	VER_UE4_24 = 518,
	VER_UE4_25 = 518,
		VER_UE4_SKINWEIGHT_PROFILE_DATA_LAYOUT_CHANGES = 519,
		VER_UE4_NON_OUTER_PACKAGE_IMPORT = 520,
	VER_UE4_26 = 522,
	VER_UE4_27 = 522,
	// look for NEW_ENGINE_VERSION over the code to find places where version constants should be inserted.
	// LATEST_SUPPORTED_UE4_VERSION should be updated too.
};

int GetUE4CustomVersion(const FArchive& Ar, const FGuid& Guid);


/*
 * Rules for writing code for custom versions
 * - Each version is a "struct". Ideally could use "enum class", however we're adding some code into it, so "struct" is ideal.
 * - Use "enum Type" inside of it.
 * - Each enum has members:
 *   - VersionPlusOne - equals to the recent declared constant + 1, used to correctly set up LatestVersion without explicit
 *     mentioning of the latest constant name.
 *   - optional: ForceVersion - use it in a case we're not declaring the latest version as a constant, so LatestVersion value
 *     will work correctly.
 * - There's "static Type Get(const FArchive& Ar)" function, it computes custom version based on Game constant. Needed in order
 *   to make unversioned packages to work.
 * - This function does comparison of game constant with engine number PLUS ONE, so games which are derived from e.g. UE4.25
 *   will work correctly after using logic "Game < GAME_UE4(26)", i.e. game is using engine smaller than UE4.26. If we'll use
 *   "Game <= GAME_UE4.25", this WILL work for UE4.25, but not for 4.25-based games which has custom override.
 */

struct FFrameworkObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		MoveCompressedAnimDataToTheDDC = 5,
		// UE4.12 = 6
		SmartNameRefactor = 7,
		// UE4.13 = 12
		RemoveSoundWaveCompressionName = 12,
		// UE4.14 = 17
		MoveCurveTypesToSkeleton = 15,
		CacheDestructibleOverlaps = 16,
		GeometryCacheMissingMaterials = 17,	// not needed now - for UGeometryCache
		// UE4.15 = 22
		// UE4.16 = 23
		// UE4.17 = 28
		// UE4.18 = 30
		// UE4.19 = 33
		// UE4.20, UE4.21 = 34
		// UE4.22, UE4.23 = 35
		// UE4.24 = 36
		// UE4.25-UE4.27 = 37

		ForceVersion = 37, // the recent version, added here just to force LatestVersion to be correct

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0xCFFC743F, 0x43B04480, 0x939114DF, 0x171D2073 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;

#if TEKKEN7
		if (Ar.Game == GAME_Tekken7) return (Type)14;		// pre-UE4.14
#endif

		if (Ar.Game < GAME_UE4(12))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(13))
			return (Type)6;
		if (Ar.Game < GAME_UE4(14))
			return RemoveSoundWaveCompressionName;
		if (Ar.Game < GAME_UE4(15))
			return GeometryCacheMissingMaterials;
		if (Ar.Game < GAME_UE4(16))
			return (Type)22;
		if (Ar.Game < GAME_UE4(17))
			return (Type)23;
		if (Ar.Game < GAME_UE4(18))
			return (Type)28;
		if (Ar.Game < GAME_UE4(19))
			return (Type)30;
		if (Ar.Game < GAME_UE4(20))
			return (Type)33;
		if (Ar.Game < GAME_UE4(22))
			return (Type)34;
		if (Ar.Game < GAME_UE4(24))
			return (Type)35;
		if (Ar.Game < GAME_UE4(25))
			return (Type)36;
		if (Ar.Game < GAME_UE4(28))
			return (Type)37;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FEditorObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.12 = 2
		// UE4.13 = 6
		// UE4.14 = 8
		RefactorMeshEditorMaterials = 8,
		// UE4.15 = 14
		UPropertryForMeshSection = 10,
		// UE4.16 = 17
		// UE4.17, UE4.18 = 20
		// UE4.19 = 23
		AddedMorphTargetSectionIndices = 23,
		// UE4.20 = 24
		// UE4.21 = 26
		// UE4.22 = 30
		StaticMeshDeprecatedRawMesh = 28,
		MeshDescriptionBulkDataGuid = 29,
		// UE4.23 = 34
		// UE4.24 = 37
		// UE4.25 = 38
		// UE4.26, UE4.27 = 40

		ForceVersion = 40,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0xE4B068ED, 0xF49442E9, 0xA231DA0B, 0x2E46BB41 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;

#if TEKKEN7
		if (Ar.Game == GAME_Tekken7) return (Type)7;		// pre-UE4.14
#endif
#if PARAGON
		if (Ar.Game == GAME_Paragon) return (Type)22;
#endif

		if (Ar.Game < GAME_UE4(12))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(13))
			return (Type)2;
		if (Ar.Game < GAME_UE4(14))
			return (Type)6;
		if (Ar.Game < GAME_UE4(15))
			return RefactorMeshEditorMaterials;
		if (Ar.Game < GAME_UE4(16))
			return (Type)14;
		if (Ar.Game < GAME_UE4(17))
			return (Type)17;
		if (Ar.Game < GAME_UE4(19))
			return (Type)20;
		if (Ar.Game < GAME_UE4(20))
			return AddedMorphTargetSectionIndices;
		if (Ar.Game < GAME_UE4(21))
			return (Type)24;
		if (Ar.Game < GAME_UE4(22))
			return (Type)26;
		if (Ar.Game < GAME_UE4(23))
			return (Type)30;
		if (Ar.Game < GAME_UE4(24))
			return (Type)34;
		if (Ar.Game < GAME_UE4(25))
			return (Type)37;
		if (Ar.Game < GAME_UE4(26))
			return (Type)38;
		if (Ar.Game < GAME_UE4(28))
			return (Type)40;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FSkeletalMeshCustomVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.13 = 4
		CombineSectionWithChunk = 1,
		CombineSoftAndRigidVerts = 2,
		RecalcMaxBoneInfluences = 3,
		SaveNumVertices = 4,
		// UE4.14 = 5
		// UE4.15 = 7
		UseSharedColorBufferFormat = 6,		// separate vertex stream for vertex influences
		UseSeparateSkinWeightBuffer = 7,	// use FColorVertexStream for both static and skeletal meshes
		// UE4.16, UE4.17 = 9
		NewClothingSystemAdded = 8,
		// UE4.18 = 10
		CompactClothVertexBuffer = 10,
		// UE4.19 = 15
		RemoveSourceData = 11,
		SplitModelAndRenderData = 12,
		RemoveTriangleSorting = 13,
		RemoveDuplicatedClothingSections = 14,
		DeprecateSectionDisabledFlag = 15,
		// UE4.20-UE4.22 = 16
		SectionIgnoreByReduceAdded = 16,
		// UE4.23-UE4.25 = 17
		SkinWeightProfiles = 17, //todo: FSkeletalMeshLODModel::Serialize (editor mesh)
		// UE4.26, UE4.27 = 18
		RemoveEnableClothLOD = 18, //todo

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0xD78A4A00, 0xE8584697, 0xBAA819B5, 0x487D46B4 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;
#if PARAGON
		if (Ar.Game == GAME_Paragon) return (Type)12;
#endif
#if UT4
		if (Ar.Game == GAME_UT4) return (Type)5;
#endif

		if (Ar.Game < GAME_UE4(13))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(14))
			return SaveNumVertices;
		if (Ar.Game < GAME_UE4(15))
			return (Type)5;
		if (Ar.Game < GAME_UE4(16))
			return UseSeparateSkinWeightBuffer;
		if (Ar.Game < GAME_UE4(18)) // 4.16 and 4.17
			return (Type)9;
		if (Ar.Game < GAME_UE4(19))
			return CompactClothVertexBuffer;
		if (Ar.Game < GAME_UE4(20))
			return DeprecateSectionDisabledFlag;
		if (Ar.Game < GAME_UE4(23))
			return SectionIgnoreByReduceAdded;
		if (Ar.Game < GAME_UE4(26))
			return SkinWeightProfiles;
		if (Ar.Game < GAME_UE4(28))
			return RemoveEnableClothLOD;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FCoreObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,

		// UE4.12-UE4.14 = 1
		// UE4.15-UE4.21 = 2
		// UE4.22-UE4.24 = 3
		SkeletalMaterialEditorDataStripping = 3,
		// UE4.25-UE4.27 = 4

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x375EC13C, 0x06E448FB, 0xB50084F0, 0x262A717E };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;
		if (Ar.Game < GAME_UE4(12))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(15))
			return (Type)1;
		if (Ar.Game < GAME_UE4(22))
			return (Type)2;
		if (Ar.Game < GAME_UE4(25))
			return SkeletalMaterialEditorDataStripping;
		if (Ar.Game < GAME_UE4(28))
			return (Type)4;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FRenderingObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.14 = 4
		// UE4.15 = 12
		TextureStreamingMeshUVChannelData = 10,
		// UE4.16 = 15
		// UE4.17 = 19
		// UE4.18 = 20
		// UE4.19 = 25
		// UE4.20 = 26
		IncreaseNormalPrecision = 26,
		// UE4.21 = 27
		// UE4.22 = 28
		// UE4.23 = 31
		// UE4.24 = 36
		StaticMeshSectionForceOpaqueField = 37,
		// UE4.25 = 43
		// UE4.26 = 44
		// UE4.27 = 45

		ForceVersion = 45,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x12F88B9F, 0x88754AFC, 0xA67CD90C, 0x383ABD29 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;

#if TEKKEN7
		if (Ar.Game == GAME_Tekken7) return (Type)9;		// pre-UE4.14
#endif

		if (Ar.Game < GAME_UE4(12))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(13))
			return (Type)2;
		if (Ar.Game < GAME_UE4(14))
			return (Type)4;
		if (Ar.Game < GAME_UE4(16))	// 4.14 and 4.15
			return (Type)12;
		if (Ar.Game < GAME_UE4(17))
			return (Type)15;
		if (Ar.Game < GAME_UE4(18))
			return (Type)19;
		if (Ar.Game < GAME_UE4(19))
			return (Type)20;
		if (Ar.Game < GAME_UE4(20))
			return (Type)25;
		if (Ar.Game < GAME_UE4(21))
			return IncreaseNormalPrecision;
		if (Ar.Game < GAME_UE4(22))
			return (Type)27;
		if (Ar.Game < GAME_UE4(23))
			return (Type)28;
		if (Ar.Game < GAME_UE4(24))
			return (Type)31;
		if (Ar.Game < GAME_UE4(25))
			return (Type)36;
		if (Ar.Game < GAME_UE4(26))
			return (Type)43;
		if (Ar.Game < GAME_UE4(27))
			return (Type)44;
		if (Ar.Game < GAME_UE4(28))
			return (Type)45;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FAnimObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.21-UE4.24 = 2
		StoreMarkerNamesOnSkeleton = 2,
		// UE4.25 = 7
		IncreaseBoneIndexLimitPerChunk = 4,
		UnlimitedBoneInfluences = 5,
		// UE4.26, UE4.27 = 15

		ForceVersion = 15,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0xAF43A65D, 0x7FD34947, 0x98733E8E, 0xD9C1BB05 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;
		if (Ar.Game < GAME_UE4(21))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(25))
			return StoreMarkerNamesOnSkeleton;
		if (Ar.Game < GAME_UE4(26))
			return (Type)7;
		if (Ar.Game < GAME_UE4(28))
			return (Type)15;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FAnimPhysObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.16 = 3
		RemoveUIDFromSmartNameSerialize = 5,
		// UE4.17 = 7
		SmartNameRefactorForDeterministicCooking = 10,
		// UE4.18 = 12
		AddLODToCurveMetaData = 12,
		// UE4.19 = 16
		ChangeRetargetSourceReferenceToSoftObjectPtr = 15,
		// UE4.20-UE4.27 = 17

		ForceVersion = 17,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x29E575DD, 0xE0A34627, 0x9D10D276, 0x232CDCEA };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;
		if (Ar.Game < GAME_UE4(16))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(17))
			return (Type)3;
		if (Ar.Game < GAME_UE4(18))
			return (Type)7;
		if (Ar.Game < GAME_UE4(19))
			return AddLODToCurveMetaData;
		if (Ar.Game < GAME_UE4(20))
			return (Type)16;
		if (Ar.Game < GAME_UE4(28))
			return (Type)17;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FReleaseObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.19 = 12
		AddSkeletalMeshSectionDisable = 12,
		// Not used in our code, so for version mapping please refer to "Get()" method below...

		ForceVersion = 43,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x9C54D522, 0xA8264FBE, 0x94210746, 0x61B482D0 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;
		if (Ar.Game < GAME_UE4(11))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(13))
			return (Type)1;
		if (Ar.Game < GAME_UE4(14))
			return (Type)3;
		if (Ar.Game < GAME_UE4(15))
			return (Type)4;
		if (Ar.Game < GAME_UE4(16))
			return (Type)7;
		if (Ar.Game < GAME_UE4(17))
			return (Type)9;
		if (Ar.Game < GAME_UE4(19))
			return (Type)10;
		if (Ar.Game < GAME_UE4(20))
			return AddSkeletalMeshSectionDisable;
		if (Ar.Game < GAME_UE4(21))
			return (Type)17;
		if (Ar.Game < GAME_UE4(23))
			return (Type)20;
		if (Ar.Game < GAME_UE4(24))
			return (Type)23;
		if (Ar.Game < GAME_UE4(25))
			return (Type)28;
		if (Ar.Game < GAME_UE4(26))
			return (Type)30;
		if (Ar.Game < GAME_UE4(27))
			return (Type)37;
		if (Ar.Game < GAME_UE4(28))
			return (Type)43;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};

struct FEnterpriseObjectVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,
		// UE4.24 = 8
		MeshDescriptionBulkDataGuidIsHash = 8,

		ForceVersion = 10,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static Type Get(const FArchive& Ar)
	{
		static const FGuid GUID = { 0x9DFFBCD6, 0x494F0158, 0xE2211282, 0x3C92A888 };
		int ver = GetUE4CustomVersion(Ar, GUID);
		if (ver >= 0)
			return (Type)ver;
		if (Ar.Game < GAME_UE4(20))
			return BeforeCustomVersionWasAdded;
		if (Ar.Game < GAME_UE4(21))
			return (Type)1;
		if (Ar.Game < GAME_UE4(22))
			return (Type)4;
		if (Ar.Game < GAME_UE4(24))
			return (Type)6;
		if (Ar.Game < GAME_UE4(25))
			return MeshDescriptionBulkDataGuidIsHash;
		if (Ar.Game < GAME_UE4(28))
			return (Type)10;
		// NEW_ENGINE_VERSION
		return LatestVersion;
	}
};


#endif // UNREAL4

#endif // __UE4_VERSION_H__
