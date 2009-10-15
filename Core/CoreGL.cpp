#include "Core.h"
#include "CoreGL.h"

void GL_CheckError()
{
	GLenum err = glGetError();
	if (!err) return;
	appError("OpenGL error %X", err);
}


#if USE_GLSL

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

	printf("%s: %s shader %s:\n", (status == GL_TRUE) ? "WARNING" : "ERROR", type, name);
	printf("%s\n",infoLog);

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

	printf("%s: program %s:\n", (status == GL_TRUE) ? "WARNING" : "ERROR", name);
	printf("%s\n",infoLog);

	delete infoLog;

	if (status != GL_TRUE) appError("error in program %s", name);
}


static void CompileShader(GLuint shader, const char *src, const char *defines, bool isFragShader)
{
	guard(CompileShader);

	const char *name = src;
	src = strchr(src, 0) + 1;

	// prepare source
	char buffer[16384];
	int srcLen = strlen(src);
	int defLen = defines ? strlen(defines) : 0;
	assert(defLen + srcLen + 512 < ARRAY_COUNT(buffer));

	char *dst = buffer;
	if (defines)
	{
		memcpy(dst, defines, defLen);
		dst += defLen;
		if (!isFragShader)	//?? do we need this ?
		{
			strcpy(dst, "\n#define VERTEX_SHADER 1\n");
			dst = strchr(dst, 0);
		}
		// append "#line 0" to keep correct line numbering
		strcpy(dst, "\n#line 0\n");
		dst = strchr(dst, 0);
	}

	memcpy(dst, src, srcLen);
	dst += srcLen;

	appSprintf(dst, ARRAY_COUNT(buffer) - srcLen,
		"\nvoid main() { %s(); }\n",
		isFragShader ? "PixelShaderMain" : "VertexShaderMain"
	);

	// compile
	const char *pBuffer = buffer;
	glShaderSource(shader, 1, &pBuffer, NULL);
	glCompileShader(shader);

	// validate
	CheckShader(shader, isFragShader ? "fragment" : "vertex", name);

	unguard;
}


// Note: src format is "ShaderName" "\0" "ShaderText"
void GL_MakeShader(GLuint &VsObj, GLuint &PsObj, GLuint &PrObj, const char *src, const char *defines)
{
	guard(GL_MakeShader);

	// create objects
	VsObj = glCreateShader(GL_VERTEX_SHADER);
	PsObj = glCreateShader(GL_FRAGMENT_SHADER);
	PrObj = glCreateProgram();
	if (!VsObj || !PsObj || !PrObj) appError("Unable to create shaders");
	// shaders
	CompileShader(VsObj, src, defines, false);
	CompileShader(PsObj, src, defines, true);
	// program
	glAttachShader(PrObj, VsObj);
	glAttachShader(PrObj, PsObj);
	glLinkProgram(PrObj);

	// validate
	CheckProgram(PrObj, src);

	unguardf(("%s", src));
}


#endif // USE_GLSL
