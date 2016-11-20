#ifndef __TEXT_CONTAINER_H__
#define __TEXT_CONTAINER_H__

struct CTextRec
{
	char	*text;
	CTextRec *next;
};


class CTextContainer
{
protected:
	// static values
	int		recSize;
	char	*textBuf;
	int		bufSize;
	// dynamic values
	bool	filled;			// !empty
	int		fillPos;
	CTextRec *lastRec;
public:
	inline void Clear()
	{
		filled = false;
	}
	CTextRec *Add(const char *text);
	void Enumerate(void (*func) (const CTextRec *rec));
};


// Type 'R' should be derived from CTextRec.
template<class R, int BufferSize>
class TTextContainer : public CTextContainer
{
public:
	inline TTextContainer()
	{
		textBuf = new char [BufferSize];
		bufSize = BufferSize;
		recSize = sizeof(R);
	}
	~TTextContainer()
	{
		delete[] textBuf;
	}
	inline R *Add(const char *text)
	{
		return static_cast<R*>(CTextContainer::Add(text));
	}
	inline void Enumerate(void (*func) (const R *rec))
	{
		CTextContainer::Enumerate((void(*)(const CTextRec*)) func);
	}
};

#endif // __TEXT_CONTAINER_H__
