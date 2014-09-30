#ifndef __GLWINDOW_H__
#define __GLWINDOW_H__


#include "Core.h"

#if _WIN32
#include "Win32Types.h"
#endif

struct SDL_Window;

/*-----------------------------------------------------------------------------
	Control functions
-----------------------------------------------------------------------------*/

//!! move most 'static' code to this class
class CApplication
{
public:
	// Main application function
	void VisualizerLoop(const char *caption);

	SDL_Window* GetWindow() const;
	void ResizeWindow();
	void Exit();

	virtual void WindowCreated()
	{}
	virtual void Draw3D(float TimeDelta)
	{}
	virtual void DrawTexts(bool helpVisible);
	virtual void BeforeSwap()
	{}
	virtual void ProcessKey(int key, bool isDown)
	{}
#if _WIN32
	// Win32 message hook
	virtual void WndProc(UINT msg, WPARAM wParam, LPARAM lParam)
	{}
#endif
};

void MoveCamera(float YawDelta, float PitchDelta, float DistDelta = 0, float PanX = 0, float PanY = 0);
void FocusCameraOnPoint(const CVec3 &center);
void SetDistScale(float scale);
void SetViewOffset(const CVec3 &offset);
void ResetView();

void GetWindowSize(int &x, int &y);

// viewport params
extern bool  vpInvertXAxis;
extern CAxis viewAxis;

extern bool GShowDebugInfo;


/*-----------------------------------------------------------------------------
	Text output
-----------------------------------------------------------------------------*/

#undef RGB					// defined in windows

// constant colors
#define RGBA(r,g,b,a)		((int)((r)*255) | ((int)((g)*255)<<8) | ((int)((b)*255)<<16) | ((int)((a)*255)<<24))
#define RGB(r,g,b)			RGBA(r,g,b,1)
#define RGB255(r,g,b)		((r) | ((g)<<8) | ((b)<<16) | (255<<24))
#define RGBA255(r,g,b,a)	((r) | ((g)<<8) | ((b)<<16) | ((a)<<24))

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


#endif // __GLWINDOW_H__
