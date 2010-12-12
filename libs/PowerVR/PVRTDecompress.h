/******************************************************************************

 @File         PVRTDecompress.h

 @Title        PVRTDecompress

 @Version      

 @Copyright    Copyright (C)  Imagination Technologies Limited.

 @Platform     ANSI compatible

 @Description  PVRTC and ETC Texture Decompression.

******************************************************************************/

#ifndef _PVRTDECOMPRESS_H_
#define _PVRTDECOMPRESS_H_

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
				unsigned char* pResultImage);

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
						 const int &nMode);


#endif /* _PVRTDECOMPRESS_H_ */

/*****************************************************************************
 End of file (PVRTBoneBatch.h)
*****************************************************************************/

