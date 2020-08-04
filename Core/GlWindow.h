#ifndef __GL_WINDOW_H__
#define __GL_WINDOW_H__


#include "Core.h"
#include "GLText.h"


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
	virtual void ProcessKey(unsigned key, bool isDown);
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


// Draw text at 3D space, with arbitrary color
void DrawText3D(const CVec3 &pos, unsigned color, const char* text, ...);


// Display help about particular ket, should be called from AppDisplayTexts()
#define KEY_HELP_TAB	14
void DrawKeyHelp(const char *Key, const char *Help);

/*-----------------------------------------------------------------------------
	Viewport
-----------------------------------------------------------------------------*/

namespace Viewport
{

struct Point
{
	int X;
	int Y;
};

// Viewport size
extern Point Size;

extern Point MousePos;

extern int MouseButtons;

extern int MouseButtonsDelta;

} // namespace Viewport

/*-----------------------------------------------------------------------------
	Keyboard
-----------------------------------------------------------------------------*/

#define SPEC_KEY(x)		(SDLK_##x)
#define KEY_CTRL		0x80000000
#define KEY_SHIFT		0x40000000
#define KEY_ALT			0x20000000


#endif // __GL_WINDOW_H__
