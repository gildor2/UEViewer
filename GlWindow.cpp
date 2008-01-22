#include "Core.h"
#include "TextContainer.h"
#include "GlWindow.h"

// font
#include "GlFont.h"

#define CHARS_PER_LINE	(TEX_WIDTH/CHAR_WIDTH)

//!! remove, use dynamic link
#pragma comment(lib, "opengl32.lib")

//-----------------------------------------------------------------------------
// State variables
//-----------------------------------------------------------------------------

static bool  isHelpVisible = false;
static float frameTime;


//-----------------------------------------------------------------------------
// GL support functions
//-----------------------------------------------------------------------------

#define DEFAULT_DIST	256
#define MAX_DIST		2048
#define CLEAR_COLOR		0.2, 0.3, 0.3, 0

#define FONT_TEX_NUM	1
#define TEXT_LEFT		4
#define TEXT_TOP		4

namespace GL
{
	bool  is2Dmode = false;
	// view params (const)
	float zNear = 4;			// near clipping plane
	float zFar  = 4096;			// far clipping plane
	float yFov  = 80;
	float tFovX, tFovY;			// tan(fov_x|y)
	bool  invertXAxis = false;
	// window size
	int   width  = 800;
	int   height = 600;
	// text output position
	int   textX, textY;
	// matrices
	float projectionMatrix[4][4];
	float modelMatrix[4][4];
	// mouse state
	int   mouseButtons;			// bit mask: left=1, middle=2, right=4, wheel up=8, wheel down=16
	// view state
	CVec3 viewAngles;
	float viewDist   = DEFAULT_DIST;
	CVec3 viewOrigin = { -DEFAULT_DIST, 0, 0 };
	float distScale  = 1;
	CVec3 rotOrigin  = {0, 0, 0};
	CVec3 viewOffset = {0, 0, 0};
	CAxis viewAxis;				// generated from angles


	//-------------------------------------------------------------------------
	// Switch 2D/3D rendering mode
	//-------------------------------------------------------------------------

	void Set2Dmode()
	{
		if (is2Dmode) return;
		is2Dmode = true;
		textX = TEXT_LEFT;
		textY = TEXT_TOP;

		glViewport(0, 0, width, height);
		glScissor(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, width, height, 0, 0, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_CULL_FACE);
	}


	void Set3Dmode()
	{
		if (!is2Dmode) return;
		is2Dmode = false;

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(&projectionMatrix[0][0]);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(&modelMatrix[0][0]);
		glViewport(0, 0, width, height);
		glScissor(0, 0, width, height);
		glEnable(GL_CULL_FACE);
//		glCullFace(GL_FRONT);
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

	//-------------------------------------------------------------------------
	// Text output
	//-------------------------------------------------------------------------

	void LoadFont()
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
		glBindTexture(GL_TEXTURE_2D, FONT_TEX_NUM);
		glTexImage2D(GL_TEXTURE_2D, 0, 4, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		delete pic;
	}

	void DrawChar(char c, int color, int textX, int textY)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, FONT_TEX_NUM);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5);

		static const float colorTable[][3] = {
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
			if (s)
				glColor3f(0, 0, 0);
			else
				glColor3fv(colorTable[color]);
			glTexCoord2f(s0, t0);
			glVertex3f(x1+s, y1+s, 0);
			glTexCoord2f(s1, t0);
			glVertex3f(x2+s, y1+s, 0);
			glTexCoord2f(s1, t1);
			glVertex3f(x2+s, y2+s, 0);
			glTexCoord2f(s0, t1);
			glVertex3f(x1+s, y2+s, 0);
		}

		glEnd();
	}

	void text(const char *text)
	{
		int color = 7;

		while (char c = *text++)
		{
			if (c == '\n')
			{
				textX =  TEXT_LEFT;
				textY += CHAR_HEIGHT;
				continue;
			}
			if (c == COLOR_ESCAPE)
			{
				char c2 = *text;
				if (c2 >= '0' && c2 <= '7')
				{
					color = c2 - '0';
					text++;
					continue;
				}
			}
			DrawChar(c, color, textX, textY);
			textX += CHAR_WIDTH;
		}
	}

#define FORMAT_BUF(fmt,buf)		\
	va_list	argptr;				\
	va_start(argptr, fmt);		\
	char msg[4096];				\
	vsnprintf(ARRAY_ARG(buf), fmt, argptr); \
	va_end(argptr);


	void textf(const char *fmt, ...)
	{
		FORMAT_BUF(fmt, msg);
		text(msg);
	}

	//-------------------------------------------------------------------------
	// called when window resized
	void OnResize(int w, int h)
	{
		width  = w;
		height = h;
		SDL_SetVideoMode(width, height, 24, SDL_OPENGL|SDL_RESIZABLE);
		LoadFont();
		// init gl
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
//		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_SCISSOR_TEST);
//		glShadeModel(GL_SMOOTH);
//		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		Set2Dmode();
	}


	//-------------------------------------------------------------------------
	// Mouse control
	//-------------------------------------------------------------------------

	void OnMouseButton(int type, int button)
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
		}
		else if (prevButtons && !mouseButtons)
		{
			SDL_ShowCursor(1);
			SDL_WM_GrabInput(SDL_GRAB_OFF);
		}
	}


	void OnMouseMove(int dx, int dy)
	{
		if (!mouseButtons) return;

		float xDelta = (float)dx / width;
		float yDelta = (float)dy / height;
		if (invertXAxis)
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
			viewDist += yDelta * 400;
		}
		CAxis axis;
		axis.FromEuler(viewAngles);
		if (mouseButtons & SDL_BUTTON(SDL_BUTTON_MIDDLE))
		{
			// pan camera
			VectorMA(rotOrigin, xDelta * viewDist * 2, axis[1]);
			VectorMA(rotOrigin, yDelta * viewDist * 2, axis[2]);
		}
		viewDist = bound(viewDist, 100 * distScale, MAX_DIST * distScale);
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
		if (!invertXAxis)
			viewAxis[1].Negate();
//		DrawTextLeft("origin: %6.1f %6.1f %6.1f", VECTOR_ARG(viewOrigin));
//		DrawTextLeft("angles: %6.1f %6.1f %6.1f", VECTOR_ARG(viewAngles));
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
		tFovX = tFovY / height * width; // tan(xFov * M_PI / 360.0f);
		float zMin = zNear;
		float zMax = zFar;
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
		DrawTextLeft("zFar: %g;  frustum: x[%g, %g] y[%g, %g]", zFar, xMin, xMax, yMin, yMax);
		DrawTextLeft("----- projection matrix -----");
		DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][0], m[1][0], m[2][0], m[3][0]);
		DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][1], m[1][1], m[2][1], m[3][1]);
		DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][2], m[1][2], m[2][2], m[3][2]);
		DrawTextLeft("{%9.4g, %9.4g, %9.4g, %9.4g}", m[0][3], m[1][3], m[2][3], m[3][3]);
#endif
#undef m
	}

	//-------------------------------------------------------------------------
	void Init(const char *caption)
	{
		// init SDL
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
			appError("Failed to initialize SDL");

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

		SDL_WM_SetCaption(caption, caption);
		OnResize(width, height);
	}

	void Shutdown()
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

} // end of GL namespace


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


void ClearTexts()
{
	nextLeft_y = nextRight_y = TOP_TEXT_POS;
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
			GL::DrawChar(c, color, x, y);
			x += CHAR_WIDTH;
		}
		if (!s) return;							// all done

		y += CHAR_HEIGHT;
		text = s + 1;
	}
}


void FlushTexts()
{
	Text.Enumerate(DrawText);
	nextLeft_y = nextRight_y = TOP_TEXT_POS;
	ClearTexts();
}


void DrawTextPos(int x, int y, const char *text)
{
	CRText *rec = Text.Add(text);
	if (!rec) return;
	rec->x = x;
	rec->y = y;
}


void DrawTextLeft(const char *text, ...)
{
	int w, h;
	if (nextLeft_y >= GL::height) return;	// out of screen
	FORMAT_BUF(text, msg);
	GetTextExtents(msg, w, h);
	DrawTextPos(LEFT_BORDER, nextLeft_y, msg);
	nextLeft_y += h;
}


void DrawTextRight(const char *text, ...)
{
	int w, h;
	if (nextRight_y >= GL::height) return;	// out of screen
	FORMAT_BUF(text, msg);
	GetTextExtents(msg, w, h);
	DrawTextPos(GL::width - RIGHT_BORDER - w, nextRight_y, msg);
	nextRight_y += h;
}


// Project 3D point to screen coordinates; return false when not in view frustum
static bool ProjectToScreen(const CVec3 &pos, int scr[2])
{
	CVec3	vec;
	VectorSubtract(pos, GL::viewOrigin, vec);

	float z = dot(vec, GL::viewAxis[0]);
	if (z <= GL::zNear) return false;			// not visible

	float x = dot(vec, GL::viewAxis[1]) / z / GL::tFovX;
	if (x < -1 || x > 1) return false;

	float y = dot(vec, GL::viewAxis[2]) / z / GL::tFovY;
	if (y < -1 || y > 1) return false;

	scr[0] = appRound(/*GL::x + */ GL::width  * (0.5 - x / 2));
	scr[1] = appRound(/*GL::y + */ GL::height * (0.5 - y / 2));

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
// Hook functions
//-----------------------------------------------------------------------------

static void Display()
{
	// clear screen buffer
	glClearColor(CLEAR_COLOR);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 3D drawings
	GL::BuildMatrices();
	GL::Set3Dmode();

	// enable lighting
	static const float lightPos[4]      = {100, 200, 100, 0};
	static const float lightAmbient[4]  = {0.3, 0.3, 0.3, 1};
//	static const float specIntens[4]    = {1, 1, 1, 0};
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);		// allow non-normalized normal arrays
//	glEnable(GL_LIGHTING);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
	glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmbient);
//	glMaterialfv(GL_FRONT, GL_SPECULAR, specIntens);
//	glMaterialf(GL_FRONT, GL_SHININESS, 12);

	// draw scene
	AppDrawFrame();

	// disable lighting
	glColor3f(1, 1, 1);
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);

	// 2D drawings
	GL::Set2Dmode();

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
}


static bool RequestingQuit = false;

static void OnKeyboard(unsigned key)
{
	switch (tolower(key))
	{
	case SDLK_ESCAPE:
		RequestingQuit = true;
		break;
	case 'h':
		isHelpVisible = !isHelpVisible;
		break;
	case 'r':
		GL::ResetView();
		break;
	default:
		AppKeyEvent(key);
	}
}


//-----------------------------------------------------------------------------
// Main function
//-----------------------------------------------------------------------------

void VisualizerLoop(const char *caption)
{
	GL::Init(caption);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	GL::ResetView();
	// main loop
	SDL_Event evt;
	while (!RequestingQuit)
	{
		while (SDL_PollEvent(&evt))
		{
			switch (evt.type)
			{
			case SDL_KEYDOWN:
				OnKeyboard(evt.key.keysym.sym);
				break;
			case SDL_VIDEORESIZE:
				GL::OnResize(evt.resize.w, evt.resize.h);
				break;
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
				GL::OnMouseButton(evt.type, evt.button.button);
				break;
			case SDL_MOUSEMOTION:
				GL::OnMouseMove(evt.motion.xrel, evt.motion.yrel);
				break;
			case SDL_QUIT:
				RequestingQuit = true;
				break;
			}
		}
		Display();
	}
	// shutdown
	GL::Shutdown();
}
