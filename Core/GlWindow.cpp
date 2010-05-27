#include "Core.h"
#include "TextContainer.h"
#include "GlWindow.h"
#include "CoreGL.h"

// font
#include "GlFont.h"

#define CHARS_PER_LINE			(TEX_WIDTH/CHAR_WIDTH)
#define TEXT_SCROLL_LINES		(CHAR_HEIGHT/2)


#define LIGHTING_MODES			1		// allow switching scene lighting modes with Ctrl+L
#define DUMP_TEXTS				1		// allow Ctrl+D to dump all onscreen texts to a log
#define FUNNY_BACKGROUND		1		// draw gradient background
#define SMART_RESIZE			1		// redraw window contents while resizing window
#define USE_BLOOM				1

#if !_WIN32
#undef SMART_RESIZE						// not compatible with Linux (has ugly effects and program hung)
#endif

#if RENDERING

#if _MSC_VER
#pragma comment(lib, "opengl32.lib")
#endif


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
#define MIN_DIST				25
#define MAX_DIST				2048
#define CLEAR_COLOR				0.3, 0.4, 0.6, 1
#define CLEAR_COLOR2			0.2, 0.4, 0.3, 1
//#define CLEAR_COLOR				0.5, 0.6, 0.7, 1
//#define CLEAR_COLOR2			0.22, 0.2, 0.18, 1


//-----------------------------------------------------------------------------
// State variables
//-----------------------------------------------------------------------------

static bool  isHelpVisible = false;
static float frameTime;

static bool  is2Dmode = false;

// window size
static int   winWidth  = 800;
static int   winHeight = 600;

// matrices
static float projectionMatrix[4][4];
static float modelMatrix[4][4];

// view state
static CVec3 viewAngles;
static float viewDist   = DEFAULT_DIST;
static CVec3 viewOrigin = { -DEFAULT_DIST, 0, 0 };
static CVec3 rotOrigin  = {0, 0, 0};
static CVec3 viewOffset = {0, 0, 0};
       CAxis viewAxis;				// generated from angles

// view params (const)
static float zNear = 1;//??4;		// near clipping plane -- should auto-adjust
static float zFar  = 4096;			// far clipping plane
static float yFov  = 80;
static float tFovX, tFovY;			// tan(fov_x|y)

// mouse state
static int   mouseButtons;			// bit mask: left=1, middle=2, right=4, wheel up=8, wheel down=16


//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

static float distScale  = 1;
bool   vpInvertXAxis = false;


//-----------------------------------------------------------------------------
// Viewport support
//-----------------------------------------------------------------------------


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
	if (GL_SUPPORT(QGL_2_0)) glUseProgram(0);
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
	byte *pic = (byte*)malloc(TEX_WIDTH * TEX_HEIGHT * 4);
	int i;
	byte *p, *dst;
	for (i = 0, p = TEX_DATA, dst = pic; i < TEX_WIDTH * TEX_HEIGHT / 8; i++, p++)
	{
		byte s = *p;
		for (int bit = 0; bit < 8; bit++, dst += 4)
		{
			dst[0] = 255;
			dst[1] = 255;
			dst[2] = 255;
			dst[3] = (s & (1 << bit)) ? 255 : 0;
		}
	}
	// upload it
	glGenTextures(1, &FontTexNum);
	glBindTexture(GL_TEXTURE_2D, FontTexNum);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	free(pic);
}


static void DrawChar(char c, int color, int textX, int textY)
{
	if (textX <= -CHAR_WIDTH || textY <= -CHAR_HEIGHT ||
		textX > winWidth || textY > winHeight)
		return;				// outside of screen

	static const float colorTable[][3] =
	{
		{0, 0, 0},
		{1, 0, 0},
		{0, 1, 0},
		{1, 1, 0},
		{0, 0, 1},
		{1, 0, 1},
		{0, 1, 1},
		{1, 1, 1}
	};

	glBegin(GL_QUADS);

	c -= FONT_FIRST_CHAR;

	int x1 = textX;
	int y1 = textY;
	int x2 = textX + CHAR_WIDTH;
	int y2 = textY + CHAR_HEIGHT;
	int line = c / CHARS_PER_LINE;
	int col  = c - line * CHARS_PER_LINE;
	float s0 = (col      * CHAR_WIDTH)  / (float)TEX_WIDTH;
	float s1 = ((col+1)  * CHAR_WIDTH)  / (float)TEX_WIDTH;
	float t0 = (line     * CHAR_HEIGHT) / (float)TEX_HEIGHT;
	float t1 = ((line+1) * CHAR_HEIGHT) / (float)TEX_HEIGHT;

	for (int s = 1; s >= 0; s--)
	{
		// s=1 -> shadow, s=0 -> char
		glColor3fv(s ? colorTable[0] : colorTable[color]);
		glTexCoord2f(s0, t0);
		glVertex2f(x1+s, y1+s);
		glTexCoord2f(s1, t0);
		glVertex2f(x2+s, y1+s);
		glTexCoord2f(s1, t1);
		glVertex2f(x2+s, y2+s);
		glTexCoord2f(s0, t1);
		glVertex2f(x1+s, y2+s);
	}

	glEnd();
}

//-----------------------------------------------------------------------------
// called when window resized
static void ResizeWindow(int w, int h)
{
	guard(ResizeWindow);

	winWidth  = w;
	winHeight = h;
	SDL_SetVideoMode(winWidth, winHeight, 24, SDL_OPENGL|SDL_RESIZABLE);

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

void GetWindowSize(int &x, int &y)
{
	x = winWidth;
	y = winHeight;
}


//-----------------------------------------------------------------------------
// Mouse control
//-----------------------------------------------------------------------------

#if !_WIN32
static int dropMouseMotion = 0;
#endif

static void OnMouseButton(int type, int button)
{
	int prevButtons = mouseButtons;
	// update mouse buttons state
	int mask = SDL_BUTTON(button);
	if (type == SDL_MOUSEBUTTONDOWN)
		mouseButtons |= mask;
	else
		mouseButtons &= ~mask;
	// show/hide cursor
	if (!prevButtons && mouseButtons)
	{
		SDL_ShowCursor(0);
		SDL_WM_GrabInput(SDL_GRAB_ON);
#if !_WIN32
		// in linux, when calling SDL_ShowCursor(0), SDL will produce unnecessary mouse
		// motion event, which will cause major scene rotation if not removed; here
		// we will remove this event
		dropMouseMotion = 2;
#endif
	}
	else if (prevButtons && !mouseButtons)
	{
		SDL_ShowCursor(1);
		SDL_WM_GrabInput(SDL_GRAB_OFF);
	}
}


static void OnMouseMove(int dx, int dy)
{
	if (!mouseButtons) return;

#if !_WIN32
	if (dropMouseMotion > 0)
	{
		dropMouseMotion--;
		return;
	}
#endif

	float xDelta = (float)dx / winWidth;
	float yDelta = (float)dy / winHeight;
	if (vpInvertXAxis)
		xDelta = -xDelta;

	if (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT))
	{
		// rotate camera
		viewAngles[YAW]   -= xDelta * 360;
		viewAngles[PITCH] += yDelta * 360;
		// bound angles
		viewAngles[YAW]   = fmod(viewAngles[YAW], 360);
		viewAngles[PITCH] = bound(viewAngles[PITCH], -90, 90);
	}
	if (mouseButtons & SDL_BUTTON(SDL_BUTTON_RIGHT))
	{
		// change distance to object
		viewDist += yDelta * 400 * distScale;
	}
	CAxis axis;
	axis.FromEuler(viewAngles);
	if (mouseButtons & SDL_BUTTON(SDL_BUTTON_MIDDLE))
	{
		// pan camera
		VectorMA(rotOrigin, xDelta * viewDist * 2, axis[1]);
		VectorMA(rotOrigin, yDelta * viewDist * 2, axis[2]);
	}
	viewDist = bound(viewDist, MIN_DIST * distScale, MAX_DIST * distScale);
	VectorScale(axis[0], -viewDist, viewOrigin);
	viewOrigin.Add(rotOrigin);
	viewOrigin.Add(viewOffset);
}


//-------------------------------------------------------------------------
// Building modelview and projection matrices
//-------------------------------------------------------------------------
void BuildMatrices()
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

	SDL_WM_SetCaption(caption, caption);
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
#define LEFT_BORDER		CHAR_WIDTH
#define RIGHT_BORDER	CHAR_WIDTH


struct CRText : public CTextRec
{
	short	x, y;
};

static TTextContainer<CRText, 65536> Text;

static int nextLeft_y  = TOP_TEXT_POS;
static int nextRight_y = TOP_TEXT_POS;
static int textOffset  = 0;


void ClearTexts()
{
	nextLeft_y = nextRight_y = TOP_TEXT_POS + textOffset;
	Text.Clear();
}


static void GetTextExtents(const char *s, int &width, int &height)
{
	int x = 0, w = 0;
	int h = CHAR_HEIGHT;
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
			h += CHAR_HEIGHT;
			continue;
		}
		x += CHAR_WIDTH;
	}
	width = max(x, w);
	height = h;
}


static void DrawText(const CRText *rec)
{
	int y = rec->y;
	const char *text = rec->text;

	int color = 7;
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
					color = c2 - '0';
					i++;
					continue;
				}
			}
			DrawChar(c, color, x, y);
			x += CHAR_WIDTH;
		}
		if (!s) return;							// all done

		y += CHAR_HEIGHT;
		text = s + 1;
	}
}

#if DUMP_TEXTS
static bool dumpTexts = false;
#endif

void FlushTexts()
{
	// setup GL
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, FontTexNum);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5);

#if DUMP_TEXTS
	appSetNotifyHeader("Screen texts");
#endif
	Text.Enumerate(DrawText);
	nextLeft_y = nextRight_y = TOP_TEXT_POS;
	ClearTexts();
#if DUMP_TEXTS
	appSetNotifyHeader(NULL);
	dumpTexts = false;
#endif
}


void DrawTextPos(int x, int y, const char *text)
{
#if DUMP_TEXTS
	if (dumpTexts)
	{
		// dump to log
		appNotify("%s", text);
		// hack to view all texts
		nextLeft_y = nextRight_y = TOP_TEXT_POS;
	}
#endif
	CRText *rec = Text.Add(text);
	if (!rec) return;
	rec->x = x;
	rec->y = y;
}


#define FORMAT_BUF(fmt,buf)		\
	va_list	argptr;				\
	va_start(argptr, fmt);		\
	char msg[4096];				\
	vsnprintf(ARRAY_ARG(buf), fmt, argptr); \
	va_end(argptr);


void DrawTextLeft(const char *text, ...)
{
	int w, h;
	if (nextLeft_y >= winHeight) return;		// out of screen
	FORMAT_BUF(text, msg);
	GetTextExtents(msg, w, h);
	if (nextLeft_y + h >= 0)
		DrawTextPos(LEFT_BORDER, nextLeft_y, msg);
	nextLeft_y += h;
}


void DrawTextRight(const char *text, ...)
{
	int w, h;
	if (nextRight_y >= winHeight) return;		// out of screen
	FORMAT_BUF(text, msg);
	GetTextExtents(msg, w, h);
	if (nextRight_y + h >= 0)
		DrawTextPos(winWidth - RIGHT_BORDER - w, nextRight_y, msg);
	nextRight_y += h;
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


void DrawText3D(const CVec3 &pos, const char *text, ...)
{
	int coords[2];
	if (!ProjectToScreen(pos, coords)) return;
	FORMAT_BUF(text, msg);
	DrawTextPos(coords[0], coords[1], msg);
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

	// proform horizontal blurring
	FB[1].Use();
	BloomPassShader.Use();
	BloomPassShader.SetUniform("Tex", 0);
	BloomPassShader.SetUniform("Step", 1.0f / width, 0.0f);
	FB[0].Flush();

	// proform vertical blurring
	FB[0].Use();
	BloomPassShader.SetUniform("Step", 0.0f, 1.0f / height);
	FB[1].Flush();

	// final blend
	CFramebuffer::Unset();
	Set2Dmode(true);					//?? do it automatically in CFramebuffer::Unset() ?
//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	BloomBlendShader.Use();
	BloomBlendShader.SetUniform("BlurTex", 0);
	BloomBlendShader.SetUniform("OrigTex", 1);

	glActiveTexture(GL_TEXTURE1);
	FBO.BindTexture();					// original screen -> TMU1
	glActiveTexture(GL_TEXTURE0);
	FB[0].Flush();						// render with bloom component as TMU0

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
	if (GL_SUPPORT(QGL_2_0)) CShader::Unset();
	// clear screen buffer
#if FUNNY_BACKGROUND
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	Set2Dmode();
	glBegin(GL_QUADS);
	glColor4f(CLEAR_COLOR);
	glVertex2f(0, 0);
	glVertex2f(winWidth, 0);
	glColor4f(CLEAR_COLOR2);
	glVertex2f(winWidth, winHeight);
	glVertex2f(0, winHeight);
	glEnd();
	glClear(GL_DEPTH_BUFFER_BIT);
#else
	glClearColor(CLEAR_COLOR);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif // FUNNY_BACKGROUND
}

static void Display()
{
	guard(Display);

	GCurrentFrame++;

#if USE_BLOOM
	bool useBloom = ( GL_SUPPORT(QGL_EXT_FRAMEBUFFER_OBJECT|QGL_2_0) == (QGL_EXT_FRAMEBUFFER_OBJECT|QGL_2_0) );
	if (GDisableFBO) useBloom = false;
	static CFramebuffer FBO(true, GL_SUPPORT(QGL_ARB_TEXTURE_FLOAT));
	if (useBloom) PostEffectPrepare(FBO);
#endif // USE_BLOOM

	DrawBackground();

	// 3D drawings
	BuildMatrices();
	Set3Dmode();

	// enable lighting
	static const float lightPos[4]      = {1000, 2000, 2000, 0};
	static const float lightAmbient[4]  = {0.1, 0.1, 0.15, 1};
	static const float specIntens[4]    = {0.7, 0.7, 0.5,  0};
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
	glMaterialf (GL_FRONT, GL_SHININESS, 20);
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
	AppDrawFrame();

	// restore draw state
	BindDefaultMaterial(true);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// disable lighting
	glColor3f(1, 1, 1);
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);

#if USE_BLOOM
	if (useBloom) BloomScene(FBO);
#endif // USE_BLOOM

	// 2D drawings
	Set2Dmode();

	// display help when needed
	if (isHelpVisible)
	{
		DrawTextLeft(S_RED"Help:\n-----\n"S_WHITE
					"Esc         exit\n"
					"H           toggle help\n"
					"LeftMouse   rotate view\n"
					"RightMouse  zoom view\n"
					"MiddleMouse move camera\n"
					"R           reset view");
	}
	AppDisplayTexts(isHelpVisible);
	FlushTexts();

	SDL_GL_SwapBuffers();

	unguard;
}


static bool RequestingQuit = false;

static void OnKeyboard(unsigned key, unsigned mod)
{
	key = tolower(key);

	if (mod & KMOD_CTRL)
		key |= KEY_CTRL;
	else if (mod & KMOD_SHIFT)
		key |= KEY_SHIFT;
	else if (mod & KMOD_ALT)
		key |= KEY_ALT;

	switch (key)
	{
	case SDLK_ESCAPE:
		RequestingQuit = true;
		break;
	case 'h':
		isHelpVisible = !isHelpVisible;
		break;
	case 'r':
		ResetView();
		break;
	case SPEC_KEY(PAGEUP)|KEY_CTRL:
		textOffset += TEXT_SCROLL_LINES;
		if (textOffset > 0) textOffset = 0;
		break;
	case SPEC_KEY(PAGEDOWN)|KEY_CTRL:
		textOffset -= TEXT_SCROLL_LINES;
		break;
#if LIGHTING_MODES
	case 'l'|KEY_CTRL:
		if (++lightingMode == LIGHTING_LAST) lightingMode = 0;
		break;
#endif
	case 'g'|KEY_CTRL:
		{
			// enable/disable extensions
			static unsigned extensionMask = 0;
			BindDefaultMaterial(true);
			if (GL_SUPPORT(QGL_2_0)) glUseProgram(0);
			Exchange(gl_config.extensionMask, extensionMask);
		}
		break;
#if DUMP_TEXTS
	case 'd'|KEY_CTRL:
		dumpTexts = true;
		break;
#endif
	default:
		AppKeyEvent(key);
	}
}


//-----------------------------------------------------------------------------
// Main function
//-----------------------------------------------------------------------------

#if SMART_RESIZE
static int OnEvent(const SDL_Event *evt)
{
	// Solution is from this topic:
	// http://www.gamedev.net/community/forums/topic.asp?topic_id=428022
	if (evt->type == SDL_VIDEORESIZE)
	{
		ResizeWindow(evt->resize.w, evt->resize.h);
		Display();
		return 0;	// drop this event (already handled)
	}
	return 1;		// add event to queue
}
#endif // SMART_RESIZE

void VisualizerLoop(const char *caption)
{
	guard(VisualizerLoop);

	Init(caption);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#if SMART_RESIZE
	SDL_SetEventFilter(&OnEvent);
#endif
	ResetView();
	// main loop
	SDL_Event evt;
	while (!RequestingQuit)
	{
		while (SDL_PollEvent(&evt))
		{
			switch (evt.type)
			{
			case SDL_KEYDOWN:
				OnKeyboard(evt.key.keysym.sym, evt.key.keysym.mod);
				break;
#if !SMART_RESIZE
			case SDL_VIDEORESIZE:
				ResizeWindow(evt.resize.w, evt.resize.h);
				break;
#endif
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
			}
		}
		Display();
	}
	// shutdown
	Shutdown();

	unguard;
}

#endif // RENDERING
