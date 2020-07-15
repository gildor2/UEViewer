// This code is in the public domain -- castanyo@yahoo.es

#ifndef NV_IMAGE_IMAGE_H
#define NV_IMAGE_IMAGE_H

#include <nvcore/Debug.h>
#include <nvimage/nvimage.h>

namespace nv
{
	class Color32;

	/// 32 bit RGBA image.
	class NVIMAGE_CLASS Image
	{
	public:

		enum { Color32_Size = 4 };

		enum Format
		{
			Format_RGB,
			Format_ARGB,
		};

		Image();
		Image(const Image & img);
		~Image();

		const Image & operator=(const Image & img);


		void allocate(uint w, uint h);
		bool load(const char * name);

		void wrap(void * data, uint w, uint h);
		void unwrap();

		NV_FORCEINLINE uint width() const
		{
			return m_width;
		}
		NV_FORCEINLINE uint height() const
		{
			return m_height;
		}

		const Color32 * scanline(uint h) const;
		Color32 * scanline(uint h);

		NV_FORCEINLINE const Color32 * pixels() const
		{
			return m_data;
		}
		NV_FORCEINLINE Color32 * pixels()
		{
			return m_data;
		}

		NV_FORCEINLINE const Color32 & pixel(uint idx) const
		{
			nvDebugCheck(idx < m_width * m_height);
			// Work with undefined Color32
			// return m_data[idx];
			return *(Color32*) ((char*)m_data + idx * Color32_Size);
		}
		NV_FORCEINLINE Color32 & pixel(uint idx)
		{
			nvDebugCheck(idx < m_width * m_height);
			// Work with undefined Color32
			// return m_data[idx];
			return *(Color32*) ((char*)m_data + idx * Color32_Size);
		}

		const Color32 & pixel(uint x, uint y) const;
		Color32 & pixel(uint x, uint y);

		NV_FORCEINLINE Format format() const
		{
			return m_format;
		}
		void setFormat(Format f);

		void fill(Color32 c);

	private:
		void free();

	private:
		uint m_width;
		uint m_height;
		Format m_format;
		Color32 * m_data;
	};


	inline const Color32 & Image::pixel(uint x, uint y) const
	{
		nvDebugCheck(x < width() && y < height());
		return pixel(y * width() + x);
	}

	inline Color32 & Image::pixel(uint x, uint y)
	{
		nvDebugCheck(x < width() && y < height());
		return pixel(y * width() + x);
	}

} // nv namespace


#endif // NV_IMAGE_IMAGE_H
