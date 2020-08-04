#ifndef __GL_TEXT_H__
#define __GL_TEXT_H__

#undef None // defined in X11/Xlib.h, included from Linux SDL headers

/*-----------------------------------------------------------------------------
	Text output
-----------------------------------------------------------------------------*/

#define DUMP_TEXTS 1		// allow Ctrl+D to dump all onscreen texts to a log

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

// Color codes for DrawText... functions
#define COLOR_ESCAPE	'^'		// could be used for quick location of color-processing code

#define S_BLACK			"^0"
#define S_RED			"^1"
#define S_GREEN			"^2"
#define S_YELLOW		"^3"
#define S_BLUE			"^4"
#define S_MAGENTA		"^5"
#define S_CYAN			"^6"
#define S_WHITE			"^7"

// Hyperlink support
#define S_HYPER_START	'{'
#define S_HYPER_END		'}'
#define S_HYPERLINK(text)	"{" text "}"

// Draw "stacked" text bound to the particular window corner
void DrawTextLeft(const char* text, ...);
void DrawTextRight(const char* text, ...);
void DrawTextBottomLeft(const char* text, ...);
void DrawTextBottomRight(const char* text, ...);

// Hyperlink support. Functions returns 'true' if link is clicked, and 'isHover' points
// to boolean value which will receive 'true' if mouse points at the link.
bool DrawTextLeftH(bool* isHover, const char* text, ...);
bool DrawTextRightH(bool* isHover, const char* text, ...);
bool DrawTextBottomLeftH(bool* isHover, const char* text, ...);
bool DrawTextBottomRightH(bool* isHover, const char* text, ...);

enum class ETextAnchor
{
	TopLeft,		// DrawTextLeft
	TopRight,		// DrawTextRight
	BottomLeft,		// DrawTextBottomLeft
	BottomRight,	// DrawTextBottomRight

	// internally used values
	None,
	Last
};

void DrawText(ETextAnchor anchor, const char* text, ...);
void DrawText(ETextAnchor anchor, unsigned color, const char* text, ...);

bool DrawTextH(ETextAnchor anchor, bool* isHover, const char* text, ...);
bool DrawTextH(ETextAnchor anchor, bool* isHover, unsigned color, const char* text, ...);

void PrepareFontTexture();
void ClearTexts();
void FlushTexts();

// Internal function for drawing text at arbitrary 2D position
void DrawTextPos(int x, int y, const char* text, unsigned color, bool bHyperlink = false, bool bHighlightLink = false, ETextAnchor anchor = ETextAnchor::None);


// Allows to disable rendering of any texts on screen
extern bool GShowDebugInfo;

// Scroll text by half font height units. When Amount is 0, scroll will be reset.
void ScrollText(int Amount);

#if DUMP_TEXTS
extern bool GDumpTexts;
#endif

#endif // __GL_TEXT_H__
