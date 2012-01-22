#include "Core.h"
#include "UnrealClasses.h"

#if RENDERING

#include "MeshInstance.h"


void CMeshInstance::SetMaterial(UUnrealMaterial *Mat, int Index, int PolyFlags)
{
	guard(CMeshInstance::SetMaterial);
	if (!bColorMaterials)
	{
		glColor3f(1, 1, 1);
		if (Mat)
			Mat->SetMaterial(PolyFlags);
		else
			BindDefaultMaterial();
	}
	else
	{
		BindDefaultMaterial(true);
		glDisable(GL_CULL_FACE);
		int color = Index + 1;
		if (color > 7) color = 7;
#define C(n)	( ((color >> n) & 1) * 0.5f + 0.3f )
		glColor3f(C(0), C(1), C(2));
#undef C
	}
	unguard;
}

#endif // RENDERING
