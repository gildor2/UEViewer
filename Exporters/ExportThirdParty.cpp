#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnThirdParty.h"

#include "Exporters.h"


#if UNREAL3

void ExportGfx(const USwfMovie *Swf)
{
	FArchive *Ar = CreateExportArchive(Swf, EFileArchiveOptions::Default, "%s.gfx", Swf->Name);
	if (Ar)
	{
		Ar->Serialize((void*)&Swf->RawData[0], Swf->RawData.Num());
		delete Ar;
	}
}

void ExportFaceFXAnimSet(const UFaceFXAnimSet *Fx)
{
	FArchive *Ar = CreateExportArchive(Fx, EFileArchiveOptions::Default, "%s.fxa", Fx->Name);
	if (Ar)
	{
		Ar->Serialize((void*)&Fx->RawFaceFXAnimSetBytes[0], Fx->RawFaceFXAnimSetBytes.Num());
		delete Ar;
	}
}

void ExportFaceFXAsset(const UFaceFXAsset *Fx)
{
	FArchive *Ar = CreateExportArchive(Fx, EFileArchiveOptions::Default, "%s.fxa", Fx->Name);
	if (Ar)
	{
		Ar->Serialize((void*)&Fx->RawFaceFXActorBytes[0], Fx->RawFaceFXActorBytes.Num());
		delete Ar;
	}
}

#endif // UNREAL3
