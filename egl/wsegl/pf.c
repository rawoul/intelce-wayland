#include <wayland-util.h>

#include "wayland-wsegl.h"

#define PF(pf_g, pf_egl, bpp, has_alpha, renderable) \
	{ GDL_PF_##pf_g, GMA_PF_##pf_g, WSEGL_PIXELFORMAT_##pf_egl, \
		bpp, has_alpha, renderable }

static const struct wayland_pixel_format pixel_formats[] = {
	PF(ARGB_32,       ARGB8888,  4,  true,   true),
	PF(RGB_32,        XRGB8888,  4,  false,  true),
	PF(ARGB_16_1555,  ARGB1555,  2,  true,   true),
	PF(ARGB_16_4444,  ARGB4444,  2,  true,   true),
	PF(RGB_16,        RGB565,    2,  false,  true),
	PF(AY16,          88,    2,  true,   false),
	PF(A8,            8,     1,  true,   false),
};

const struct wayland_pixel_format *
convert_gdl_pixel_format(gdl_pixel_format_t pf)
{
	for (unsigned i = 0; i < ARRAY_LENGTH(pixel_formats); i++)
		if (pixel_formats[i].gdl_pf == pf)
			return &pixel_formats[i];

	return NULL;
}

const struct wayland_pixel_format *
convert_gma_pixel_format(gma_pixel_format_t pf)
{
	for (unsigned i = 0; i < ARRAY_LENGTH(pixel_formats); i++)
		if (pixel_formats[i].gma_pf == pf)
			return &pixel_formats[i];

	return NULL;
}

const struct wayland_pixel_format *
convert_wsegl_pixel_format(WSEGLPixelFormat pf)
{
	for (unsigned i = 0; i < ARRAY_LENGTH(pixel_formats); i++)
		if (pixel_formats[i].wsegl_pf == pf)
			return &pixel_formats[i];

	return NULL;
}
