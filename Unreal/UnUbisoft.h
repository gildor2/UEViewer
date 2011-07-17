#ifndef __UNUBISOFT_H__
#define __UNUBISOFT_H__


#if LEAD


class FLeadUmdFile;


FArchive* CreateUMDReader(FArchive *File);
bool ExtractUMDArchive(FArchive *UmdFile, const char *OutDir);


#endif // LEAD

#endif // __UNUBISOFT_H__
