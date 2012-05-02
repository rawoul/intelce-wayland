#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "pixmap.h"

#define ARRAY_SIZE(x)	(sizeof (x) / sizeof (*(x)))

#define DECL_PF(pf) \
	{ GMA_PF_##pf, GDL_PF_##pf }

static const struct {
	gma_pixel_format_t gma_pf;
	gdl_pixel_format_t gdl_pf;
} pixel_formats[] = {
	DECL_PF(ARGB_32),
	DECL_PF(RGB_32),
	DECL_PF(ARGB_16_1555),
	DECL_PF(ARGB_16_4444),
	DECL_PF(RGB_16),
	DECL_PF(A8),
	DECL_PF(AY16),
};

static gma_pixel_format_t
convert_from_gdl_pf(gdl_pixel_format_t gdl_pf)
{
	for (unsigned i = 0; i < ARRAY_SIZE(pixel_formats); i++)
		if (gdl_pf == pixel_formats[i].gdl_pf)
			return pixel_formats[i].gma_pf;

	return -1;
}

static gdl_pixel_format_t
convert_to_gdl_pf(gma_pixel_format_t gma_pf)
{
	for (unsigned i = 0; i < ARRAY_SIZE(pixel_formats); i++)
		if (gma_pf == pixel_formats[i].gma_pf)
			return pixel_formats[i].gdl_pf;

	return -1;
}

gdl_pixel_format_t
gma_to_gdl_pixel_format(gma_pixel_format_t gma_pf)
{
	return convert_to_gdl_pf(gma_pf);
}

static gma_ret_t
convert_gdl_err(gdl_ret_t err)
{
	switch (err) {
	case GDL_SUCCESS:
		return GMA_SUCCESS;
	case GDL_ERR_NO_MEMORY:
		return GMA_ERR_NO_MEMORY;
	case GDL_ERR_NULL_ARG:
		return GMA_ERR_NULL_ARG;
	default:
		return GMA_ERR_FAILED;
	}
}

static gma_ret_t
destroy_stub(gma_pixmap_info_t *pixmap_info)
{
	return GMA_SUCCESS;
}

static gma_ret_t
unmap_gdl(gma_pixmap_info_t *pixmap_info)
{
	gdl_surface_id_t id;
	gdl_ret_t rc;
	gma_ret_t ret = GMA_SUCCESS;

	id = (gdl_surface_id_t)pixmap_info->user_data;
	rc = gdl_unmap_surface(id);
	if (rc != GDL_SUCCESS)
		ret = convert_gdl_err(rc);

	return ret;
}

static gma_ret_t
destroy_gdl(gma_pixmap_info_t *pixmap_info)
{
	gdl_surface_id_t id;
	gdl_ret_t rc;
	gma_ret_t ret = GMA_SUCCESS;

	id = (gdl_surface_id_t)pixmap_info->user_data;
	rc = gdl_unmap_surface(id);
	if (rc != GDL_SUCCESS)
		ret = convert_gdl_err(rc);

	rc = gdl_free_surface(id);
	if (rc != GDL_SUCCESS)
		ret = convert_gdl_err(rc);

	return ret;
}

static gma_ret_t
wrap_gdl(gdl_surface_info_t *surface_info, void *data, bool take_ownership,
	 gma_pixmap_t *pixmap)
{
	gma_pixmap_info_t info;
	gma_pixmap_funcs_t funcs;

	info.type = GMA_PIXMAP_TYPE_PHYSICAL;
	info.virt_addr = data;
	info.phys_addr = surface_info->phys_addr;
	info.width = surface_info->width;
	info.height = surface_info->height;
	info.pitch = surface_info->pitch;
	info.format = convert_from_gdl_pf(surface_info->pixel_format);
	info.user_data = (void *)surface_info->id;

	funcs.destroy = take_ownership ? destroy_gdl : unmap_gdl;

	return gma_pixmap_alloc(&info, &funcs, pixmap);
}

gma_ret_t
gma_gdl_pixmap_wrap(gdl_surface_info_t *surface_info, gma_pixmap_t *pixmap)
{
	gdl_uint8 *data;
	gdl_ret_t rc;

	rc = gdl_map_surface(surface_info->id, &data, NULL);
	if (rc != GDL_SUCCESS)
		return convert_gdl_err(rc);

	return wrap_gdl(surface_info, data, false, pixmap);
}

gdl_surface_id_t
gma_gdl_pixmap_get_id(gma_pixmap_t *pixmap)
{
	gma_pixmap_info_t info;

	if (gma_pixmap_get_info(pixmap, &info) != GMA_SUCCESS)
		return GDL_SURFACE_INVALID;

	return (gdl_surface_id_t)info.user_data;
}

gma_ret_t
gma_mem_pixmap_wrap(int width, int height, int stride,
		    gma_pixel_format_t format, void *data,
		    gma_pixmap_t *pixmap)
{
	gma_pixmap_info_t info;
	gma_pixmap_funcs_t funcs;

	info.type = GMA_PIXMAP_TYPE_VIRTUAL;
	info.virt_addr = data;
	info.phys_addr = 0;
	info.width = width;
	info.height = height;
	info.pitch = stride;
	info.format = format;

	funcs.destroy = destroy_stub;

	return gma_pixmap_alloc(&info, &funcs, pixmap);
}

gma_ret_t
gma_gdl_pixmap_create(int width, int height, gma_pixel_format_t format,
		      gma_pixmap_t *pixmap)
{
	gdl_surface_info_t surface_info;
	gdl_pixel_format_t surface_pf;
	gdl_uint8 *data;
	gdl_ret_t rc;

	surface_pf = convert_to_gdl_pf(format);

	rc = gdl_alloc_surface(surface_pf, width, height, 0, &surface_info);
	if (rc != GDL_SUCCESS)
		return convert_gdl_err(rc);

	rc = gdl_map_surface(surface_info.id, &data, NULL);
	if (rc != GDL_SUCCESS) {
		gdl_free_surface(surface_info.id);
		return convert_gdl_err(rc);
	}

	return wrap_gdl(&surface_info, data, true, pixmap);
}
