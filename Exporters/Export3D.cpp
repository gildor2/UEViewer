#include "Core.h"
#include "UnCore.h"

#include "UnObject.h"
#include "UnMesh2.h"

#include "Exporters.h"


static void ExportScript(const UVertMesh *Mesh, FArchive &Ar)
{
	int i, j;

	// mesh info
	Ar.Printf(
		"class %s extends Actor;\n\n"
		"#exec MESH IMPORT MESH=%s ANIVFILE=%s_a.3d DATAFILE=%s_d.3d\n"
		"#exec MESH ORIGIN MESH=%s X=%g Y=%g Z=%g YAW=%d PITCH=%d ROLL=%d\n\n",
		Mesh->Name,
		Mesh->Name, Mesh->Name, Mesh->Name,
		Mesh->Name, FVECTOR_ARG(Mesh->MeshOrigin),
			Mesh->RotOrigin.Yaw >> 8, Mesh->RotOrigin.Pitch >> 8, Mesh->RotOrigin.Roll >> 8
	);
	// animation sequences
	for (i = 0; i < Mesh->AnimSeqs.Num(); i++)
	{
		const FMeshAnimSeq &A = Mesh->AnimSeqs[i];
		Ar.Printf(
			"#exec MESH SEQUENCE MESH=%s SEQ=%-10s STARTFRAME=%-2d NUMFRAMES=%-2d",
			Mesh->Name, *A.Name, A.StartFrame, A.NumFrames
		);
		if (A.Rate != 1.0f && A.NumFrames > 1)
			Ar.Printf(" RATE=%g", A.Rate);
		if (A.Groups.Num())
			Ar.Printf(" GROUP=%s", *A.Groups[0]);
		Ar.Printf("\n");
	}
	// mesh scale
	Ar.Printf(
		"\n#exec MESHMAP SCALE MESHMAP=%s X=%g Y=%g Z=%g\n\n",
		Mesh->Name, FVECTOR_ARG(Mesh->MeshScale)
	);
	// notifys
	for (i = 0; i < Mesh->AnimSeqs.Num(); i++)
	{
		const FMeshAnimSeq &A = Mesh->AnimSeqs[i];
		for (j = 0; j < A.Notifys.Num(); j++)
		{
			const FMeshAnimNotify &N = A.Notifys[j];
			Ar.Printf(
				"#exec MESH NOTIFY MESH=%s SEQ=%-10s TIME=%g FUNCTION=%s\n",
				Mesh->Name, *A.Name, N.Time, *N.Function
			);
		}
	}
}


struct FJSDataHeader
{
	uint16			NumPolys;
	uint16			NumVertices;
	uint16			BogusRot;		// unused
	uint16			BogusFrame;		// unused
	unsigned		BogusNorm[3];	// unused
	unsigned		FixScale;		// unused
	unsigned		Unused[3];		// unused
		// 36 bytes
	unsigned		Unknown[3];		// not documented, pad?
		// 48 bytes

	friend FArchive& operator<<(FArchive &Ar, FJSDataHeader &H)
	{
		return Ar << H.NumPolys << H.NumVertices << H.BogusRot << H.BogusFrame <<
					 H.BogusNorm[0] << H.BogusNorm[1] << H.BogusNorm[2] << H.FixScale <<
					 H.Unused[0] << H.Unused[1] << H.Unused[2] <<
					 H.Unknown[0] << H.Unknown[1] << H.Unknown[2];
	}
};

// Mesh triangle.
struct FJSMeshTri
{
	uint16			iVertex[3];		// Vertex indices.
	byte			Type;			// James' mesh type. (unused)
	byte			Color;			// Color for flat and Gouraud shaded. (unused)
	FMeshUV1		Tex[3];			// Texture UV coordinates.
	byte			TextureNum;		// Source texture offset.
	byte			Flags;			// Unreal mesh flags (currently unused).

	friend FArchive& operator<<(FArchive &Ar, FJSMeshTri &T)
	{
		return Ar << T.iVertex[0] << T.iVertex[1] << T.iVertex[2] << T.Type << T.Color <<
					 T.Tex[0] << T.Tex[1] << T.Tex[2] << T.TextureNum << T.Flags;
	}
};

static void ExportMesh(const UVertMesh *Mesh, FArchive &Ar)
{
	// write header
	FJSDataHeader hdr;
	memset(&hdr, 0, sizeof(hdr));
	hdr.NumPolys    = Mesh->Faces.Num();
	hdr.NumVertices = Mesh->Wedges.Num();
	Ar << hdr;
	// write triangles
	FJSMeshTri tri;
	memset(&tri, 0, sizeof(tri));
	for (int i = 0; i < Mesh->Faces.Num(); i++)
	{
		const FMeshFace &F = Mesh->Faces[i];
		for (int j = 0; j < 3; j++)
		{
			const FMeshWedge &W = Mesh->Wedges[F.iWedge[j]];
			tri.iVertex[j] = F.iWedge[j]; //?? W.iVertex;
			tri.Tex[j].U   = appRound(W.TexUV.U * 255);
			tri.Tex[j].V   = appRound(W.TexUV.V * 255);
		}
		tri.TextureNum = F.MaterialIndex;
		tri.Flags      = 0;		//??
		Ar << tri;
	}
}


struct FJSAnivHeader
{
	uint16			NumFrames;		// Number of animation frames.
	uint16			FrameSize;		// Size of one frame of animation.

	friend FArchive& operator<<(FArchive &Ar, FJSAnivHeader &H)
	{
		return Ar << H.NumFrames << H.FrameSize;
	}
};

static void ExportAnims(const UVertMesh *Mesh, FArchive &Ar)
{
	// write header
	FJSAnivHeader hdr;
	hdr.NumFrames = Mesh->FrameCount;
	hdr.FrameSize = Mesh->Wedges.Num() * sizeof(FMeshVert);
	Ar << hdr;
	// write vertices
	for (int i = 0; i < Mesh->FrameCount; i++)
	{
		int base = Mesh->VertexCount * i;
		for (int j = 0; j < Mesh->Wedges.Num(); j++)
		{
			FMeshVert v = Mesh->Verts[base + Mesh->Wedges[j].iVertex]; // 'Mesh' is const, cannot use in serializer
			Ar << v;
		}
	}
}


void Export3D(const UVertMesh *Mesh)
{
	guard(Export3D);

	FArchive *Ar;

	// export script file
	if (GExportScripts)
	{
		Ar = CreateExportArchive(Mesh, "%s.uc", Mesh->Name);
		if (Ar)
		{
			ExportScript(Mesh, *Ar);
			delete Ar;
		}
	}
	// export mesh data
	Ar = CreateExportArchive(Mesh, "%s_d.3d", Mesh->Name);
	if (Ar)
	{
		ExportMesh(Mesh, *Ar);
		delete Ar;
	}
	// export animation frames
	Ar = CreateExportArchive(Mesh, "%s_a.3d", Mesh->Name);
	if (Ar)
	{
		ExportAnims(Mesh, *Ar);
		delete Ar;
	}

	unguard;
}
