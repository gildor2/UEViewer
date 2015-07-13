//#define VALIDATE_SHADERS	1
//#define DUMP_SHADERS		1
#define FILTER_GLSL_SPAM	1			// ATI and Intel drivers has messages even when no errors/warnings in GLSL shaders

#define GLSLANG_DLL			"glslang.dll"

#define FORCE_GLSL_VERSION	120


#if VALIDATE_SHADERS
#	if _WIN32
#		include <windows.h>
#		include <glslang/Public/ShaderLang.h>
#	else
#		undef VALIDATE_SHADERS
#	endif
#endif


#include "Core.h"
#include "CoreGL.h"
//#include "GlWindow.h"		// for DrawTextRight()


#if RENDERING


bool GUseGLSL;


void GL_CheckGLSL()
{
	GUseGLSL = false;

	if (!GL_SUPPORT(QGL_2_0)) return;

#ifdef FORCE_GLSL_VERSION
	char *Version = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	if (!Version) return;

	int VerMajor = atoi(Version);
	int VerMinor = 0;
	if (char *s = strchr(Version, '.'))
	{
		VerMinor = atoi(s+1);
	}
	int CombinedVer = VerMajor * 100 + VerMinor;
//	printf("GL: %s (%d:%d) -- %d %d\n", Version, VerMajor, VerMinor, CombinedVer, FORCE_GLSL_VERSION);
	if (CombinedVer < FORCE_GLSL_VERSION) return;	// GLSL version is too low
#endif // FORCE_GLSL_VERSION

	GUseGLSL = true;
}


void GL_CheckError(const char *msg)
{
	GLenum err = glGetError();
	if (!err) return;
	appError("%s error %X", msg ? msg : "OpenGL", err);
}


#if USE_GLSL


/*-----------------------------------------------------------------------------
	Code to refine shader code for generic GLSL compiler compatibility
	(ATI, 3DLabs, may be others)
-----------------------------------------------------------------------------*/

static char *FindTokenEnd(char *token)
{
	while (true)
	{
		char c = tolower(*token);
		if (c == 0) return token;
		if ((c < 'a' || c > 'z') &&
			(c < '0' || c > '9') &&
			c != '_')
			return token;
		token++;
	}
}


static char *SkipToken(char *token)
{
	token = FindTokenEnd(token);
	// skip spaces
	while (true)
	{
		char c = *token;
		if (c == 0) return token;
		if (c != ' ' && c != '\t' && c != '\n') break;
		token++;
	}
	return token;
}

static bool CheckToken(char *token, const char *cmp)
{
	const char *end = FindTokenEnd(token);
	int len = end - token;
	if (len != strlen(cmp)) return false;
	return memcmp(token, cmp, len) == 0;
}

// remove vertex shader specific code from pixel shader and vice versa
static void RefineShader(char *buffer, bool isFragShader)
{
	guard(RefineShader);

	char *dst = buffer;

	int ifLevel    = 0;
	int braceLevel = 0;
	const char *dropFunc = isFragShader ? "VertexShaderMain" : "PixelShaderMain";

	while (*buffer)
	{
		if (*buffer == '#')
		{
			// preprocessor
			if (!strncmp(buffer+1, "if", 2))
				ifLevel++;
			else if (!strncmp(buffer+1, "endif", 5))
				ifLevel--;
			goto copy_line;
		}
//		if (ifLevel) goto copy_line;		// keep code inside preprocessor blocks ??
		// skip "attribute" declarations in fragment shader
		if (isFragShader && CheckToken(buffer, "attribute")) goto skip_c_line;
		if (!ifLevel)
		{
			if (CheckToken(buffer, dropFunc)) goto skip_c_block;
			char *nextTok = SkipToken(buffer);
			if (nextTok && CheckToken(nextTok, dropFunc)) goto skip_c_block;
		}
		goto copy_line;

	copy_line:		// copy until end of line
		while (*buffer)
		{
			char c = *buffer++;
			*dst++ = c;
			if (c == '\n' || c == 0) break;
			if (c == '{') braceLevel++;
			if (c == '}') braceLevel--;
		}
		continue;

/*	skip_line:		// skip until end of line
		buffer = strchr(buffer, '\n');
		if (!buffer) break;
		buffer++;
		*dst++ = '\n';
		continue; */

	skip_c_line:	// skip until C line delimiter (;)
		while (true)
		{
			char c = *buffer++;
			if (c == ';')
				goto next;
			else if (c == '\n')		// keep line count
			{
				*dst++ = '\n';
				continue;
			}
			else if (c == 0)
			{
				buffer--;			// keep pointer to NULL char
				break;
			}
		}
		continue;

	skip_c_block:	// skip {...} construction
		while (true)
		{
			char c = *buffer++;
			if (c == '}')
			{
				if (--braceLevel == 0) break;
			}
			else if (c == '{')
				braceLevel++;
			else if (c == '\n')		// keep line count
			{
				*dst++ = '\n';
				continue;
			}
			else if (c == 0)
			{
				buffer--;			// keep pointer to NULL char
				break;
			}
		}
		continue;

	next:
		continue;
	}
	*dst = 0;

	unguard;
}


/*-----------------------------------------------------------------------------
	Validate GLSL code with generic GLSL compiler
-----------------------------------------------------------------------------*/

#if VALIDATE_SHADERS

static HMODULE glslangDll     = NULL;
static bool    glslangMissing = false;


struct ShFuncs
{
	int	(*ShInitialize) ();
	void (*ShDestruct) (ShHandle);
	const char* (*ShGetInfoLog) (const ShHandle);
	ShHandle (*ShConstructCompiler) (const EShLanguage, int debugOptions);
	int (*ShCompile) (const ShHandle, const char* const shaderStrings[], const int numStrings,
		const EShOptimizationLevel, const TBuiltInResource *resources, int debugOptions);
};
static ShFuncs SH;

typedef void (APIENTRY * dummyFunc_t) ();
struct ShDummy_t {
	dummyFunc_t funcs[1];
};
#define ShFunc(struc,index)		reinterpret_cast<ShDummy_t&>(struc).funcs[index]

static const char *ShNames[] =
{
	"ShInitialize",
	"ShDestruct",
	"ShGetInfoLog",
	"ShConstructCompiler",
	"ShCompile"
};


static bool LoadGlslang()
{
	guard(LoadGlslang);

	if (glslangDll || glslangMissing)
		return glslangDll != NULL;

	glslangDll = LoadLibrary(GLSLANG_DLL);
	if (!glslangDll)
	{
		appPrintf("%s is not found\n", GLSLANG_DLL);
		glslangMissing = true;
		return false;
	}

	for (int i = 0; i < ARRAY_COUNT(ShNames); i++)
	{
		dummyFunc_t func = (dummyFunc_t) (GetProcAddress(glslangDll, ShNames[i]));
		ShFunc(SH, i) = func;
		if (!func)
		{
			appNotify("%s: missing import %s", GLSLANG_DLL, ShNames[i]);
			glslangMissing = true;
			FreeLibrary(glslangDll);
			glslangDll = NULL;
			return false;
		}
	}

	return true;

	unguard;
}


static void GlslangValidate(const char *name, const char *source, bool isFragShader)
{
	guard(GlslangValidate);

	if (!LoadGlslang()) return;

	EShLanguage language = isFragShader ? EShLangFragment : EShLangVertex;
	SH.ShInitialize();
	ShHandle compiler = SH.ShConstructCompiler(language, 0);
	if (!compiler)
	{
		appNotify("Unable to construct GLSL compiler");
		glslangMissing = true;
		return;
	}
	static const TBuiltInResource resources =
	{
		32,		// maxLights
		6,		// maxClipPlanes
		32,		// maxTextureUnits
		32,		// maxTextureCoords
		64,		// maxVertexAttribs
		4096,	// maxVertexUniformComponents
		64,		// maxVaryingFloats
		32,		// maxVertexTextureImageUnits
		32,		// maxCombinedTextureImageUnits
		32,		// maxTextureImageUnits
		4096,	// maxFragmentUniformComponents
		32		// maxDrawBuffers
	};
	if (!SH.ShCompile(compiler, &source, 1, EShOptNone, &resources, EDebugOpNone))
	{
		appNotify("Error in %s shader %s:\n%s\n",
			isFragShader ? "fragment" : "vertex", name,
			SH.ShGetInfoLog(compiler));
//		exit(0);
	}
	SH.ShDestruct(compiler);

	unguard;
}


#endif // VALIDATE_SHADERS


/*-----------------------------------------------------------------------------
	Shader core functions
-----------------------------------------------------------------------------*/

const CShader *GCurrentShader = NULL;		//?? change

static void CheckShader(GLuint obj, const char *type, const char *name)
{
	// check compilation status
	GLint status;
	glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
	// get log length
	int infologLength = 0;
	glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

	if (infologLength <= 1) return;	// NULL-terninated string

	char *infoLog = new char[infologLength];
	GLint charsWritten = 0;
	glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);

#if FILTER_GLSL_SPAM
	if (status)
	{
		static const char *checks[] = {
			"shader was successfully compiled",		// ATI
			"no errors"								// Intel
		};
		for (int i = 0; i < ARRAY_COUNT(checks); i++)
			if (appStristr(infoLog, checks[i])) return;
	}
#endif // FILTER_GLSL_SPAM

	if (status)
		appPrintf("WARNING: %s shader %s:\n%s\n", type, name, infoLog);
	else
		appNotify("ERROR: %s shader %s:\n%s", type, name, infoLog);

	delete infoLog;

	if (status != GL_TRUE) appError("error in %s shader %s", type, name);
}

static void CheckProgram(GLuint obj, const char *name)
{
	// check compilation status
	GLint status;
	glGetProgramiv(obj, GL_LINK_STATUS, &status);
	// get log length
	int infologLength = 0;
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

	if (infologLength <= 1) return;	// NULL-terninated string

	char *infoLog = new char[infologLength];
	GLint charsWritten = 0;
	glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);

#if FILTER_GLSL_SPAM
	if (status)
	{
		static const char *checks[] = {
			"fragment shader(s) linked",			// ATI
			"vertex shader(s) linked",				// ATI
			"no errors"								// Intel
		};
		for (int i = 0; i < ARRAY_COUNT(checks); i++)
			if (appStristr(infoLog, checks[i])) return;
	}
#endif // FILTER_GLSL_SPAM

	appPrintf("%s: program %s:\n", (status == GL_TRUE) ? "WARNING" : "ERROR", name);
	appPrintf("%s\n", infoLog);

	delete infoLog;

	if (status != GL_TRUE) appError("error in program %s", name);
}


static int SubstParams(char *dst, const char *src, const char **subst)
{
	guard(SubstParams);

	char *d = dst;
	int substIndex = 0;
	int lineNum = 1;
	while (true)
	{
		char c = *src++;
		if (c == 0) break;		// end of text
		if (c != '%')
		{
			*d++ = c;
			if (c == '\n') lineNum++;
			continue;
		}
		// here: % char
		c = *src++;
		assert(c == 's');		// only %s is supported
		if (!subst)
			appError("Found %%s but no subst passed");
		const char *sub = subst[substIndex++];
		if (!sub)
			appError("Wrong subst count");
#if 1
		appSprintf(d, 16384 /*!!*/, "\n#line 0 %d\n%s\n#line %d 0\n", substIndex, sub, lineNum-1);
//		appPrintf("subst %d at line %d\n", substIndex, lineNum);
#else
		strcpy(d, sub);
#endif
		d = strchr(d, 0);
	}
	if (subst && subst[substIndex] != NULL)
		appError("Wrong subst count");
	*d = 0;
	return d - dst;

	unguard;
}


static void CompileShader(GLuint shader, const char *src, const char *defines, const char **subst, bool isFragShader)
{
	guard(CompileShader);

	const char *name = src;
	const char *type = isFragShader ? "fragment" : "vertex";
	src = strchr(src, 0) + 1;

	// prepare source
	char buffer[16384];
	int srcLen = strlen(src);
	int defLen = defines ? strlen(defines) : 0;
	assert(defLen + srcLen + 512 < ARRAY_COUNT(buffer));

	char *dst = buffer;

#ifdef FORCE_GLSL_VERSION
	strcpy(dst, "#version " STR(FORCE_GLSL_VERSION) "\n");
	dst = strchr(dst, 0);
#endif // FORCE_GLSL_VERSION

	if (defines)
	{
		memcpy(dst, defines, defLen);
		dst += defLen;
		// append "#line 0" to keep correct line numbering
		strcpy(dst, "\n#line 0\n");
		dst = strchr(dst, 0);
	}

#if 0
	memcpy(dst, src, srcLen);
	dst += srcLen;
#else
	dst += SubstParams(dst, src, subst);
#endif

	appSprintf(dst, ARRAY_COUNT(buffer) - (dst - buffer),
		"\nvoid main() { %s(); }\n",
		isFragShader ? "PixelShaderMain" : "VertexShaderMain"
	);
#if DUMP_SHADERS
	FILE *f = fopen(va("%s.orig.%s", name, type), "w");
	if (f) { fprintf(f, "%s", buffer); fclose(f); }
#endif
	RefineShader(buffer, isFragShader);
#if DUMP_SHADERS
	f = fopen(va("%s.refined.%s", name, type), "w");
	if (f) { fprintf(f, "%s", buffer); fclose(f); }
#endif

	// compile
	const char *pBuffer = buffer;
	glShaderSource(shader, 1, &pBuffer, NULL);
	glCompileShader(shader);

	// validate
#if VALIDATE_SHADERS
	GlslangValidate(name, buffer, isFragShader);
#endif
	CheckShader(shader, type, name);

	unguard;
}


// Note: src format is "ShaderName" "\0" "ShaderText"
void GL_MakeShader(GLuint &VsObj, GLuint &PsObj, GLuint &PrObj, const char *src, const char *defines, const char **subst)
{
	guard(GL_MakeShader);

	// create objects
	VsObj = glCreateShader(GL_VERTEX_SHADER);
	PsObj = glCreateShader(GL_FRAGMENT_SHADER);
	PrObj = glCreateProgram();
	if (!VsObj || !PsObj || !PrObj) appError("Unable to create shaders");
	// shaders
	CompileShader(VsObj, src, defines, subst, false);
	CompileShader(PsObj, src, defines, subst, true);
	// program
	glAttachShader(PrObj, VsObj);
	glAttachShader(PrObj, PsObj);
	glLinkProgram(PrObj);

	// validate
	CheckProgram(PrObj, src);

	unguardf("%s", src);
}


void CShader::Unset()
{
	GCurrentShader = 0;
	glUseProgram(0);
}

#endif // USE_GLSL


/*-----------------------------------------------------------------------------
	CFramebuffer
-----------------------------------------------------------------------------*/

const CFramebuffer *GCurrentFramebuffer = NULL;		//?? change
bool GDisableFBO = false;

void CFramebuffer::Use()
{
	guard(CFramebuffer::Use);

	if (GDisableFBO) return;
	if (!IsValid()) SetSize(width, height); // refresh
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, FBObj);
	GL_CheckError("BindFramebuffer");
	GCurrentFramebuffer = this;
	SetViewport();

	unguard;
}

void CFramebuffer::Release()
{
	if (GL_IsValidObject(ColorTex, Timestamp))
		glDeleteTextures(1, &ColorTex);
	if (GL_IsValidObject(DepthTex, Timestamp))
		glDeleteTextures(1, &DepthTex);
	if (GL_IsValidObject(FBObj, Timestamp))
		glDeleteFramebuffersEXT(1, &FBObj);
	ColorTex = DepthTex = FBObj = 0;
}

void CFramebuffer::SetSize(int winWidth, int winHeight)
{
	guard(CFramebuffer::SetSize);

	if (IsValid() && winWidth == width && winHeight == height) return;
	if (GDisableFBO) return;
	width  = winWidth;
	height = winHeight;

	GL_ResetError();
	Release();

	// create color texture
	glGenTextures(1, &ColorTex);
	glBindTexture(GL_TEXTURE_2D, ColorTex);
	GLenum internalFormat = fpFormat ? GL_RGBA16F_ARB : GL_RGBA8;
	GLenum type           = fpFormat ? GL_HALF_FLOAT_ARB : GL_UNSIGNED_BYTE;
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, type, NULL);
	GL_CheckError("ColorTexImage");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// create depth renderbuffer
	if (hasDepth)
	{
		glGenTextures(1, &DepthTex);
		glBindTexture(GL_TEXTURE_2D, DepthTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_INT, NULL);
		GL_CheckError("DepthTexImage");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	// create frame buffer object
	glGenFramebuffersEXT(1, &FBObj);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, FBObj);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ColorTex, 0);
	GL_CheckError("SetColorTexture");
	if (hasDepth)
	{
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, DepthTex, 0);
		GL_CheckError("SetDepthTexture");
	}

	// check frame buffer status
	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	switch (status)
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
		break;		// ok
	default:
		appNotify("OpenGL error: FBO error=%X. Framebuffers are disabled", status);
		GDisableFBO = true;
		return;
	}

	// done ...
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	GL_TouchObject(Timestamp);

	unguardf("size=%dx%d", winWidth, winHeight);
}


void CFramebuffer::Flush()
{
	glEnable(GL_TEXTURE_2D);
	BindTexture();

	// setup viewport as (0,1)-(0,1)
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, 0, 1);

	// note: texture coords are identical to vertex2f
	glBegin(GL_QUADS);
	glTexCoord2f(1, 0); glVertex2f(1, 0);
	glTexCoord2f(0, 0); glVertex2f(0, 0);
	glTexCoord2f(0, 1); glVertex2f(0, 1);
	glTexCoord2f(1, 1); glVertex2f(1, 1);
	glEnd();

	glPopMatrix();

//	DrawTextRight("Flush: %d,%d", winWidth, winHeight);
}


void CFramebuffer::Unset()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	GCurrentFramebuffer = NULL;
}

#endif // RENDERING
