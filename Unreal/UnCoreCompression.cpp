#include "Core.h"
#include "UnCore.h"

// includes for package decompression
#include "lzo/lzo1x.h"
#include "zlib/zlib.h"

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


/*-----------------------------------------------------------------------------
	ZLib support
-----------------------------------------------------------------------------*/

// using Core memory manager

extern "C" void *zcalloc(int opaque, int items, int size)
{
	return appMalloc(items * size);
}

extern "C" void zcfree(int opaque, void *ptr)
{
	appFree(ptr);
}


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
	if (!bytes) return 0;

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
	appDecompress()
-----------------------------------------------------------------------------*/

int appDecompress(byte *CompressedBuffer, int CompressedSize, byte *UncompressedBuffer, int UncompressedSize, int Flags)
{
	guard(appDecompress);

#if BLADENSOUL
	if (GForceGame == GAME_BladeNSoul && Flags == COMPRESS_LZO_ENC_BNS)	// note: GForceGame is required (to not pass 'Game' here)
	{
		if (CompressedSize >= 32)
		{
			static const char *key = "qiffjdlerdoqymvketdcl0er2subioxq";
			for (int i = 0; i < CompressedSize; i++)
				CompressedBuffer[i] ^= key[i % 32];
		}
		// overide compression
		Flags = COMPRESS_LZO;
	}
#endif // BLADENSOUL

#if SMITE
	if (GForceGame == GAME_Smite && Flags == COMPRESS_LZO_ENC_SMITE)
	{
		for (int i = 0; i < CompressedSize; i++)
			CompressedBuffer[i] ^= 0x2A;
		// overide compression
		Flags = COMPRESS_LZO;
	}
#endif // SMITE

#if TAO_YUAN
	if (GForceGame == GAME_TaoYuan)	// note: GForceGame is required (to not pass 'Game' here);
	{
		static const byte key[] =
		{
			137, 35, 95, 142, 69, 136, 243, 119, 25, 35, 111, 94, 101, 136, 243, 204,
			243, 67, 95, 158, 69, 106, 107, 187, 237, 35, 103, 142, 72, 142, 243
		};
		for (int i = 0; i < CompressedSize; i++)
			CompressedBuffer[i] ^= key[i % 31];		// note: "N % 31" is not the same as "N & 0x1F"
	}
#endif // TAO_YUAN

	if (Flags == COMPRESS_FIND && CompressedSize >= 2)
	{
		byte b1 = CompressedBuffer[0];
		byte b2 = CompressedBuffer[1];
		// detect compression
		// zlib:
		//   http://tools.ietf.org/html/rfc1950
		//   http://stackoverflow.com/questions/9050260/what-does-a-zlib-header-look-like
		if ( b1 == 0x78 &&					// b1=CMF: 7=32k buffer (CINFO), 8=deflate (CM)
			(b2 == 0x9C || b2 == 0xDA) )	// b2=FLG
		{
			Flags = COMPRESS_ZLIB;
		}
		else
			Flags = COMPRESS_LZO;
	}

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
		appError("appDecompress: LZX compression is not supported");
#endif // SUPPORT_XBOX360
	}

	appError("appDecompress: unknown compression flags: %d", Flags);
	return 0;

	unguardf("CompSize=%d UncompSize=%d Flags=0x%X", CompressedSize, UncompressedSize, Flags);
}
