#include "Core.h"

#if USE_OODLE && _WIN32
#include <windows.h>
#endif

#include "UnCore.h"

// includes for package decompression
#include "lzo/lzo1x.h"
#include <zlib.h>

#if SUPPORT_XBOX360

#	if !USE_XDK

#		include "mspack/mspack.h"
#		include "mspack/lzx.h"

#	else // USE_XDK

#		if _WIN32
#		pragma comment(lib, "xcompress.lib")
		extern "C"
		{
			int  __stdcall XMemCreateDecompressionContext(int CodecType, const void* pCodecParams, unsigned Flags, void** pContext);
			void __stdcall XMemDestroyDecompressionContext(void* Context);
			int  __stdcall XMemDecompress(void* Context, void* pDestination, size_t* pDestSize, const void* pSource, size_t SrcSize);
		}
#		else  // _WIN32
#			error XDK build is not supported on this platform
#		endif // _WIN32
#	endif // USE_XDK

#endif // SUPPORT_XBOX360

#if USE_LZ4
#	include "lz4/lz4.h"
#endif

// AES code for UE4
//todo: move outside, not compression-related
#include "rijndael/rijndael.h"

/*-----------------------------------------------------------------------------
	ZLib support
-----------------------------------------------------------------------------*/

// using Core memory manager

extern "C" void *zcalloc(void* opaque, int items, int size)
{
	return appMalloc(items * size);
}

extern "C" void zcfree(void* opaque, void *ptr)
{
	appFree(ptr);
}

// Constants from zutil.c (copy-pasted here to avoid including whole C file)
extern "C" const char * const z_errmsg[10] =
{
	"need dictionary",     /* Z_NEED_DICT       2  */
	"stream end",          /* Z_STREAM_END      1  */
	"",                    /* Z_OK              0  */
	"file error",          /* Z_ERRNO         (-1) */
	"stream error",        /* Z_STREAM_ERROR  (-2) */
	"data error",          /* Z_DATA_ERROR    (-3) */
	"insufficient memory", /* Z_MEM_ERROR     (-4) */
	"buffer error",        /* Z_BUF_ERROR     (-5) */
	"incompatible version",/* Z_VERSION_ERROR (-6) */
	""
};

/*-----------------------------------------------------------------------------
	LZX support
-----------------------------------------------------------------------------*/

#if !USE_XDK && SUPPORT_XBOX360

struct mspack_file
{
	byte*		buf;
	int			bufSize;
	int			pos;
	int			rest;
};

static int mspack_read(mspack_file *file, void *buffer, int bytes)
{
	guard(mspack_read);

	if (!file->rest)
	{
		// read block header
		if (file->buf[file->pos] == 0xFF)
		{
			// [0]   = FF
			// [1,2] = uncompressed block size
			// [3,4] = compressed block size
			file->rest = (file->buf[file->pos+3] << 8) | file->buf[file->pos+4];
			file->pos += 5;
		}
		else
		{
			// [0,1] = compressed size
			file->rest = (file->buf[file->pos+0] << 8) | file->buf[file->pos+1];
			file->pos += 2;
		}
		if (file->rest > file->bufSize - file->pos)
			file->rest = file->bufSize - file->pos;
	}
	if (bytes > file->rest) bytes = file->rest;
	if (bytes <= 0) return 0;

	// copy block data
	memcpy(buffer, file->buf + file->pos, bytes);
	file->pos  += bytes;
	file->rest -= bytes;

	return bytes;
	unguard;
}

static int mspack_write(mspack_file *file, void *buffer, int bytes)
{
	guard(mspack_write);
	assert(file->pos + bytes <= file->bufSize);
	memcpy(file->buf + file->pos, buffer, bytes);
	file->pos += bytes;
	return bytes;
	unguard;
}

static void *mspack_alloc(mspack_system *self, size_t bytes)
{
	return appMalloc(bytes);
}

static void mspack_free(void *ptr)
{
	appFree(ptr);
}

static void mspack_copy(void *src, void *dst, size_t bytes)
{
	memcpy(dst, src, bytes);
}

static struct mspack_system lzxSys =
{
	NULL,				// open
	NULL,				// close
	mspack_read,
	mspack_write,
	NULL,				// seek
	NULL,				// tell
	NULL,				// message
	mspack_alloc,
	mspack_free,
	mspack_copy
};

static void appDecompressLZX(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize)
{
	guard(appDecompressLZX);

	// setup streams
	mspack_file src, dst;
	src.buf     = CompressedBuffer;
	src.bufSize = CompressedSize;
	src.pos     = 0;
	src.rest    = 0;
	dst.buf     = UncompressedBuffer;
	dst.bufSize = UncompressedSize;
	dst.pos     = 0;
	// prepare decompressor
	lzxd_stream *lzxd = lzxd_init(&lzxSys, &src, &dst, 17, 0, 256*1024, UncompressedSize);
	assert(lzxd);
	// decompress
	int r = lzxd_decompress(lzxd, UncompressedSize);
	if (r != MSPACK_ERR_OK)
		appError("lzxd_decompress(%d,%d) returned %d", CompressedSize, UncompressedSize, r);
	// free resources
	lzxd_free(lzxd);

	unguard;
}

#endif // USE_XDK

/*-----------------------------------------------------------------------------
	Oodle support
-----------------------------------------------------------------------------*/

#if USE_OODLE && HAS_OODLE

#include <oodle2.h>

static void appDecompressOodle(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize)
{
	guard(appDecompressOodle);

	size_t ret = OodleLZ_Decompress(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize,
		OodleLZ_FuzzSafe_Yes, OodleLZ_CheckCRC_No, OodleLZ_Verbosity_Minimal);
		// verbosity is set to OodleLZ_Verbosity_Minimal, so any errors will be displayed in debug output (via OutputDebugString)
	if (ret != UncompressedSize)
		appError("OodleLZ_Decompress returned %d", ret);

	unguard;
}

#endif

#if USE_OODLE && _WIN32 && !HAS_OODLE

#ifdef _WIN64
static const char* OodleDllName = "oo2core_5_win64.dll";
static const char* OodleFuncName = "OodleLZ_Decompress";
#else
static const char* OodleDllName = "oo2core_5_win32.dll";
static const char* OodleFuncName = "_OodleLZ_Decompress@56";
#endif

typedef size_t (__stdcall *OodleDecompress_t)(
	void* compressed, size_t compressedSize, void* uncompressed, size_t uncompressedSize,
	bool bIsSafe,
	bool bIsCrcCheck,
	int verboseLevel,
	void* unk1, size_t unk1Size,
	void* callback1, void* callback2,
	void* mem, size_t memSize,
	int unk2);

static bool bOodleLoaded = false;
static HMODULE hOodleDll = NULL;
static OodleDecompress_t OodleLZ_Decompress = NULL;

static void appDecompressOodle_DLL(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize)
{
	guard(appDecompressOodle_DLL);

	if (!bOodleLoaded)
	{
		// Find the dll
		// Try loading from default path(s) first
		hOodleDll = LoadLibrary(OodleDllName);

		if (!hOodleDll)
		{
			static const char* SearchPaths[] =
			{
				".", ".\\libs"
			};
			for (const char* Path : SearchPaths)
			{
				hOodleDll = LoadLibrary(va("%s\\%s", Path, OodleDllName));
				if (hOodleDll) break;
			}
		}

		if (!hOodleDll)
			appErrorNoLog("Internal Oodle decompressor failed, %s not found", OodleDllName);

		OodleLZ_Decompress = (OodleDecompress_t)GetProcAddress(hOodleDll, OodleFuncName);
		assert(OodleLZ_Decompress != NULL);

		bOodleLoaded = true;
	}

	size_t ret = OodleLZ_Decompress(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize,
		true, false, 1, NULL, 0, NULL, NULL, NULL, 0, 0);
		// verbosity is set to 1, so any errors will be displayed in debug output (via OutputDebugString)
	if (ret != UncompressedSize)
		appError("OodleLZ_Decompress returned %d", ret);

	unguard;
}

#else

inline void appDecompressOodle_DLL(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize)
{
	appError("Internal Oodle decompressor failed");
}

#endif

/*-----------------------------------------------------------------------------
	appDecompress()
-----------------------------------------------------------------------------*/

// Decryptors for compressed data
void DecryptBladeAndSoul(byte* CompressedBuffer, int CompressedSize);
void DecryptTaoYuan(byte* CompressedBuffer, int CompressedSize);
void DecryptDevlsThird(byte* CompressedBuffer, int CompressedSize);

static int FoundCompression = -1;

static int DetectCompressionMethod(byte* CompressedBuffer)
{
	int Flags = 0;

	byte b1 = CompressedBuffer[0];
	byte b2 = CompressedBuffer[1];
	// zlib:
	//   http://tools.ietf.org/html/rfc1950
	//   http://stackoverflow.com/questions/9050260/what-does-a-zlib-header-look-like
	// oodle:
	//   https://github.com/powzix/ooz, kraken.cpp, Kraken_ParseHeader()
	if ( b1 == 0x78 &&					// b1=CMF: 7=32k buffer (CINFO), 8=deflate (CM)
		(b2 == 0x9C || b2 == 0xDA) )	// b2=FLG
	{
		Flags = COMPRESS_ZLIB;
	}
#if USE_OODLE
	else if ((b1 == 0x8C || b1 == 0xCC) && (b2 == 5 || b2 == 6 || b2 == 10 || b2 == 11 || b2 == 12))
	{
		Flags = COMPRESS_OODLE;
	}
#endif // USE_OODLE
#if USE_LZ4
	else if (GForceGame >= GAME_UE4_BASE)
	{
		Flags = COMPRESS_LZ4;		// in most cases UE4 games are using either oodle or lz4 - the first one is explicitly recognizable
	}
#endif // USE_LZ4
	else
	{
		Flags = COMPRESS_LZO;		// LZO was used only with UE3 games as standard compression method
	}

	return Flags;
}

int appDecompress(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize, int Flags)
{
	int OldFlags = Flags;

	guard(appDecompress);

#if GEARSU
	if (GForceGame == GAME_GoWU)
	{
		// It is strange, but this game has 2 Flags both used for LZ4 - probably they were used for different compression
		// settings of the same algorithm.
		if (Flags == 4 || Flags == 32) Flags = COMPRESS_LZ4;
	}
#endif // GEARSU

#if BLADENSOUL
	if (GForceGame == GAME_BladeNSoul && Flags == COMPRESS_LZO_ENC_BNS)	// note: GForceGame is required (to not pass 'Game' here)
	{
		DecryptBladeAndSoul(CompressedBuffer, CompressedSize);
		// overide compression
		Flags = COMPRESS_LZO;
	}
#endif // BLADENSOUL

#if SMITE
	if (GForceGame == GAME_Smite)
	{
		if (Flags & 512)
		{
			// Simple encryption
			for (int i = 0; i < CompressedSize; i++)
				CompressedBuffer[i] ^= 0x2A;
			// Remove encryption flag
			Flags &= ~512;
		}
	#if USE_OODLE
		if (Flags == 8)
		{
			// Overide compression, appeared in late 2019 builds
			Flags = COMPRESS_OODLE;
		}
	#endif
	}
#endif // SMITE

#if MASSEFF
	if (GForceGame == GAME_MassEffectLE)
	{
		if (Flags == 0x400) Flags = COMPRESS_OODLE;
	}
#endif // MASSEFF

#if TAO_YUAN
	if (GForceGame == GAME_TaoYuan)	// note: GForceGame is required (to not pass 'Game' here);
	{
		DecryptTaoYuan(CompressedBuffer, CompressedSize);
	}
#endif // TAO_YUAN

#if DEVILS_THIRD
	if ((GForceGame == GAME_DevilsThird) && (Flags & 8))
	{
		DecryptDevlsThird(CompressedBuffer, CompressedSize);
		// override compression
		Flags &= ~8;
	}
#endif // DEVILS_THIRD

	if (Flags == COMPRESS_FIND && FoundCompression >= 0)
	{
		// Do not detect compression multiple times: there were cases (Sea of Thieves) when
		// game is using LZ4 compression, however its first 2 bytes occasionally matched oodle,
		// so one of blocks were mistakenly used oodle.
		Flags = FoundCompression;
	}
	else if (Flags == COMPRESS_FIND && CompressedSize >= 2)
	{
		Flags = DetectCompressionMethod(CompressedBuffer);
		// Cache detected compression method
		FoundCompression = Flags;
	}

restart_decompress:

	if (Flags == COMPRESS_LZO)
	{
		int r;
		r = lzo_init();
		if (r != LZO_E_OK) appError("lzo_init() returned %d", r);
		lzo_uint newLen = UncompressedSize;
		r = lzo1x_decompress_safe(CompressedBuffer, CompressedSize, UncompressedBuffer, &newLen, NULL);
		if (r != LZO_E_OK)
		{
			if (CompressedSize != UncompressedSize)
			{
				appError("lzo_decompress(%d,%d) returned %d", CompressedSize, UncompressedSize, r);
			}
			else
			{
				// This situation is unusual for UE3, it happened with Alice, and Batman 3
				// TODO: probably extend this code for other compression methods too
				memcpy(UncompressedBuffer, CompressedBuffer, UncompressedSize);
				return UncompressedSize;
			}
		}
		if (newLen != UncompressedSize) appError("len mismatch: %d != %d", newLen, UncompressedSize);
		return newLen;
	}

	if (Flags == COMPRESS_ZLIB)
	{
#if 0
		appError("appDecompress: Zlib compression is not supported");
#else
		unsigned long newLen = UncompressedSize;
		int r = uncompress(UncompressedBuffer, &newLen, CompressedBuffer, CompressedSize);
		if (r != Z_OK) appError("zlib uncompress(%d,%d) returned %d", CompressedSize, UncompressedSize, r);
//		if (newLen != UncompressedSize) appError("len mismatch: %d != %d", newLen, UncompressedSize); -- needed by Bioshock
		return newLen;
#endif
	}

	if (Flags == COMPRESS_LZX)
	{
#if SUPPORT_XBOX360
#	if !USE_XDK
		appDecompressLZX(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
		return UncompressedSize;
#	else
		void *context;
		int r;
		r = XMemCreateDecompressionContext(0, NULL, 0, &context);
		if (r < 0) appError("XMemCreateDecompressionContext failed");
		unsigned int newLen = UncompressedSize;
		r = XMemDecompress(context, UncompressedBuffer, &newLen, CompressedBuffer, CompressedSize);
		if (r < 0) appError("XMemDecompress failed");
		if (newLen != UncompressedSize) appError("len mismatch: %d != %d", newLen, UncompressedSize);
		XMemDestroyDecompressionContext(context);
		return newLen;
#	endif // USE_XDK
#else  // SUPPORT_XBOX360
		appError("appDecompress: Lzx compression is not supported");
#endif // SUPPORT_XBOX360
	}

#if USE_LZ4
	if (Flags == COMPRESS_LZ4)
	{
		int newLen = LZ4_decompress_safe((const char*)CompressedBuffer, (char*)UncompressedBuffer, CompressedSize, UncompressedSize);
		if (newLen <= 0)
			appError("LZ4_decompress_safe returned %d\n", newLen);
		if (newLen != UncompressedSize) appError("lz4 len mismatch: %d != %d", newLen, UncompressedSize);
		return newLen;
	}
#endif // USE_LZ4

#if USE_OODLE // defined for supported engine versions, it means - some games may need Oodle decompression
	if (Flags == COMPRESS_OODLE)
	{
	#if HAS_OODLE // defined in oodle.project file, it means: oodle SDK is available at build time
		appDecompressOodle(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
		return UncompressedSize;
	#elif defined(_WIN32) // fallback to oodle.dll when SDK is not compiled in
		appDecompressOodle_DLL(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
		return UncompressedSize;
	#else
		appError("appDecompress: Oodle decompression is not supported");
	#endif // HAS_OODLE
	}
#endif // USE_OODLE

	// Unknown compression flags
	guard(UnknownCompression);
#if 0
	appError("appDecompress: unknown compression flags: %d", Flags);
	return 0;
#else
	// Try to use compression detection
	if (FoundCompression >= 0)
	{
		// Already detected a working decompressor (if it wouldn't be working, we'd already crash)
	}
	else
	{
		assert(CompressedSize >= 2);
		FoundCompression = DetectCompressionMethod(CompressedBuffer);
		appNotify("appDecompress: unknown compression flags %X, detected %X, retrying ...", Flags, FoundCompression);
		Flags = FoundCompression;
	}
	Flags = FoundCompression;
	goto restart_decompress;
	unguard;
#endif

	unguardf("CompSize=%d UncompSize=%d Flags=0x%X Bytes=%02X%02X", CompressedSize, UncompressedSize, OldFlags, CompressedBuffer[0], CompressedBuffer[1]);
}


/*-----------------------------------------------------------------------------
	AES support
-----------------------------------------------------------------------------*/

//todo: move to UnCoreDecrypt.cpp

#if UNREAL4

TArray<FString> GAesKeys;

#define AES_KEYBITS		256

void appDecryptAES(byte* Data, int Size, const char* Key, int KeyLen)
{
	guard(appDecryptAES);

	if (KeyLen <= 0)
	{
		KeyLen = strlen(Key);
	}

	if (KeyLen == 0)
	{
		appErrorNoLog("Trying to decrypt AES block without providing an AES key");
	}
	if (KeyLen < KEYLENGTH(AES_KEYBITS))
	{
		appErrorNoLog("AES key is too short");
	}

	assert((Size & 15) == 0);

	unsigned long rk[RKLENGTH(AES_KEYBITS)];
	int nrounds = rijndaelSetupDecrypt(rk, (const byte*)Key, AES_KEYBITS);

	for (int pos = 0; pos < Size; pos += 16)
	{
		rijndaelDecrypt(rk, nrounds, Data + pos, Data + pos);
	}

	unguard;
}

#endif // UNREAL4
