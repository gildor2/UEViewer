/******************************************************************************

 @File         PVRTDecompress.cpp

 @Title        PVRTDecompress

 @Version

 @Copyright    Copyright (C)  Imagination Technologies Limited.

 @Platform     ANSI compatible

 @Description  PVRTC Texture Decompression.

******************************************************************************/

/*****************************************************************************
 * INCLUDES
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "PVRTDecompress.h"
#include "PVRTTexture.h"

#undef assert
#define assert(x)

/*****************************************************************************
 * defines and consts
 *****************************************************************************/
#define PT_INDEX (2)	// The Punch-through index

#define BLK_Y_SIZE 	(4) // always 4 for all 2D block types

#define BLK_X_MAX	(8)	// Max X dimension for blocks

#define BLK_X_2BPP	(8) // dimensions for the two formats
#define BLK_X_4BPP	(4)

#define WRAP_COORD(Val, Size) ((Val) & ((Size)-1))

#define POWER_OF_2(X)   util_number_is_power_2(X)

/*
	Define an expression to either wrap or clamp large or small vals to the
	legal coordinate range
*/
#define LIMIT_COORD(Val, Size, AssumeImageTiles) \
      ((AssumeImageTiles)? WRAP_COORD((Val), (Size)): PVRT_CLAMP((Val), 0, (Size)-1))

/*****************************************************************************
 * Useful typedefs
 *****************************************************************************/
typedef PVRTuint32 U32;
typedef PVRTuint8 U8;

/***********************************************************
				DECOMPRESSION ROUTINES
************************************************************/

/*!***********************************************************************
 @Struct	AMTC_BLOCK_STRUCT
 @Brief
*************************************************************************/
typedef struct
{
	// Uses 64 bits pre block
	U32 PackedData[2];
}AMTC_BLOCK_STRUCT;


static void Decompress(AMTC_BLOCK_STRUCT *pCompressedData,
					   const int Do2bitMode,
					   const int XDim,
					   const int YDim,
					   const int AssumeImageTiles,
					   unsigned char* pResultImage);

/*!***********************************************************************
 @Function		PVRTDecompressPVRTC
 @Input			pCompressedData The PVRTC texture data to decompress
 @Input			Do2bitMode Signifies whether the data is PVRTC2 or PVRTC4
 @Input			XDim X dimension of the texture
 @Input			YDim Y dimension of the texture
 @Modified		pResultImage The decompressed texture data
 @Description	Decompresses PVRTC to RGBA 8888
*************************************************************************/
void PVRTDecompressPVRTC(const void *pCompressedData,
				const int Do2bitMode,
				const int XDim,
				const int YDim,
				unsigned char* pResultImage)
{
	Decompress((AMTC_BLOCK_STRUCT*)pCompressedData,Do2bitMode,XDim,YDim,1,pResultImage);
}

 /*!***********************************************************************
  @Function		util_number_is_power_2
  @Input		input A number
  @Returns		TRUE if the number is an integer power of two, else FALSE.
  @Description	Check that a number is an integer power of two, i.e.
             	1, 2, 4, 8, ... etc.
				Returns FALSE for zero.
*************************************************************************/
#if 0
int util_number_is_power_2( unsigned  input )
{
  unsigned minus1;

  if( !input ) return 0;

  minus1 = input - 1;
  return ( (input | minus1) == (input ^ minus1) ) ? 1 : 0;
}
#else
inline bool util_number_is_power_2( unsigned input )
{
  return ( input & (input - 1) ) == 0;
}
#endif


/*!***********************************************************************
 @Function		Unpack5554Colour
 @Input			pBlock
 @Input			ABColours
 @Description	Given a block, extract the colour information and convert
 				to 5554 formats
*************************************************************************/
static void Unpack5554Colour(const AMTC_BLOCK_STRUCT *pBlock,
							 int   ABColours[2][4])
{
	U32 RawBits[2];

	int i;

	// Extract A and B
	RawBits[0] = pBlock->PackedData[1] & (0xFFFE); // 15 bits (shifted up by one)
	RawBits[1] = pBlock->PackedData[1] >> 16;	   // 16 bits

	// step through both colours
	for(i = 0; i < 2; i++)
	{
		// If completely opaque
		if(RawBits[i] & (1<<15))
		{
			// Extract R and G (both 5 bit)
			ABColours[i][0] = (RawBits[i] >> 10) & 0x1F;
			ABColours[i][1] = (RawBits[i] >>  5) & 0x1F;

			/*
				The precision of Blue depends on  A or B. If A then we need to
				replicate the top bit to get 5 bits in total
			*/
			ABColours[i][2] = RawBits[i] & 0x1F;
			if(i==0)
			{
				ABColours[0][2] |= ABColours[0][2] >> 4;
			}

			// set 4bit alpha fully on...
			ABColours[i][3] = 0xF;
		}
		else // Else if colour has variable translucency
		{
			/*
				Extract R and G (both 4 bit).
				(Leave a space on the end for the replication of bits
			*/
			ABColours[i][0] = (RawBits[i] >>  (8-1)) & 0x1E;
			ABColours[i][1] = (RawBits[i] >>  (4-1)) & 0x1E;

			// replicate bits to truly expand to 5 bits
			ABColours[i][0] |= ABColours[i][0] >> 4;
			ABColours[i][1] |= ABColours[i][1] >> 4;

			// grab the 3(+padding) or 4 bits of blue and add an extra padding bit
			ABColours[i][2] = (RawBits[i] & 0xF) << 1;

			/*
				expand from 3 to 5 bits if this is from colour A, or 4 to 5 bits if from
				colour B
			*/
			if(i==0)
			{
				ABColours[0][2] |= ABColours[0][2] >> 3;
			}
			else
			{
				ABColours[0][2] |= ABColours[0][2] >> 4;
			}

			// Set the alpha bits to be 3 + a zero on the end
			ABColours[i][3] = (RawBits[i] >> 11) & 0xE;
		}
	}
}

/*!***********************************************************************
 @Function		UnpackModulations
 @Input			pBlock
 @Input			Do2bitMode
 @Input			ModulationVals
 @Input			ModulationModes
 @Input			StartX
 @Input			StartY
 @Description	Given the block and the texture type and it's relative
 				position in the 2x2 group of blocks, extract the bit
 				patterns for the fully defined pixels.
*************************************************************************/
static void	UnpackModulations(const AMTC_BLOCK_STRUCT *pBlock,
							  const int Do2bitMode,
							  int ModulationVals[8][16],
							  int ModulationModes[8][16],
							  int StartX,
							  int StartY)
{
	int BlockModMode;
	U32 ModulationBits;

	int x, y;

	BlockModMode= pBlock->PackedData[1] & 1;
	ModulationBits	= pBlock->PackedData[0];

	// if it's in an interpolated mode
	if(Do2bitMode && BlockModMode)
	{
		/*
			run through all the pixels in the block. Note we can now treat all the
			"stored" values as if they have 2bits (even when they didn't!)
		*/
		for(y = 0; y < BLK_Y_SIZE; y++)
		{
			for(x = 0; x < BLK_X_2BPP; x++)
			{
				ModulationModes[y+StartY][x+StartX] = BlockModMode;

				// if this is a stored value...
				if(((x^y)&1) == 0)
				{
					ModulationVals[y+StartY][x+StartX] = ModulationBits & 3;
					ModulationBits >>= 2;
				}
			}
		}
	}
	else if(Do2bitMode) // else if direct encoded 2bit mode - i.e. 1 mode bit per pixel
	{
		for(y = 0; y < BLK_Y_SIZE; y++)
		{
			for(x = 0; x < BLK_X_2BPP; x++)
			{
				ModulationModes[y+StartY][x+StartX] = BlockModMode;

				// double the bits so 0=> 00, and 1=>11
				if(ModulationBits & 1)
				{
					ModulationVals[y+StartY][x+StartX] = 0x3;
				}
				else
				{
					ModulationVals[y+StartY][x+StartX] = 0x0;
				}
				ModulationBits >>= 1;
			}
		}
	}
	else // else its the 4bpp mode so each value has 2 bits
	{
		for(y = 0; y < BLK_Y_SIZE; y++)
		{
			for(x = 0; x < BLK_X_4BPP; x++)
			{
				ModulationModes[y+StartY][x+StartX] = BlockModMode;

				ModulationVals[y+StartY][x+StartX] = ModulationBits & 3;
				ModulationBits >>= 2;
			}
		}
	}

	// make sure nothing is left over
	assert(ModulationBits==0);
}

/*!***********************************************************************
 @Function		InterpolateColours
 @Input			ColourP
 @Input			ColourQ
 @Input			ColourR
 @Input			ColourS
 @Input			Do2bitMode
 @Input			x
 @Input			y
 @Modified		Result
 @Description	This performs a HW bit accurate interpolation of either the
				A or B colours for a particular pixel.

				NOTE: It is assumed that the source colours are in ARGB 5554
				format - This means that some "preparation" of the values will
				be necessary.
*************************************************************************/
static void InterpolateColours(const int ColourP[4],
						  const int ColourQ[4],
						  const int ColourR[4],
						  const int ColourS[4],
						  const int Do2bitMode,
						  const int x,
						  const int y,
						  int Result[4])
{
	int u, v, uscale;
	int k;

	int tmp1, tmp2;

#if 0
	int P[4], Q[4], R[4], S[4];

	// Copy the colours
	for(k = 0; k < 4; k++)
	{
		P[k] = ColourP[k];
		Q[k] = ColourQ[k];
		R[k] = ColourR[k];
		S[k] = ColourS[k];
	}
#else
	#define P ColourP
	#define Q ColourQ
	#define R ColourR
	#define S ColourS
#endif

	// put the x and y values into the right range
	v = (y & 0x3) | ((~y & 0x2) << 1);

	if(Do2bitMode)
		u = (x & 0x7) | ((~x & 0x4) << 1);
	else
		u = (x & 0x3) | ((~x & 0x2) << 1);

	// get the u and v scale amounts
	v  = v - BLK_Y_SIZE/2;

	if(Do2bitMode)
	{
		u = u - BLK_X_2BPP/2;
		uscale = 8;
	}
	else
	{
		u = u - BLK_X_4BPP/2;
		uscale = 4;
	}

	for(k = 0; k < 4; k++)
	{
		tmp1 = P[k] * uscale + u * (Q[k] - P[k]);
		tmp2 = R[k] * uscale + u * (S[k] - R[k]);

		tmp1 = tmp1 * 4 + v * (tmp2 - tmp1);

		Result[k] = tmp1;
	}
#undef P
#undef Q
#undef R
#undef S

	// Lop off the appropriate number of bits to get us to 8 bit precision
	if(Do2bitMode)
	{
		// do RGB
		Result[0] >>= 2;
		Result[1] >>= 2;
		Result[2] >>= 2;
		Result[3] >>= 1;
	}
	else
	{
		// do RGB  (A is ok)
		Result[0] >>= 1;
		Result[1] >>= 1;
		Result[2] >>= 1;
	}

	// sanity check
	for(k = 0; k < 4; k++)
	{
		assert(Result[k] < 256);
	}


	/*
		Convert from 5554 to 8888

		do RGB 5.3 => 8
	*/
	for(k = 0; k < 3; k++)
	{
		Result[k] += Result[k] >> 5;
	}

	Result[3] += Result[3] >> 4;

	// 2nd sanity check
	for(k = 0; k < 4; k++)
	{
		assert(Result[k] < 256);
	}

}

/*!***********************************************************************
 @Function		GetModulationValue
 @Input			x
 @Input			y
 @Input			Do2bitMode
 @Input			ModulationVals
 @Input			ModulationModes
 @Input			Mod
 @Input			DoPT
 @Description	Get the modulation value as a numerator of a fraction of 8ths
*************************************************************************/
static void GetModulationValue(int x,
							   int y,
							   const int Do2bitMode,
							   const int ModulationVals[8][16],
							   const int ModulationModes[8][16],
							   int *Mod,
							   int *DoPT)
{
	static const int RepVals0[4] = {0, 3, 5, 8};
	static const int RepVals1[4] = {0, 4, 4, 8};

	int ModVal;

	// Map X and Y into the local 2x2 block
	y = (y & 0x3) | ((~y & 0x2) << 1);

	if(Do2bitMode)
		x = (x & 0x7) | ((~x & 0x4) << 1);
	else
		x = (x & 0x3) | ((~x & 0x2) << 1);

	// assume no PT for now
	*DoPT = 0;

	// extract the modulation value. If a simple encoding
	if(ModulationModes[y][x]==0)
	{
		ModVal = RepVals0[ModulationVals[y][x]];
	}
	else if(Do2bitMode)
	{
		// if this is a stored value
		if(((x^y)&1)==0)
			ModVal = RepVals0[ModulationVals[y][x]];
		else if(ModulationModes[y][x] == 1) // else average from the neighbours if H&V interpolation..
		{
			ModVal = (RepVals0[ModulationVals[y-1][x]] +
					  RepVals0[ModulationVals[y+1][x]] +
					  RepVals0[ModulationVals[y][x-1]] +
					  RepVals0[ModulationVals[y][x+1]] + 2) / 4;
		}
		else if(ModulationModes[y][x] == 2) // else if H-Only
		{
			ModVal = (RepVals0[ModulationVals[y][x-1]] +
					  RepVals0[ModulationVals[y][x+1]] + 1) / 2;
		}
		else // else it's V-Only
		{
			ModVal = (RepVals0[ModulationVals[y-1][x]] +
					  RepVals0[ModulationVals[y+1][x]] + 1) / 2;
		}
	}
	else // else it's 4BPP and PT encoding
	{
		ModVal = RepVals1[ModulationVals[y][x]];

		*DoPT = ModulationVals[y][x] == PT_INDEX;
	}

	*Mod =ModVal;
}

/*!***********************************************************************
 @Function		TwiddleUV
 @Input			YSize	Y dimension of the texture in pixels
 @Input			XSize	X dimension of the texture in pixels
 @Input			YPos	Pixel Y position
 @Input			XPos	Pixel X position
 @Returns		The twiddled offset of the pixel
 @Description	Given the Block (or pixel) coordinates and the dimension of
 				the texture in blocks (or pixels) this returns the twiddled
 				offset of the block (or pixel) from the start of the map.

				NOTE the dimensions of the texture must be a power of 2
*************************************************************************/
static int DisableTwiddlingRoutine = 0;

static U32 TwiddleUV(U32 YSize, U32 XSize, U32 YPos, U32 XPos)
{
	U32 Twiddled;

	U32 MinDimension;
	U32 MaxValue;

	U32 SrcBitPos;
	U32 DstBitPos;

	int ShiftCount;

	assert(YPos < YSize);
	assert(XPos < XSize);

	assert(POWER_OF_2(YSize));
	assert(POWER_OF_2(XSize));

	if(YSize < XSize)
	{
		MinDimension = YSize;
		MaxValue	 = XPos;
	}
	else
	{
		MinDimension = XSize;
		MaxValue	 = YPos;
	}

	// Nasty hack to disable twiddling
	if(DisableTwiddlingRoutine)
		return (YPos* XSize + XPos);

	// Step through all the bits in the "minimum" dimension
	SrcBitPos = 1;
	DstBitPos = 1;
	Twiddled  = 0;
	ShiftCount = 0;

	while(SrcBitPos < MinDimension)
	{
		if(YPos & SrcBitPos)
		{
			Twiddled |= DstBitPos;
		}

		if(XPos & SrcBitPos)
		{
			Twiddled |= (DstBitPos << 1);
		}


		SrcBitPos <<= 1;
		DstBitPos <<= 2;
		ShiftCount += 1;

	}

	// prepend any unused bits
	MaxValue >>= ShiftCount;

	Twiddled |=  (MaxValue << (2*ShiftCount));

	return Twiddled;
}

/*!***********************************************************************
 @Function		Decompress
 @Input			pCompressedData The PVRTC texture data to decompress
 @Input			Do2BitMode Signifies whether the data is PVRTC2 or PVRTC4
 @Input			XDim X dimension of the texture
 @Input			YDim Y dimension of the texture
 @Input			AssumeImageTiles Assume the texture data tiles
 @Modified		pResultImage The decompressed texture data
 @Description	Decompresses PVRTC to RGBA 8888
*************************************************************************/
static void Decompress(AMTC_BLOCK_STRUCT *pCompressedData,
				const int Do2bitMode,
				const int XDim,
				const int YDim,
				const int AssumeImageTiles,
				unsigned char* pResultImage)
{
	int x, y;
	int i, j;

	int BlkX, BlkY;
	int BlkXp1, BlkYp1;
	int XBlockSize;
	int BlkXDim, BlkYDim;

	int StartX, StartY;

	int ModulationVals[8][16];
	int ModulationModes[8][16];

	int Mod, DoPT;

	unsigned int uPosition;

	// local neighbourhood of blocks
	AMTC_BLOCK_STRUCT *pBlocks[2][2];

	AMTC_BLOCK_STRUCT *pPrevious[2][2] = {{NULL, NULL}, {NULL, NULL}};

	// Low precision colours extracted from the blocks
	struct
	{
		int Reps[2][4];
	}Colours5554[2][2];

	// Interpolated A and B colours for the pixel
	int ASig[4], BSig[4];

	int Result[4];

	if(Do2bitMode)
		XBlockSize = BLK_X_2BPP;
	else
		XBlockSize = BLK_X_4BPP;

	// For MBX don't allow the sizes to get too small
	BlkXDim = PVRT_MAX(2, XDim / XBlockSize);
	BlkYDim = PVRT_MAX(2, YDim / BLK_Y_SIZE);

	/*
		Step through the pixels of the image decompressing each one in turn

		Note that this is a hideously inefficient way to do this!
	*/
	/// Compiler comparison: VC6=0.81s, VC7=0.9s, VC8=0.74, VC9=0.79
	for(BlkY = 0; BlkY < BlkYDim; BlkY++)
	{
		BlkYp1 = LIMIT_COORD(BlkY+1, BlkYDim, AssumeImageTiles);

		for(BlkX = 0; BlkX < BlkXDim; BlkX++)
		{
			BlkXp1 = LIMIT_COORD(BlkX+1, BlkXDim, AssumeImageTiles);

			// Map to block memory locations
			// compute the positions of the other 3 blocks

			if (BlkX > 0)
			{
				pBlocks[0][0] = pBlocks[0][1];
				pBlocks[1][0] = pBlocks[1][1];
				Colours5554[0][0] = Colours5554[0][1];
				Colours5554[1][0] = Colours5554[1][1];
			}
			else
			{
				pBlocks[0][0] = pCompressedData + TwiddleUV(BlkYDim, BlkXDim, BlkY,   BlkX);
				pBlocks[1][0] = pCompressedData + TwiddleUV(BlkYDim, BlkXDim, BlkYp1, BlkX);
				Unpack5554Colour(pBlocks[0][0], Colours5554[0][0].Reps);
				Unpack5554Colour(pBlocks[1][0], Colours5554[1][0].Reps);
			}

			pBlocks[0][1] = pCompressedData + TwiddleUV(BlkYDim, BlkXDim, BlkY,   BlkXp1);
			pBlocks[1][1] = pCompressedData + TwiddleUV(BlkYDim, BlkXDim, BlkYp1, BlkXp1);
			Unpack5554Colour(pBlocks[0][1], Colours5554[0][1].Reps);
			Unpack5554Colour(pBlocks[1][1], Colours5554[1][1].Reps);

			UnpackModulations(pBlocks[0][0], Do2bitMode, ModulationVals, ModulationModes, 0,          0);
			UnpackModulations(pBlocks[1][0], Do2bitMode, ModulationVals, ModulationModes, 0,          BLK_Y_SIZE);
			UnpackModulations(pBlocks[0][1], Do2bitMode, ModulationVals, ModulationModes, XBlockSize, 0);
			UnpackModulations(pBlocks[1][1], Do2bitMode, ModulationVals, ModulationModes, XBlockSize, BLK_Y_SIZE);

			y = BlkY * BLK_Y_SIZE + BLK_Y_SIZE/2;
			for(int dy = 0; dy < BLK_Y_SIZE; dy++, y++)
			{
				y = LIMIT_COORD(y, YDim, AssumeImageTiles);

				x = BlkX * XBlockSize + XBlockSize/2;
				for(int dx = 0; dx < XBlockSize; dx++, x++)
				{
					x = LIMIT_COORD(x, XDim, AssumeImageTiles);

					// decompress the pixel.  First compute the interpolated A and B signals
					/// InterpolateColours: 0.4s
					InterpolateColours(Colours5554[0][0].Reps[0],
									   Colours5554[0][1].Reps[0],
									   Colours5554[1][0].Reps[0],
									   Colours5554[1][1].Reps[0],
									   Do2bitMode, x, y,
									   ASig);

					InterpolateColours(Colours5554[0][0].Reps[1],
									   Colours5554[0][1].Reps[1],
									   Colours5554[1][0].Reps[1],
									   Colours5554[1][1].Reps[1],
									   Do2bitMode, x, y,
									   BSig);

					/// GetModulationValue: 0.1s
					GetModulationValue(x,y, Do2bitMode, (const int (*)[16])ModulationVals, (const int (*)[16])ModulationModes,
									   &Mod, &DoPT);

					// compute the modulated colour
					for(i = 0; i < 4; i++)
					{
						Result[i] = ASig[i] * 8 + Mod * (BSig[i] - ASig[i]);
						Result[i] >>= 3;
					}

					if(DoPT)
						Result[3] = 0;

					// Store the result in the output image
					uPosition = (x+y*XDim)<<2;
					pResultImage[uPosition+0] = (U8)Result[0];
					pResultImage[uPosition+1] = (U8)Result[1];
					pResultImage[uPosition+2] = (U8)Result[2];
					pResultImage[uPosition+3] = (U8)Result[3];
				} // dx
			} // dy
		} // BlkX
	} // BlkY
}

/****************************
**	ETC Compression
****************************/

/*****************************************************************************
Macros
*****************************************************************************/
#define _CLAMP_(X,Xmin,Xmax) (  (X)<(Xmax) ?  (  (X)<(Xmin)?(Xmin):(X)  )  : (Xmax)    )

/*****************************************************************************
Constants
******************************************************************************/
unsigned int ETC_FLIP =  0x01000000;
unsigned int ETC_DIFF = 0x02000000;
const int mod[8][4]={{2, 8,-2,-8},
					{5, 17, -5, -17},
					{9, 29, -9, -29},
					{13, 42, -13, -42},
					{18, 60, -18, -60},
					{24, 80, -24, -80},
					{33, 106, -33, -106},
					{47, 183, -47, -183}};

 /*!***********************************************************************
 @Function		modifyPixel
 @Input			red		Red value of pixel
 @Input			green	Green value of pixel
 @Input			blue	Blue value of pixel
 @Input			x	Pixel x position in block
 @Input			y	Pixel y position in block
 @Input			modBlock	Values for the current block
 @Input			modTable	Modulation values
 @Returns		Returns actual pixel colour
 @Description	Used by ETCTextureDecompress
*************************************************************************/
unsigned long modifyPixel(int red, int green, int blue, int x, int y, unsigned long modBlock, int modTable)
{
	int index = x*4+y, pixelMod;
	unsigned long mostSig = modBlock<<1;

	if (index<8)
		pixelMod = mod[modTable][((modBlock>>(index+24))&0x1)+((mostSig>>(index+8))&0x2)];
	else
		pixelMod = mod[modTable][((modBlock>>(index+8))&0x1)+((mostSig>>(index-8))&0x2)];

	red = _CLAMP_(red+pixelMod,0,255);
	green = _CLAMP_(green+pixelMod,0,255);
	blue = _CLAMP_(blue+pixelMod,0,255);

	return ((red<<16) + (green<<8) + blue)|0xff000000;
}

 /*!***********************************************************************
 @Function		ETCTextureDecompress
 @Input			pSrcData The ETC texture data to decompress
 @Input			x X dimension of the texture
 @Input			y Y dimension of the texture
 @Modified		pDestData The decompressed texture data
 @Input			nMode The format of the data
 @Returns		The number of bytes of ETC data decompressed
 @Description	Decompresses ETC to RGBA 8888
*************************************************************************/
int ETCTextureDecompress(const void * const pSrcData, const int &x, const int &y, const void *pDestData,const int &/*nMode*/)
{
	unsigned long blockTop, blockBot, *input = (unsigned long*)pSrcData, *output;
	unsigned char red1, green1, blue1, red2, green2, blue2;
	bool bFlip, bDiff;
	int modtable1,modtable2;

	for(int i=0;i<y;i+=4)
	{
		for(int m=0;m<x;m+=4)
		{
				blockTop = *(input++);
				blockBot = *(input++);

			output = (unsigned long*)pDestData + i*x +m;

			// check flipbit
			bFlip = (blockTop & ETC_FLIP) != 0;
			bDiff = (blockTop & ETC_DIFF) != 0;

			if(bDiff)
			{	// differential mode 5 colour bits + 3 difference bits
				// get base colour for subblock 1
				blue1 = (unsigned char)((blockTop&0xf80000)>>16);
				green1 = (unsigned char)((blockTop&0xf800)>>8);
				red1 = (unsigned char)(blockTop&0xf8);

				// get differential colour for subblock 2
				signed char blues = (signed char)(blue1>>3) + ((signed char) ((blockTop & 0x70000) >> 11)>>5);
				signed char greens = (signed char)(green1>>3) + ((signed char)((blockTop & 0x700) >>3)>>5);
				signed char reds = (signed char)(red1>>3) + ((signed char)((blockTop & 0x7)<<5)>>5);

				blue2 = (unsigned char)blues;
				green2 = (unsigned char)greens;
				red2 = (unsigned char)reds;

				red1 = red1 +(red1>>5);	// copy bits to lower sig
				green1 = green1 + (green1>>5);	// copy bits to lower sig
				blue1 = blue1 + (blue1>>5);	// copy bits to lower sig

				red2 = (red2<<3) +(red2>>2);	// copy bits to lower sig
				green2 = (green2<<3) + (green2>>2);	// copy bits to lower sig
				blue2 = (blue2<<3) + (blue2>>2);	// copy bits to lower sig
			}
			else
			{	// individual mode 4 + 4 colour bits
				// get base colour for subblock 1
				blue1 = (unsigned char)((blockTop&0xf00000)>>16);
				blue1 = blue1 +(blue1>>4);	// copy bits to lower sig
				green1 = (unsigned char)((blockTop&0xf000)>>8);
				green1 = green1 + (green1>>4);	// copy bits to lower sig
				red1 = (unsigned char)(blockTop&0xf0);
				red1 = red1 + (red1>>4);	// copy bits to lower sig

				// get base colour for subblock 2
				blue2 = (unsigned char)((blockTop&0xf0000)>>12);
				blue2 = blue2 +(blue2>>4);	// copy bits to lower sig
				green2 = (unsigned char)((blockTop&0xf00)>>4);
				green2 = green2 + (green2>>4);	// copy bits to lower sig
				red2 = (unsigned char)((blockTop&0xf)<<4);
				red2 = red2 + (red2>>4);	// copy bits to lower sig
			}
			// get the modtables for each subblock
			modtable1 = (blockTop>>29)&0x7;
			modtable2 = (blockTop>>26)&0x7;

			if(!bFlip)
			{	// 2 2x4 blocks side by side

				for(int j=0;j<4;j++)	// vertical
				{
					for(int k=0;k<2;k++)	// horizontal
					{
						*(output+j*x+k) = modifyPixel(red1,green1,blue1,k,j,blockBot,modtable1);
						*(output+j*x+k+2) = modifyPixel(red2,green2,blue2,k+2,j,blockBot,modtable2);
					}
				}

			}
			else
			{	// 2 4x2 blocks on top of each other
				for(int j=0;j<2;j++)
				{
					for(int k=0;k<4;k++)
					{
						*(output+j*x+k) = modifyPixel(red1,green1,blue1,k,j,blockBot,modtable1);
						*(output+(j+2)*x+k) = modifyPixel(red2,green2,blue2,k,j+2,blockBot,modtable2);
					}
				}
			}
		}
	}

	return x*y/2;
}

/*!***********************************************************************
@Function		PVRTDecompressETC
@Input			pSrcData The ETC texture data to decompress
@Input			x X dimension of the texture
@Input			y Y dimension of the texture
@Modified		pDestData The decompressed texture data
@Input			nMode The format of the data
@Returns		The number of bytes of ETC data decompressed
@Description	Decompresses ETC to RGBA 8888
*************************************************************************/
int PVRTDecompressETC(const void * const pSrcData,
						 const unsigned int &x,
						 const unsigned int &y,
						 void *pDestData,
						 const int &nMode)
{
	int i32read;

	if(x<ETC_MIN_TEXWIDTH || y<ETC_MIN_TEXHEIGHT)
	{	// decompress into a buffer big enough to take the minimum size
		char* pTempBuffer =	(char*)malloc(PVRT_MAX(x,ETC_MIN_TEXWIDTH)*PVRT_MAX(y,ETC_MIN_TEXHEIGHT)*4);
		i32read = ETCTextureDecompress(pSrcData,PVRT_MAX(x,ETC_MIN_TEXWIDTH),PVRT_MAX(y,ETC_MIN_TEXHEIGHT),pTempBuffer,nMode);

		for(unsigned int i=0;i<y;i++)
		{	// copy from larger temp buffer to output data
			memcpy((char*)(pDestData)+i*x*4,pTempBuffer+PVRT_MAX(x,ETC_MIN_TEXWIDTH)*4*i,x*4);
		}

		if(pTempBuffer) free(pTempBuffer);
	}
	else	// decompress larger MIP levels straight into the output data
		i32read = ETCTextureDecompress(pSrcData,x,y,pDestData,nMode);

	// swap r and b channels
	unsigned char* pSwap = (unsigned char*)pDestData, swap;

	for(unsigned int i=0;i<y;i++)
		for(unsigned int j=0;j<x;j++)
		{
			swap = pSwap[0];
			pSwap[0] = pSwap[2];
			pSwap[2] = swap;
			pSwap+=4;
		}

	return i32read;
}

/*****************************************************************************
 End of file (PVRTDecompress.cpp)
*****************************************************************************/
