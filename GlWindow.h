#ifndef __GLWINDOW_H__
#define __GLWINDOW_H__


#define FREEGLUT_STATIC
#include <freeglut.h>

#undef GetClassName		// windows define

#include "Core.h"


void VisualizerLoop(const char *caption);
void AppDrawFrame();
void AppKeyEvent(unsigned char key);
void AppDisplayTexts(bool helpVisible);


namespace GL
{
	void text(const char *text, int x = -1, int y = -1);
	void textf(const char *fmt, ...);
	void SetViewOffset(const CVec3 &offset);
};


#endif // __GLWINDOW_H__
