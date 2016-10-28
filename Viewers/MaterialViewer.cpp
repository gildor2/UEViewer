#include "Core.h"
#include "UnrealClasses.h"
#include "MathSSE.h"			// for CVec4

#if RENDERING

#include "UnMaterial2.h"
#include "UnMaterial3.h"

#include "ObjectViewer.h"


#define NEW_OUTLINE		1


bool CMaterialViewer::ShowOutline = false;
bool CMaterialViewer::ShowChannels = false;
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
	case 'c':
		ShowChannels = !ShowChannels;
		break;
	default:
		CObjectViewer::ProcessKey(key);
	}
	unguard;
}


void CMaterialViewer::ShowHelp()
{
	CObjectViewer::ShowHelp();
	DrawKeyHelp("M", "show material graph");
	if (IsTexture)
		DrawKeyHelp("C", "show texture channels");
}


void CMaterialViewer::Draw3D(float TimeDelta)
{
	if (IsTexture && ShowChannels) return;

	static const CVec3 origin = { -150, 100, 100 };
//	static const CVec3 origin = { -150, 50, 50 };
	CVec3 lightPosV;
	viewAxis.UnTransformVector(origin, lightPosV);

#if 0
	// show light source
	glDisable(GL_LIGHTING);
	BindDefaultMaterial(true);
	glBegin(GL_LINES);
	glColor3f(1, 0, 0);
	CVec3 tmp;
	tmp = lightPosV;
	tmp[0] -= 20; glVertex3fv(tmp.v); tmp[0] += 40; glVertex3fv(tmp.v);
	tmp = lightPosV;
	tmp[1] -= 20; glVertex3fv(tmp.v); tmp[1] += 40; glVertex3fv(tmp.v);
	tmp = lightPosV;
	tmp[2] -= 20; glVertex3fv(tmp.v); tmp[2] += 40; glVertex3fv(tmp.v);
	glEnd();
#endif

	glColor3f(1, 1, 1);

	if (!IsTexture)
	{
		glEnable(GL_LIGHTING);	// no lighting for textures
		float lightPos[4];
		lightPos[0] = lightPosV[0];
		lightPos[1] = lightPosV[1];
		lightPos[2] = lightPosV[2];
		lightPos[3] = 0;
		glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
//		glMaterialf(GL_FRONT, GL_SHININESS, 20);
	}

	// bind material
	UUnrealMaterial *Mat = static_cast<UUnrealMaterial*>(Object);
	Mat->SetMaterial();

	// check tangent space
	GLint aNormal = -1;
	GLint aTangent = -1;
//	GLint aBinormal = -1;
	const CShader *Sh = GCurrentShader;
	if (Sh)
	{
		aNormal    = Sh->GetAttrib("normal");
		aTangent   = Sh->GetAttrib("tangent");
//		aBinormal  = Sh->GetAttrib("binormal");
	}

	// and draw box ...
#define A 100
// vertex
#define V000 {-A, -A, -A}
#define V001 {-A, -A,  A}
#define V010 {-A,  A, -A}
#define V011 {-A,  A,  A}
#define V100 { A, -A, -A}
#define V101 { A, -A,  A}
#define V110 { A,  A, -A}
#define V111 { A,  A,  A}
	static const CVec3 box[] =
	{
		V001, V000, V010, V011,		// near   (x=-A)
		V111, V110,	V100, V101,		// far    (x=+A)
		V101, V100, V000, V001,		// left   (y=-A)
		V011, V010, V110, V111,		// right  (y=+A)
		V010, V000, V100, V110,		// bottom (z=-A)
		V001, V011, V111, V101,		// top    (z=+A)
#undef A
	};
#define REP4(...)	{__VA_ARGS__},{__VA_ARGS__},{__VA_ARGS__},{__VA_ARGS__}
	static const CVec4 normal[] =
	{
		REP4(-1, 0, 0, 1 ),
		REP4( 1, 0, 0, 1 ),
		REP4( 0,-1, 0, 1 ),
		REP4( 0, 1, 0, 1 ),
		REP4( 0, 0,-1, 1 ),
		REP4( 0, 0, 1, 1 )
	};
	static const CVec3 tangent[] =
	{
		REP4( 0,-1, 0 ),
		REP4( 0, 1, 0 ),
		REP4( 1, 0, 0 ),
		REP4(-1, 0, 0 ),
		REP4( 1, 0, 0 ),
		REP4(-1, 0, 0 )
	};
//	static const CVec3 binormal[] =
//	{
//		REP4( 0, 0, 1 ),
//		REP4( 0, 0, 1 ),
//		REP4( 0, 0, 1 ),
//		REP4( 0, 0, 1 ),
//		REP4( 0,-1, 0 ),
//		REP4( 0,-1, 0 )
//	};
#undef REP4
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

#if 0
	// verify tangents, should be suitable for binormal computation in shaders
	// (note: we're not verifying correspondence with UV coordinates)
	for (int i = 0; i < 24; i++)
	{
		CVec4 n4 = normal[i];
		CVec3 n = n4.ToVec3();
		CVec3 t = tangent[i];
		CVec3 b = binormal[i];
		CVec3 b2;
		cross(n, t, b2);
		VectorScale(b2, n4[3], b2);
		float dd = VectorDistance(b2, b);
		if (dd > 0.001f) appPrintf("dist[%d] = %g\n", i, dd);
	}
#endif

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	glVertexPointer(3, GL_FLOAT, sizeof(CVec3), box);
	glNormalPointer(GL_FLOAT, sizeof(CVec4), normal);
	glTexCoordPointer(2, GL_FLOAT, 0, tex);

	if (aNormal >= 0)
	{
		glEnableVertexAttribArray(aNormal);
		// send 4 components to decode binormal in shader
		glVertexAttribPointer(aNormal, 4, GL_FLOAT, GL_FALSE, sizeof(CVec4), normal);
	}
	if (aTangent >= 0)
	{
		glEnableVertexAttribArray(aTangent);
		glVertexAttribPointer(aTangent,  3, GL_FLOAT, GL_FALSE, sizeof(CVec3), tangent);
	}
//	if (aBinormal >= 0)
//	{
//		glEnableVertexAttribArray(aBinormal);
//		glVertexAttribPointer(aBinormal, 3, GL_FLOAT, GL_FALSE, sizeof(CVec3), binormal);
//	}

	glDrawElements(GL_QUADS, ARRAY_COUNT(inds), GL_UNSIGNED_INT, inds);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	// disable tangents
	if (aNormal >= 0)
		glDisableVertexAttribArray(aNormal);
	if (aTangent >= 0)
		glDisableVertexAttribArray(aTangent);
//	if (aBinormal >= 0)
//		glDisableVertexAttribArray(aBinormal);

	BindDefaultMaterial(true);

#if 0
	glBegin(GL_LINES);
	glColor3f(0.2, 0.2, 1);
	for (int i = 0; i < ARRAY_COUNT(box); i++)
	{
		glVertex3fv(box[i].v);
		CVec3 tmp;
		VectorMA(box[i], 20, normal[i], tmp);
		glVertex3fv(tmp.v);
	}
	glEnd();
	glColor3f(1, 1, 1);
#endif
}


void CMaterialViewer::Draw2D()
{
	guard(CMaterialViewer::Draw2D);
	CObjectViewer::Draw2D();

	if (IsTexture && ShowChannels)
	{
		UUnrealMaterial *Mat = static_cast<UUnrealMaterial*>(Object);
		Mat->SetMaterial();
		int width, height;
		Window->GetWindowSize(width, height);
		int w = min(width, height) / 2;
		for (int i = 0; i < 4; i++)
		{
			int x0 =  (i       & 1) * w;
			int y0 = ((i >> 1) & 1) * w;
			static const CVec3 colors[4] =
			{
				{ 1, 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }	// normal, red, green, blue
			};
			glColor3fv(colors[i].v);
			glBegin(GL_QUADS);
			glTexCoord2f(1, 0);
			glVertex2f(x0+w, y0);
			glTexCoord2f(0, 0);
			glVertex2f(x0, y0);
			glTexCoord2f(0, 1);
			glVertex2f(x0, y0+w);
			glTexCoord2f(1, 1);
			glVertex2f(x0+w, y0+w);
			glEnd();
		}
	}

	if (Object->IsA("BitmapMaterial"))
	{
		const UBitmapMaterial *Tex = static_cast<UBitmapMaterial*>(Object);

		const char *fmt = EnumToName(Tex->Format);
		DrawTextLeft(S_GREEN "Width   :" S_WHITE " %d\n"
					 S_GREEN "Height  :" S_WHITE " %d\n"
					 S_GREEN "Format  :" S_WHITE " %s",
					 Tex->USize, Tex->VSize,
					 fmt ? fmt : "???");
	}

#if UNREAL3
	if (Object->IsA("Texture2D"))
	{
		const UTexture2D *Tex = static_cast<UTexture2D*>(Object);

		const char *fmt = EnumToName(Tex->Format);
		DrawTextLeft(S_GREEN "Width   :" S_WHITE " %d\n"
					 S_GREEN "Height  :" S_WHITE " %d\n"
					 S_GREEN "Format  :" S_WHITE " %s\n"
					 S_GREEN "TFCName :" S_WHITE " %s",
					 Tex->SizeX, Tex->SizeY,
					 fmt ? fmt : "???", *Tex->TextureFileCacheName);
		// get first available mipmap to find its size
		//!! todo: use CTextureData for this to avoid any code copy-pastes
		//!! also, display real texture format (TPF_...), again - with CTextureData use
		const TArray<FTexture2DMipMap> *MipsArray = Tex->GetMipmapArray();
		const FTexture2DMipMap *Mip = NULL;
		for (int i = 0; i < MipsArray->Num(); i++)
			if ((*MipsArray)[i].Data.BulkData)
			{
				Mip = &(*MipsArray)[i];
				break;
			}
		int width = 0, height = 0;
		if (Mip)
		{
			width  = Mip->SizeX;
			height = Mip->SizeY;
		}
		if (width != Tex->SizeX || height != Tex->SizeY)
		{
			if (width && height)
				DrawTextLeft(S_RED"Stripped to %dx%d", width, height);
			else
				DrawTextLeft(S_RED"Bad texture (no mipmaps)");
		}
	}
#endif // UNREAL3

	if (ShowOutline)
	{
		DrawTextLeft("");
		OutlineMaterial(Object);
	}

	unguard;
}


CMaterialViewer::CMaterialViewer(UUnrealMaterial* Material, CApplication* Window)
:	CObjectViewer(Material, Window)
{
	IsTexture = Material->IsTexture();
	Material->Lock();
}

CMaterialViewer::~CMaterialViewer()
{
	guard(CMaterialViewer::~);
	UUnrealMaterial *Mat = static_cast<UUnrealMaterial*>(Object);
	Mat->Unlock();
	unguard;
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
		Outline(S_RED "------------------------------------------");
#endif
		Outline(S_GREEN "%s", propNames);
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
	int i;

	InitProps(indent == 0);
	int oldIndent = textIndent;
	textIndent = indent;

	Outline(S_RED "%s'%s'", Obj->GetClassName(), Obj->Name);

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
#if BIOSHOCK
		PROP(NormalMap)
#endif
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
		PROP2(BlendMode, EBlendMode)
		for (int i = 0; i < Mat->ReferencedTextures.Num(); i++)
		{
			const UTexture3 *Tex = Mat->ReferencedTextures[i];
			if (!Tex) continue;
			Outline("Textures[%d] = %s", i, Tex->Name);
		}
	MAT_END

	MAT_BEGIN(UMaterialInstanceConstant)
		PROP(Parent)
		// texture
		if (Mat->TextureParameterValues.Num()) Outline(S_YELLOW"Texture parameters:");
		for (i = 0; i < Mat->TextureParameterValues.Num(); i++)
		{
			const FTextureParameterValue &P = Mat->TextureParameterValues[i];
			Outline("%s = %s", *P.ParameterName, P.ParameterValue ? P.ParameterValue->Name : "NULL");
		}
		// scalar
		if (Mat->ScalarParameterValues.Num()) Outline(S_YELLOW"Scalar parameters");
		for (i = 0; i < Mat->ScalarParameterValues.Num(); i++)
		{
			const FScalarParameterValue &P = Mat->ScalarParameterValues[i];
			Outline("%s = %g", *P.ParameterName, P.ParameterValue);
		}
		// vector
		if (Mat->VectorParameterValues.Num()) Outline(S_YELLOW"Vector parameters");
		for (i = 0; i < Mat->VectorParameterValues.Num(); i++)
		{
			const FVectorParameterValue &P = Mat->VectorParameterValues[i];
			Outline("%s = %g %g %g %g", *P.ParameterName, FCOLOR_ARG(P.ParameterValue));
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

#endif // RENDERING
