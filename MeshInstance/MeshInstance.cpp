#include "Core.h"
#include "UnrealClasses.h"
#include "UnMesh.h"
#include "MeshInstance.h"
#include "TypeConvert.h"


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


void CLodMeshInstance::SetMesh(const ULodMesh *Mesh)
{
	pMesh = Mesh;
	SetAxis(Mesh->RotOrigin, BaseTransform.axis);
	BaseTransform.origin[0] = Mesh->MeshOrigin.X * Mesh->MeshScale.X;
	BaseTransform.origin[1] = Mesh->MeshOrigin.Y * Mesh->MeshScale.Y;
	BaseTransform.origin[2] = Mesh->MeshOrigin.Z * Mesh->MeshScale.Z;

	BaseTransformScaled.axis = BaseTransform.axis;
	CVec3 tmp;
	tmp[0] = 1.0f / Mesh->MeshScale.X;
	tmp[1] = 1.0f / Mesh->MeshScale.Y;
	tmp[2] = 1.0f / Mesh->MeshScale.Z;
	BaseTransformScaled.axis.PrescaleSource(tmp);
	BaseTransformScaled.origin = CVT(Mesh->MeshOrigin);
}

UUnrealMaterial *CLodMeshInstance::GetMaterial(int Index, int *PolyFlags)
{
	guard(CLodMeshInstance::GetMaterial);
	int TexIndex = 1000000;
	if (PolyFlags) *PolyFlags = 0;
	if (Index < pMesh->Materials.Num())
	{
		const FMeshMaterial &M = pMesh->Materials[Index];
		TexIndex  = M.TextureIndex;
		if (PolyFlags) *PolyFlags = M.PolyFlags;
	}
	// it is possible, that Textures array is empty (mesh textured by script)
	return (TexIndex < pMesh->Textures.Num()) ? MATERIAL_CAST(pMesh->Textures[TexIndex]) : NULL;
	unguard;
}

void CLodMeshInstance::SetMaterial(int Index)
{
	guard(CLodMeshInstance::SetMaterial);
	int PolyFlags;
	UUnrealMaterial *Mat = GetMaterial(Index, &PolyFlags);
	CMeshInstance::SetMaterial(Mat, Index, PolyFlags);
	unguard;
}
