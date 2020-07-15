// This code is in the public domain -- castanyo@yahoo.es

#include <nvcore/Debug.h>
#include <nvcore/Ptr.h>

#include <nvmath/Color.h>

#include <nvimage/Image.h>
#if !UMODEL
#include <nvimage/ImageIO.h>
#endif


using namespace nv;

static_assert(sizeof(Color32) == Image::Color32_Size, "Wrong Color32 size!");

Image::Image() : m_width(0), m_height(0), m_format(Format_RGB), m_data(NULL)
{
}

Image::Image(const Image & img) : m_data(NULL)
{
	allocate(img.m_width, img.m_height);
	m_format = img.m_format;
	memcpy(m_data, img.m_data, sizeof(Color32) * m_width * m_height);
}

Image::~Image()
{
	free();
}

const Image & Image::operator=(const Image & img)
{
	allocate(img.m_width, img.m_height);
	m_format = img.m_format;
	memcpy(m_data, img.m_data, sizeof(Color32) * m_width * m_height);
	return *this;
}


void Image::allocate(uint w, uint h)
{
	m_width = w;
	m_height = h;
	m_data = (Color32 *)realloc(m_data, w * h * sizeof(Color32));
}

#if !UMODEL
bool Image::load(const char * name)
{
	free();

	AutoPtr<Image> img(ImageIO::load(name));
	if (img == NULL) {
		return false;
	}

	swap(m_width, img->m_width);
	swap(m_height, img->m_height);
	swap(m_format, img->m_format);
	swap(m_data, img->m_data);

	return true;
}
#endif

void Image::wrap(void * data, uint w, uint h)
{
	free();
	m_data = (Color32 *)data;
	m_width = w;
	m_height = h;
}

void Image::unwrap()
{
	m_data = NULL;
	m_width = 0;
	m_height = 0;
}


void Image::free()
{
	::free(m_data);
	m_data = NULL;
}


const Color32 * Image::scanline(uint h) const
{
	nvDebugCheck(h < m_height);
	return m_data + h * m_width;
}

Color32 * Image::scanline(uint h)
{
	nvDebugCheck(h < m_height);
	return m_data + h * m_width;
}

void Image::setFormat(Image::Format f)
{
	m_format = f;
}

void Image::fill(Color32 c)
{
	const uint size = m_width * m_height;
	for (uint i = 0; i < size; ++i)
	{
		m_data[i] = c;
	}
}
