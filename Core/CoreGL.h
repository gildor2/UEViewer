#define USE_GLSL			1			//?? move to Build.h ?


#if _WIN32
// avoid include of windows.h
#	ifndef APIENTRY
#		define APIENTRY __stdcall
#	endif
#	ifndef WINAPI
#		define WINAPI   __stdcall
#	endif
#	ifndef WINGDIAPI
#		define WINGDIAPI
		typedef unsigned		HDC;
		typedef unsigned		HGLRC;
		typedef const char *	LPCSTR;
		typedef int				BOOL;
		typedef unsigned char	BYTE;
		typedef unsigned short	WORD;
		typedef unsigned int	UINT;
		typedef int (APIENTRY *PROC)();
		typedef void PIXELFORMATDESCRIPTOR;		// structure
		typedef PIXELFORMATDESCRIPTOR * LPPIXELFORMATDESCRIPTOR;
#	endif // WINGDIAPI
#	ifndef CONST
#		define CONST const
#	endif
#endif // _WIN32

#include <GL/gl.h>
#include <GL/glext.h>

#define USE_SDL			1
#define NO_GL_LOG		1
#include "GLBind.h"

bool	QGL_Init(const char *libName);
void	QGL_Shutdown();
void	QGL_InitExtensions();

struct gl_config_t
{
	unsigned	extensionMask;
	unsigned	disabledExt;
	unsigned	ignoredExt;
	const char *extensions;
	const char *extensions2;
};

extern gl_config_t gl_config;

#define GL_SUPPORT(ext)	(gl_config.extensionMask & (ext))


void GL_CheckError();


/*-----------------------------------------------------------------------------
	Timestamps
-----------------------------------------------------------------------------*/

extern int GCurrentFrame;		// current rendering frame number
extern int GContextFrame;		// frame number when GL context was (re)created

inline bool GL_IsValidObject(unsigned handle, int timestamp)
{
	if (!handle) return false;
	return timestamp >= GContextFrame;
}

inline bool GL_TouchObject(int &timestamp)
{
	bool result = timestamp >= GContextFrame;
	timestamp = GCurrentFrame;
	return result;
}


/*-----------------------------------------------------------------------------
	Shaders
-----------------------------------------------------------------------------*/

#if USE_GLSL

// Note: src format is "ShaderName" "\0" "ShaderText"
void GL_MakeShader(GLuint &VsObj, GLuint &PsObj, GLuint &PrObj, const char *src, const char *defines = NULL);

extern const class CShader *GCurrentShader;		//?? change


class CShader
{
public:
	// constructor
	CShader()
	:	VsObj(0)
	,	PsObj(0)
	,	PrObj(0)
	,	Timestamp(0)
	{}
	// release
	~CShader()
	{
		Release();
	}
	void Release()
	{
		if (!IsValid() || !GL_SUPPORT(QGL_2_0)) return;
		glDetachShader(PrObj, VsObj);
		glDetachShader(PrObj, PsObj);
		glDeleteShader(VsObj);
		glDeleteShader(PsObj);
		glDeleteProgram(PrObj);
		VsObj = PsObj = PrObj = 0;
	}
	// management
	inline bool IsValid() const
	{
		return GL_IsValidObject(PrObj, Timestamp);
	}
	inline void Make(const char *src, const char *defines = NULL)
	{
		GL_MakeShader(VsObj, PsObj, PrObj, src, defines);
	}
	inline GLuint Use()
	{
		glUseProgram(PrObj);
		GL_TouchObject(Timestamp);
		GCurrentShader = this;
		return PrObj;
	}
	// uniform assignment
	inline bool SetUniform(const char *name, int value) const
	{
		GLint u = glGetUniformLocation(PrObj, name);
		if (u == -1) return false;
		glUniform1i(u, value);
		return true;
	}
	// attributes
	inline GLint GetAttrib(const char *name) const
	{
		return glGetAttribLocation(PrObj, name);
	}
	inline void SetAttrib(GLint attrib, const CVec3 &value) const
	{
		glVertexAttrib3fv(attrib, value.v);	//?? don't need as method
	}

protected:
	int			Timestamp;
	GLuint		VsObj;
	GLuint		PsObj;
	GLuint		PrObj;
};


/*??
 * move outside - implemented in UnRenderer.cpp
 */

enum GenericShaderType
{
	GS_Textured,
	GS_White,
	GS_NormalMap,
	GS_Count
};

void BindDefaultMaterial(bool White = false);
const CShader &GL_UseGenericShader(GenericShaderType type);


#endif // USE_GLSL
