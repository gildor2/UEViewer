#ifndef __GAME_LIST_H__
#define __GAME_LIST_H__

struct GameInfo
{
	const char *Name;
	const char *Switch;
	int			Enum;
};

extern const GameInfo GListOfGames[];


const char *GetEngineName(int Game);
void PrintGameList(bool tags = false);
int FindGameTag(const char *name);


class FVirtualFileSystem
{
public:
	virtual ~FVirtualFileSystem()
	{}

//	virtual int NumFiles() const = 0;
//	virtual const char* FileName(int i) = 0;
};


#endif // __GAME_LIST_H__
