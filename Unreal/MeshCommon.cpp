#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"			// for typeinfo
#include "MeshCommon.h"
#include "UnMathTools.h"		// CVertexShare


// WARNING for BuildNnnCommon functions: do not access Verts[i] directly, use VERT macro only!
#define VERT(n)		OffsetPointer(Verts, (n) * VertexSize)

void BuildNormalsCommon(CMeshVertex *Verts, int VertexSize, int NumVerts, const CIndexBuffer &Indices)
{
	guard(BuildNormalsCommon);

	int i, j;

	// Find vertices to share.
	// We are using very simple algorithm here: to share all vertices with the same position
	// independently on normals of faces which share this vertex.
	TArray<CVec3> tmpNorm;
	tmpNorm.Add(NumVerts);							// really will use Points.Num() items, which value is smaller than NumVerts
	CVertexShare Share;
	Share.Prepare(Verts, NumVerts, VertexSize);
	for (i = 0; i < NumVerts; i++)
	{
		static const CVec3 NullVec = { 0, 0, 0 };
		Share.AddVertex(VERT(i)->Position, NullVec);
	}

	CIndexBuffer::IndexAccessor_t Index = Indices.GetAccessor();
	for (i = 0; i < Indices.Num() / 3; i++)
	{
		CMeshVertex *V[3];
		CVec3 *N[3];
		for (j = 0; j < 3; j++)
		{
			int idx = Index(i * 3 + j);				// index in Verts[]
			V[j] = VERT(idx);
			N[j] = &tmpNorm[Share.WedgeToVert[idx]]; // remap to shared verts
		}

		// compute edges
		CVec3 D[3];				// 0->1, 1->2, 2->0
		VectorSubtract(V[1]->Position, V[0]->Position, D[0]);
		VectorSubtract(V[2]->Position, V[1]->Position, D[1]);
		VectorSubtract(V[0]->Position, V[2]->Position, D[2]);
		// compute face normal
		CVec3 norm;
		cross(D[1], D[0], norm);
		norm.Normalize();
		// compute angles
		for (j = 0; j < 3; j++) D[j].Normalize();
		float angle[3];
		angle[0] = acos(-dot(D[0], D[2]));
		angle[1] = acos(-dot(D[0], D[1]));
		angle[2] = acos(-dot(D[1], D[2]));
		// add normals for triangle verts
		for (j = 0; j < 3; j++)
			VectorMA(*N[j], angle[j], norm);
	}

	// normalize shared normals ...
	for (i = 0; i < Share.Points.Num(); i++)
		tmpNorm[i].Normalize();

	// ... then place ("unshare") normals to Verts
	for (i = 0; i < NumVerts; i++)
		VERT(i)->Normal = tmpNorm[Share.WedgeToVert[i]];

	unguard;
}


void BuildTangentsCommon(CMeshVertex *Verts, int VertexSize, const CIndexBuffer &Indices)
{
	guard(BuildTangentsCommon);

	int i, j;

	CIndexBuffer::IndexAccessor_t Index = Indices.GetAccessor();
	for (i = 0; i < Indices.Num() / 3; i++)
	{
		CMeshVertex *V[3];
		for (j = 0; j < 3; j++)
		{
			int idx = Index(i * 3 + j);
			V[j] = VERT(idx);
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
			CMeshVertex &DW = *V[j];
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

	unguard;
}
