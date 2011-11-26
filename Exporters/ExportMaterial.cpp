#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMaterial.h"

#include "Exporters.h"


void ExportMaterial(const UUnrealMaterial *Mat)
{
	guard(ExportMaterial);

#if RENDERING				// requires UUnrealMaterial::GetParams()

	if (!Mat) return;

	CMaterialParams Params;
	Mat->GetParams(Params);
	if (Params.IsNull() || Params.Diffuse == Mat) return;	// empty/unknown material, or material itself is a texture

	FArchive *Ar = CreateExportArchive(Mat, "%s.mat", Mat->Name);
	if (!Ar) return;

#define PROC(Arg)	\
	if (Params.Arg) \
	{				\
		Ar->Printf(#Arg"=%s\n", Params.Arg->Name); \
		ExportObject(Params.Arg); \
	}

	PROC(Diffuse);
	PROC(Normal);
	PROC(Specular);
	PROC(SpecPower);
	PROC(Opacity);
	PROC(Emissive);

#endif // RENDERING

	delete Ar;

	unguardf(("%s'%s'", Mat->GetClassName(), Mat->Name));
}
