#include "Core.h"

#if RENDERING

#include "CoreGL.h"
#include "GLText.h"
#include "TextContainer.h"

#include "GlWindow.h" // for Viewport

// font
#include "GlFont.h"

// Font configuration
#define CHARS_PER_LINE			(TEX_WIDTH/CHAR_WIDTH)
#define FONT_SPACING			1
#define TEXT_SCROLL_LINES		((CHAR_HEIGHT-FONT_SPACING)/2)
//#define SHOW_FONT_TEX			1		// show font texture

bool GShowDebugInfo = true;

static GLuint FontTexNum = 0;

static void LoadFont()
{
	// decompress font texture
	byte *pic = (byte*)appMalloc(TEX_WIDTH * TEX_HEIGHT * 4);
	const byte *p;
	byte *dst;

	// unpack 4 bit-per-pixel data with RLE encoding of null bytes
	for (p = TEX_DATA, dst = pic; p < TEX_DATA + ARRAY_COUNT(TEX_DATA); /*empty*/)
	{
		byte s = *p++;
		if (s & 0x80)
		{
			// unpack data
			// using *17 here: 0*17=>0, 15*17=>255
			for (int count = (s & 0x7F) + 1; count > 0; count--)
			{
				s = *p++;
				dst[0] = dst[1] = dst[2] = 255; dst += 3;
				*dst++ = (s >> 4) * 17;
				dst[0] = dst[1] = dst[2] = 255; dst += 3;
				*dst++ = (s & 0xF) * 17;
			}
		}
		else
		{
			// zero bytes
			for (int count = (s + 2) * 2; count > 0; count--)
			{
				dst[0] = dst[1] = dst[2] = 255; dst += 3;
				*dst++ = 0;
			}
		}
	}
//	printf("p[%d], dst[%d] -> %g\n", p - TEX_DATA, dst - pic, float(dst - pic) / 4 / TEX_WIDTH);

	// upload it
	glGenTextures(1, &FontTexNum);
	glBindTexture(GL_TEXTURE_2D, FontTexNum);
	// the best whould be to use 8-bit format with A=(var) and RGB=FFFFFF, but GL_ALPHA has RGB=0;
	// format with RGB=0 is not suitable for font shadow rendering because we must use GL_SRC_COLOR
	// blending; that's why we're using GL_RGBA here
	glTexImage2D(GL_TEXTURE_2D, 0, 4, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	appFree(pic);
}

void PrepareFontTexture()
{
	if (!glIsTexture(FontTexNum))
	{
		// possibly context was recreated ...
		InvalidateContext();
		LoadFont();
	}
}

static void DrawChar(char c, unsigned color, int textX, int textY)
{
	if (textX <= -CHAR_WIDTH || textY <= -CHAR_HEIGHT ||
		textX > Viewport::Size.X || textY > Viewport::Size.Y)
		return;				// outside of screen

	glBegin(GL_QUADS);

	c -= FONT_FIRST_CHAR;

	// screen coordinates
	int x1 = textX;
	int y1 = textY;
	int x2 = textX + CHAR_WIDTH - FONT_SPACING;
	int y2 = textY + CHAR_HEIGHT - FONT_SPACING;

	// texture coordinates
	int line = c / CHARS_PER_LINE;
	int col  = c - line * CHARS_PER_LINE;

	float s0 = col * CHAR_WIDTH;
	float s1 = s0 + CHAR_WIDTH - FONT_SPACING;
	float t0 = line * CHAR_HEIGHT;
	float t1 = t0 + CHAR_HEIGHT - FONT_SPACING;

	s0 /= TEX_WIDTH;
	s1 /= TEX_WIDTH;
	t0 /= TEX_HEIGHT;
	t1 /= TEX_HEIGHT;

	unsigned color2 = color & 0xFF000000;	// RGB=0, keep alpha
	for (int s = 1; s >= 0; s--)
	{
		// s=1 -> shadow, s=0 -> char
		glColor4ubv((GLubyte*)&color2);
		glTexCoord2f(s1, t0);
		glVertex2f(x2+s, y1+s);
		glTexCoord2f(s0, t0);
		glVertex2f(x1+s, y1+s);
		glTexCoord2f(s0, t1);
		glVertex2f(x1+s, y2+s);
		glTexCoord2f(s1, t1);
		glVertex2f(x2+s, y2+s);
		color2 = color;
	}

	glEnd();
}


//-----------------------------------------------------------------------------
// Text buffer
//-----------------------------------------------------------------------------

#define TOP_TEXT_POS	CHAR_HEIGHT
#define BOTTOM_TEXT_POS	CHAR_HEIGHT
#define LEFT_BORDER		CHAR_WIDTH
#define RIGHT_BORDER	CHAR_WIDTH


struct CRText : public CTextRec
{
	short			x, y;
	ETextAnchor		anchor;
	bool			bHasHyperlink;		// wether hyperlink tags should be stripped or not
	bool			bHighlightLink;		// wether hyperlink should be highlighted or not
	unsigned		color;
};

static TTextContainer<CRText, 65536> GTextContainer;

static int nextText_y[int(ETextAnchor::Last)];
static int textOffset = 0;

#define I 255
#define o 51
static const unsigned colorTable[8] =
{
	RGB255(0, 0, 0),
	RGB255(I, o, o),
	RGB255(o, I, o),
	RGB255(I, I, o),
	RGB255(o, o, I),
	RGB255(I, o, I),
	RGB255(o, I, I),
	RGB255(I, I, I)
};

#define WHITE_COLOR		RGB(255,255,255)

#undef I
#undef o


void ClearTexts()
{
	nextText_y[int(ETextAnchor::TopLeft)] = nextText_y[int(ETextAnchor::TopRight)] = TOP_TEXT_POS + textOffset;
	nextText_y[int(ETextAnchor::BottomLeft)] = nextText_y[int(ETextAnchor::BottomRight)] = 0;
	GTextContainer.Clear();
}


static void GetTextExtents(const char* s, int &width, int &height, bool bHyperlink)
{
	int x = 0, w = 0;
	int h = CHAR_HEIGHT - FONT_SPACING;
	while (char c = *s++)
	{
		if (c == COLOR_ESCAPE)
		{
			if (*s)
				s++;
			continue;
		}
		if (bHyperlink)
		{
			if (c == S_HYPER_START || c == S_HYPER_END)
				continue;
		}
		if (c == '\n')
		{
			if (x > w) w = x;
			x = 0;
			h += CHAR_HEIGHT - FONT_SPACING;
			continue;
		}
		x += CHAR_WIDTH - FONT_SPACING;
	}
	width = max(x, w);
	height = h;
}


static void DrawText(const CRText *rec)
{
	int y = rec->y;
	const char *text = rec->text;

	if (rec->anchor == ETextAnchor::BottomLeft || rec->anchor == ETextAnchor::BottomRight)
	{
		y = y + Viewport::Size.Y - nextText_y[int(rec->anchor)] - BOTTOM_TEXT_POS;
	}

	unsigned color = rec->color;
	unsigned color2 = color;

	while (true)
	{
		const char* s = strchr(text, '\n');
		int len = s ? s - text : strlen(text);

		int x = rec->x;
		for (int i = 0; i < len; i++)
		{
			char c = text[i];

			// Test special characters
			if (c == COLOR_ESCAPE)
			{
				char c2 = text[i+1];
				if (c2 >= '0' && c2 <= '7')
				{
					color = color2 = colorTable[c2 - '0'];
					i++;
					continue;
				}
			}
			else if (rec->bHasHyperlink)
			{
				if (c == S_HYPER_START)
				{
					if (rec->bHighlightLink)
						color = RGB255(70, 180, 255);
					continue;
				}
				else if (c == S_HYPER_END)
				{
					color = color2;
					continue;
				}
			}

			DrawChar(c, color, x, y);
			x += CHAR_WIDTH - FONT_SPACING;
		}
		if (!s) return;							// all done

		y += CHAR_HEIGHT - FONT_SPACING;
		text = s + 1;
	}
}

#if DUMP_TEXTS
bool GDumpTexts = false;
#endif

void FlushTexts()
{
	if (GUseGLSL) glUseProgram(0);				//?? default shader will not allow alpha on text
	// setup GL
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_2D, FontTexNum);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if 0
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5);
#else
	glDisable(GL_ALPHA_TEST);
#endif
#if SHOW_FONT_TEX
	glColor3f(1, 1, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(1, 0);
	glVertex2f(winWidth, 0);
	glTexCoord2f(0, 0);
	glVertex2f(winWidth-TEX_WIDTH, 0);
	glTexCoord2f(0, 1);
	glVertex2f(winWidth-TEX_WIDTH, TEX_HEIGHT);
	glTexCoord2f(1, 1);
	glVertex2f(winWidth, TEX_HEIGHT);
	glEnd();
#endif // SHOW_FONT_TEX

#if DUMP_TEXTS
	appSetNotifyHeader("Screen texts");
#endif
	GTextContainer.Enumerate(DrawText);
	ClearTexts();
#if DUMP_TEXTS
	appSetNotifyHeader(NULL);
	GDumpTexts = false;
#endif
}


void DrawTextPos(int x, int y, const char* text, unsigned color, bool bHyperlink, bool bHighlightLink, ETextAnchor anchor)
{
	if (!GShowDebugInfo) return;

	CRText *rec = GTextContainer.Add(text);
	if (!rec) return;
	rec->x      = x;
	rec->y      = y;
	rec->anchor = anchor;
	rec->bHasHyperlink = bHyperlink;
	rec->bHighlightLink = bHighlightLink;
	rec->color  = color;
}


static bool DrawTextAtAnchor(ETextAnchor anchor, unsigned color, bool bHyperlink, bool* pHover, const char* fmt, va_list argptr)
{
	guard(DrawTextAtAnchor);

	assert(anchor < ETextAnchor::Last);

	bool bClicked = false;

	bool isBottom = (anchor >= ETextAnchor::BottomLeft);
	bool isLeft   = (anchor == ETextAnchor::TopLeft || anchor == ETextAnchor::BottomLeft);

	int pos_y = nextText_y[int(anchor)];

#if DUMP_TEXTS
	if (GDumpTexts) pos_y = Viewport::Size.Y / 2;		// trick to avoid text culling
#endif

	if (!isBottom && pos_y >= Viewport::Size.Y && !GDumpTexts)	// out of screen
		return bClicked;

	char msg[4096];
	vsnprintf(ARRAY_ARG(msg), fmt, argptr);
	int w, h;
	GetTextExtents(msg, w, h, bHyperlink);

	nextText_y[int(anchor)] = pos_y + h;

	if (!isBottom && pos_y + h <= 0 && !GDumpTexts)		// out of screen
		return bClicked;

	// Determine X position depending on anchor
	int pos_x = isLeft ? LEFT_BORDER : Viewport::Size.X - RIGHT_BORDER - w;

	// Check if mouse points at hyperlink
	bool bHighlightLink = false;

	if (bHyperlink)
	{
		// Do the rough estimation of having mouse in the whole text's bounds
		if (pos_y <= Viewport::MousePos.Y && Viewport::MousePos.Y < pos_y + h &&
			pos_x <= Viewport::MousePos.X && Viewport::MousePos.X <= pos_x + w)
		{
			// Check if we'll get into exact hyperlink bounds, verify only X coordinate now
			int offset = 0;
			int linkStart = -1;
			const char* s = msg;
			while (char c = *s++)
			{
				if (c == '\n' || c == S_HYPER_END)
					break;
				if (c == COLOR_ESCAPE)
					continue;
				if (c == S_HYPER_START)
				{
					linkStart = offset;
					continue;
				}
				// Count a character as printable
				offset++;
			}
			if (linkStart > 0 &&
				pos_x + (CHAR_WIDTH - FONT_SPACING) * linkStart <= Viewport::MousePos.X &&
				Viewport::MousePos.X < pos_x + (CHAR_WIDTH - FONT_SPACING) * offset)
			{
				bHighlightLink = true;
				// Check if hyperlink has been clicked this frame
				// We're catching "click" event. Can't do that with "release button" because
				// mouse capture (SDL_SetRelativeMouseMode) generates extra events on Windows,
				// so mouse position will jump between actual one and window center.
				if ((Viewport::MouseButtonsDelta & 1) && (Viewport::MouseButtons & 1))
				{
					bClicked = true;
				}
			}
		}

		if (pHover) *pHover = bHighlightLink;
	}

	// Put the text into queue
	DrawTextPos(pos_x, pos_y, msg, color, bHyperlink, bHighlightLink, anchor);

#if DUMP_TEXTS
	if (GDumpTexts)
	{
		// drop color escape characters
		char *d = msg;
		char *s = msg;
		while (char c = *s++)
		{
			if (c == COLOR_ESCAPE && *s)
			{
				s++;
				continue;
			}
			*d++ = c;
		}
		*d = 0;
		appNotify("%s", msg);
	}
#endif

	return bClicked;

	unguard;
}


#define DRAW_TEXT(anchor,color,hyperlink,pHover,fmt) \
	va_list	argptr;				\
	va_start(argptr, fmt);		\
	bool bClicked = DrawTextAtAnchor(anchor, color, hyperlink, pHover, fmt, argptr); \
	va_end(argptr);


void DrawTextLeft(const char* text, ...)
{
	DRAW_TEXT(ETextAnchor::TopLeft, WHITE_COLOR, false, NULL, text);
}

void DrawTextRight(const char* text, ...)
{
	DRAW_TEXT(ETextAnchor::TopRight, WHITE_COLOR, false, NULL, text);
}

void DrawTextBottomLeft(const char* text, ...)
{
	DRAW_TEXT(ETextAnchor::BottomLeft, WHITE_COLOR, false, NULL, text);
}

void DrawTextBottomRight(const char* text, ...)
{
	DRAW_TEXT(ETextAnchor::BottomRight, WHITE_COLOR, false, NULL, text);
}

void DrawText(ETextAnchor anchor, const char* text, ...)
{
	DRAW_TEXT(anchor, WHITE_COLOR, false, NULL, text);
}

void DrawText(ETextAnchor anchor, unsigned color, const char* text, ...)
{
	DRAW_TEXT(anchor, color, false, NULL, text);
}


bool DrawTextLeftH(bool* isHover, const char* text, ...)
{
	DRAW_TEXT(ETextAnchor::TopLeft, WHITE_COLOR, true, isHover, text);
	return bClicked;
}

bool DrawTextRightH(bool* isHover, const char* text, ...)
{
	DRAW_TEXT(ETextAnchor::TopRight, WHITE_COLOR, true, isHover, text);
	return bClicked;
}

bool DrawTextBottomLeftH(bool* isHover, const char* text, ...)
{
	DRAW_TEXT(ETextAnchor::BottomLeft, WHITE_COLOR, true, isHover, text);
	return bClicked;
}

bool DrawTextBottomRightH(bool* isHover, const char* text, ...)
{
	DRAW_TEXT(ETextAnchor::BottomRight, WHITE_COLOR, true, isHover, text);
	return bClicked;
}

bool DrawTextH(ETextAnchor anchor, bool* isHover, const char* text, ...)
{
	DRAW_TEXT(anchor, WHITE_COLOR, true, isHover, text);
	return bClicked;
}

bool DrawTextH(ETextAnchor anchor, bool* isHover, unsigned color, const char* text, ...)
{
	DRAW_TEXT(anchor, color, true, isHover, text);
	return bClicked;
}

void ScrollText(int Amount)
{
	if (Amount == 0)
	{
		textOffset = 0;
	}
	else
	{
		textOffset += Amount * TEXT_SCROLL_LINES;
		if (textOffset > 0)
			textOffset = 0; // clamp to top
	}
}

#endif // RENDERING
