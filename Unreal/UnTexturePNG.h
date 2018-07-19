#ifndef __UNTEXTUREPNG_H__
#define __UNTEXTUREPNG_H__

bool UncompressPNG(const unsigned char* CompressedData, int CompressedSize, int Width, int Height, unsigned char* pic, bool bgra);

#endif // __UNTEXTUREPNG_H__
