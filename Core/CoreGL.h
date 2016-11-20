#ifndef __CORE_GL_H__
#define __CORE_GL_H__

#define USE_GLSL			1			//?? move to Build.h ?

#include "Win32Types.h"

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

#define GL_SUPPORT(ext)	( (gl_config.extensionMask & (ext)) != 0 )

extern bool GUseGLSL;

void GL_CheckGLSL();
void GL_CheckError(const char *msg = NULL);

inline void GL_ResetError()
{
	glGetError();
}


/*-----------------------------------------------------------------------------
	Timestamp
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
//?? should be "const char **subst" ?
void GL_MakeShader(GLuint &VsObj, GLuint &PsObj, GLuint &PrObj, const char *src, const char *defines = NULL, const char **subst = NULL);

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
		if (!IsValid() || !GUseGLSL) return;
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
	inline void Make(const char *src, const char *defines = NULL, const char **subst = NULL)
	{
		GL_MakeShader(VsObj, PsObj, PrObj, src, defines, subst);
	}

	inline GLuint Use()
	{
		glUseProgram(PrObj);
		GL_TouchObject(Timestamp);
		GCurrentShader = this;
		return PrObj;
	}
	static void Unset();

	// uniform assignment
	inline bool SetUniform(const char *name, int value) const
	{
		GLint u = glGetUniformLocation(PrObj, name);
		if (u == -1) return false;
		glUniform1i(u, value);
		return true;
	}
	inline bool SetUniform(const char *name, float value) const
	{
		GLint u = glGetUniformLocation(PrObj, name);
		if (u == -1) return false;
		glUniform1f(u, value);
		return true;
	}
	inline bool SetUniform(const char *name, float value1, float value2) const
	{
		GLint u = glGetUniformLocation(PrObj, name);
		if (u == -1) return false;
		glUniform2f(u, value1, value2);
		return true;
	}
	inline bool SetUniform(const char *name, const CVec3& value) const
	{
		GLint u = glGetUniformLocation(PrObj, name);
		if (u == -1) return false;
		glUniform3fv(u, 1, value.v);
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


/*-----------------------------------------------------------------------------
	Frame Buffer Object
-----------------------------------------------------------------------------*/

extern const class CFramebuffer *GCurrentFramebuffer;		//?? change
extern bool GDisableFBO;

class CFramebuffer
{
public:
	CFramebuffer(bool UseDepth = false, bool useFp = false)
	:	FBObj(0)
	,	ColorTex(0)
	,	DepthTex(0)
	,	width(0)
	,	height(0)
	,	Timestamp(0)
	,	hasDepth(UseDepth)
	,	fpFormat(useFp)
	{}

	void SetSize(int winWidth, int winHeight);
	void Release();

	inline bool IsValid() const
	{
		return GL_IsValidObject(FBObj, Timestamp);
	}
	inline int Width() const
	{
		return width;
	}
	inline int Height() const
	{
		return height;
	}

	void Use();				//?? rename to Bind()
	static void Unset();

	inline void SetViewport() const
	{
		glViewport(0, 0, width, height);
		glScissor (0, 0, width, height);
	}

	inline void BindTexture() const
	{
		glBindTexture(GL_TEXTURE_2D, ColorTex);
	}
	inline void BindDepthTexture() const
	{
		glBindTexture(GL_TEXTURE_2D, DepthTex);
	}

	void Flush();			//?? rename to Draw()

protected:
	int			Timestamp;
	GLuint		FBObj;
	GLuint		ColorTex;
	GLuint		DepthTex;
	int			width, height;
	bool		hasDepth;
	bool		fpFormat;
};

#endif // USE_GLSL

#endif // __CORE_GL_H__
