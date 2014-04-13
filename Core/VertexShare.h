#ifndef __VERTEX_SHARE_H__
#define __VERTEX_SHARE_H__

#define USE_HASHING	1		// can disable this to compare performance

// structure which helps to share vertices between wedges
struct CVertexShare
{
	TArray<CVec3>	Points;
	TArray<CVec3>	Normals;
	TArray<int>		WedgeToVert;
	TArray<int>		VertToWedge;
	int				WedgeIndex;

#if USE_HASHING
	// hashing
	CVec3			Mins, Maxs;
	CVec3			Extents;
	int				Hash[1024];
	TArray<int>		HashNext;
#endif // USE_HASHING

	void Prepare(const CMeshVertex *Verts, int NumVerts, int VertexSize)
	{
		WedgeIndex = 0;
		Points.Empty(NumVerts);
		Normals.Empty(NumVerts);
		WedgeToVert.Empty(NumVerts);
		VertToWedge.Empty(NumVerts);
		VertToWedge.Add(NumVerts);
#if USE_HASHING
		// compute bounds for better hashing
		ComputeBounds(&Verts->Position, NumVerts, VertexSize, Mins, Maxs);
		VectorSubtract(Maxs, Mins, Extents);
		Extents[0] += 1; Extents[1] += 1; Extents[2] += 1; // avoid zero divide
		HashNext.Empty(NumVerts);
		HashNext.Add(NumVerts);
		// initialize Hash and HashNext with -1
		memset(Hash, -1, sizeof(Hash));
		for (int i = 0; i < NumVerts; i++)
			HashNext[i] = -1;
#endif // USE_HASHING
	}

	int AddVertex(const CVec3 &Pos, const CVec3 &Normal)
	{
		int PointIndex = -1;

#if USE_HASHING
		// compute hash
		int h = appFloor(
			( (Pos[0] - Mins[0]) / Extents[0] + (Pos[1] - Mins[1]) / Extents[1] + (Pos[2] - Mins[2]) / Extents[2] ) // 0..3
			* (ARRAY_COUNT(Hash) / 3.0f * 16)			// multiply to 16 for better spreading inside Hash array
		) % ARRAY_COUNT(Hash);
		// find point with the same position and normal
		for (PointIndex = Hash[h]; PointIndex >= 0; PointIndex = HashNext[PointIndex])
		{
			if (Points[PointIndex] == Pos && Normals[PointIndex] == Normal)
				break;		// found it
		}
#else
		// find wedge with the same position and normal
		while (true)
		{
			PointIndex = Points.FindItem(Pos, PointIndex + 1);
			if (PointIndex == INDEX_NONE) break;
			if (Normals[PointIndex] == Normal) break;
		}
#endif // USE_HASHING
		if (PointIndex == INDEX_NONE)
		{
			// point was not found - create it
			PointIndex = Points.AddItem(Pos);
			Normals.AddItem(Normal);
#if USE_HASHING
			// add to Hash
			HashNext[PointIndex] = Hash[h];
			Hash[h] = PointIndex;
#endif // USE_HASHING
		}

		// remember vertex <-> wedge map
		WedgeToVert.AddItem(PointIndex);
		VertToWedge[PointIndex] = WedgeIndex++;

		return PointIndex;
	}
};

#endif // __VERTEX_SHARE_H__
