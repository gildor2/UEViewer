#include <nvimage/Image.h>
#include <nvimage/DirectDrawSurface.h>
#undef __FUNC__						// conflicted with our guard macros

void DecodeDDS(const unsigned char* Data, int USize, int VSize, nv::DDSHeader& header, nv::Image& image);
void WriteDDSHeader(unsigned char* Data, nv::DDSHeader& header);
