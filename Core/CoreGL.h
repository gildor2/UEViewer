#if _WIN32
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
