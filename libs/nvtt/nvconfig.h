#ifndef NV_CONFIG
#define NV_CONFIG

//#cmakedefine HAVE_UNISTD_H
#ifndef HAVE_STDARG_H
#define HAVE_STDARG_H 1
#endif
//#cmakedefine HAVE_SIGNAL_H
//#cmakedefine HAVE_EXECINFO_H
#ifndef HAVE_MALLOC_H
#define HAVE_MALLOC_H
#endif

//#cmakedefine HAVE_PNG
//#cmakedefine HAVE_JPEG
//#cmakedefine HAVE_TIFF
//#cmakedefine HAVE_OPENEXR

//#cmakedefine HAVE_MAYA

#if _MSC_VER
#define __restrict		//?? empty; check version?
#endif

#define NV_NO_ASSERT	1
#define UMODEL			1

#endif // NV_CONFIG
