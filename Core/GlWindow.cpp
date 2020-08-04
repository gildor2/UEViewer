#include "Core.h"

#if RENDERING

#include <SDL2/SDL_syswm.h>
#undef DrawText

#include "GlWindow.h"
#include "CoreGL.h"


#define LIGHTING_MODES			1		// allow switching scene lighting modes with Ctrl+L
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

namespace Viewport
{

Point Size = { 800, 600 };

Point MousePos = { 0, 0 };

// Variables used to store mouse position before switching to relative mode, so
// the position will be restored after releasing mouse buttons.
int MouseButtons = 0;			// bit mask: left=1, middle=2, right=4, wheel up=8, wheel down=16
int MouseButtonsDelta = 0;		// when some bit is set, it indicates that this button was pressed/released

} // namespace Viewport

static float frameTime;
static unsigned lastFrameTime = 0;

static bool  is2Dmode = false;

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


//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

static float distScale  = 1;
bool   vpInvertXAxis = false;


//-----------------------------------------------------------------------------
// Switch 2D/3D rendering mode
//-----------------------------------------------------------------------------

static void Set2Dmode(bool force = false)
{
	// force changes in viewport
	//!! bad looking code
	int width = Viewport::Size.X, height = Viewport::Size.Y;
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
	int width = Viewport::Size.X, height = Viewport::Size.Y;
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


//-----------------------------------------------------------------------------

static SDL_Window* sdlWindow;
static SDL_GLContext sdlContext;

// called when window resized
static void ResizeWindow(int w, int h)
{
	guard(ResizeWindow);

	Viewport::Size.X = w;
	Viewport::Size.Y = h;
	SDL_SetWindowSize(sdlWindow, Viewport::Size.X, Viewport::Size.Y);
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

	PrepareFontTexture();

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

//todo: remove this?
void CApplication::GetWindowSize(int &x, int &y)
{
	x = Viewport::Size.X;
	y = Viewport::Size.Y;
}


void CApplication::ToggleFullscreen()
{
	IsFullscreen = !IsFullscreen;

	if (IsFullscreen)
	{
		SavedWinWidth = Viewport::Size.X;
		SavedWinHeight = Viewport::Size.Y;
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

static void OnMouseButton(int type, int button)
{
	int prevButtons = Viewport::MouseButtons;

	// Update mouse buttons state, catch button state changes
	int mask = SDL_BUTTON(button);
	if (type == SDL_MOUSEBUTTONDOWN)
		Viewport::MouseButtons |= mask;
	else
		Viewport::MouseButtons &= ~mask;

	// Store info about changed button state
	Viewport::MouseButtonsDelta = Viewport::MouseButtons ^ prevButtons;

	// Capture/release the mouse
	if (!prevButtons && Viewport::MouseButtons)
	{
		// Grabbing mouse
		SDL_GetMouseState(&Viewport::MousePos.X, &Viewport::MousePos.Y);
		SDL_SetRelativeMouseMode(SDL_TRUE);		// switch to relative mode - mouse cursor will be hidden and remains in single place
	}
	else if (prevButtons && !Viewport::MouseButtons)
	{
		// Releasing mouse
		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_WarpMouseInWindow(sdlWindow, Viewport::MousePos.X, Viewport::MousePos.Y);
	}
}


static void OnMouseMove(int mx, int my)
{
	if (!Viewport::MouseButtons)
	{
		// Just update mouse position
		SDL_GetMouseState(&Viewport::MousePos.X, &Viewport::MousePos.Y);
		return;
	}

	float xDelta = (float)mx / Viewport::Size.X;
	float yDelta = (float)my / Viewport::Size.Y;
	if (vpInvertXAxis)
		xDelta = -xDelta;

	float YawDelta = 0, PitchDelta = 0, DistDelta = 0, PanX = 0, PanY = 0;

	if (Viewport::MouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT))
	{
		// rotate camera
		YawDelta   = -xDelta * 360;
		PitchDelta =  yDelta * 360;
	}
	if (Viewport::MouseButtons & SDL_BUTTON(SDL_BUTTON_RIGHT))
	{
		// change distance to object
		DistDelta = yDelta * 400 * distScale;
	}
	if (Viewport::MouseButtons & SDL_BUTTON(SDL_BUTTON_MIDDLE))
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
	tFovX = tFovY / Viewport::Size.Y * Viewport::Size.X; // tan(xFov * M_PI / 360.0f);
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
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Viewport::Size.X, Viewport::Size.Y,
		SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
	if (!sdlWindow)
	{
		const char* errorString = SDL_GetError();
		appError("Failed to create SDL window\nError: %s", errorString);
	}
	sdlContext = SDL_GL_CreateContext(sdlWindow);
	#if LIMIT_FPS
	SDL_GL_SetSwapInterval(0);			// FPS will be limited by Sleep()
	#else
	SDL_GL_SetSwapInterval(1);			// allow waiting for vsync to reduce CPU usage
	#endif

	// initialize GL
	ResizeWindow(Viewport::Size.X, Viewport::Size.Y);

//	appPrintf("OpenGL %s / GLSL %s / %s\n",
//		glGetString(GL_VERSION),
//		glGetString(GL_SHADING_LANGUAGE_VERSION),
//		glGetString(GL_VENDOR));

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
// Drawing text at 3D position
//-----------------------------------------------------------------------------

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

	scr[0] = appRound(/*winX + */ Viewport::Size.X * (0.5 - x / 2));
	scr[1] = appRound(/*winY + */ Viewport::Size.Y * (0.5 - y / 2));

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
	FBO.SetSize(Viewport::Size.X, Viewport::Size.Y);
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
	glVertex2f(Viewport::Size.X, 0);
	glVertex2f(0, 0);
	glColor4f(CLEAR_COLOR2);
	glVertex2f(0, Viewport::Size.Y);
	glVertex2f(Viewport::Size.X, Viewport::Size.Y);
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
		DrawKeyHelp("Ctrl+PgUp/PgDn", "scroll this text");
		DrawKeyHelp("Ctrl+MouseWheel", "scroll this text");
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

void CApplication::ProcessKey(unsigned key, bool isDown)
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
		ScrollText(1);
		break;
	case SPEC_KEY(PAGEDOWN)|KEY_CTRL:
	case SDLK_KP_3|KEY_CTRL:
		ScrollText(-1);
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
		GDumpTexts = true;
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
				ProcessKey((unsigned)evt.key.keysym.sym, false);
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
			case SDL_MOUSEWHEEL:
				{
					// Ctrl+Wheel - scroll text
					if (SDL_GetModState() & (KMOD_LCTRL | KMOD_RCTRL))
					{
						if (evt.wheel.y > 0)
						{
							ScrollText(6);
						}
						else if (evt.wheel.y < 0)
						{
							ScrollText(-6);
						}
					}
				}
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

		Viewport::MouseButtonsDelta = 0;		// reset "clicked" state
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
	::ResizeWindow(Viewport::Size.X, Viewport::Size.Y);
}

void CApplication::Exit()
{
	RequestingQuit = true;
}

#endif // RENDERING
