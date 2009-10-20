#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"
#include "UnPackage.h"		// for Package->Name


void ExportMaterial(const UUnrealMaterial *Mat)
{
	if (!Mat) return;

	CMaterialParams Params;
	Mat->GetParams(Params);
	if (Params.IsNull()) return;

	char filename[256];
	appSprintf(ARRAY_ARG(filename), "%s/%s/%s.mat", Mat->Package->Name, Mat->GetClassName(), Mat->Name);
	appMakeDirectoryForFile(filename);
	FFileReader Ar(filename, false);

#define PROC(Arg)		if (Params.Arg) Ar.Printf(#Arg"=%s\n", Params.Arg->Name);
	PROC(Diffuse);
	PROC(Normal);
	PROC(Specular);
	PROC(SpecPower);
	PROC(Opacity);
	PROC(Emissive);
}
