#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"

#include "Exporters.h"						// for GetExportPath()


void ExportMaterial(const UUnrealMaterial *Mat, FArchive &DummyAr)
{
	guard(ExportMaterial);

#if RENDERING				// requires UUnrealMaterial::GetParams()

	if (!Mat) return;

	CMaterialParams Params;
	Mat->GetParams(Params);
	if (Params.IsNull() || Params.Diffuse == Mat) return;	// empty/unknown material, or material itself is a texture

	char filename[512];
	appSprintf(ARRAY_ARG(filename), "%s/%s.mat", GetExportPath(Mat), Mat->Name);
	FFileReader Ar(filename, false);

#define PROC(Arg)	\
	if (Params.Arg) \
	{				\
		Ar.Printf(#Arg"=%s\n", Params.Arg->Name); \
		ExportObject(Params.Arg); \
	}

	PROC(Diffuse);
	PROC(Normal);
	PROC(Specular);
	PROC(SpecPower);
	PROC(Opacity);
	PROC(Emissive);

#endif // RENDERING

	unguardf(("%s'%s'", Mat->GetClassName(), Mat->Name));
}
