#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "MeshInstance.h"
#include "GlWindow.h"			// for RGB macro


unsigned CMeshInstance::GetMaterialDebugColor(int Index)
{
	int color = Index + 1;
	static const byte table[] = { 64, 255, 128, 0 };
#define C(x)	( (x) & 1 ) | ( ((x) >> 2) & 2 )
	return RGB255(table[C(color)], table[C(color >> 1)], table[C(color >> 2)]);
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
