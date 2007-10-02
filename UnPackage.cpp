#include "Core.h"

#include "UnCore.h"
#include "UnPackage.h"


UnPackage::UnPackage(const char *filename)
:	FFileReader(filename)
{
	appStrncpyz(SelfName, filename, ARRAY_COUNT(SelfName));

	// read summary
	*this << Summary;
	if (Summary.Tag != PACKAGE_FILE_TAG)
		appError("Wrong tag in package %s\n", SelfName);
	ArVer = Summary.FileVersion;
	PKG_LOG(("Package: %s Ver: %d Names: %d Exports: %d Imports: %d\n", SelfName, Summary.FileVersion,
		Summary.NameCount, Summary.ExportCount, Summary.ImportCount));

	// read name table
	if (Summary.NameCount > 0)
	{
		Seek(Summary.NameOffset);
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
					*this << c;
					buf[len] = c;
					if (!c) break;
				}
				assert(len < ARRAY_COUNT(buf));
				NameTable[i] = strdup(buf);
				// skip object flags
				int tmp;
				*this << tmp;
			}
			else
			{
				int len;
				*this << AR_INDEX(len);
				char *s = NameTable[i] = new char[len];
				while (len-- > 0)
					*this << *s++;
//				PKG_LOG(("Name[%d]: \"%s\"\n", i, NameTable[i]));
				// skip object flags
				int tmp;
				*this << tmp;
			}
		}
	}

	// load import table
	if (Summary.ImportCount > 0)
	{
		Seek(Summary.ImportOffset);
		FObjectImport *Imp = ImportTable = new FObjectImport[Summary.ImportCount];
		for (int i = 0; i < Summary.ImportCount; i++, Imp++)
		{
			*this << *Imp;
//			PKG_LOG(("Import[%d]: %s'%s'\n", i, *Imp->ClassName, *Imp->ObjectName));
		}
	}

	// load exports table
	if (Summary.ExportCount > 0)
	{
		Seek(Summary.ExportOffset);
		FObjectExport *Exp = ExportTable = new FObjectExport[Summary.ExportCount];
		for (int i = 0; i < Summary.ExportCount; i++, Exp++)
		{
			*this << *Exp;
//			PKG_LOG(("Export[%d]: %s'%s' offs=%08X size=%08X\n", i, GetObjectName(Exp->ClassIndex),
//				*Exp->ObjectName, Exp->SerialOffset, Exp->SerialSize));
		}
	}
}
