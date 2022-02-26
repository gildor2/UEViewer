#include "Core.h"
#include "UnCore.h"
#include "MathSSE.h"			// for CVec4

#if RENDERING

#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMaterial/UnMaterial2.h"
#include "UnrealMaterial/UnMaterial3.h"

#include "ObjectViewer.h"

// For ColorFilter_ush
#include "Shaders.h"


#define NEW_OUTLINE		1


bool CMaterialViewer::ShowOutline = false;
bool CMaterialViewer::ShowChannels = false;
int CMaterialViewer::ShapeIndex = 0;


/*-----------------------------------------------------------------------------
	Main code
-----------------------------------------------------------------------------*/

void CMaterialViewer::ProcessKey(unsigned key)
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
	case 's':
		if (++ShapeIndex >= 2)
			ShapeIndex = 0;
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
	DrawKeyHelp("S", "toggle preview shape");
}

// This function could be used to debug mesh generation code
static void ShowNormals(const CVec3* Verts, const CVec4* Normals, const CVec3* Tangents, int NumVerts)
{
	glDisable(GL_LIGHTING);
	BindDefaultMaterial(true);

	glBegin(GL_LINES);
	glColor3f(0.5, 1, 0);
	CVec3 tmp;
	const float VisualLength = 5.0f;
	for (int i = 0; i < NumVerts; i++)
	{
		glVertex3fv(Verts[i].v);
		VectorMA(Verts[i], VisualLength, Normals[i], tmp);
		glVertex3fv(tmp.v);
	}
	glColor3f(0, 0.5f, 1);
	for (int i = 0; i < NumVerts; i++)
	{
		const CVec3 &v = Verts[i];
		glVertex3fv(v.v);
		VectorMA(v, VisualLength, Tangents[i], tmp);
		glVertex3fv(tmp.v);
	}
	glColor3f(1, 0, 0.5f);
	for (int i = 0; i < NumVerts; i++)
	{
		// decode binormal
		CVec3 binormal;
		cross(Normals[i], Tangents[i], binormal);
		binormal.Scale(Normals[i].v[3]);
		// render
		const CVec3 &v = Verts[i];
		glVertex3fv(v.v);
		VectorMA(v, VisualLength, binormal, tmp);
		glVertex3fv(tmp.v);
	}
	glEnd();
}

static void DrawBoxMesh(GLint aNormal, GLint aTangent)
{
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

	glVertexPointer(3, GL_FLOAT, sizeof(CVec3), box);
	glNormalPointer(GL_FLOAT, sizeof(CVec4), normal);
	glTexCoordPointer(2, GL_FLOAT, 0, tex);

	if (aNormal >= 0)
	{
		// send 4 components to decode binormal in shader
		glVertexAttribPointer(aNormal, 4, GL_FLOAT, GL_FALSE, sizeof(CVec4), normal);
	}
	if (aTangent >= 0)
	{
		glVertexAttribPointer(aTangent,  3, GL_FLOAT, GL_FALSE, sizeof(CVec3), tangent);
	}

	glDrawElements(GL_QUADS, ARRAY_COUNT(inds), GL_UNSIGNED_INT, inds);

//	ShowNormals(box, normal, tangent, ARRAY_COUNT(box));
}


static void DrawSphereMesh(GLint aNormal, GLint aTangent)
{
	// 'quality' corresponds to number of segments in 180 degrees, we have 180x360 degrees unwrapped sphere
	const int quality = 32;
	const float radius = 150.0f;
	const float invQuality = 1.0f / quality;
	const int vertsByX = quality * 2;
	const int vertsByY = quality + 1;
	const int numVerts = vertsByX * vertsByY;
	const int numTris = vertsByX * (vertsByY - 1) * 2;

	static CVec3 pos[numVerts];
	static float tex[numVerts*2];
	static CVec4 normal[numVerts];
	static CVec3 tangent[numVerts];
	static uint32 inds[numTris * 3];

	static bool initialized = false;
	if (!initialized)
	{
		// Build vertices
		int vertIndex = 0;
		for (int y = 0; y < vertsByY; y++)
		{
			for (int x = 0; x < vertsByX; x++, vertIndex++)
			{
				// vertex positions
				float longitude = (x * invQuality - 1.0f) * M_PI;
				float latitude = y * invQuality * M_PI;
				CVec3& p = pos[vertIndex];
				p[0] = radius * sin(longitude) * sin(latitude);
				p[1] = radius * cos(longitude) * sin(latitude);
				p[2] = radius * cos(latitude);

				// normal
				CVec4& n = normal[vertIndex];
				n[0] = sin(longitude) * sin(latitude);
				n[1] = cos(longitude) * sin(latitude);
				n[2] = cos(latitude);
				n[3] = 1.0f;

				// tangent
				CVec3& t = tangent[vertIndex];
				t[0] = sin(longitude+M_PI/2);
				t[1] = cos(longitude+M_PI/2);
				t[2] = 0;

				// texture coordinates
				tex[vertIndex*2  ] = x * invQuality;
				tex[vertIndex*2+1] = y * invQuality;
			}
		}
		assert(vertIndex == numVerts);

		// Build triangles
		int triangle = 0;
		for (int y = 0; y < vertsByY - 1; y++)
		{
			for (int x = 0; x < vertsByX; x++, triangle += 2)
			{
				int index1 = x + y * vertsByX;
				int index2 = (x + 1) % vertsByX + y * vertsByX; // next by X direction, wrap
				int index3 = index1 + vertsByX;					// next line
				int index4 = index2 + vertsByX;					// ...
				inds[triangle * 3 + 0] = index1;
				inds[triangle * 3 + 1] = index3;
				inds[triangle * 3 + 2] = index4;
				inds[triangle * 3 + 3] = index1;
				inds[triangle * 3 + 4] = index4;
				inds[triangle * 3 + 5] = index2;
			}
		}
		assert(triangle == numTris);

		initialized = true;
	}

	glVertexPointer(3, GL_FLOAT, sizeof(CVec3), pos);
	glNormalPointer(GL_FLOAT, sizeof(CVec4), normal);
	glTexCoordPointer(2, GL_FLOAT, 0, tex);

	if (aNormal >= 0)
	{
		// send 4 components to decode binormal in shader
		glVertexAttribPointer(aNormal, 4, GL_FLOAT, GL_FALSE, sizeof(CVec4), normal);
	}
	if (aTangent >= 0)
	{
		glVertexAttribPointer(aTangent,  3, GL_FLOAT, GL_FALSE, sizeof(CVec3), tangent);
	}

	glDrawElements(GL_TRIANGLES, numTris * 3, GL_UNSIGNED_INT, inds);

//	ShowNormals(pos, normal, tangent, numVerts);
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
	const UUnrealMaterial *Mat = static_cast<const UUnrealMaterial*>(Object);
	NonConstMaterial->SetMaterial();

	// check tangent space
	GLint aNormal = -1;
	GLint aTangent = -1;
	const CShader *Sh = GCurrentShader;
	if (Sh)
	{
		aNormal = Sh->GetAttrib("normal");
		aTangent = Sh->GetAttrib("tangent");
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	// enable tangents
	if (aNormal >= 0)
		glEnableVertexAttribArray(aNormal);
	if (aTangent >= 0)
		glEnableVertexAttribArray(aTangent);

	// draw a mesh ...
	switch (ShapeIndex)
	{
	case 0:
		DrawBoxMesh(aNormal, aTangent);
		break;
	case 1:
		DrawSphereMesh(aNormal, aTangent);
		break;
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	// disable tangents
	if (aNormal >= 0)
		glDisableVertexAttribArray(aNormal);
	if (aTangent >= 0)
		glDisableVertexAttribArray(aTangent);

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
		const UUnrealMaterial *Mat = static_cast<const UUnrealMaterial*>(Object);
		NonConstMaterial->SetMaterial();
		int width, height;
		Window->GetWindowSize(width, height);
		int w = min(width, height) / 2;

		static CShader shader;
		if (GUseGLSL)
		{
			if (!shader.IsValid()) shader.Make(ColorFilter_ush);
			shader.Use();
		}

		for (int i = 0; i < 4; i++)
		{
			int x0 =  (i       & 1) * w;
			int y0 = ((i >> 1) & 1) * w;
			static const CVec3 colors[4] =
			{
				{ 1, 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }	// normal, red, green, blue
			};
			if (GUseGLSL)
			{
				// Use color filter, so we could display RGB channels as colors, and alpha as white
				CVec4 filter, colorizer;
				filter.Set(0, 0, 0, 0);
				filter.v[i] = 1.0f;
				colorizer.Set(0, 0, 0, 0);
				if (i < 3)
				{
					colorizer.v[i] = 1.0f;
				}
				else
				{
					colorizer.Set(1, 1, 1, 1);
				}
				shader.SetUniform("FilterValue", filter);
				shader.SetUniform("Colorizer", colorizer);
				shader.SetUniform("Tex", 0);
			}
			else
			{
				glColor3fv(colors[i].v);
			}
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

		BindDefaultMaterial();
	}

	// UE1 and UE2
	if (Object->IsA("BitmapMaterial"))
	{
		const UBitmapMaterial* Tex = static_cast<const UBitmapMaterial*>(Object);

		const char *fmt = EnumToName(Tex->Format);
		DrawTextLeft(S_GREEN "Width   :" S_WHITE " %d\n"
					 S_GREEN "Height  :" S_WHITE " %d\n"
					 S_GREEN "Format  :" S_WHITE " %s",
					 Tex->USize, Tex->VSize,
					 fmt ? fmt : "???");

		if (Object->IsA("Texture"))
		{
			const UTexture* Tex2 = static_cast<const UTexture*>(Object);
			int width = 0, height = 0;
			// There are some textures which has mips with no data, display a message about stripped texture size.
			// Example: --ut2 DemoPlayerSkins.utx -obj=DemoSkeleton
			for (int i = 0; i < Tex2->Mips.Num(); i++)
			{
				const FMipmap& Mip = Tex2->Mips[i];
				if (Mip.DataArray.Num())
				{
					width = Mip.USize;
					height = Mip.VSize;
					break;
				}
			}
			if (width != Tex2->USize || height != Tex2->VSize)
			{
				if (width && height)
					DrawTextLeft(S_RED"Packaged size: %dx%d", width, height);
				else
					DrawTextLeft(S_RED"Bad texture (no mipmaps)");
			}
		}
	}

#if UNREAL3
	// UE3 and UE4
	if (Object->IsA("Texture2D"))
	{
		const UTexture2D *Tex = static_cast<const UTexture2D*>(Object);

		const char *fmt = EnumToName(Tex->Format);
		DrawTextLeft(S_GREEN "Width   :" S_WHITE " %d\n"
					 S_GREEN "Height  :" S_WHITE " %d\n"
					 S_GREEN "Format  :" S_WHITE " %s\n",
					 Tex->SizeX, Tex->SizeY,
					 fmt ? fmt : "???");
		if (stricmp(Tex->TextureFileCacheName, "None") != 0)
		{
			DrawTextLeft(S_GREEN "TFCName :" S_WHITE " %s", *Tex->TextureFileCacheName);
		}
		// get first available mipmap to find its size
		//!! todo: use CTextureData for this to avoid any code copy-pastes
		//!! also, display real texture format (TPF_...), again - with CTextureData use
		const TArray<FTexture2DMipMap> *MipsArray = Tex->GetMipmapArray();
		int width = 0, height = 0;
		for (int i = 0; i < MipsArray->Num(); i++)
		{
			const FTexture2DMipMap& Mip = (*MipsArray)[i];
			if (Mip.Data.BulkData)
			{
				width = Mip.SizeX;
				height = Mip.SizeY;
				break;
			}
		}
		if (width != Tex->SizeX || height != Tex->SizeY)
		{
			if (width && height)
				DrawTextLeft(S_RED"Cooked size: %dx%d", width, height);
			else if (Tex->SourceArt.BulkData && Tex->Source.bPNGCompressed)
				DrawTextLeft("Texture in PNG format");
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
	NonConstMaterial = const_cast<UUnrealMaterial*>(static_cast<const UUnrealMaterial*>(Object));

	Material->Lock();
}

CMaterialViewer::~CMaterialViewer()
{
	guard(CMaterialViewer::~);
	NonConstMaterial->Unlock();
	unguard;
}


/*-----------------------------------------------------------------------------
	Displaying material graph
-----------------------------------------------------------------------------*/

static int textIndent, prevIndent;
#if NEW_OUTLINE
static bool levelFinished[256];
#endif

static bool Outline(const char *fmt, ...)
{
	char buf[1024];
	int offset = 0;

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

	bool bClicked = false;
	if (strchr(fmt, S_HYPER_START) == NULL)
		DrawTextLeft("%s", buf);				// not a link
	else
		bClicked = DrawTextLeftH(NULL, "%s", buf); // link

	return bClicked;
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

void CMaterialViewer::FlushProps()
{
	guard(CMaterialViewer::FlushProps);

	// print ordinary props
	if (propBuf[0])
	{
		Outline("---"); // separator between parameter values and properties
		Outline("%s", propBuf);
	}
	// print material links, links[] array may be modified while printing!
	int savedFirstLink = firstLink;
	int lastLink = firstLink + numLinks;
	firstLink = lastLink;				// for nested material properties
	bool linkUsed[MAX_LINKS];
	memset(linkUsed, 0, sizeof(linkUsed));
	for (int i = savedFirstLink; i < lastLink; i++)
	{
		// find duplicates
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

	unguard;
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
	char buf[256];
	appSprintf(ARRAY_ARG(buf), "%s = %s (%d)", name, n ? n : "???", value);
	appStrcatn(ARRAY_ARG(propBuf), buf);
}


void CMaterialViewer::OutlineMaterial(const UObject *Obj, int indent)
{
	guard(CMaterialViewer::OutlineMaterial);
	assert(Obj);
	int i;

	InitProps(indent == 0);
	int oldIndent = textIndent;
	textIndent = indent;

	bool bHyperlink = indent != 0;
	if (!bHyperlink)
	{
		Outline(S_RED "%s'%s'", Obj->GetClassName(), Obj->Name);
	}
	else
	{
		bool bClicked = Outline(S_RED S_HYPERLINK("%s'%s'"), Obj->GetClassName(), Obj->Name);
		if (bClicked)
			JumpTo(Obj);
	}

#define MAT_BEGIN(ClassName)	\
	if (Obj->IsA(#ClassName+1)) \
	{							\
		guard(ClassName);		\
		const ClassName *Mat = static_cast<const ClassName*>(Obj);

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
			if (Outline("Textures[%d] = " S_HYPERLINK("%s"), i, Tex->Name))
				JumpTo(Tex);
		}
		// texture parameters
		if (Mat->CollectedTextureParameters.Num())
		{
			Outline(S_YELLOW"Texture parameters:");
			for (const CTextureParameterValue &P : Mat->CollectedTextureParameters)
				if (Outline("%s = " S_HYPERLINK("%s"), *P.Name, P.Texture ? P.Texture->Name : "NULL"))
					JumpTo(P.Texture);
		}
		// scalar
		if (Mat->CollectedScalarParameters.Num())
		{
			Outline(S_YELLOW"Scalar parameters:");
			for (const CScalarParameterValue &P : Mat->CollectedScalarParameters)
				Outline("%s = %g", *P.Name, P.Value);
		}
		// vector
		if (Mat->CollectedVectorParameters.Num())
		{
			Outline(S_YELLOW"Vector parameters:");
			for (const CVectorParameterValue &P : Mat->CollectedVectorParameters)
				Outline("%s = %g %g %g %g", *P.Name, FCOLOR_ARG(P.Value));
		}
	MAT_END

	MAT_BEGIN(UMaterialInstanceConstant)
		if (Mat->Parent != Mat)
		{
			PROP(Parent)
		}
		else
		{
			Outline(S_RED"Parent = SELF");
		}
		// texture
		if (Mat->TextureParameterValues.Num())
		{
			Outline(S_YELLOW"Texture parameters:");
			for (const FTextureParameterValue &P : Mat->TextureParameterValues)
				if (Outline("%s = " S_HYPERLINK("%s"), P.GetName(), P.ParameterValue ? P.ParameterValue->Name : "NULL"))
					JumpTo(P.ParameterValue);
		}
		// scalar
		if (Mat->ScalarParameterValues.Num())
		{
			Outline(S_YELLOW"Scalar parameters");
			for (const FScalarParameterValue &P : Mat->ScalarParameterValues)
				Outline("%s = %g", P.GetName(), P.ParameterValue);
		}
		// vector
		if (Mat->VectorParameterValues.Num())
		{
			Outline(S_YELLOW"Vector parameters");
			for (const FVectorParameterValue &P : Mat->VectorParameterValues)
				Outline("%s = %g %g %g %g", P.GetName(), FCOLOR_ARG(P.ParameterValue));
		}
	MAT_END
#endif // UNREAL3

	FlushProps();
	textIndent = oldIndent;

	unguard;
}

#endif // RENDERING
