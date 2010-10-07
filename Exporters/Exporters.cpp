#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnPackage.h"		// for Package->Name

#include "Exporters.h"


static char BaseExportDir[512];

void appSetBaseExportDirectory(const char *Dir)
{
	strcpy(BaseExportDir, Dir);
}


const char* GetExportPath(const UObject *Obj)
{
	if (!BaseExportDir[0])
		appSetBaseExportDirectory(".");	// to simplify code
	static char buf[512];
	appSprintf(ARRAY_ARG(buf), "%s/%s/%s", BaseExportDir, Obj->Package->Name, Obj->GetClassName());
	appMakeDirectory(buf);
	return buf;
}
