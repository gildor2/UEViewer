#include "Core.h"

#include "UnCore.h"
#include "UnPackage.h"


UnPackage::UnPackage(const char *filename)
{
	appStrncpyz(SelfName, filename, ARRAY_COUNT(SelfName));

	FILE *f;
	if (!(f = fopen(SelfName, "rb")))
		appError("Unable to open package file %s\n", SelfName);
	Ar.Setup(f, true);

	// read summary
	Ar << Summary;
	if (Summary.Tag != PACKAGE_FILE_TAG)
		appError("Wrong tag in package %s\n", SelfName);
	Ar.ArVer = Summary.FileVersion;
	PKG_LOG(("Package: %s Ver: %d Names: %d Exports: %d Imports: %d\n", SelfName, Summary.FileVersion,
		Summary.NameCount, Summary.ExportCount, Summary.ImportCount));

	// read name table
	if (Summary.NameCount > 0)
	{
		Ar.Seek(Summary.NameOffset);
		NameTable = new char*[Summary.NameCount];
		for (int i = 0; i < Summary.NameCount; i++)
		{
			if (Summary.FileVersion < 64)
			{
				char buf[256];
				int len;
				for (len = 0; len < ARRAY_COUNT(buf); len++)
				{
					char c;
					Ar << c;
					buf[len] = c;
					if (!c) break;
				}
				assert(len < ARRAY_COUNT(buf));
				NameTable[i] = strdup(buf);
				// skip object flags
				int tmp;
				Ar << tmp;
			}
			else
			{
				int len;
				Ar << AR_INDEX(len);
				char *s = NameTable[i] = new char[len];
				while (len-- > 0)
					Ar << *s++;
//				PKG_LOG("Name[%d]: \"%s\"\n", i, NameTable[i]);
				// skip object flags
				int tmp;
				Ar << tmp;
			}
		}
	}

	// load import table
	if (Summary.ImportCount > 0)
	{
		Ar.Seek(Summary.ImportOffset);
		FObjectImport *Imp = ImportTable = new FObjectImport[Summary.ImportCount];
		for (int i = 0; i < Summary.ImportCount; i++, Imp++)
		{
			Ar << *Imp;
//			PKG_LOG(("Import[%d]: %s'%s'\n", i, GetName(Imp->ClassName), GetName(Imp->ObjectName)));
		}
	}

	// load exports table
	if (Summary.ExportCount > 0)
	{
		Ar.Seek(Summary.ExportOffset);
		FObjectExport *Exp = ExportTable = new FObjectExport[Summary.ExportCount];
		for (int i = 0; i < Summary.ExportCount; i++, Exp++)
		{
			Ar << *Exp;
//			PKG_LOG(("Export[%d]: %s'%s' offs=%08X size=%08X\n", i, GetClassName(Exp->ClassIndex),
//				GetName(Exp->ObjectName), Exp->SerialOffset, Exp->SerialSize));
		}
	}

	fclose(f);
}
