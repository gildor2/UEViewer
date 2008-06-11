#include "ObjectViewer.h"


void CMaterialViewer::Draw3D()
{
	// bind material
	UMaterial *Mat = static_cast<UMaterial*>(Object);
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
