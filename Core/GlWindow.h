#ifndef __GL_WINDOW_H__
#define __GL_WINDOW_H__


#include "Core.h"

#if _WIN32
#include "Win32Types.h"
#endif

/*-----------------------------------------------------------------------------
	Application class
-----------------------------------------------------------------------------*/

//!! - move most 'static' code to this class
//!! - rename to CWindow (but this class has VisualizerLoop - this is global function)
class CApplication
{
public:
	CApplication();
	virtual ~CApplication();

	// Main application function
	void VisualizerLoop(const char *caption);

	struct SDL_Window* GetWindow() const;
	void ResizeWindow();
	void GetWindowSize(int &x, int &y);
	void ToggleFullscreen();
	void Exit();

	virtual void WindowCreated()
	{}
	virtual void Draw3D(float TimeDelta)
	{}
	virtual void DrawTexts();
	virtual void BeforeSwap()
	{}
	virtual void ProcessKey(int key, bool isDown);
#if _WIN32
	// Win32 message hook
	virtual void WndProc(UINT msg, WPARAM wParam, LPARAM lParam)
	{}
#endif

protected:
	static int OnEvent(void *userdata, SDL_Event *evt);
	void Display();
	void HandleKeyDown(unsigned key, unsigned mod);

	bool		RequestingQuit;
	bool		IsHelpVisible;
	bool		IsFullscreen;

	int			SavedWinWidth;			// saved window size before switching to fullscreen mode
	int			SavedWinHeight;
};

void MoveCamera(float YawDelta, float PitchDelta, float DistDelta = 0, float PanX = 0, float PanY = 0);
void FocusCameraOnPoint(const CVec3 &center);
void SetDistScale(float scale);
void SetViewOffset(const CVec3 &offset);
void ResetView();

// viewport params
extern bool  vpInvertXAxis;
extern CVec3 viewOrigin;
extern CAxis viewAxis;

extern bool GShowDebugInfo;


/*-----------------------------------------------------------------------------
	Text output
-----------------------------------------------------------------------------*/

#undef RGB					// defined in windows headers

// constant colors
#define RGBA(r,g,b,a)		( (unsigned)((r)*255) | ((unsigned)((g)*255)<<8) | ((unsigned)((b)*255)<<16) | ((unsigned)((a)*255)<<24) )
#define RGB(r,g,b)			RGBA(r,g,b,1)
#define RGB255(r,g,b)		( (unsigned) ((r) | ((g)<<8) | ((b)<<16) | (255<<24)) )
#define RGBA255(r,g,b,a)	( (r) | ((g)<<8) | ((b)<<16) | ((a)<<24) )

// computed colors
//?? make as methods; or - constructor of CColor
#define RGBAS(r,g,b,a)		(appRound((r)*255) | (appRound((g)*255)<<8) | (appRound((b)*255)<<16) | (appRound((a)*255)<<24))
#define RGBS(r,g,b)			(appRound((r)*255) | (appRound((g)*255)<<8) | (appRound((b)*255)<<16) | (255<<24))


void DrawTextLeft(const char *text, ...);
void DrawTextRight(const char *text, ...);
void DrawTextBottomLeft(const char *text, ...);
void DrawTextBottomRight(const char *text, ...);
void DrawText3D(const CVec3 &pos, unsigned color, const char *text, ...);

enum ETextAnchor
{
	TA_TopLeft,			// DrawTextLeft
	TA_TopRight,		// DrawTextRight
	TA_BottomLeft,
	TA_BottomRight,

	// internally used values
	TA_None,
	TA_Last
};

void DrawText(ETextAnchor anchor, const char *text, ...);
void DrawText(ETextAnchor anchor, unsigned color, const char *text, ...);

void FlushTexts();

// called from AppDisplayTexts() callback
#define KEY_HELP_TAB	11
void DrawKeyHelp(const char *Key, const char *Help);


/*-----------------------------------------------------------------------------
	Keyboard
-----------------------------------------------------------------------------*/

#define SPEC_KEY(x)		(SDLK_##x)
#define KEY_CTRL		0x80000000
#define KEY_SHIFT		0x40000000
#define KEY_ALT			0x20000000


#endif // __GL_WINDOW_H__
