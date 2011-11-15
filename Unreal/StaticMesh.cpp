#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"		// for typeinfo
#include "StaticMesh.h"


// code of this function is similar to BuildNormals() from SkelMeshInstance.cpp
void CStaticMeshLod::BuildTangents()
{
	guard(CStaticMesh::BuildTangents);

	if (HasTangents) return;

	int i, j;

	CIndexBuffer::IndexAccessor_t Index = Indices.GetAccessor();
	for (i = 0; i < Indices.Num() / 3; i++)
	{
		CStaticMeshVertex *V[3];
		for (j = 0; j < 3; j++)
		{
			int idx = Index(Indices, i * 3 + j);
			V[j] = &Verts[idx];
		}

		// compute tangent
		CVecT tang;
		float U0 = V[0]->UV[0].U;
		float V0 = V[0]->UV[0].V;
		float U1 = V[1]->UV[0].U;
		float V1 = V[1]->UV[0].V;
		float U2 = V[2]->UV[0].U;
		float V2 = V[2]->UV[0].V;

		if (V0 == V2)
		{
			// W[0] -> W[2] -- axis for tangent
			VectorSubtract(V[2]->Position, V[0]->Position, tang);
			// we should make tang to look in side of growing U
			if (U2 < U0) tang.Negate();
		}
		else
		{
			float pos = (V1 - V0) / (V2 - V0);			// fraction, where W[1] is placed betwen W[0] and W[2] (may be < 0 or > 1)
			CVecT tmp;
			Lerp(V[0]->Position, V[2]->Position, pos, tmp);
			VectorSubtract(tmp, V[1]->Position, tang);
			// tang.V == W[1].V; but tang.U may be greater or smaller than W[1].U
			// we should make tang to look in side of growing U
			float tU = Lerp(U0, U2, pos);
			if (tU < U1) tang.Negate();
		}
		// now, tang is on triangle plane
		// now we should place tangent orthogonal to normal, then normalize vector
		float binormalScale = 1.0f;
		for (j = 0; j < 3; j++)
		{
			CStaticMeshVertex &DW = *V[j];
			const CVecT &norm = DW.Normal;
			float pos = dot(norm, tang);
			CVecT tang2;
			VectorMA(tang, -pos, norm, tang2);
			tang2.Normalize();
			DW.Tangent = tang2;
			cross(DW.Normal, DW.Tangent, tang2);
			tang2.Normalize();
			if (j == 0)		// do this only once for triangle
			{
				// check binormal sign
				// find two points with different V
				int W1 = 0;
				int W2 = (V1 != V0) ? 1 : 2;
				// check projections of these points to binormal
				float p1 = dot(V[W1]->Position, tang2);
				float p2 = dot(V[W2]->Position, tang2);
				if ((p1 - p2) * (V[W1]->UV[0].V - V[W2]->UV[0].V) < 0)
					binormalScale = -1.0f;
			}
			tang2.Scale(binormalScale);
			DW.Binormal = tang2;
		}
	}

	HasTangents = true;

	unguard;
}
