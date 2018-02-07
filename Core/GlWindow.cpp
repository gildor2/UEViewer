#include <SDL2/SDL_syswm.h>
#undef DrawText

#include "Core.h"

#if RENDERING

#include "TextContainer.h"
#include "GlWindow.h"
#include "CoreGL.h"

// font
#include "GlFont.h"

#define CHARS_PER_LINE			(TEX_WIDTH/CHAR_WIDTH)
#define FONT_SPACING			1
#define TEXT_SCROLL_LINES		((CHAR_HEIGHT-FONT_SPACING)/2)
//#define SHOW_FONT_TEX			1		// show font texture


#define LIGHTING_MODES			1		// allow switching scene lighting modes with Ctrl+L
#define DUMP_TEXTS				1		// allow Ctrl+D to dump all onscreen texts to a log
#define FUNNY_BACKGROUND		1		// draw gradient background
#define SMART_RESIZE			1		// redraw window contents while resizing window
#define USE_BLOOM				1
//#define SHOW_FPS				1
#define LIMIT_FPS				1

#if !_WIN32
#undef SMART_RESIZE						// not compatible with Linux (has ugly effects and program hung)
#endif

// Win32+SDL bug: when window has lost the focus while some keys are pressed keys will never be released
// unless user will press and release these keys manually (frequently happens when switching to another
// application using Alt+Tab key - Alt key becomes "sticky")
// This bug appears in SDL 1.3 because it has no SDL_ResetKeyboard() call when application has lost the
// focus (SDL 1.2 has such call!)
#define FIX_STICKY_MOD_KEYS		1

#if _MSC_VER
#pragma comment(lib, "opengl32.lib")
#endif

//#if SDL_VERSION_ATLEAST(1,3,0)
//#define NEW_SDL					1
//#endif

#if MAX_DEBUG

static void CheckSDLError()
{
	const char *err = SDL_GetError();
	if (err && err[0])
	{
		appPrintf("SDL ERROR: %s\n", err);
		SDL_ClearError();
	}
}

#define SDL_CHECK_ERROR	CheckSDLError()

#else

#define SDL_CHECK_ERROR

#endif // SDL_CHECK_ERROR

#if LIGHTING_MODES

enum
{
	LIGHTING_NONE,
	LIGHTING_SPECULAR,
	LIGHTING_DIFFUSE,
	LIGHTING_LAST
};

static int lightingMode = LIGHTING_SPECULAR;

#endif // LIGHTING_MODES

int GCurrentFrame = 1;
int GContextFrame = 0;


inline void InvalidateContext()
{
	GContextFrame = GCurrentFrame + 1;
	GCurrentFrame += 2;
}

//-----------------------------------------------------------------------------
// Some constants
//-----------------------------------------------------------------------------

#define DEFAULT_DIST			256
#define DEFAULT_FOV				80
#define MIN_DIST				25
#define MAX_DIST				2048
#define CLEAR_COLOR				0.05f, 0.05f, 0.05f, 1.0f
#define CLEAR_COLOR2			0.2f, 0.2f, 0.2f, 1.0f
//#define CLEAR_COLOR			0.3, 0.4, 0.6, 1
//#define CLEAR_COLOR2			0.2, 0.4, 0.3, 1
//#define CLEAR_COLOR			0.5, 0.6, 0.7, 1
//#define CLEAR_COLOR2			0.22, 0.2, 0.18, 1


//-----------------------------------------------------------------------------
// State variables
//-----------------------------------------------------------------------------

static float frameTime;
static unsigned lastFrameTime = 0;

static bool  is2Dmode = false;

// window size
static int   winWidth  = 800;
static int   winHeight = 600;

// matrices
static float projectionMatrix[4][4];
static float modelMatrix[4][4];

// view state
static CVec3 viewAngles;
static float viewDist   = 0;
static CVec3 rotOrigin  = {0, 0, 0};
static CVec3 viewOffset = {0, 0, 0};
// camera transform
       CVec3 viewOrigin = { -DEFAULT_DIST, 0, 0 };
       CAxis viewAxis;				// generated from angles

// view params (const)
static float zNear = 1;//??4;		// near clipping plane -- should auto-adjust
static float zFar  = 4096;			// far clipping plane
static float yFov  = DEFAULT_FOV;
static float tFovX, tFovY;			// tan(fov_x|y)

// mouse state
static int   mouseButtons;			// bit mask: left=1, middle=2, right=4, wheel up=8, wheel down=16


//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

static float distScale  = 1;
bool   vpInvertXAxis = false;

bool   GShowDebugInfo = true;


//-----------------------------------------------------------------------------
// Switch 2D/3D rendering mode
//-----------------------------------------------------------------------------

static void Set2Dmode(bool force = false)
{
	// force changes in viewport
	//!! bad looking code
	int width = winWidth, height = winHeight;
	if (GCurrentFramebuffer)
	{
		width  = GCurrentFramebuffer->Width();
		height = GCurrentFramebuffer->Height();
	}
	glViewport(0, 0, width, height);
	glScissor (0, 0, width, height);
//	DrawTextRight("Set2DMode: %d,%d", width, height);

	if (is2Dmode && !force) return;
	is2Dmode = true;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_CULL_FACE);
	//?? disable shading
	BindDefaultMaterial(true);
	if (GUseGLSL) glUseProgram(0);
}


static void Set3Dmode()
{
	// force changes in viewport
	//!! bad looking code
	int width = winWidth, height = winHeight;
	if (GCurrentFramebuffer)
	{
		width  = GCurrentFramebuffer->Width();
		height = GCurrentFramebuffer->Height();
	}
	glViewport(0, 0, width, height);
	glScissor (0, 0, width, height);
	//?? note: part above is a common code

	if (!is2Dmode) return;
	is2Dmode = false;

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(&projectionMatrix[0][0]);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(&modelMatrix[0][0]);
	glEnable(GL_CULL_FACE);
//	glCullFace(GL_FRONT);
}

void ResetView()
{
	viewAngles.Set(0, 180, 0);
	viewDist = DEFAULT_DIST * distScale;
	viewOrigin.Set(DEFAULT_DIST * distScale, 0, 0);
	viewOrigin.Add(viewOffset);
	rotOrigin.Zero();
	yFov = DEFAULT_FOV;
}

void SetDistScale(float scale)
{
	distScale = scale;
	ResetView();
}

void SetViewOffset(const CVec3 &offset)
{
	viewOffset = offset;
	ResetView();
}

//-----------------------------------------------------------------------------
// Text output
//-----------------------------------------------------------------------------

static GLuint	FontTexNum = 0;

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


static void DrawChar(char c, unsigned color, int textX, int textY)
{
	if (textX <= -CHAR_WIDTH || textY <= -CHAR_HEIGHT ||
		textX > winWidth || textY > winHeight)
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

static SDL_Window		*sdlWindow;
static SDL_GLContext	sdlContext;

// called when window resized
static void ResizeWindow(int w, int h)
{
	guard(ResizeWindow);

	winWidth  = w;
	winHeight = h;
	SDL_SetWindowSize(sdlWindow, winWidth, winHeight);
	SDL_CHECK_ERROR;

	static bool loaded = false;
	if (!loaded)
	{
		loaded = true;
		// Init our GL binder
		// Do it only once?
		if (!QGL_Init("(SDL)"))	// no library name required
		{
			appError("Unable to bind to OpenGL");
			return;
		}
		QGL_InitExtensions();
		GL_CheckGLSL();
	}

	if (!glIsTexture(FontTexNum))
	{
		// possibly context was recreated ...
		InvalidateContext();
		LoadFont();
	}

	// init gl
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
//	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_SCISSOR_TEST);
//	glShadeModel(GL_SMOOTH);
//	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	Set2Dmode();

	unguard;
}

void CApplication::GetWindowSize(int &x, int &y)
{
	x = winWidth;
	y = winHeight;
}


void CApplication::ToggleFullscreen()
{
	IsFullscreen = !IsFullscreen;

	if (IsFullscreen)
	{
		SavedWinWidth = winWidth;
		SavedWinHeight = winHeight;
		// get desktop display mode
		SDL_DisplayMode desktopMode;
		if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(sdlWindow), &desktopMode) != 0)
		{
			appPrintf("ERROR: unable to get desktop display mode\n");
			IsFullscreen = false;
			return;
		}
		// and apply it to the window
		SDL_SetWindowDisplayMode(sdlWindow, &desktopMode);
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN);
	}
	else
	{
		SDL_SetWindowFullscreen(sdlWindow, 0);
		::ResizeWindow(SavedWinWidth, SavedWinHeight);
	}
}


//-----------------------------------------------------------------------------
// Mouse control
//-----------------------------------------------------------------------------

// Variables used to store mouse position before switching to relative mode, so
// the position will be restored after releasing mouse buttons.
static int mousePosX, mousePosY;

static void OnMouseButton(int type, int button)
{
	int prevButtons = mouseButtons;
	// update mouse buttons state
	int mask = SDL_BUTTON(button);
	if (type == SDL_MOUSEBUTTONDOWN)
		mouseButtons |= mask;
	else
		mouseButtons &= ~mask;
	if (!prevButtons && mouseButtons)
	{
		// grabbing mouse
		SDL_GetMouseState(&mousePosX, &mousePosY);
		SDL_SetRelativeMouseMode(SDL_TRUE);		// switch to relative mode - mouse cursor will be hidden and remains in single place
	}
	else if (prevButtons && !mouseButtons)
	{
		// releasing mouse
		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_WarpMouseInWindow(sdlWindow, mousePosX, mousePosY);
	}
}


static void OnMouseMove(int mx, int my)
{
	if (!mouseButtons) return;

	float xDelta = (float)mx / winWidth;
	float yDelta = (float)my / winHeight;
	if (vpInvertXAxis)
		xDelta = -xDelta;

	float YawDelta = 0, PitchDelta = 0, DistDelta = 0, PanX = 0, PanY = 0;

	if (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT))
	{
		// rotate camera
		YawDelta   = -xDelta * 360;
		PitchDelta =  yDelta * 360;
	}
	if (mouseButtons & SDL_BUTTON(SDL_BUTTON_RIGHT))
	{
		// change distance to object
		DistDelta = yDelta * 400 * distScale;
	}
	if (mouseButtons & SDL_BUTTON(SDL_BUTTON_MIDDLE))
	{
		// pan camera
		PanX = xDelta * viewDist * 2;
		PanY = yDelta * viewDist * 2;
	}
	MoveCamera(YawDelta, PitchDelta, DistDelta, PanX, PanY);
}


void MoveCamera(float YawDelta, float PitchDelta, float DistDelta, float PanX, float PanY)
{
	// rotate camera
	viewAngles[YAW]   += YawDelta;
	viewAngles[PITCH] += PitchDelta;
	// bound angles
	viewAngles[YAW]   = fmod(viewAngles[YAW], 360);
	viewAngles[PITCH] = bound(viewAngles[PITCH], -90, 90);

	// change distance to object
	viewDist += DistDelta;

	CAxis axis;
	axis.FromEuler(viewAngles);

	// pan camera
	VectorMA(rotOrigin, PanX, axis[1]);
	VectorMA(rotOrigin, PanY, axis[2]);

	// recompute viewOrigin
	viewDist = bound(viewDist, MIN_DIST * distScale, MAX_DIST * distScale);
	VectorScale(axis[0], -viewDist, viewOrigin);
	viewOrigin.Add(rotOrigin);
	viewOrigin.Add(viewOffset);
}


void FocusCameraOnPoint(const CVec3 &center)
{
	rotOrigin = center;

	CAxis axis;
	axis.FromEuler(viewAngles);
	// recompute viewOrigin
	viewDist = bound(viewDist, MIN_DIST * distScale, MAX_DIST * distScale);
	VectorScale(axis[0], -viewDist, viewOrigin);
	viewOrigin.Add(rotOrigin);
	viewOrigin.Add(viewOffset);
}


//-------------------------------------------------------------------------
// Building modelview and projection matrices
//-------------------------------------------------------------------------
static void BuildMatrices()
{
	// view angles -> view axis
	Euler2Vecs(viewAngles, &viewAxis[0], &viewAxis[1], &viewAxis[2]);
	if (!vpInvertXAxis)
		viewAxis[1].Negate();
//	DrawTextLeft("origin: %6.1f %6.1f %6.1f", VECTOR_ARG(viewOrigin));
//	DrawTextLeft("angles: %6.1f %6.1f %6.1f", VECTOR_ARG(viewAngles));
#if 0
	DrawTextLeft("---- view axis ----");
	DrawTextLeft("[0]: %g %g %g",    VECTOR_ARG(viewAxis[0]));
	DrawTextLeft("[1]: %g %g %g",    VECTOR_ARG(viewAxis[1]));
	DrawTextLeft("[2]: %g %g %g",    VECTOR_ARG(viewAxis[2]));
#endif

	// compute modelview matrix
	/* Matrix contents:
	 *  a00   a01   a02    -x
	 *  a10   a11   a12    -y
	 *  a20   a21   a22    -z
	 *    0     0     0     1
	 * where: x = dot(a0,org); y = dot(a1,org); z = dot(a2,org)
	 */
	float	matrix[4][4];	// temporary matrix
	int		i, j, k;
	// matrix[0..2][0..2] = viewAxis
	memset(matrix, 0, sizeof(matrix));
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			matrix[i][j] = viewAxis[j][i];
	matrix[3][0] = - dot(viewOrigin, viewAxis[0]);
	matrix[3][1] = - dot(viewOrigin, viewAxis[1]);
	matrix[3][2] = - dot(viewOrigin, viewAxis[2]);
	matrix[3][3] = 1;
	// rotate model: modelMatrix = baseMatrix * matrix
	static const float baseMatrix[4][4] = // axis {0 0 -1} {-1 0 0} {0 1 0}
	{
		{  0,  0, -1,  0},
		{ -1,  0,  0,  0},
		{  0,  1,  0,  0},
		{  0,  0,  0,  1}
	};
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
		{
			float s = 0;
			for (k = 0; k < 4; k++)
				s += baseMatrix[k][j] * matrix[i][k];
			modelMatrix[i][j] = s;
		}
#if 0
#define m matrix // modelMatrix
	DrawTextLeft("----- modelview matrix ------");
	for (i = 0; i < 4; i++)
		DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][i], m[1][i], m[2][i], m[3][i]);
#undef m
#endif

	// compute projection matrix
	tFovY = tan(yFov * M_PI / 360.0f);
	tFovX = tFovY / winHeight * winWidth; // tan(xFov * M_PI / 360.0f);
	float zMin = zNear * distScale;
	float zMax = zFar  * distScale;
	float xMin = -zMin * tFovX;
	float xMax =  zMin * tFovX;
	float yMin = -zMin * tFovY;
	float yMax =  zMin * tFovY;
	/* Matrix contents:
	 *  |   0    1    2    3
	 * -+-------------------
	 * 0|   A    0    C    0
	 * 1|   0    B    D    0
	 * 2|   0    0    E    F
	 * 3|   0    0   -1    0
	 */
#define m projectionMatrix
	memset(m, 0, sizeof(m));
	m[0][0] = zMin * 2 / (xMax - xMin);				// A
	m[1][1] = zMin * 2 / (yMax - yMin);				// B
	m[2][0] =  (xMax + xMin) / (xMax - xMin);		// C
	m[2][1] =  (yMax + yMin) / (yMax - yMin);		// D
	m[2][2] = -(zMax + zMin) / (zMax - zMin);		// E
	m[2][3] = -1;
	m[3][2] = -2.0f * zMin * zMax / (zMax - zMin);	// F

#if 0
	DrawTextLeft("zMax: %g;  frustum: x[%g, %g] y[%g, %g]", zMax, xMin, xMax, yMin, yMax);
	DrawTextLeft("----- projection matrix -----");
	DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][0], m[1][0], m[2][0], m[3][0]);
	DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][1], m[1][1], m[2][1], m[3][1]);
	DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][2], m[1][2], m[2][2], m[3][2]);
	DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][3], m[1][3], m[2][3], m[3][3]);
#endif
#undef m
}

//-----------------------------------------------------------------------------

static void Init(const char *caption)
{
	guard(SDL.Init);

	// init SDL
	if (SDL_Init(SDL_INIT_VIDEO) == -1)
		appError("Failed to initialize SDL");

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
//	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

	sdlWindow = SDL_CreateWindow(caption,
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, winWidth, winHeight,
		SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
	if (!sdlWindow) appError("Failed to create SDL window");
	sdlContext = SDL_GL_CreateContext(sdlWindow);
	#if LIMIT_FPS
	SDL_GL_SetSwapInterval(0);			// FPS will be limited by Sleep()
	#else
	SDL_GL_SetSwapInterval(1);			// allow waiting for vsync to reduce CPU usage
	#endif

	// initialize GL
	ResizeWindow(winWidth, winHeight);

	unguard;
}

static void Shutdown()
{
	guard(Shutdown);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	InvalidateContext();
	unguard;
}


//-----------------------------------------------------------------------------
// Text output
//-----------------------------------------------------------------------------

#define TOP_TEXT_POS	CHAR_HEIGHT
#define BOTTOM_TEXT_POS	CHAR_HEIGHT
#define LEFT_BORDER		CHAR_WIDTH
#define RIGHT_BORDER	CHAR_WIDTH


struct CRText : public CTextRec
{
	short			x, y;
	ETextAnchor		anchor;
	unsigned		color;
};

static TTextContainer<CRText, 65536> Text;

static int nextText_y[TA_Last];
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


static void ClearTexts()
{
	nextText_y[TA_TopLeft] = nextText_y[TA_TopRight] = TOP_TEXT_POS + textOffset;
	nextText_y[TA_BottomLeft] = nextText_y[TA_BottomRight] = 0;
	Text.Clear();
}


static void GetTextExtents(const char *s, int &width, int &height)
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

	if (rec->anchor == TA_BottomLeft || rec->anchor == TA_BottomRight)
	{
		y = y + winHeight - nextText_y[rec->anchor] - BOTTOM_TEXT_POS;
	}

	unsigned color = rec->color;

	while (true)
	{
		const char *s = strchr(text, '\n');
		int len = s ? s - text : strlen(text);

		int x = rec->x;
		for (int i = 0; i < len; i++)
		{
			char c = text[i];
			if (c == COLOR_ESCAPE)
			{
				char c2 = text[i+1];
				if (c2 >= '0' && c2 <= '7')
				{
					color = colorTable[c2 - '0'];
					i++;
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
static bool dumpTexts = false;
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
	Text.Enumerate(DrawText);
	ClearTexts();
#if DUMP_TEXTS
	appSetNotifyHeader(NULL);
	dumpTexts = false;
#endif
}


static void DrawTextPos(int x, int y, const char *text, unsigned color, ETextAnchor anchor = TA_None)
{
	if (!GShowDebugInfo) return;

	CRText *rec = Text.Add(text);
	if (!rec) return;
	rec->x      = x;
	rec->y      = y;
	rec->anchor = anchor;
	rec->color  = color;
}


static void DrawTextAtAnchor(ETextAnchor anchor, unsigned color, const char *fmt, va_list argptr)
{
	guard(DrawTextAtAnchor);

	assert(anchor >= 0 && anchor < TA_Last);

	bool isBottom = (anchor >= TA_BottomLeft);
	bool isLeft   = (anchor == TA_TopLeft || anchor == TA_BottomLeft);

	int pos_y = nextText_y[anchor];

#if DUMP_TEXTS
	if (dumpTexts) pos_y = winHeight / 2;				// trick ...
#endif

	if (!isBottom && pos_y >= winHeight && !dumpTexts)	// out of screen
		return;

	char msg[4096];
	vsnprintf(ARRAY_ARG(msg), fmt, argptr);
	int w, h;
	GetTextExtents(msg, w, h);

	nextText_y[anchor] = pos_y + h;

	if (!isBottom && pos_y + h <= 0 && !dumpTexts)		// out of screen
		return;

	DrawTextPos(isLeft ? LEFT_BORDER : winWidth - RIGHT_BORDER - w, pos_y, msg, color, anchor);

#if DUMP_TEXTS
	if (dumpTexts)
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

	unguard;
}


#define DRAW_TEXT(anchor,color,fmt)	\
	va_list	argptr;				\
	va_start(argptr, fmt);		\
	DrawTextAtAnchor(anchor, color, fmt, argptr); \
	va_end(argptr);


void DrawTextLeft(const char *text, ...)
{
	DRAW_TEXT(TA_TopLeft, WHITE_COLOR, text);
}


void DrawTextRight(const char *text, ...)
{
	DRAW_TEXT(TA_TopRight, WHITE_COLOR, text);
}


void DrawTextBottomLeft(const char *text, ...)
{
	DRAW_TEXT(TA_BottomLeft, WHITE_COLOR, text);
}


void DrawTextBottomRight(const char *text, ...)
{
	DRAW_TEXT(TA_BottomRight, WHITE_COLOR, text);
}


void DrawText(ETextAnchor anchor, const char *text, ...)
{
	DRAW_TEXT(anchor, WHITE_COLOR, text);
}


void DrawText(ETextAnchor anchor, unsigned color, const char *text, ...)
{
	DRAW_TEXT(anchor, color, text);
}


// Project 3D point to screen coordinates; return false when not in view frustum
static bool ProjectToScreen(const CVec3 &pos, int scr[2])
{
	CVec3	vec;
	VectorSubtract(pos, viewOrigin, vec);

	float z = dot(vec, viewAxis[0]);
	if (z <= zNear) return false;				// not visible

	float x = dot(vec, viewAxis[1]) / z / tFovX;
	if (x < -1 || x > 1) return false;

	float y = dot(vec, viewAxis[2]) / z / tFovY;
	if (y < -1 || y > 1) return false;

	scr[0] = appRound(/*winX + */ winWidth  * (0.5 - x / 2));
	scr[1] = appRound(/*winY + */ winHeight * (0.5 - y / 2));

	return true;
}


void DrawText3D(const CVec3 &pos, unsigned color, const char *text, ...)
{
	int coords[2];
	if (!ProjectToScreen(pos, coords)) return;

	va_list	argptr;
	va_start(argptr, text);
	char msg[4096];
	vsnprintf(ARRAY_ARG(msg), text, argptr);
	va_end(argptr);

	DrawTextPos(coords[0], coords[1], msg, color);
}


//-----------------------------------------------------------------------------
// Bloom implementation
//-----------------------------------------------------------------------------

#if USE_BLOOM

#include "../Unreal/Shaders.h"				//?? bad place to include

static void PostEffectPrepare(CFramebuffer &FBO)
{
	guard(PostEffectPrepare);
	FBO.SetSize(winWidth, winHeight);
	FBO.Use();
	unguard;
}

static void BloomScene(CFramebuffer &FBO)
{
	guard(BloomScene);

	Set2Dmode();
	glDisable(GL_BLEND);

	int width  = FBO.Width() / 2;
	int height = FBO.Height() / 2;

	static CFramebuffer FB[2];
	FB[0].SetSize(width, height);
	FB[1].SetSize(width, height);

	static CShader BloomGatherShader, BloomPassShader, BloomBlendShader;
	if (!BloomGatherShader.IsValid()) BloomGatherShader.Make(BloomGather_ush);
	if (!BloomPassShader.IsValid())   BloomPassShader.Make(BloomPass_ush);
	if (!BloomBlendShader.IsValid())  BloomBlendShader.Make(BloomBlend_ush);

	// get overbright color
	//?? function: FB1 -> FB2: FB2.Use(), FB1.Flush()
	FB[0].Use();
	BloomGatherShader.Use();
	FBO.Flush();

	// perform horizontal blurring
	FB[1].Use();
	BloomPassShader.Use();
	BloomPassShader.SetUniform("Tex", 0);
	BloomPassShader.SetUniform("Step", 1.0f / width, 0.0f);
	FB[0].Flush();

	// perform vertical blurring
	FB[0].Use();
	BloomPassShader.SetUniform("Step", 0.0f, 1.0f / height);
	FB[1].Flush();

	// final blend
	CFramebuffer::Unset();
	Set2Dmode(true);					//?? do it automatically in CFramebuffer::Unset() ?
//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	BloomBlendShader.Use();
	BloomBlendShader.SetUniform("BlurTex",  0);
	BloomBlendShader.SetUniform("OrigTex",  1);
	BloomBlendShader.SetUniform("DepthTex", 2);

	glActiveTexture(GL_TEXTURE2);
	FBO.BindDepthTexture();
	glActiveTexture(GL_TEXTURE1);
	FBO.BindTexture();					// original screen -> TMU1
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_DEPTH_TEST);			// shader will write to the depth
	FB[0].Flush();						// render with bloom component as TMU0
	glEnable(GL_DEPTH_TEST);

	CShader::Unset();
	BindDefaultMaterial();
	unguard;
}

#endif // USE_BLOOM

//-----------------------------------------------------------------------------
// "Hook" functions
//-----------------------------------------------------------------------------

static void DrawBackground()
{
	if (GUseGLSL) CShader::Unset();
	// clear screen buffer
#if FUNNY_BACKGROUND
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	Set2Dmode();
	glBegin(GL_QUADS);
	glColor4f(CLEAR_COLOR);
	glVertex2f(winWidth, 0);
	glVertex2f(0, 0);
	glColor4f(CLEAR_COLOR2);
	glVertex2f(0, winHeight);
	glVertex2f(winWidth, winHeight);
	glEnd();
	glClear(GL_DEPTH_BUFFER_BIT);
#else
	glClearColor(CLEAR_COLOR);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif // FUNNY_BACKGROUND
}

//!! rename function!
void CApplication::Display()
{
	guard(CApplication::Display);

	GCurrentFrame++;

	unsigned currentTime = appMilliseconds();
	float TimeDelta = (currentTime - lastFrameTime) / 1000.0f;
	lastFrameTime = currentTime;
#if SHOW_FPS
	if (TimeDelta > 0)
		DrawTextRight(S_YELLOW"FPS: %3.0f", 1.0f / TimeDelta);
#endif // SHOW_FPS

#if USE_BLOOM
	bool useBloom = (GL_SUPPORT(QGL_EXT_FRAMEBUFFER_OBJECT) && GUseGLSL && !GDisableFBO);
	static CFramebuffer FBO(true, GL_SUPPORT(QGL_ARB_TEXTURE_FLOAT));
	if (useBloom) PostEffectPrepare(FBO);
#endif // USE_BLOOM

	DrawBackground();

	// 3D drawings
	BuildMatrices();
	Set3Dmode();

	// enable lighting
	static const float lightPos[4]      = {1000, 2000, 2000,  0};
	static const float lightAmbient[4]  = {0.1f, 0.1f, 0.15f, 1};
	static const float specIntens[4]    = {0.4f, 0.4f, 0.4f,  0};
	static const float black[4]         = {0,   0,   0,    0};
	static const float white[4]         = {1,   1,   1,    0};
	glEnable(GL_COLOR_MATERIAL);
//	glColorMaterial(GL_FRONT,GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);		// allow non-normalized normal arrays
//	glEnable(GL_LIGHTING);
	// light parameters
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE,  white);
	glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmbient);
	glLightfv(GL_LIGHT0, GL_SPECULAR, white);
	// material parameters
	glMaterialfv(GL_FRONT, GL_DIFFUSE,  white);
	glMaterialfv(GL_FRONT, GL_AMBIENT,  white);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specIntens);
	glMaterialf (GL_FRONT, GL_SHININESS, 5);
	// Use GL_EXT_separate_specular_color without check (in worst case GL error code will be set)
	glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
#if LIGHTING_MODES
	if (lightingMode == LIGHTING_NONE)
	{
		// disable diffuse and saturate ambient
		glLightfv(GL_LIGHT0, GL_DIFFUSE, black);
		glLightfv(GL_LIGHT0, GL_AMBIENT, white);
	}
	if (lightingMode != LIGHTING_SPECULAR)
		glMaterialfv(GL_FRONT, GL_SPECULAR, black);
#endif // LIGHTING_MODES

	// draw scene
	Draw3D(TimeDelta);

	// restore draw state
	BindDefaultMaterial(true);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_ALPHA_TEST);

	// disable lighting
	glColor3f(1, 1, 1);
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);

#if USE_BLOOM
	if (useBloom) BloomScene(FBO);
#endif // USE_BLOOM

	// 2D drawings
	Set2Dmode();

	DrawTexts();
	FlushTexts();

	// swap buffers
	BeforeSwap();
	SDL_GL_SwapWindow(sdlWindow);

	unguard;
}


void DrawKeyHelp(const char *Key, const char *Help)
{
	DrawTextLeft(S_YELLOW "%-" STR(KEY_HELP_TAB) "s " S_WHITE "%s", Key, Help);
}

void CApplication::DrawTexts()
{
	// display help when needed
	if (IsHelpVisible)
	{
		DrawTextLeft(S_RED "Keyboard:\n~~~~~~~~~");
		DrawKeyHelp("Esc",         "exit");
		DrawKeyHelp("H",           "toggle help");
		DrawKeyHelp("Alt+Enter",   "toggle fullscreen");
		DrawKeyHelp("LeftMouse",   "rotate view");
		DrawKeyHelp("RightMouse",  "zoom view");
		DrawKeyHelp("MiddleMouse", "move camera");
		DrawKeyHelp("R",           "reset view");
	}
}


void CApplication::HandleKeyDown(unsigned key, unsigned mod)
{
	key = tolower(key);

	if (mod & KMOD_CTRL)
		key |= KEY_CTRL;
	else if (mod & KMOD_SHIFT)
		key |= KEY_SHIFT;
	else if (mod & KMOD_ALT)
		key |= KEY_ALT;

	ProcessKey(key, true);
}

void CApplication::ProcessKey(int key, bool isDown)
{
	if (!isDown)
		return;

	switch (key)
	{
	case SDLK_ESCAPE:
		RequestingQuit = true;
		break;
	case SDLK_RETURN|KEY_ALT:
		ToggleFullscreen();
		break;
	case 'h':
		IsHelpVisible = !IsHelpVisible;
		break;
	case 'r':
		ResetView();
		break;
	case SPEC_KEY(PAGEUP)|KEY_CTRL:
	case SDLK_KP_9|KEY_CTRL:
		textOffset += TEXT_SCROLL_LINES;
		if (textOffset > 0) textOffset = 0;
		break;
	case SPEC_KEY(PAGEDOWN)|KEY_CTRL:
	case SDLK_KP_3|KEY_CTRL:
		textOffset -= TEXT_SCROLL_LINES;
		break;
#if LIGHTING_MODES
	case 'l'|KEY_CTRL:
		if (++lightingMode == LIGHTING_LAST) lightingMode = 0;
		break;
#endif
	case 'g'|KEY_CTRL:
		{
			// enable/disable extensions and GLSL
			static unsigned extensionMask = 0;
			static bool     StoreUseGLSL  = false;
			BindDefaultMaterial(true);
			if (GUseGLSL) glUseProgram(0);
			Exchange(gl_config.extensionMask, extensionMask);
			Exchange(StoreUseGLSL, GUseGLSL);
		}
		break;
	case 'q'|KEY_CTRL:
		GShowDebugInfo = !GShowDebugInfo;
		break;
#if DUMP_TEXTS
	case 'd'|KEY_CTRL:
		dumpTexts = true;
		break;
#endif
	case SPEC_KEY(UP)|KEY_SHIFT:
	case SPEC_KEY(DOWN)|KEY_SHIFT:
		{
			float oldFov = yFov;
			yFov += (key == (SPEC_KEY(UP)|KEY_SHIFT)) ? +5 : -5;
			yFov = bound(yFov, 10, 120);
			if (yFov != oldFov)
			{
				float s = tan(oldFov * M_PI / 360) / tan(yFov * M_PI / 360);
				distScale *= s;
				viewDist  *= s;
			}
			appPrintf("new fov: %g\n", yFov);
			MoveCamera(0, 0, 0, 0, 0);
			break;
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Main function
//-----------------------------------------------------------------------------

CApplication::CApplication()
:	IsHelpVisible(false)
,	RequestingQuit(false)
,	IsFullscreen(false)
{}

CApplication::~CApplication()
{}

#if SMART_RESIZE

int CApplication::OnEvent(void *userdata, SDL_Event *evt)
{
	if (evt->type == SDL_WINDOWEVENT && evt->window.event == SDL_WINDOWEVENT_RESIZED)
	{
		CApplication* app = (CApplication*)userdata;
		::ResizeWindow(evt->window.data1, evt->window.data2);
		app->Display();
		return 0;	// drop this event (already handled)
	}
	return 1;		// add event to queue
}

#endif // SMART_RESIZE


// localized keyboard could return different chars for some keys - should translate them back to English keyboard
// note: there is no scancodes in SDL1.2
static int TranslateKey(int sym, int scan)
{
	static const struct
	{
		uint16 scan;
		uint16 sym;
	} scanToSym[] =
	{
		SDL_SCANCODE_LEFTBRACKET, '[',
		SDL_SCANCODE_RIGHTBRACKET, ']',
		SDL_SCANCODE_COMMA, ',',
		SDL_SCANCODE_PERIOD, '.',
	};
	for (int i = 0; i < ARRAY_COUNT(scanToSym); i++)
		if (scanToSym[i].scan == scan)
		{
//			appPrintf("%d -> %c\n", scanToSym[i].scan, scanToSym[i].sym);
			return scanToSym[i].sym;
		}
//	appPrintf("%d\n", scan);
	return sym;
}

void CApplication::VisualizerLoop(const char *caption)
{
	guard(VisualizerLoop);

	Init(caption);
	WindowCreated();
	ClearTexts();

	// Hook window messages
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

#if LIMIT_FPS
	// get display refresh rate
	SDL_DisplayMode desktopMode;
	int frameTime = 0;
	if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(sdlWindow), &desktopMode) == 0)
		frameTime = 1000 / desktopMode.refresh_rate;
#endif // LIMIT_FPS
#if SMART_RESIZE
	SDL_SetEventFilter(&OnEvent, this);
#endif // SMART_RESIZE
	if (viewDist == 0)
		ResetView();			// may be initialized before VisualizerLoop call
	// main loop
	SDL_Event evt;
	bool disableUpdate = false;
	while (!RequestingQuit)
	{
		while (SDL_PollEvent(&evt))
		{
			switch (evt.type)
			{
			case SDL_KEYDOWN:
				HandleKeyDown(TranslateKey(evt.key.keysym.sym, evt.key.keysym.scancode), evt.key.keysym.mod);
				break;
			case SDL_KEYUP:
				ProcessKey(evt.key.keysym.sym, false);
				break;
			case SDL_WINDOWEVENT:
				switch (evt.window.event)
				{
				case SDL_WINDOWEVENT_MINIMIZED:
					disableUpdate = true;
					break;
				case SDL_WINDOWEVENT_RESTORED:
					disableUpdate = false;
					break;
	#if !SMART_RESIZE
				case SDL_WINDOWEVENT_RESIZED:
					::ResizeWindow(evt.window.data1, evt.window.data2);
					break;
	#endif // SMART_RESIZE
	#if FIX_STICKY_MOD_KEYS
				case SDL_WINDOWEVENT_FOCUS_LOST:
					SDL_SetModState(KMOD_NONE);
					break;
	#endif // FIX_STICKY_MOD_KEYS
				}
				break;
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
				OnMouseButton(evt.type, evt.button.button);
				break;
			case SDL_MOUSEMOTION:
				OnMouseMove(evt.motion.xrel, evt.motion.yrel);
				break;
			case SDL_QUIT:
				RequestingQuit = true;
				break;
#if _WIN32
			case SDL_SYSWMEVENT:
				WndProc(evt.syswm.msg->msg.win.msg, evt.syswm.msg->msg.win.wParam, evt.syswm.msg->msg.win.lParam);
				break;
#endif
			}
		}
		if (!disableUpdate)
		{
#if LIMIT_FPS
			unsigned time = SDL_GetTicks();
			Display();			// draw the scene
			int renderTime = SDL_GetTicks() - time;
			// ensure refresh rate
			static unsigned lastTime = 0;
			if (lastTime && frameTime)
			{
				int delay = frameTime - renderTime;
				if (delay < 0) delay = 0;
				SDL_Delay(delay);
			}
			lastTime = time;
#else
			Display();			// draw the scene
#endif // LIMIT_FPS
		}
		else
		{
			SDL_Delay(100);
		}
	}
	// shutdown
	Shutdown();

	unguard;
}

SDL_Window* CApplication::GetWindow() const
{
	return sdlWindow;
}

void CApplication::ResizeWindow()
{
	::ResizeWindow(winWidth, winHeight);
}

void CApplication::Exit()
{
	RequestingQuit = true;
}

#endif // RENDERING
