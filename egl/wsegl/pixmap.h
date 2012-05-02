#ifndef PIXMAP_H_
# define PIXMAP_H_

#include <libgma.h>
#include <libgdl.h>

gdl_pixel_format_t gma_to_gdl_pixel_format(gma_pixel_format_t gma_pf);

gdl_surface_id_t gma_gdl_pixmap_get_id(gma_pixmap_t *pixmap);

gma_ret_t gma_gdl_pixmap_wrap(gdl_surface_info_t *surface_info,
			      gma_pixmap_t *pixmap);

gma_ret_t gma_gdl_pixmap_create(int width, int height,
				gma_pixel_format_t format,
				gma_pixmap_t *pixmap);

gma_ret_t gma_mem_pixmap_wrap(int width, int height, int stride,
			      gma_pixel_format_t format, void *data,
			      gma_pixmap_t *pixmap);

#endif /* !PIXMAP_H_ */
