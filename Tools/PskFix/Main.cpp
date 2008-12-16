#include "Core.h"
#include "UnrealClasses.h"
#include "../../Exporters/Psk.h"


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
	// display usage
	if (argc < 1)
	{
	help:
		printf(	"PSK skeleton rotator\n"
				"Usage: pskfix <original psk> [<modified psk>]\n"
		);
		exit(0);
	}

	// parse command line
	int arg = 1;
	const char *argSrcName = argv[arg];
	if (!argSrcName) goto help;
	const char *argDstName = argv[arg+1];
	if (!argDstName) argDstName = argSrcName;

	FILE *f;

	// read psk file
	printf("Loading file %s ...\n", argSrcName);
	f = fopen(argSrcName, "rb");
	if (!f)
	{
		printf("ERROR: unable to open source file\n");
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	int size = ftell(f);
	fseek(f, 0, SEEK_SET);

	byte *buf = new byte[size];
	if (fread(buf, size, 1, f) != 1)
	{
		printf("ERROR: unable to read file\n");
		exit(1);
	}
	fclose(f);

	// process file
	const VChunkHeader *Hdr = (VChunkHeader*)buf;
	if (strcmp(Hdr->ChunkID, "ACTRHEAD") != 0)
	{
		printf("ERROR: wrong file format\n");
		exit(1);
	}
	Hdr++;		// skip ACTRHEAD chunk

	// find chunks
	static struct
	{
		const char *Name;
		int        TypeSize;
		void       *Data;
		int        Count;
	} chunks[] =
	{
		{ "PNTS0000", sizeof(FVector)   },		// [0]
		{ "VTXW0000", sizeof(VVertex)   },		// [1]
		{ "FACE0000", sizeof(VTriangle) },		// [2]
		{ "MATT0000", sizeof(VMaterial) },		// [3]
		{ "REFSKELT", sizeof(VBone)     }		// [4]
	};
	int i;
	for (i = 0; i < ARRAY_COUNT(chunks); i++)
	{
		if (strcmp(Hdr->ChunkID, chunks[i].Name) != 0)
		{
			printf("ERROR: wrong chunk ID \"%s\", expecting \"%s\"\n", Hdr->ChunkID, chunks[i].Name);
			exit(1);
		}
		if (Hdr->DataSize != chunks[i].TypeSize)
		{
			printf("ERROR: wrong data size in chunk \"%s\": have %d, expecting %d\n", Hdr->ChunkID, Hdr->DataSize, chunks[i].TypeSize);
			exit(1);
		}

		int skipSize = Hdr->DataSize * Hdr->DataCount;
		chunks[i].Data  = (void*) (Hdr+1);
		chunks[i].Count = Hdr->DataCount;
		Hdr = (VChunkHeader*) ((byte*)chunks[i].Data + skipSize);
	}

	// process bones
	VBone *B = (VBone*) chunks[4].Data;
	int numBones = chunks[4].Count;
	printf("Processing %d bones ...\n", numBones);
	for (i = 0; i < numBones; i++, B++)
	{
		if (i == 0)
		{
			static const CQuat rot = { 0, 0.707107, 0.707107, 0 };
			CQuat &Q = (CQuat&) B->BonePos.Orientation;
			Q.y *= -1; Q.w *= -1;
			Q.Mul(rot);
			Q.y *= -1; //Q.w *= -1;
			Q.x *= -1;
//			Q.w *= -1;
			Exchange(Q.x, Q.y);
			Exchange(Q.z, Q.w);
			Q.z *= -1;
			Q.w *= -1;
		}
		else
		{
			VBone BO = *B;
			B->BonePos.Position.X    =  BO.BonePos.Position.X;
			B->BonePos.Position.Y    =  BO.BonePos.Position.Z;		//-Z
			B->BonePos.Position.Z    = -BO.BonePos.Position.Y;		//+Y
			B->BonePos.Orientation.X =  BO.BonePos.Orientation.X;
			B->BonePos.Orientation.Y =  BO.BonePos.Orientation.Z;	//-Z
			B->BonePos.Orientation.Z = -BO.BonePos.Orientation.Y;	//+Y
			B->BonePos.Orientation.W =  BO.BonePos.Orientation.W;
		}
	}
	// save file
	printf("Saving file %s ...\n", argDstName);
	f = fopen(argDstName, "wb");
	if (!f)
	{
		printf("ERROR: unable to open destination file\n");
		exit(1);
	}
	if (fwrite(buf, size, 1, f) != 1)
	{
		printf("ERROR: unable to write file\n");
		exit(1);
	}
	fclose(f);

	// cleanup
	delete buf;

	return 0;
}
