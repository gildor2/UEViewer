/******************************************************************************

 @File         PVRTTexture.h

 @Title        PVRTTexture

 @Version      

 @Copyright    Copyright (C)  Imagination Technologies Limited.

 @Platform     ANSI compatible

 @Description  Texture loading.

******************************************************************************/
#ifndef _PVRTTEXTURE_H_
#define _PVRTTEXTURE_H_

#include "PVRTGlobal.h"
/*!***************************************************************************
Macros
*****************************************************************************/

/*!***************************************************************************
Describes the header of a PVR header-texture
*****************************************************************************/
struct PVR_Texture_Header
{
	PVRTuint32 dwHeaderSize;			/*!< size of the structure */
	PVRTuint32 dwHeight;				/*!< height of surface to be created */
	PVRTuint32 dwWidth;				/*!< width of input surface */
	PVRTuint32 dwMipMapCount;			/*!< number of mip-map levels requested */
	PVRTuint32 dwpfFlags;				/*!< pixel format flags */
	PVRTuint32 dwTextureDataSize;		/*!< Total size in bytes */
	PVRTuint32 dwBitCount;			/*!< number of bits per pixel  */
	PVRTuint32 dwRBitMask;			/*!< mask for red bit */
	PVRTuint32 dwGBitMask;			/*!< mask for green bits */
	PVRTuint32 dwBBitMask;			/*!< mask for blue bits */
	PVRTuint32 dwAlphaBitMask;		/*!< mask for alpha channel */
	PVRTuint32 dwPVR;					/*!< magic number identifying pvr file */
	PVRTuint32 dwNumSurfs;			/*!< the number of surfaces present in the pvr */
} ;

/*****************************************************************************
* ENUMS
*****************************************************************************/

enum PixelType
{
	MGLPT_ARGB_4444 = 0x00,
	MGLPT_ARGB_1555,
	MGLPT_RGB_565,
	MGLPT_RGB_555,
	MGLPT_RGB_888,
	MGLPT_ARGB_8888,
	MGLPT_ARGB_8332,
	MGLPT_I_8,
	MGLPT_AI_88,
	MGLPT_1_BPP,
	MGLPT_VY1UY0,
	MGLPT_Y1VY0U,
	MGLPT_PVRTC2,
	MGLPT_PVRTC4,
	MGLPT_PVRTC2_2,
	MGLPT_PVRTC2_4,

	OGL_RGBA_4444= 0x10,
	OGL_RGBA_5551,
	OGL_RGBA_8888,
	OGL_RGB_565,
	OGL_RGB_555,
	OGL_RGB_888,
	OGL_I_8,
	OGL_AI_88,
	OGL_PVRTC2,
	OGL_PVRTC4,

	// OGL_BGRA_8888 extension
	OGL_BGRA_8888,

	D3D_DXT1 = 0x20,
	D3D_DXT2,
	D3D_DXT3,
	D3D_DXT4,
	D3D_DXT5,

	D3D_RGB_332,
	D3D_AI_44,
	D3D_LVU_655,
	D3D_XLVU_8888,
	D3D_QWVU_8888,

	//10 bits per channel
	D3D_ABGR_2101010,
	D3D_ARGB_2101010,
	D3D_AWVU_2101010,

	//16 bits per channel
	D3D_GR_1616,
	D3D_VU_1616,
	D3D_ABGR_16161616,

	//HDR formats
	D3D_R16F,
	D3D_GR_1616F,
	D3D_ABGR_16161616F,

	//32 bits per channel
	D3D_R32F,
	D3D_GR_3232F,
	D3D_ABGR_32323232F,

	// Ericsson
	ETC_RGB_4BPP,
	ETC_RGBA_EXPLICIT,
	ETC_RGBA_INTERPOLATED,

	// DX10


	ePT_DX10_R32G32B32A32_FLOAT= 0x50,
	ePT_DX10_R32G32B32A32_UINT ,
	ePT_DX10_R32G32B32A32_SINT,

	ePT_DX10_R32G32B32_FLOAT,
	ePT_DX10_R32G32B32_UINT,
	ePT_DX10_R32G32B32_SINT,

	ePT_DX10_R16G16B16A16_FLOAT ,
	ePT_DX10_R16G16B16A16_UNORM,
	ePT_DX10_R16G16B16A16_UINT ,
	ePT_DX10_R16G16B16A16_SNORM ,
	ePT_DX10_R16G16B16A16_SINT ,

	ePT_DX10_R32G32_FLOAT ,
	ePT_DX10_R32G32_UINT ,
	ePT_DX10_R32G32_SINT ,

	ePT_DX10_R10G10B10A2_UNORM ,
	ePT_DX10_R10G10B10A2_UINT ,

	ePT_DX10_R11G11B10_FLOAT ,

	ePT_DX10_R8G8B8A8_UNORM ,
	ePT_DX10_R8G8B8A8_UNORM_SRGB ,
	ePT_DX10_R8G8B8A8_UINT ,
	ePT_DX10_R8G8B8A8_SNORM ,
	ePT_DX10_R8G8B8A8_SINT ,

	ePT_DX10_R16G16_FLOAT ,
	ePT_DX10_R16G16_UNORM ,
	ePT_DX10_R16G16_UINT ,
	ePT_DX10_R16G16_SNORM ,
	ePT_DX10_R16G16_SINT ,

	ePT_DX10_R32_FLOAT ,
	ePT_DX10_R32_UINT ,
	ePT_DX10_R32_SINT ,

	ePT_DX10_R8G8_UNORM ,
	ePT_DX10_R8G8_UINT ,
	ePT_DX10_R8G8_SNORM ,
	ePT_DX10_R8G8_SINT ,

	ePT_DX10_R16_FLOAT ,
	ePT_DX10_R16_UNORM ,
	ePT_DX10_R16_UINT ,
	ePT_DX10_R16_SNORM ,
	ePT_DX10_R16_SINT ,

	ePT_DX10_R8_UNORM,
	ePT_DX10_R8_UINT,
	ePT_DX10_R8_SNORM,
	ePT_DX10_R8_SINT,

	ePT_DX10_A8_UNORM,
	ePT_DX10_R1_UNORM,
	ePT_DX10_R9G9B9E5_SHAREDEXP,
	ePT_DX10_R8G8_B8G8_UNORM,
	ePT_DX10_G8R8_G8B8_UNORM,

	ePT_DX10_BC1_UNORM,
	ePT_DX10_BC1_UNORM_SRGB,

	ePT_DX10_BC2_UNORM,
	ePT_DX10_BC2_UNORM_SRGB,

	ePT_DX10_BC3_UNORM,
	ePT_DX10_BC3_UNORM_SRGB,

	ePT_DX10_BC4_UNORM,
	ePT_DX10_BC4_SNORM,

	ePT_DX10_BC5_UNORM,
	ePT_DX10_BC5_SNORM,

	//ePT_DX10_B5G6R5_UNORM,			// defined but obsolete - won't actually load in DX10
	//ePT_DX10_B5G5R5A1_UNORM,
	//ePT_DX10_B8G8R8A8_UNORM,
	//ePT_DX10_B8G8R8X8_UNORM,

	// OpenVG

	/* RGB{A,X} channel ordering */
	ePT_VG_sRGBX_8888  = 0x90,
	ePT_VG_sRGBA_8888,
	ePT_VG_sRGBA_8888_PRE,
	ePT_VG_sRGB_565,
	ePT_VG_sRGBA_5551,
	ePT_VG_sRGBA_4444,
	ePT_VG_sL_8,
	ePT_VG_lRGBX_8888,
	ePT_VG_lRGBA_8888,
	ePT_VG_lRGBA_8888_PRE,
	ePT_VG_lL_8,
	ePT_VG_A_8,
	ePT_VG_BW_1,

	/* {A,X}RGB channel ordering */
	ePT_VG_sXRGB_8888,
	ePT_VG_sARGB_8888,
	ePT_VG_sARGB_8888_PRE,
	ePT_VG_sARGB_1555,
	ePT_VG_sARGB_4444,
	ePT_VG_lXRGB_8888,
	ePT_VG_lARGB_8888,
	ePT_VG_lARGB_8888_PRE,

	/* BGR{A,X} channel ordering */
	ePT_VG_sBGRX_8888,
	ePT_VG_sBGRA_8888,
	ePT_VG_sBGRA_8888_PRE,
	ePT_VG_sBGR_565,
	ePT_VG_sBGRA_5551,
	ePT_VG_sBGRA_4444,
	ePT_VG_lBGRX_8888,
	ePT_VG_lBGRA_8888,
	ePT_VG_lBGRA_8888_PRE,

	/* {A,X}BGR channel ordering */
	ePT_VG_sXBGR_8888,
	ePT_VG_sABGR_8888 ,
	ePT_VG_sABGR_8888_PRE,
	ePT_VG_sABGR_1555,
	ePT_VG_sABGR_4444,
	ePT_VG_lXBGR_8888,
	ePT_VG_lABGR_8888,
	ePT_VG_lABGR_8888_PRE,

	// max cap for iterating
	END_OF_PIXEL_TYPES,

	MGLPT_NOTYPE = 0xff

};

/*****************************************************************************
* constants
*****************************************************************************/

const PVRTuint32 PVRTEX_MIPMAP		= (1<<8);		// has mip map levels
const PVRTuint32 PVRTEX_TWIDDLE		= (1<<9);		// is twiddled
const PVRTuint32 PVRTEX_BUMPMAP		= (1<<10);		// has normals encoded for a bump map
const PVRTuint32 PVRTEX_TILING		= (1<<11);		// is bordered for tiled pvr
const PVRTuint32 PVRTEX_CUBEMAP		= (1<<12);		// is a cubemap/skybox
const PVRTuint32 PVRTEX_FALSEMIPCOL	= (1<<13);		// are there false coloured MIP levels
const PVRTuint32 PVRTEX_VOLUME		= (1<<14);		// is this a volume texture
const PVRTuint32 PVRTEX_ALPHA			= (1<<15);		// v2.1 is there transparency info in the texture
const PVRTuint32 PVRTEX_VERTICAL_FLIP	= (1<<16);		// v2.1 is the texture vertically flipped

const PVRTuint32 PVRTEX_PIXELTYPE		= 0xff;			// pixel type is always in the last 16bits of the flags
const PVRTuint32 PVRTEX_IDENTIFIER	= 0x21525650;	// the pvr identifier is the characters 'P','V','R'

const PVRTuint32 PVRTEX_V1_HEADER_SIZE = 44;			// old header size was 44 for identification purposes

const PVRTuint32 PVRTC2_MIN_TEXWIDTH		= 16;
const PVRTuint32 PVRTC2_MIN_TEXHEIGHT		= 8;
const PVRTuint32 PVRTC4_MIN_TEXWIDTH		= 8;
const PVRTuint32 PVRTC4_MIN_TEXHEIGHT		= 8;
const PVRTuint32 ETC_MIN_TEXWIDTH			= 4;
const PVRTuint32 ETC_MIN_TEXHEIGHT		= 4;
const PVRTuint32 DXT_MIN_TEXWIDTH			= 4;
const PVRTuint32 DXT_MIN_TEXHEIGHT		= 4;

/****************************************************************************
** Functions
****************************************************************************/

/*!***************************************************************************
@Function		PVRTTextureCreate
@Input			w			Size of the texture
@Input			h			Size of the texture
@Input			wMin		Minimum size of a texture level
@Input			hMin		Minimum size of a texture level
@Input			nBPP		Bits per pixel of the format
@Input			bMIPMap		Create memory for MIP-map levels also?
@Return			Allocated texture memory (must be free()d)
@Description	Creates a PVR_Texture_Header structure, including room for
the specified texture, in memory.
*****************************************************************************/
PVR_Texture_Header *PVRTTextureCreate(
									  unsigned int		w,
									  unsigned int		h,
									  const unsigned int	wMin,
									  const unsigned int	hMin,
									  const unsigned int	nBPP,
									  const bool			bMIPMap);

/*!***************************************************************************
@Function		PVRTTextureTile
@Modified		pOut		The tiled texture in system memory
@Input			pIn			The source texture
@Input			nRepeatCnt	Number of times to repeat the source texture
@Description	Allocates and fills, in system memory, a texture large enough
to repeat the source texture specified number of times.
*****************************************************************************/
void PVRTTextureTile(
					 PVR_Texture_Header			**pOut,
					 const PVR_Texture_Header	* const pIn,
					 const int					nRepeatCnt);

/****************************************************************************
** Internal Functions
****************************************************************************/

/*!***************************************************************************
@Function		PVRTTextureLoadTiled
@Modified		pDst			Texture to place the tiled data
@Input			nWidthDst		Width of destination texture
@Input			nHeightDst		Height of destination texture
@Input			pSrc			Texture to tile
@Input			nWidthSrc		Width of source texture
@Input			nHeightSrc		Height of source texture
@Input 			nElementSize	Bytes per pixel
@Input			bTwiddled		True if the data is twiddled
@Description	Needed by PVRTTextureTile() in the various PVRTTextureAPIs
*****************************************************************************/
void PVRTTextureLoadTiled(
						  PVRTuint8		* const pDst,
						  const unsigned int	nWidthDst,
						  const unsigned int	nHeightDst,
						  const PVRTuint8	* const pSrc,
						  const unsigned int	nWidthSrc,
						  const unsigned int	nHeightSrc,
						  const unsigned int	nElementSize,
						  const bool			bTwiddled);


/*!***************************************************************************
@Function		PVRTTextureTwiddle
@Output		a	Twiddled value
@Input			u	Coordinate axis 0
@Input			v	Coordinate axis 1
@Description	Combine a 2D coordinate into a twiddled value
*****************************************************************************/
void PVRTTextureTwiddle(unsigned int &a, const unsigned int u, const unsigned int v);

/*!***************************************************************************
@Function		PVRTTextureDeTwiddle
@Output		u	Coordinate axis 0
@Output		v	Coordinate axis 1
@Input			a	Twiddled value
@Description	Extract 2D coordinates from a twiddled value.
*****************************************************************************/
void PVRTTextureDeTwiddle(unsigned int &u, unsigned int &v, const unsigned int a);


#endif /* _PVRTTEXTURE_H_ */

/*****************************************************************************
End of file (PVRTTexture.h)
*****************************************************************************/

