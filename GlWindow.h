#define FREEGLUT_STATIC
#include <freeglut.h>

#include "Core.h"


void VisualizerLoop(const char *caption);
void AppDrawFrame();
void AppKeyEvent(unsigned char key);
void AppDisplayTexts(bool helpVisible);


namespace GL
{
	void text(const char *text, int x = -1, int y = -1);
	void textf(const char *fmt, ...);
};
