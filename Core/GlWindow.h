#ifndef __GLWINDOW_H__
#define __GLWINDOW_H__


#include "Core.h"

/*-----------------------------------------------------------------------------
	Control functions
-----------------------------------------------------------------------------*/

void VisualizerLoop(const char *caption);
void AppDrawFrame();
void AppKeyEvent(int key);
void AppDisplayTexts(bool helpVisible);


void MoveCamera(float YawDelta, float PitchDelta, float DistDelta = 0, float PanX = 0, float PanY = 0);
void SetDistScale(float scale);
void SetViewOffset(const CVec3 &offset);
void ResetView();

void GetWindowSize(int &x, int &y);

// viewport params
extern bool  vpInvertXAxis;
extern CAxis viewAxis;


/*-----------------------------------------------------------------------------
	Text output
-----------------------------------------------------------------------------*/

void DrawTextLeft(const char *text, ...);
void DrawTextRight(const char *text, ...);
void DrawText3D(const CVec3 &pos, const char *text, ...);
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
