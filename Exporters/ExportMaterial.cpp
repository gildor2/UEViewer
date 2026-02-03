#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMaterial/UnMaterial3.h"
#include "UnrealMaterial/UnMaterialExpression.h"
#include "unrealPackage/UnPackage.h"
#include "Exporters.h"



bool IsValidCString(const char* str, size_t maxLen = 1024)
{
	if (!str) return false;

	__try {
		for (size_t i = 0; i < maxLen; ++i)
		{
			if (str[i] == '\0') return true; // found null terminator
		}
		return false; // too long or unterminated
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false; // access violation or bad pointer
	}
}

//Hack. get texture type by addition in texture name
FString GetTextureType(const UUnrealMaterial* Tex)
{
	struct TextureSuffix {
		const char* suffix;
		const char* type;
	};

	TextureSuffix suffixMap[] =
	{
		{ "_D", "Diffuse" },
		{ "_D2", "Diffuse" },
		//{ "_pack", "Diffuse"}, // ... In custom materials, but used common
		{ "_N", "Normal" },
		{ "_MASK", "Mask" },
		{ "_Cube", "Cubemap" },
		{ "_crl", "Detail"}, // ???
	};

	if (IsValidCString(Tex->Name) == false)
	{
		printf("Invalid texture name / pointer \n");
		return "";
	}

	const char* name = Tex->Name;
	int nameLen = strlen(name);

	for (int i = 0; i < ARRAY_COUNT(suffixMap); i++)
	{
		const char* suffix = suffixMap[i].suffix;
		int suffixLen = strlen(suffix);

		if (nameLen >= suffixLen && stricmp(name + nameLen - suffixLen, suffix) == 0)
		{
			printf("Type: %s for texture %s\n", suffixMap[i].type, name);
			return FString(suffixMap[i].type);
		}
	}

	return "";
}

void ExportMaterial(const UUnrealMaterial* Mat)
{
	guard(ExportMaterial);

#if RENDERING				// requires UUnrealMaterial::GetParams()

	if (!Mat) return;

	if (Mat->IsTextureCube())
	{
		ExportCubemap(Mat);
		return;
	}

	if (Mat->IsTexture())
	{
		ExportTexture(Mat);
		return;
	}

	//todo: handle Mat->IsTexture(), Mat->IsTextureCube() to select exporter code
	//todo: remove separate texture handling from Main.cpp exporter registration

	if (Mat->IsA("MaterialInstanceConstant"))
	{
		const UMaterialInstance* MIC = (const UMaterialInstance*)Mat;
		// Access Violation 0x00000000 occurs if Parent is NULL or invalid
		if (!MIC->Parent) {
			appPrintf("Skipping %s: NULL Parent material detected to prevent crash.\n", *Mat->Name);
			return;
		}
	}

	TArray<UUnrealMaterial*> AllTextures;
	CMaterialParams Params;
	printf("Will append referenced textures\n");
	Mat->AppendReferencedTextures(AllTextures, false);
	printf("Appended Referenced Textures\n");

	for (int i = 0; i < AllTextures.Num(); i++)
	{
		UObject* Tex = AllTextures[i];  // Use UObject* if you're dumping textures
		if (Tex)
		{
			char FullName[256];  // Allocate a buffer
			Tex->GetFullName(FullName, sizeof(FullName), true, true, true);
			appPrintf("Referenced_U texture [%d]: %s\n", i, FullName);
		}
	}



	FArchive* Ar = CreateExportArchive(Mat, EFileArchiveOptions::TextFile, "%s.mat", Mat->Name);
	if (!Ar) return;

	TArray<UObject*> ToExport;

	if (Mat->IsA("Material3"))
	{
		const UMaterial3* Mat3 = static_cast<const UMaterial3*>(Mat);
		for (int i = 0; i < Mat3->ReferencedTextures.Num(); i++)
		{
			UTexture3* Tex = Mat3->ReferencedTextures[i];
			if (Tex)
			{
				appPrintf("Exporting raw texture [%d]: %s\n", i, Tex->Name);
				ToExport.AddUnique(Tex);
			}
		}
	}

#define PROC(Arg)	\
	if (Params.Arg) \
	{				\
		Ar->Printf(#Arg"=%s\n", Params.Arg->Name); \
		ToExport.AddUnique(Params.Arg); \
	}

	PROC(Diffuse);
	//PROC(DiffuseColor);
	PROC(Normal);
	PROC(Specular);
	PROC(SpecPower);
	PROC(Opacity);
	PROC(Emissive);
	PROC(Cube);
	PROC(Mask);

	//Testing these:
	PROC(Detail);
	PROC(AO);
	PROC(GlowMap);
	PROC(PaintMask);
	PROC(TeamColor);
	PROC(ColorLookup);
	PROC(DecalTexture);
	PROC(TilingPattern);
	PROC(HexMask);
	PROC(Environment);
	PROC(Reflection);
	PROC(Overlay);
	PROC(Noise);
	PROC(Roughness);
	PROC(Metallic);

	// Dump material properties to a separate file
	FArchive* PropAr = CreateExportArchive(Mat, EFileArchiveOptions::TextFile, "%s.props.txt", Mat->Name);
	if (PropAr)
	{
		Mat->GetTypeinfo()->SaveProps(Mat, *PropAr);


		delete PropAr;
	}

	TArray<TMapPair<const char*, const char*>> ExportTexMap;

	// HACK - First iterration, to gather all type-texture map. The type is assumed based on texture name, so there is no guarantee it is correct.
	int numOtherTextures = 0;

	printf("Processing textures start\n");
	for (int i = 0; i < AllTextures.Num(); i++)
	{
		UUnrealMaterial* Tex = AllTextures[i];

		// 1. Strict validation
		if (!Tex || !Tex->Name) continue;

		// 2. Immediate duplication to keep memory safe
		const char* TexPath = appStrdup(Tex->Name);

		FString TextureType = GetTextureType(Tex);
		const char* FinalParamName = NULL;

		if (TextureType.Len() > 0)
		{
			// 3. Duplicate IMMEDIATELY before FString can fluctuate
			FinalParamName = appStrdup(*TextureType);
		}
		else
		{
			// 4. Use a larger buffer and safe formatting
			char OtherBuf[128];
			appSprintf(ARRAY_ARG(OtherBuf), "Other[%d]", numOtherTextures++);
			FinalParamName = appStrdup(OtherBuf);
		}

		// 5. Final safety check before adding to the Map
		if (FinalParamName && TexPath)
		{
			ExportTexMap.Add({ FinalParamName, TexPath });
		}
	}

	printf("Processing textures done\n");

	if (Mat->IsA("Material3"))
	{
		// Second iteration: export named texture parameters from material expressions and overwrite the previous list.
		// There can be a mistake if few parameters are using the same texture path...
		// Second iteration: export named texture parameters
		const UMaterial3* Material = static_cast<const UMaterial3*>(Mat);
		for (int i = 0; i < Material->Expressions.Num(); i++)
		{
			printf("Processing expression %d\n", i);
			UObject* Expr = Material->Expressions[i];
			printf("Got expression pointer\n");
			if (Expr == nullptr) continue;

			// CRITICAL: Only process if it is actually a Texture Sample Parameter
			if (!Expr->IsA("MaterialExpressionTextureSampleParameter"))
			{
				continue;
			}
			printf("Before cast");
			auto* TexSampleParameter = static_cast<const UMaterialExpressionTextureSampleParameter*>(Expr);
			printf("After cast");

			if (!TexSampleParameter->Texture) continue;


			const char* paramName = nullptr;
			const char* texPath = nullptr;
			printf("Found texture sample parameter expression\n");
			auto ExpTex = TexSampleParameter->Texture;
			if (ExpTex == nullptr)
				continue;
			if (TexSampleParameter->Texture == nullptr)
				continue;

			paramName = TexSampleParameter->ParameterName ? TexSampleParameter->ParameterName : "";
			printf("  Parameter name: %s\n", paramName);

			texPath = nullptr;
			if (ExpTex && IsValidCString(ExpTex->Name))
			{
				printf("  Texture path: \n", ExpTex->Name);
				texPath = appStrdup(ExpTex->Name);
			}
			else if (IsValidCString(paramName) && paramName != "None")
			{
				printf("  Texture is null, but parameter is: %s \n", paramName);
				texPath = "None";
			}
			else
			{
				printf("  Texture name is null\n");
				texPath = appStrdup("Unknown");
				continue;
			}


			printf("  Texture path: %s\n", texPath);
			bool KeyChanged = false;
			for (int x = 0; x < ExportTexMap.Num(); x++)
			{
				printf("  Map %d: %s=%s\n", x, ExportTexMap[x].Key, ExportTexMap[x].Value);
				if (ExportTexMap[x].Key == nullptr || ExportTexMap[x].Value == nullptr || paramName == nullptr || ExpTex->Name == nullptr)
				{
					continue;
				}
				//appPrintf("texPath ptr=%p ExportTexMap[%d].Value ptr=%p\n", (void*)texPath, x, (void*)ExportTexMap[x].Value);

				printf("  Comparing %s to %s\n", ExportTexMap[x].Value, texPath);
				if (!paramName || !texPath) continue;
				if (strlen(paramName) > 4096 || strlen(texPath) > 4096) continue;

				//printf("%d Comparing parameter %s=%s to %s=%s\n", ExportTexMap.Num(), ExportTexMap[x].Key, ExportTexMap[x].Value, paramName, texPath);

				if (strcmp(ExportTexMap[x].Value, texPath) == 0)
				{
					appPrintf("Changing texture parameter %s=%s to %s=%s\n", ExportTexMap[x].Key, ExportTexMap[x].Value, paramName, texPath);
					ExportTexMap[x].Key = paramName;
					KeyChanged = true;
					break;
				}
			}

			if (KeyChanged == false)
			{
				printf("Added %s=%s\n", paramName, texPath);
				ExportTexMap.Add({ appStrdup(paramName), appStrdup(texPath) });
			}

		}
	}

	printf("Exporting texture parameters to archive\n");
	for (int i = 0; i < ExportTexMap.Num(); i++)
	{
		printf("Adding to texture to archive\n");
		const char* safeKey = ExportTexMap[i].Key != nullptr ? ExportTexMap[i].Key : "Null";
		const char* safeValue = ExportTexMap[i].Value != nullptr ? ExportTexMap[i].Value : "Null"; // value can be still null
		printf("Exporting texture parameter %s=%s\n", safeKey, safeValue);

		if (safeKey != "None")
			Ar->Printf("%s=%s\n", safeKey, safeValue);
	}

	printf("Adding all textures to ToExport list\n");
	for (int i = 0; i < AllTextures.Num(); i++)
	{
		UUnrealMaterial* ExpMat = AllTextures[i];
		if (ExpMat == NULL) continue;
		ToExport.AddUnique(ExpMat);
	}


	delete Ar; // close .mat file

	// We have done with current object, now let's export referenced objects.
	for (UObject* Obj : ToExport)
	{
		if (Obj != Mat) // UTextureCube::GetParams() adds self to "Cube" field
			ExportObject(Obj);
	}

	printf("Material export done: %s\n", Mat->Name);
	// For MaterialInstanceConstant, export its parent too
	if (Mat->IsA("MaterialInstanceConstant"))
	{
		printf("Exporting parent material\n");
		const UMaterialInstanceConstant* Inst = static_cast<const UMaterialInstanceConstant*>(Mat);
		printf("Parent material: %s\n", Inst->Parent ? Inst->Parent->Name : "NULL");
		if (Inst != nullptr && Inst->Parent)
		{
			//ExportMaterial(Inst->Parent);
			//ToExport.AddUnique(Inst->Parent);
		}
		printf("Parent material export done\n");
	}


#endif // RENDERING

	unguardf("%s'%s'", Mat->GetClassName(), Mat->Name);
}



