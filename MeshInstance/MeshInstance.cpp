#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "MeshInstance.h"
#include "GlWindow.h"			// for RGB macro


//!! This code is based on GetBoneInfColor(). Should place both in some place (utility functions)
//!! and rename to "DebugColorByIndex()" etc. 2 versions: RGBA and float3.
unsigned CMeshInstance::GetMaterialDebugColor(int Index)
{
	// most of this code is targetted to make maximal color combinations
	// which are maximally different for adjacent BoneIndex values
	static const byte table[]  = { byte(255*0.9f), byte(255*0.3f), byte(255*0.6f), byte(255*0.0f) };
	static const int  table2[] = { 0, 1, 2, 4, 7, 3, 5, 6 };
	Index = (Index & 0xFFF8) | table2[Index & 7] ^ 7;
	#define C(x)	( (x) & 1 ) | ( ((x) >> 2) & 2 )
	return RGB255(table[C(Index)], table[C(Index >> 1)], table[C(Index >> 2)]);
	#undef C
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
		unsigned color = GetMaterialDebugColor(Index);
		glColor4ubv((GLubyte*)&color);
	}
	unguard;
}

#endif // RENDERING
