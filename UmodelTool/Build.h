#define DO_GUARD		1
//#define MAX_DEBUG		1		// Maximal debugging level
//#define DEBUG_MEMORY	1
#define RENDERING		1
#define THREADING		1
#define PROFILE			1
#define DECLARE_VIEWER_PROPS	1
//#define VSTUDIO_INTEGRATION		1	// improved debugging with Visual Studio

//#define PRIVATE_BUILD	1

#include "GameDefines.h"

#ifdef __APPLE__
#undef RENDERING //todo?
#undef THREADING //todo?
#endif

// some private games
#if PRIVATE_BUILD
//-- none
#endif
