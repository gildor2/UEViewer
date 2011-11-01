#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnPackage.h"		// for Package->Name

#include "Exporters.h"


static char BaseExportDir[512];

bool GUncook    = false;
bool GUseGroups = false;

void appSetBaseExportDirectory(const char *Dir)
{
	strcpy(BaseExportDir, Dir);
}


const char* GetExportPath(const UObject *Obj)
{
	if (!BaseExportDir[0])
		appSetBaseExportDirectory(".");	// to simplify code

	const char *PackageName = (GUncook) ? Obj->GetUncookedPackageName() : Obj->Package->Name;

	static char group[512];
	if (GUseGroups)
	{
		// get group name
		// include cooked package name when not uncooking
		Obj->GetFullName(ARRAY_ARG(group), false, !GUncook);
		// replace all '.' with '/'
		for (char *s = group; *s; s++)
			if (*s == '.') *s = '/';
	}
	else
	{
		strcpy(group, Obj->GetClassName());
	}

	static char buf[1024];
	appSprintf(ARRAY_ARG(buf), "%s/%s%s%s", BaseExportDir, PackageName,
		(group[0]) ? "/" : "", group);
	appMakeDirectory(buf);
	return buf;
}
