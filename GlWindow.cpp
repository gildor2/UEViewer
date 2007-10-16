#include "Core.h"
#include "GlWindow.h"


//-----------------------------------------------------------------------------
// State variables
//-----------------------------------------------------------------------------

static bool  isHelpVisible = false;
static float frameTime;


//-----------------------------------------------------------------------------
// GL support functions
//-----------------------------------------------------------------------------

#define DEFAULT_DIST	256

namespace GL
{
	bool  is2Dmode = false;
	// view params (const)
	float zNear = 4;			// near clipping plane
	float zFar  = 4096;			// far clipping plane
	float yFov  = 80;
	bool  invertXAxis = false;
	// window size
	int   width  = 800;
	int   height = 600;
	// matrices
	float projectionMatrix[4][4];
	float modelMatrix[4][4];
	// mouse state
	int   mouseButtons;			// bit mask: left=1, middle=2, right=4, wheel up=8, wheel down=16
	int   mouseX, mouseY;
	// view state
	CVec3 viewAngles;
	float viewDist   = DEFAULT_DIST;
	CVec3 viewOrigin = { -DEFAULT_DIST, 0, 0 };
	float distScale  = 1;
	CVec3 viewOffset = {0, 0, 0};
	CAxis viewAxis;				// generated from angles


	//-------------------------------------------------------------------------
	// Switch 2D/3D rendering mode
	//-------------------------------------------------------------------------

	void set2Dmode()
	{
		if (is2Dmode) return;
		is2Dmode = true;

		glViewport(0, 0, width, height);
		glScissor(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, width, height, 0, 0, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_CULL_FACE);
	}


	void set3Dmode()
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

	void text(const char *text, int x, int y)
	{
		if (x >= 0 && y >= 0)
			glRasterPos2i(x, y);
		glutBitmapString(GLUT_BITMAP_8_BY_13, (unsigned char*) text);
	}


	void textf(const char *fmt, ...)
	{
		va_list	argptr;
		va_start(argptr, fmt);
		char msg[4096];
		vsnprintf(ARRAY_ARG(msg), fmt, argptr);
		va_end(argptr);
		text(msg);
	}


	//-------------------------------------------------------------------------
	// called when window resized
	void ReshapeFunc(int w, int h)
	{
		width  = w;
		height = h;
		glViewport(0, 0, w, h);
	}


	//-------------------------------------------------------------------------
	// Mouse control
	//-------------------------------------------------------------------------

	void OnMouseButton(int button, int state, int x, int y)
	{
		int mask = 1 << button;
		if (state == GLUT_DOWN)
			mouseButtons |= mask;
		else
			mouseButtons &= ~mask;
		mouseX = x; mouseY = y;
	}


	void OnMouseMove(int x, int y)
	{
		int dx = x - mouseX;
		int dy = y - mouseY;
		mouseX = x; mouseY = y;
		if (!mouseButtons) return;

		if (mouseButtons & 1)	// left mouse button
		{
			// rotate camera
			float yawDelta = (float)dx / width * 360;
			if (GL::invertXAxis)
				yawDelta = -yawDelta;
			viewAngles[YAW]   -= yawDelta;
			viewAngles[PITCH] -= (float)dy / height * 360;
			// bound angles
			viewAngles[YAW]   = fmod(viewAngles[YAW], 360);
			viewAngles[PITCH] = bound(viewAngles[PITCH], -90, 90);
		}
		if (mouseButtons & 4)	// right mouse button
			viewDist += (float)dy / height * 400;
		viewDist = bound(viewDist, 100 * distScale, 1024 * distScale);
		CVec3 viewDir;
		Euler2Vecs(viewAngles, &viewDir, NULL, NULL);
		VectorScale(viewDir, -viewDist, viewOrigin);
		viewOrigin.Add(viewOffset);
	}


	//-------------------------------------------------------------------------
	// Building modelview and projection matrices
	//-------------------------------------------------------------------------
	void buildMatrices()
	{
		// view angles -> view axis
		Euler2Vecs(viewAngles, &viewAxis[0], &viewAxis[1], &viewAxis[2]);
		if (!invertXAxis)
			viewAxis[1].Negate();
//		textf("origin: %6.1f %6.1f %6.1f\n", VECTOR_ARG(viewOrigin));
//		textf("angles: %6.1f %6.1f %6.1f\n", VECTOR_ARG(viewAngles));
#if 0
		textf("---- view axis ----\n");
		textf("[0]: %g %g %g\n",    VECTOR_ARG(viewAxis[0]));
		textf("[1]: %g %g %g\n",    VECTOR_ARG(viewAxis[1]));
		textf("[2]: %g %g %g\n",    VECTOR_ARG(viewAxis[2]));
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
		textf("----- modelview matrix ------\n");
		for (i = 0; i < 4; i++)
			textf("{%9.4g, %9.4g, %9.4g, %9.4g}\n", m[0][i], m[1][i], m[2][i], m[3][i]);
#undef m
#endif

		// compute projection matrix
		float tFovY = tan(yFov * M_PI / 360.0f);
		float tFovX = tFovY / height * width; // tan(xFov * M_PI / 360.0f);
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
		textf("zFar: %g;  frustum: x[%g, %g] y[%g, %g]\n", zFar, xMin, xMax, yMin, yMax);
		textf("----- projection matrix -----\n");
		textf("{%9.4g, %9.4g, %9.4g, %9.4g}\n", m[0][0], m[1][0], m[2][0], m[3][0]);
		textf("{%9.4g, %9.4g, %9.4g, %9.4g}\n", m[0][1], m[1][1], m[2][1], m[3][1]);
		textf("{%9.4g, %9.4g, %9.4g, %9.4g}\n", m[0][2], m[1][2], m[2][2], m[3][2]);
		textf("{%9.4g, %9.4g, %9.4g, %9.4g}\n", m[0][3], m[1][3], m[2][3], m[3][3]);
#endif
#undef m
	}


	//-------------------------------------------------------------------------
	void init(const char *caption)
	{
		// init glut
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
		glutInitWindowSize(width, height);
//		glutInitWindowPosition(0, 0);
		glutCreateWindow(caption);
		// common hooks
		glutReshapeFunc(ReshapeFunc);
		glutMouseFunc(OnMouseButton);
		glutMotionFunc(OnMouseMove);

		// init gl
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
//		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_SCISSOR_TEST);
//		glShadeModel(GL_SMOOTH);
//		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		set2Dmode();
	}

} // end of GL namespace


//-----------------------------------------------------------------------------
// Hook functions
//-----------------------------------------------------------------------------

static void OnDisplay()
{
	// set default text position
	glRasterPos2i(20, 20);
	// clear screen buffer
	glClearColor(0.2, 0.2, 0.4, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 3D drawings
	GL::buildMatrices();
	GL::set3Dmode();

	// enable lighting
	static const float lightPos[4] = {100, 200, 100, 0};
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHT0);
//	glEnable(GL_LIGHTING);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

	// draw scene
	AppDrawFrame();

	// disable lighting
	glColor3f(1, 1, 1);
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);

	// 2D drawings
	GL::set2Dmode();

	// display help when needed
	if (isHelpVisible)
	{
		GL::text("Help:\n-----\n"
				 "Esc         exit\n"
				 "H           toggle help\n"
				 "LeftMouse   rotate view\n"
				 "RightMouse  move view\n"
				 "R           reset view\n");
	}
	AppDisplayTexts(isHelpVisible);

	glutSwapBuffers();
}


static void OnKeyboard(unsigned char key, int x, int y)
{
	switch (tolower(key))
	{
	case 0x1B:					// Esc
		glutLeaveMainLoop();	// freeglut only; MJK glut callback should use 'exit(0)'
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


static void OnSpecial(int key, int x, int y)
{
	AppKeyEvent(key + 256);
}


// timer handler
static void OnTimer(int timerId)
{
	// display scene
	OnDisplay();
	// and restart timer
	glutTimerFunc(timerId, OnTimer, 10);
}


//-----------------------------------------------------------------------------
// Main function
//-----------------------------------------------------------------------------

void VisualizerLoop(const char *caption)
{
	// create glut window
	char *fakeArgv[2];
	int  fakeArgc = 0;
	glutInit(&fakeArgc, fakeArgv);
	GL::init(caption);
	// application hooks
	glutDisplayFunc(OnDisplay);
	glutKeyboardFunc(OnKeyboard);
	glutSpecialFunc(OnSpecial);
	OnTimer(0);					// init timer
	GL::ResetView();
	// start
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);	// freeglut only
	glutMainLoop();
}
