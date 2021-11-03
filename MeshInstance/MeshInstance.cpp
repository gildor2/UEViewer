#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"

#if RENDERING

#include "MeshInstance.h"
#include "GlWindow.h"			// for RGB macro


bool CMeshInstance::bColorMaterials = false;


unsigned CMeshInstance::GetMaterialDebugColor(int Index)
{
#if 0
	//!! This code is based on GetBoneInfColor(). Should place both in some place (utility functions)
	//!! and rename to "DebugColorByIndex()" etc. 2 versions: RGBA and float3.
	// Most of this code is targeted to make maximal color combinations
	// which are maximally different for adjacent BoneIndex values
	static const byte table[]  = { byte(255*0.9f), byte(255*0.3f), byte(255*0.6f), byte(255*0.0f) };
	static const int  table2[] = { 0, 1, 2, 4, 7, 3, 5, 6 };
	Index = (Index & 0xFFF8) | table2[Index & 7] ^ 7;
	#define C(x)	( (x) & 1 ) | ( ((x) >> 2) & 2 )
	return RGB255(table[C(Index)], table[C(Index >> 1)], table[C(Index >> 2)]);
	#undef C
#else
	// Use predefined color table
	// Macro for converting hex color code to RGBA (byte swapping, setting alpha to 255)
	#define C(x)  ( (x >> 16) | ((x & 0xff) << 16) | (x & 0xff00) | 0xff000000 )
	static const uint32 table[] = {
		C(0xCAB2D6), C(0xFFFF99), C(0x1F78B4), C(0x33A02C), C(0xE31A1C), C(0xFF7F00),
		C(0xFFFF33), C(0x6A3D9A), C(0xB15928), C(0xFDCDAC), C(0xB2DF8A), C(0xFDBF6F),
		C(0xCBD5E8), C(0xF4CAE4), C(0xE6F5C9), C(0xFFF2AE), C(0xF1E2CC), C(0xCCCCCC),
		C(0x8C564B), C(0xE377C2), C(0x7F7F7F), C(0xBCBD22), C(0x17BECF), C(0xC49C94),
		C(0xF7B6D2), C(0xC7C7C7), C(0xDBDB8D), C(0x9EDAE5), C(0xAF5500), C(0xB3E2CD),
		C(0xA6CEE3), C(0xFB9A99),
	};
	#undef C
	return table[(Index ^ 2) % ARRAY_COUNT(table)];
#endif
}


void CMeshInstance::SetMaterial(UUnrealMaterial *Mat, int Index)
{
	guard(CMeshInstance::SetMaterial);
	if (!bColorMaterials)
	{
		glColor3f(1, 1, 1);
		if (Mat)
			Mat->SetMaterial();
		else
			BindDefaultMaterial();
	}
	else
	{
		BindDefaultMaterial(true);
		glDisable(GL_CULL_FACE);
#if 0
		if (Index == HighlightMaterialIndex)
		{
			// Some HDR color
			glColor4f(1.0f, 1.0f, 2.0f, 1.0f);
		}
		else
		{
			unsigned color = GetMaterialDebugColor(Index);
			glColor4ubv((GLubyte*)&color);
		}
#else
		unsigned color = GetMaterialDebugColor(Index);
		if (HighlightMaterialIndex >= 0 && Index != HighlightMaterialIndex)
		{
			// Dim color
			color >>= 2;
			color &= 0x3f3f3f3f;
			// Fix alpha
			color |= 0xff000000;
		}
		glColor4ubv((GLubyte*)&color);
#endif
	}
	unguard;
}

#endif // RENDERING
