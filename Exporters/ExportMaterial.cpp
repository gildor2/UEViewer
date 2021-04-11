#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMaterial/UnMaterial3.h"

#include "Exporters.h"


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
	//todo: remove separate texture handling from Main.cpp exporter registraction

	TArray<UUnrealMaterial*> AllTextures;
	Mat->AppendReferencedTextures(AllTextures, false);

	CMaterialParams Params;
	Mat->GetParams(Params);
	if ((Params.IsNull() || Params.Diffuse == Mat) && AllTextures.Num() == 0)
	{
		// empty/unknown material, or material itself is a texture
		appPrintf("Ignoring %s'%s' due to empty parameters\n", Mat->GetClassName(), Mat->Name);
		return;
	}

	FArchive* Ar = CreateExportArchive(Mat, EFileArchiveOptions::TextFile, "%s.mat", Mat->Name);
	if (!Ar) return;

	TArray<UObject*> ToExport;

#define PROC(Arg)	\
	if (Params.Arg) \
	{				\
		Ar->Printf(#Arg"=%s\n", Params.Arg->Name); \
		ToExport.AddUnique(Params.Arg); \
	}

	PROC(Diffuse);
	PROC(Normal);
	PROC(Specular);
	PROC(SpecPower);
	PROC(Opacity);
	PROC(Emissive);
	PROC(Cube);
	PROC(Mask);

	// Dump material properties to a separate file
	FArchive* PropAr = CreateExportArchive(Mat, EFileArchiveOptions::TextFile, "%s.props.txt", Mat->Name);
	if (PropAr)
	{
		Mat->GetTypeinfo()->SaveProps(Mat, *PropAr);
		delete PropAr;
	}

	// Export other textures
	int numOtherTextures = 0;
	for (int i = 0; i < AllTextures.Num(); i++)
	{
		UUnrealMaterial* Tex = AllTextures[i];
		if (ToExport.FindItem(Tex) < 0)
		{
			Ar->Printf("Other[%d]=%s\n", numOtherTextures++, Tex->Name);
			ToExport.Add(Tex);
		}
	}

	delete Ar; // close .mat file

	// We have done with current object, now let's export referenced objects.

	for (UObject* Obj : ToExport)
	{
		if (Obj != Mat) // UTextureCube::GetParams() adds self to "Cube" field
			ExportObject(Obj);
	}

	// For MaterialInstanceConstant, export its parent too
	if (Mat->IsA("MaterialInstanceConstant"))
	{
		const UMaterialInstanceConstant* Inst = static_cast<const UMaterialInstanceConstant*>(Mat);
		if (Inst->Parent)
		{
			ExportMaterial(Inst->Parent);
		}
	}

#endif // RENDERING

	unguardf("%s'%s'", Mat->GetClassName(), Mat->Name);
}
