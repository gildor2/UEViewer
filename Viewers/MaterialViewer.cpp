#include "Core.h"
#include "UnrealClasses.h"

#include "ObjectViewer.h"


#define NEW_OUTLINE		1


bool CMaterialViewer::ShowOutline = false;
static void OutlineMaterial(UObject *Obj, int indent = 0);


/*-----------------------------------------------------------------------------
	Main code
-----------------------------------------------------------------------------*/

void CMaterialViewer::ProcessKey(int key)
{
	guard(CObjectViewer::ProcessKey);
	switch (key)
	{
	case 'm':
		ShowOutline = !ShowOutline;
		break;
	default:
		CObjectViewer::ProcessKey(key);
	}
	unguard;
}


void CMaterialViewer::ShowHelp()
{
	CObjectViewer::ShowHelp();
	DrawTextLeft("M           show material graph");
}


void CMaterialViewer::Draw3D()
{
	glColor4f(1, 1, 1, 1);
	// bind material
	UUnrealMaterial *Mat = static_cast<UUnrealMaterial*>(Object);
	Mat->Bind(0);
	// and draw box
	static const CVec3 box[] =
	{
#define A 100
#define V000 {-A, -A, -A}
#define V001 {-A, -A,  A}
#define V010 {-A,  A, -A}
#define V011 {-A,  A,  A}
#define V100 { A, -A, -A}
#define V101 { A, -A,  A}
#define V110 { A,  A, -A}
#define V111 { A,  A,  A}
		V001, V000, V010, V011,		// near   (x=0)
		V111, V110,	V100, V101,		// far    (x=1)
		V101, V100, V000, V001,		// left   (y=0)
		V011, V010, V110, V111,		// right  (y=1)
		V010, V000, V100, V110,		// bottom (z=0)
		V001, V011, V111, V101,		// top    (z=1)
#undef A
	};
	static const float tex[][2] =
	{
		{0, 0}, {0, 1}, {1, 1}, {1, 0},
		{0, 0}, {0, 1}, {1, 1}, {1, 0},
		{0, 0}, {0, 1}, {1, 1}, {1, 0},
		{0, 0}, {0, 1}, {1, 1}, {1, 0},
		{0, 0}, {0, 1}, {1, 1}, {1, 0},
		{0, 0}, {0, 1}, {1, 1}, {1, 0}
	};
	static const int inds[] =
	{
		 0, 1, 2, 3,
		 4, 5, 6, 7,
		 8, 9,10,11,
		12,13,14,15,
		16,17,18,19,
		20,21,22,23
	};

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(CVec3), box);
	glTexCoordPointer(2, GL_FLOAT, 0, tex);
	glDrawElements(GL_QUADS, ARRAY_COUNT(inds), GL_UNSIGNED_INT, inds);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_TEXTURE_2D);
}


void CMaterialViewer::Draw2D()
{
	guard(CMaterialViewer::Draw2D);
	CObjectViewer::Draw2D();

	if (Object->IsA("BitmapMaterial"))
	{
		const UBitmapMaterial *Tex = static_cast<UBitmapMaterial*>(Object);

		const char *fmt = EnumToName("ETextureFormat", Tex->Format);
		DrawTextLeft(S_GREEN"Width  :"S_WHITE" %d\n"
					 S_GREEN"Height :"S_WHITE" %d\n"
					 S_GREEN"Format :"S_WHITE" %s",
					 Tex->USize, Tex->VSize, fmt ? fmt : "???");
	}

#if UNREAL3
	if (Object->IsA("Texture2D"))
	{
		const UTexture2D *Tex = static_cast<UTexture2D*>(Object);

		const char *fmt = EnumToName("EPixelFormat", Tex->Format);
		DrawTextLeft(S_GREEN"Width  :"S_WHITE" %d\n"
					 S_GREEN"Height :"S_WHITE" %d\n"
					 S_GREEN"Format :"S_WHITE" %s",
					 Tex->SizeX, Tex->SizeY, fmt ? fmt : "???");
	}
#endif // UNREAL3

	if (ShowOutline)
	{
		DrawTextLeft("");
		OutlineMaterial(Object);
	}

	unguard;
}


CMaterialViewer::~CMaterialViewer()
{
	UUnrealMaterial *Mat = static_cast<UUnrealMaterial*>(Object);
	Mat->Release();
}


/*-----------------------------------------------------------------------------
	Displaying material graph
-----------------------------------------------------------------------------*/

static int textIndent, prevIndent;
#if NEW_OUTLINE
static bool levelFinished[256];
#endif

static void Outline(const char *fmt, ...)
{
	char buf[1024];
	int offset = 0;
//	buf[offset++] = textIndent + '0'; buf[offset++] = ':';	//??
//	buf[offset++] = prevIndent + '0'; buf[offset++] = ':';	//??
#if !NEW_OUTLINE
	buf[offset++] = '^'; buf[offset++] = '1';	// S_RED
#else
	buf[offset++] = '^'; buf[offset++] = '6';	// S_CYAN
#endif
	for (int level = 0; level < textIndent; level++)
	{
#if !NEW_OUTLINE
		if (level < textIndent-1 || textIndent <= prevIndent || !fmt[0])
		{
			buf[offset++] = '|';
			buf[offset++] = ' ';
		}
		else
		{
			buf[offset++] = '+';
			buf[offset++] = '-';
		}
#else
		static const char *data[] = {
			"+-",
			"| ",
			"*-",
			"  ",
		};
		const char *line;
		bool finished = levelFinished[level];
		if (level < textIndent-1)
			line = finished ? data[3] : data[1];	// intermediate level
		else if (!fmt[0])
			line = data[1];							// last level, no text
		else if (textIndent > prevIndent)
			line = finished ? data[2] : data[0];	// last level with text, first line
		else
			line = finished ? data[3] : data[1];	// last level with text, following lines
		buf[offset++] = line[0];
		buf[offset++] = line[1];
#endif
		buf[offset] = buf[offset-1]; offset++;
//		buf[offset] = buf[offset-1]; offset++;
#if NEW_OUTLINE
		buf[offset++] = ' ';
#endif
	}
	buf[offset++] = '^'; buf[offset++] = '7';	// S_WHITE
	if (fmt[0] || prevIndent > textIndent)
		prevIndent = textIndent;
	if (!fmt[0])
		prevIndent = 0;

	va_list	argptr;
	va_start(argptr, fmt);
	vsnprintf(buf + offset, ARRAY_COUNT(buf) - offset, fmt, argptr);
	va_end(argptr);

	DrawTextLeft("%s", buf);
}


static char propBuf[1024];

#define MAX_LINKS			256

static const char      *linkNames[MAX_LINKS];
static UUnrealMaterial *links[MAX_LINKS];
static int             firstLink, numLinks;

inline void InitProps(bool firstLevel)
{
	propBuf[0] = 0;
	numLinks   = 0;
	if (firstLevel)
	{
		firstLink  = 0;
		prevIndent = 0;
	}
}

inline void FlushProps()
{
	// print ordinary props
	if (propBuf[0])
		Outline("%s", propBuf);
	// print material links, links[] array may be modified while printing!
	int savedFirstLink = firstLink;
	int lastLink = firstLink + numLinks;
	firstLink = lastLink;				// for nested material properties
	bool linkUsed[MAX_LINKS];
	memset(linkUsed, 0, sizeof(linkUsed));
	for (int i = savedFirstLink; i < lastLink; i++)
	{
		//!! find duplicates
		if (linkUsed[i]) continue;		// already dumped

		// find same properties
		char propNames[1024];
		strcpy(propNames, linkNames[i]);
#if NEW_OUTLINE
		levelFinished[textIndent] = true;
#endif
		for (int j = i + 1; j < lastLink; j++)
		{
			if (links[j] == links[i])
			{
				linkUsed[j] = true;
				appStrcatn(ARRAY_ARG(propNames), ", ");
				appStrcatn(ARRAY_ARG(propNames), linkNames[j]);
			}
#if NEW_OUTLINE
			if (!linkUsed[j]) levelFinished[textIndent] = false; // has other props on the same level
#endif
		}

		textIndent++;
#if NEW_OUTLINE
		Outline("");
#else
		Outline(S_RED"------------------------------------------");
#endif
		Outline(S_GREEN"%s", propNames);
		OutlineMaterial(links[i], textIndent);	//?? textIndent
		textIndent--;
	}
	// restore data
	firstLink = savedFirstLink;
}

static void Prop(bool value, const char *name)
{
	if (!value) return;
	if (propBuf[0])
		appStrcatn(ARRAY_ARG(propBuf), ", ");
	appStrcatn(ARRAY_ARG(propBuf), name);
}

static void Prop(UUnrealMaterial *value, const char *name)
{
	if (!value) return;
	assert(firstLink + numLinks < ARRAY_COUNT(linkNames));

	links    [firstLink + numLinks] = value;
	linkNames[firstLink + numLinks] = name;
	numLinks++;
}

static void PropEnum(int value, const char *name, const char *EnumName)
{
	const char *n = EnumToName(EnumName, value);
	Outline("%s = %s (%d)", name, n ? n : "???", value);
}


//?? rename indent -> level
static void OutlineMaterial(UObject *Obj, int indent)
{
	guard(OutlineMaterial);
	assert(Obj);

	InitProps(indent == 0);
	int oldIndent = textIndent;
	textIndent = indent;

	Outline(S_RED"%s'%s'", Obj->GetClassName(), Obj->Name);

	bool processed = false;
#define MAT_BEGIN(ClassName)	\
	if (Obj->IsA(#ClassName+1)) \
	{							\
		guard(ClassName);		\
		processed = true;		\
		ClassName *Mat = static_cast<ClassName*>(Obj);

#define MAT_END					\
		unguard;				\
	}

#define PROP(name)				\
		Prop(Mat->name, #name);

#define PROP2(name,type)		\
		PropEnum(Mat->name, #name, #type);

	MAT_BEGIN(UTexture)
		PROP(bMasked)
		PROP(bAlphaTexture)
		PROP(bTwoSided)
	MAT_END

	MAT_BEGIN(UFinalBlend)
		PROP2(FrameBufferBlending, EFrameBufferBlending)
		PROP(ZWrite)
		PROP(ZTest)
		PROP(AlphaTest)
		PROP(TwoSided)
	MAT_END

	MAT_BEGIN(UModifier)
		PROP(Material)
	MAT_END

	MAT_BEGIN(UShader)
		PROP2(OutputBlending, EOutputBlending)
		PROP(TwoSided)
		PROP(Wireframe)
		PROP(ModulateStaticLighting2X)
		PROP(PerformLightingOnSpecularPass)
		PROP(ModulateSpecular2X)
		// materials
		PROP(Diffuse)
		PROP(Opacity)
		PROP(Specular)
		PROP(SpecularityMask)
		PROP(SelfIllumination)
		PROP(SelfIlluminationMask)
	MAT_END

	MAT_BEGIN(UCombiner)
		PROP2(CombineOperation, EColorOperation)
		PROP2(AlphaOperation, EAlphaOperation)
		PROP(InvertMask)
		PROP(Modulate2X)
		PROP(Modulate4X)
		PROP(Material1)
		PROP(Material2)
		PROP(Mask)
	MAT_END

#if UNREAL3
	MAT_BEGIN(UMaterial3)
		PROP(TwoSided)
		PROP(bDisableDepthTest)
		PROP(bIsMasked)
		for (int i = 0; i < Mat->ReferencedTextures.Num(); i++)
		{
			const UTexture3 *Tex = Mat->ReferencedTextures[i];
			if (!Tex) continue;
			Outline("Textures[%d] = %s", i, Tex->Name);
		}
	MAT_END

	MAT_BEGIN(UMaterialInstanceConstant)
		for (int i = 0; i < Mat->TextureParameterValues.Num(); i++)
		{
			const FTextureParameterValue &P = Mat->TextureParameterValues[i];
			Outline("%s = %s", *P.ParameterName, P.ParameterValue ? P.ParameterValue->Name : "NULL");
		}
	MAT_END
#endif // UNREAL3

	if (!processed)
	{
		//!! unknown material
	}
	FlushProps();
	textIndent = oldIndent;

	unguard;
}
