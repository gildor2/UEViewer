#include "Core.h"
#include "TextContainer.h"


CTextRec *CTextContainer::Add(const char *text)
{
	if (!text || !*text) return NULL;				// empty text

	if (!filled)
	{	// 1st record - perform initialization
		lastRec = NULL;
		fillPos = 0;
	}

	CTextRec *rec = (CTextRec*) &textBuf[fillPos];
	int len = strlen(text);
	if (!len) return NULL;

	int size = recSize + len + 1;
	if (fillPos + size > bufSize) return NULL;		// out of buffer space

	memset(rec, 0, recSize);
	rec->text = (char*) OffsetPointer(rec, recSize);
	memcpy(rec->text, text, len+1);

	if (lastRec) lastRec->next = rec;
	lastRec = rec;
	fillPos += size;

	filled  = true;

	return rec;
}


void CTextContainer::Enumerate(void (*func) (const CTextRec *rec))
{
	guard(CTextContainer::Enumerate);
	if (!filled) return;
	for (CTextRec *rec = (CTextRec*) textBuf; rec; rec = rec->next)
		func(rec);
	unguard;
}
