#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnThirdParty.h"

#include "Exporters.h"


#if UNREAL3

void ExportGfx(const USwfMovie *Swf, FArchive &Ar)
{
	Ar.Serialize((void*)&Swf->RawData[0], Swf->RawData.Num());
}

void ExportFaceFXAnimSet(const UFaceFXAnimSet *Fx, FArchive &Ar)
{
	Ar.Serialize((void*)&Fx->RawFaceFXAnimSetBytes[0], Fx->RawFaceFXAnimSetBytes.Num());
}

void ExportFaceFXAsset(const UFaceFXAsset *Fx, FArchive &Ar)
{
	Ar.Serialize((void*)&Fx->RawFaceFXActorBytes[0], Fx->RawFaceFXActorBytes.Num());
}

#endif // UNREAL3
