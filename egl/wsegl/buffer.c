#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pixmap.h"
#include "wayland-wsegl.h"

WSEGLError
wayland_alloc_buffer(struct wayland_display *display, int width, int height,
		     const struct wayland_pixel_format *format,
		     struct wayland_buffer **out_buffer)
{
	struct wayland_buffer *buffer;
	gma_pixmap_t pixmap;
	gma_ret_t rc;
	WSEGLError err;

	buffer = calloc(1, sizeof (*buffer));
	if (!buffer) {
		dbg("cannot allocate buffer struct");
		return WSEGL_OUT_OF_MEMORY;
	}

	rc = gma_gdl_pixmap_create(width, height, format->gma_pf, &pixmap);
	if (rc != GMA_SUCCESS) {
		dbg("failed to create %dx%d gdl buffer", width, height);
		free(buffer);
		return WSEGL_OUT_OF_MEMORY;
	}

	err = wayland_bind_gma_buffer(display, buffer, pixmap);
	if (err != WSEGL_SUCCESS) {
		gma_pixmap_release(&pixmap);
		free(buffer);
		return err;
	}

	buffer->wl_buffer = wl_gdl_create_buffer(display->wl_gdl,
						 gma_gdl_pixmap_get_id(pixmap));
	if (!buffer->wl_buffer) {
		wayland_destroy_buffer(display, buffer);
		return WSEGL_OUT_OF_MEMORY;
	}

	*out_buffer = buffer;

	return WSEGL_SUCCESS;
}

void
wayland_destroy_buffer(struct wayland_display *display,
		       struct wayland_buffer *buffer)
{
	if (buffer == NULL)
		return;

	if (buffer->wl_buffer)
		wl_buffer_destroy(buffer->wl_buffer);

	wayland_unbind_buffer(display, buffer);

	if (buffer->pixmap)
		gma_pixmap_release(&buffer->pixmap);

	free(buffer);
}

WSEGLError
wayland_bind_gma_buffer(struct wayland_display *display,
			struct wayland_buffer *buffer,
			gma_pixmap_t pixmap)
{
	const struct wayland_pixel_format *format;
	gma_pixmap_info_t pixmap_info;
	int size;
	PVR2DERROR pvr_rc;
	PVR2DMEMINFO *meminfo;

	if (gma_pixmap_get_info(pixmap, &pixmap_info) != GMA_SUCCESS) {
		dbg("failed to get gma pixmap info");
		return WSEGL_BAD_NATIVE_PIXMAP;
	}

	format = convert_gma_pixel_format(pixmap_info.format);
	if (!format) {
		dbg("unsupported gma pixel format");
		return WSEGL_BAD_NATIVE_PIXMAP;
	}

	if (pixmap_info.pitch % format->bpp != 0) {
		dbg("invalid surface pitch");
		return WSEGL_BAD_NATIVE_PIXMAP;
	}

	size = pixmap_info.pitch * pixmap_info.height;

	if (pixmap_info.type == GMA_PIXMAP_TYPE_PHYSICAL) {
		unsigned long page_addr = pixmap_info.phys_addr &
			~(getpagesize() - 1);

		pvr_rc = PVR2DMemWrap(display->pvr2d_context,
				      pixmap_info.virt_addr,
				      PVR2D_WRAPFLAG_CONTIGUOUS,
				      size, &page_addr, &meminfo);
	} else {
		pvr_rc = PVR2DMemWrap(display->pvr2d_context,
				      pixmap_info.virt_addr,
				      PVR2D_WRAPFLAG_NONCONTIGUOUS,
				      size, IMG_NULL, &meminfo);
	}

	if (pvr_rc != PVR2D_OK) {
		dbg("failed to wrap surface buffer: %s",
		    pvr2d_strerror(pvr_rc));
		return WSEGL_OUT_OF_MEMORY;
	}

	gma_pixmap_add_ref(pixmap);

	buffer->width = pixmap_info.width;
	buffer->height = pixmap_info.height;
	buffer->pitch = pixmap_info.pitch;
	buffer->meminfo = meminfo;
	buffer->pixmap = pixmap;
	buffer->format = format;

	return WSEGL_SUCCESS;
}

void
wayland_unbind_buffer(struct wayland_display *display,
		      struct wayland_buffer *buffer)
{
	if (!buffer)
		return;

	if (buffer->meminfo) {
		PVR2DMemFree(display->pvr2d_context, buffer->meminfo);
		buffer->meminfo = NULL;
	}

	if (buffer->pixmap)
		gma_pixmap_release(&buffer->pixmap);
}
