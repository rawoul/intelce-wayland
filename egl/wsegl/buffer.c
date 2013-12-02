#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "wayland-wsegl.h"

static gma_ret_t
pixmap_destroy_shm(gma_pixmap_info_t *pixmap_info)
{
	int fd;
	gma_ret_t ret = GMA_SUCCESS;

	munmap(pixmap_info->virt_addr,
	       pixmap_info->height * pixmap_info->pitch);

	fd = (int)pixmap_info->user_data;
	if (close(fd) < 0)
		ret = GMA_ERR_FAILED;

	return ret;
}

static WSEGLError
create_shm_pixmap(int width, int height,
		  const struct wayland_pixel_format *format,
		  gma_pixmap_t *pixmap, gma_pixmap_info_t *pixmap_info)
{
	gma_pixmap_info_t info;
	gma_pixmap_funcs_t funcs;
	char filename[] = "/tmp/wayland-shm-XXXXXX";
	void *data;
	int stride;
	int size;
	int fd;

	fd = mkstemp(filename);
	if (fd < 0) {
		dbg("failed to create file for SHM buffer: %m");
		return WSEGL_OUT_OF_MEMORY;
	}

	unlink(filename);

	stride = align(width * format->bpp, format->bpp * 2);
	size = height * stride;

	if (fallocate(fd, 0, 0, size) < 0) {
		dbg("failed to allocate %d bytes for SHM buffer: %m", size);
		close(fd);
		return WSEGL_OUT_OF_MEMORY;
	}

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		dbg("failed to map SHM buffer data: %m");
		close(fd);
		return WSEGL_OUT_OF_MEMORY;
	}

	info.type = GMA_PIXMAP_TYPE_VIRTUAL;
	info.virt_addr = data;
	info.phys_addr = 0;
	info.width = width;
	info.height = height;
	info.pitch = stride;
	info.format = format->gma_pf;
	info.user_data = (void *)fd;

	funcs.destroy = pixmap_destroy_shm;

	if (gma_pixmap_alloc(&info, &funcs, pixmap) != GMA_SUCCESS) {
		dbg("failed to allocate SHM pixmap");
		pixmap_destroy_shm(&info);
	}

	*pixmap_info = info;

	return WSEGL_SUCCESS;
}

static gma_ret_t
pixmap_destroy_gdl(gma_pixmap_info_t *pixmap_info)
{
	gdl_surface_id_t id;
	gdl_ret_t rc;
	gma_ret_t ret = GMA_SUCCESS;

	id = (gdl_surface_id_t)pixmap_info->user_data;
	rc = gdl_unmap_surface(id);
	if (rc != GDL_SUCCESS)
		ret = GMA_ERR_FAILED;

	rc = gdl_free_surface(id);
	if (rc != GDL_SUCCESS)
		ret = GMA_ERR_FAILED;

	return ret;
}

static WSEGLError
create_gdl_pixmap(int width, int height,
		  const struct wayland_pixel_format *format,
		  gma_pixmap_t *pixmap, gma_pixmap_info_t *pixmap_info)
{
	gma_pixmap_info_t info;
	gma_pixmap_funcs_t funcs;
	gdl_surface_info_t surface_info;
	gdl_uint8 *data;
	gdl_ret_t rc;

	rc = gdl_alloc_surface(format->gdl_pf, width, height, 0, &surface_info);
	if (rc != GDL_SUCCESS) {
		dbg("failed to allocate %dx%d %s GDL surface: %s",
		    width, height, format->name, gdl_get_error_string(rc));
		return WSEGL_OUT_OF_MEMORY;
	}

	rc = gdl_map_surface(surface_info.id, &data, NULL);
	if (rc != GDL_SUCCESS) {
		dbg("failed to map GDL surface: %s", gdl_get_error_string(rc));
		gdl_free_surface(surface_info.id);
		return WSEGL_OUT_OF_MEMORY;
	}

	info.type = GMA_PIXMAP_TYPE_PHYSICAL;
	info.virt_addr = data;
	info.phys_addr = surface_info.phys_addr;
	info.width = surface_info.width;
	info.height = surface_info.height;
	info.pitch = surface_info.pitch;
	info.format = format->gma_pf;
	info.user_data = (void *)surface_info.id;

	funcs.destroy = pixmap_destroy_gdl;

	if (gma_pixmap_alloc(&info, &funcs, pixmap) != GMA_SUCCESS) {
		dbg("failed to allocate GMA pixmap");
		pixmap_destroy_gdl(&info);
	}

	*pixmap_info = info;

	return WSEGL_SUCCESS;
}

WSEGLError
wayland_alloc_buffer(struct wayland_display *display, int width, int height,
		     const struct wayland_pixel_format *format,
		     struct wayland_buffer **out_buffer)
{
	struct wayland_buffer *buffer;
	gma_pixmap_t pixmap;
	gma_pixmap_info_t pi;
	WSEGLError err;

	buffer = calloc(1, sizeof (*buffer));
	if (!buffer) {
		dbg("cannot allocate buffer struct");
		return WSEGL_OUT_OF_MEMORY;
	}

	if (display->wl_gdl)
		err = create_gdl_pixmap(width, height, format, &pixmap, &pi);
	else
		err = create_shm_pixmap(width, height, format, &pixmap, &pi);

	if (err != WSEGL_SUCCESS) {
		free(buffer);
		return WSEGL_OUT_OF_MEMORY;
	}

	err = wayland_bind_gma_buffer(display, buffer, pixmap);
	if (err != WSEGL_SUCCESS) {
		gma_pixmap_release(&pixmap);
		free(buffer);
		return err;
	}

	if (display->wl_gdl) {
		buffer->id = (gdl_surface_id_t)pi.user_data;
		buffer->wl_buffer =
			wl_gdl_create_buffer(display->wl_gdl, buffer->id);
	} else {
		struct wl_shm_pool *pool;

		buffer->id = (int)pi.user_data;
		pool = wl_shm_create_pool(display->wl_shm, buffer->id,
					  pi.pitch * pi.height);

		buffer->wl_buffer =
			wl_shm_pool_create_buffer(pool, 0,
						  pi.width, pi.height,
						  pi.pitch, format->wl_pf);

		wl_shm_pool_destroy(pool);
	}

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
				      size, NULL, &meminfo);
	}

	if (pvr_rc != PVR2D_OK) {
		dbg("failed to wrap surface buffer: %s",
		    pvr2d_strerror(pvr_rc));
		return WSEGL_OUT_OF_MEMORY;
	}

	gma_pixmap_add_ref(pixmap);

	buffer->id = -1;
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
