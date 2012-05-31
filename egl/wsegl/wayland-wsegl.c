#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include <EGL/egl.h>

#include "pixmap.h"
#include "wayland-wsegl.h"

#define MAX_SWAP_COUNT	100

static WSEGLConfig display_configs[] = {
	{ WSEGL_DRAWABLE_WINDOW | WSEGL_DRAWABLE_PIXMAP,
	  WSEGL_PIXELFORMAT_XRGB8888, WSEGL_FALSE, 0,
	  0, IMG_NULL, WSEGL_OPAQUE, 0 },

	{ WSEGL_DRAWABLE_WINDOW | WSEGL_DRAWABLE_PIXMAP,
	  WSEGL_PIXELFORMAT_ARGB8888, WSEGL_FALSE, 0,
	  0, IMG_NULL, WSEGL_OPAQUE, 0 },

	{ WSEGL_DRAWABLE_WINDOW | WSEGL_DRAWABLE_PIXMAP,
	  WSEGL_PIXELFORMAT_RGB565, WSEGL_FALSE, 0,
	  0, IMG_NULL, WSEGL_OPAQUE, 0 },

	{ WSEGL_NO_DRAWABLE, 0, 0, 0, 0, 0, 0, 0 }
};

static const WSEGLCaps display_caps[] = {
	{ WSEGL_CAP_WINDOWS_USE_HW_SYNC, 1 },
	{ WSEGL_CAP_MIN_SWAP_INTERVAL, 0 },
	{ WSEGL_CAP_MAX_SWAP_INTERVAL, MAX_SWAP_COUNT },
	{ WSEGL_NO_CAPS, 0 }
};

static bool
pointer_is_dereferencable(void *p)
{
	uintptr_t addr = (uintptr_t) p;
	unsigned char valid = 0;
	const long page_size = getpagesize();

	if (p == NULL)
		return false;

	// align addr to page_size
	addr &= ~(page_size - 1);

	if (mincore((void *) addr, page_size, &valid) < 0)
		return false;

	return (valid & 0x01) == 0x01;
}

static WSEGLError
WSEGL_IsDisplayValid(NativeDisplayType native_display)
{
	if (native_display == EGL_DEFAULT_DISPLAY)
		return WSEGL_BAD_NATIVE_DISPLAY;

	if (pointer_is_dereferencable((void *) native_display)) {
		void *ptr = *(void **) native_display;

		// wl_display is a wl_proxy, which is a wl_object.
		// wl_object's first element points to the interface type
		if (ptr == &wl_display_interface)
			return WSEGL_SUCCESS;
	}

	return WSEGL_BAD_NATIVE_DISPLAY;
}

static WSEGLError
WSEGL_CloseDisplay(WSEGLDisplayHandle display_handle)
{
	struct wayland_display *display =
		(struct wayland_display *)display_handle;

	if (!display)
		return WSEGL_SUCCESS;

	if (display->wl_gdl)
		wl_gdl_destroy(display->wl_gdl);

	if (display->pvr2d_context)
		PVR2DDestroyDeviceContext(display->pvr2d_context);

	if (display->pvrsrv_connection)
		PVRSRVDisconnect(display->pvrsrv_connection);

	if (display->gdl_init)
		gdl_close();

	free(display);

	return WSEGL_SUCCESS;
}

static WSEGLError
WSEGL_InitialiseDisplay(NativeDisplayType native_display,
			WSEGLDisplayHandle *display_handle,
			const WSEGLCaps **caps,
			WSEGLConfig **configs)
{
	struct wayland_display *display;
	gdl_ret_t gdl_rc;
	PVR2DERROR pvr2d_rc;
	PVRSRV_ERROR pvrsrv_rc;
	uint32_t id;

	dbg("initializing Wayland display");

	display = calloc(1, sizeof (*display));
	if (!display)
		return WSEGL_OUT_OF_MEMORY;

	display->wl_display = (struct wl_display *) native_display;

	id = wl_display_get_global(display->wl_display, "wl_gdl", 1);
	if (id == 0)
		wl_display_roundtrip(display->wl_display);

	id = wl_display_get_global(display->wl_display, "wl_gdl", 1);
	if (id == 0) {
		dbg("wayland gdl interface is not available");
		WSEGL_CloseDisplay(display);
		return WSEGL_CANNOT_INITIALISE;
	}

	gdl_rc = gdl_init(0);
	if (gdl_rc != GDL_SUCCESS) {
		dbg("failed gdl init");
		WSEGL_CloseDisplay(display);
		return WSEGL_CANNOT_INITIALISE;
	}

	display->gdl_init = true;

	display->wl_gdl = wl_display_bind(display->wl_display, id,
					  &wl_gdl_interface);
	if (!display->wl_gdl) {
		dbg("cannot bind wayland gdl interface");
		WSEGL_CloseDisplay(display);
		return WSEGL_CANNOT_INITIALISE;
	}

	pvr2d_rc = PVR2DCreateDeviceContext(1, &display->pvr2d_context, 0);
	if (pvr2d_rc != PVR2D_OK) {
		dbg("failed to create pvr2d context: %s",
		    pvr2d_strerror(pvr2d_rc));
		WSEGL_CloseDisplay(display);
		return WSEGL_OUT_OF_MEMORY;
	}

	pvrsrv_rc = PVRSRVConnect(&display->pvrsrv_connection, 0);
	if (pvrsrv_rc != PVRSRV_OK) {
		dbg("failed to connect pvr services");
		WSEGL_CloseDisplay(display);
		return WSEGL_CANNOT_INITIALISE;
	}

	*caps = display_caps;
	*configs = display_configs;
	*display_handle = (WSEGLDisplayHandle) display;

	return WSEGL_SUCCESS;
}

static WSEGLError
WSEGL_CreateWindowDrawable(WSEGLDisplayHandle display_handle,
			   WSEGLConfig *config,
			   WSEGLDrawableHandle *drawable_handle,
			   NativeWindowType native_window,
			   WSEGLRotationAngle *rotation_angle)
{
	struct wayland_display *display =
		(struct wayland_display *) display_handle;
	struct wayland_drawable *drawable;
	const struct wayland_pixel_format *egl_pf;
	struct wl_egl_window *egl_window;

	if (display_handle == NULL || drawable_handle == NULL)
		return WSEGL_BAD_NATIVE_WINDOW;

	if (config == NULL ||
	    !(config->ui32DrawableType & WSEGL_DRAWABLE_WINDOW)) {
		dbg("selected config does not support window drawables");
		return WSEGL_BAD_CONFIG;
	}

	egl_pf = convert_wsegl_pixel_format(config->ePixelFormat);
	if (!egl_pf) {
		dbg("invalid config pixel format");
		return WSEGL_BAD_CONFIG;
	}

	if (!egl_pf->renderable) {
		dbg("cannot render to pixel format");
		return WSEGL_BAD_CONFIG;
	}

	egl_window = native_window;
	if (!egl_window || !egl_window->surface) {
		dbg("null native window handle");
		return WSEGL_BAD_NATIVE_WINDOW;
	}

	drawable = calloc(1, sizeof (*drawable));
	if (!drawable)
		return WSEGL_OUT_OF_MEMORY;

	drawable->display = display;
	drawable->type = WSEGL_DRAWABLE_WINDOW;
	drawable->format = egl_pf;
	drawable->width = egl_window->width;
	drawable->height = egl_window->height;

	drawable->window.egl_window = egl_window;
	drawable->window.num_buffers = 0;
	drawable->window.max_buffers = BUFFER_COUNT;
	drawable->window.swap_interval = 1;
	drawable->window.swap_count = MAX_SWAP_COUNT;

	*drawable_handle = (WSEGLDrawableHandle) drawable;
	*rotation_angle = 0;

	return WSEGL_SUCCESS;
}

static void
destroy_drawable_window(struct wayland_drawable *drawable)
{
	struct wayland_window *win = &drawable->window;

	for (int i = 0; i < BUFFER_ID_MAX; i++)
		wayland_destroy_buffer(drawable->display,
				       win->buffers[i]);
}

static WSEGLError
WSEGL_CreateImageDrawable(WSEGLDisplayHandle display_handle,
			  WSEGLDrawableHandle *drawable_handle,
			  NativePixmapType native_pixmap)
{
	struct wayland_display *display =
		(struct wayland_display *) display_handle;
	struct wayland_drawable *drawable;
	struct wayland_buffer *buffer;
	WSEGLError err;

	if (drawable_handle == NULL || !native_pixmap)
		return WSEGL_BAD_NATIVE_PIXMAP;

	drawable = calloc(1, sizeof (*drawable));
	if (!drawable)
		return WSEGL_OUT_OF_MEMORY;

	buffer = calloc(1, sizeof (*buffer));
	if (!buffer) {
		free(drawable);
		return WSEGL_OUT_OF_MEMORY;
	}

	err = wayland_bind_gma_buffer(display, buffer, native_pixmap);
	if (err != WSEGL_SUCCESS) {
		free(buffer);
		free(drawable);
		return err;
	}

	drawable->display = display;
	drawable->type = WSEGL_DRAWABLE_PIXMAP;
	drawable->width = buffer->width;
	drawable->height = buffer->height;
	drawable->pixmap.buffer = buffer;
	drawable->format = buffer->format;

	*drawable_handle = drawable;

	return WSEGL_SUCCESS;
}

static WSEGLError
WSEGL_CreatePixmapDrawable(WSEGLDisplayHandle display_handle,
			   WSEGLConfig *config,
			   WSEGLDrawableHandle *drawable_handle,
			   NativePixmapType native_pixmap,
			   WSEGLRotationAngle *rotation_angle)
{
	struct wayland_display *display =
		(struct wayland_display *) display_handle;
	struct wayland_drawable *drawable;
	const struct wayland_pixel_format *egl_pf;
	struct wl_egl_pixmap *egl_pixmap;
	WSEGLError err;

	if (config == NULL) {
		// config is not set, we are creating an EGL image
		return WSEGL_CreateImageDrawable(display_handle,
						 drawable_handle,
						 native_pixmap);
	}

	if (drawable_handle == NULL || !native_pixmap)
		return WSEGL_BAD_NATIVE_PIXMAP;

	if (!(config->ui32DrawableType & WSEGL_DRAWABLE_PIXMAP)) {
		dbg("selected config does not allow pixmap drawables");
		return WSEGL_BAD_CONFIG;
	}

	egl_pf = convert_wsegl_pixel_format(config->ePixelFormat);
	if (!egl_pf) {
		dbg("invalid config pixel format");
		return WSEGL_BAD_CONFIG;
	}

	if (!egl_pf->renderable) {
		dbg("cannot render to pixel format");
		return WSEGL_BAD_CONFIG;
	}

	egl_pixmap = native_pixmap;
	if (!egl_pixmap) {
		dbg("null native pixmap handle");
		return WSEGL_BAD_NATIVE_WINDOW;
	}

	drawable = calloc(1, sizeof (*drawable));
	if (!drawable)
		return WSEGL_OUT_OF_MEMORY;

	err = wayland_alloc_buffer(display,
				   egl_pixmap->width,
				   egl_pixmap->height,
				   egl_pf,
				   &drawable->pixmap.buffer);
	if (err != WSEGL_SUCCESS) {
		free(drawable);
		return err;
	}

	drawable->display = display;
	drawable->type = WSEGL_DRAWABLE_PIXMAP;
	drawable->width = drawable->pixmap.buffer->width;
	drawable->height = drawable->pixmap.buffer->height;
	drawable->format = egl_pf;
	drawable->pixmap.egl_pixmap = egl_pixmap;

	egl_pixmap->buffer = drawable->pixmap.buffer->wl_buffer;

	*drawable_handle = drawable;
	*rotation_angle = 0;

	return WSEGL_SUCCESS;
}

static void
destroy_drawable_pixmap(struct wayland_drawable *drawable)
{
	struct wayland_pixmap *pixmap = &drawable->pixmap;

	if (pixmap->egl_pixmap)
		wayland_destroy_buffer(drawable->display, pixmap->buffer);
	else
		wayland_unbind_buffer(drawable->display, pixmap->buffer);
}

static void
wayland_drawable_destroy(struct wayland_drawable *drawable)
{
	if (!drawable)
		return;

	switch (drawable->type) {
	case WSEGL_DRAWABLE_WINDOW:
		destroy_drawable_window(drawable);
		break;
	case WSEGL_DRAWABLE_PIXMAP:
		destroy_drawable_pixmap(drawable);
		break;
	}

	free(drawable);
}

static WSEGLError
WSEGL_DeleteDrawable(WSEGLDrawableHandle drawable_handle)
{
	struct wayland_drawable *drawable =
		(struct wayland_drawable *)drawable_handle;

	wayland_drawable_destroy(drawable);

	return WSEGL_SUCCESS;
}

static inline void
_swap_pointers(const void **p1, const void **p2)
{
	const void *tmp;

	tmp = *p1;
	*p1 = *p2;
	*p2 = tmp;
}

#define swap_pointers(p1, p2) \
	_swap_pointers((const void **) p1, (const void **) p2)

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	static const struct wl_callback_listener frame_listener = {
		frame_callback
	};

	struct wayland_window *window = data;

	if (callback) {
		wl_callback_destroy(callback);
		window->swap_count++;
	}

	if (window->swap_count < window->swap_interval) {
		callback = wl_surface_frame(window->egl_window->surface);
		wl_callback_add_listener(callback, &frame_listener, window);
	}
}

static WSEGLError
WSEGL_SwapDrawable(WSEGLDrawableHandle drawable_handle,
		   unsigned long ui32Data)
{
	struct wayland_drawable *drawable =
		(struct wayland_drawable *)drawable_handle;
	struct wayland_display *display;
	struct wayland_window *window;
	struct wayland_buffer *buffer;
	PVRSRV_CLIENT_MEM_INFO *cmeminfo;
	PVRSRV_ERROR pvrsrv_rc;

	if (drawable->type != WSEGL_DRAWABLE_WINDOW)
		return WSEGL_SUCCESS;

	display = drawable->display;
	window = &drawable->window;
	buffer = window->buffers[BUFFER_ID_BACK];

	cmeminfo = buffer->meminfo->hPrivateData;
	pvrsrv_rc = PVRSRVWaitForWriteOpSync(display->pvrsrv_connection,
					     cmeminfo->psClientSyncInfo);
	if (pvrsrv_rc != PVRSRV_OK)
		dbg("failed to flush gfx queue");

	while (window->swap_count < window->swap_interval) {
		dbg("wait for swap to finish");
		wl_display_flush(display->wl_display);
		wl_display_iterate(display->wl_display, WL_DISPLAY_READABLE);
	}

	window->swap_count = 0;
	frame_callback(window, NULL, 0);

	dbg("swap surface=%u w=%d(%d) h=%d",
	    gma_gdl_pixmap_get_id(buffer->pixmap),
	    buffer->width, buffer->pitch, buffer->height);

	wl_surface_attach(window->egl_window->surface,
			  buffer->wl_buffer, 0, 0);
	wl_surface_damage(window->egl_window->surface, 0, 0,
			  buffer->width, buffer->height);

	buffer->lock = 1;

	swap_pointers(&window->buffers[BUFFER_ID_FRONT],
		      &window->buffers[BUFFER_ID_BACK]);

	return WSEGL_SUCCESS;
}

static WSEGLError
WSEGL_SwapControlInterval(WSEGLDrawableHandle drawable_handle,
			  unsigned long ui32Interval)
{
	struct wayland_drawable *drawable =
		(struct wayland_drawable *)drawable_handle;

	if (drawable->type != WSEGL_DRAWABLE_WINDOW)
		return WSEGL_SUCCESS;

	dbg("set swap interval %lu", ui32Interval);

	drawable->window.swap_interval = ui32Interval;

	return WSEGL_SUCCESS;
}

static WSEGLError
WSEGL_WaitNative(WSEGLDrawableHandle drawable_handle,
		 unsigned long ui32Engine)
{
	if (drawable_handle == NULL)
		return WSEGL_BAD_NATIVE_PIXMAP;

	if (ui32Engine != WSEGL_DEFAULT_NATIVE_ENGINE)
		return WSEGL_BAD_NATIVE_ENGINE;

	return WSEGL_SUCCESS;
}

static WSEGLError
WSEGL_CopyFromDrawable(WSEGLDrawableHandle drawable_handle,
		       NativePixmapType native_pixmap)
{
	if (drawable_handle == NULL)
		return WSEGL_BAD_DRAWABLE;

	if (native_pixmap == NULL)
		return WSEGL_BAD_NATIVE_PIXMAP;

	//FIXME: implement this

	return WSEGL_BAD_DRAWABLE;
}

static WSEGLError
WSEGL_CopyFromPBuffer(void *addr,
		      unsigned long width,
		      unsigned long height,
		      unsigned long stride,
		      WSEGLPixelFormat pixel_format,
		      NativePixmapType native_pixmap)
{
	if (native_pixmap == NULL)
		return WSEGL_BAD_NATIVE_PIXMAP;

	//FIXME: implement this

	return WSEGL_BAD_NATIVE_PIXMAP;
}

static void
get_buffer_parameters(struct wayland_buffer *buffer,
		      WSEGLDrawableParams *params)
{
	params->ui32Width = buffer->width;
	params->ui32Height = buffer->height;
	params->ui32Stride = buffer->pitch / buffer->format->bpp;
	params->ePixelFormat = buffer->format->wsegl_pf;
	params->pvLinearAddress = buffer->meminfo->pBase;
	params->ui32HWAddress = buffer->meminfo->ui32DevAddr;
	params->hPrivateData = buffer->meminfo->hPrivateData;
}

static void
buffer_release(void *data, struct wl_buffer *wl_buffer)
{
	struct wayland_buffer *buffer = data;

	dbg("release buffer %d", gma_gdl_pixmap_get_id(buffer->pixmap));

	buffer->lock = false;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static struct wayland_buffer *
window_get_render_buffer(struct wayland_drawable *drawable)
{
	struct wayland_display *display = drawable->display;
	struct wayland_window *window = &drawable->window;
	struct wayland_buffer *buffer;

	/*
	 * Try to use an already allocated and unlocked buffer.
	 */
	for (int i = 0; i < window->num_buffers; i++) {
		if (!window->bufferpool[i]->lock)
			return window->bufferpool[i];
	}

	/*
	 * Try to allocate a new buffer.
	 */
	if (window->num_buffers < window->max_buffers) {
		WSEGLError err;

		err = wayland_alloc_buffer(display,
					   window->egl_window->width,
					   window->egl_window->height,
					   drawable->format, &buffer);
		if (err == WSEGL_SUCCESS) {
			wl_buffer_add_listener(buffer->wl_buffer,
					       &buffer_listener, buffer);

			window->bufferpool[window->num_buffers++] = buffer;

			return buffer;
		}
	}

	/*
	 * Wait for a buffer to be unlocked.
	 */
	if (window->num_buffers < 2) {
		dbg("not enough buffers for window");
		return NULL;
	}

	dbg("wait for buffer");
	wl_display_flush(display->wl_display);

	for (buffer = NULL; !buffer; ) {
		wl_display_iterate(display->wl_display, WL_DISPLAY_READABLE);
		for (int i = 0; i < window->num_buffers; i++) {
			if (!window->bufferpool[i]->lock) {
				buffer = window->bufferpool[i];
				break;
			}
		}
	}

	dbg("  -> done");

	return buffer;
}

static WSEGLError
WSEGL_GetDrawableParameters(WSEGLDrawableHandle drawable_handle,
			    WSEGLDrawableParams *source_params,
			    WSEGLDrawableParams *render_params)
{
	struct wayland_drawable *drawable =
		(struct wayland_drawable *)drawable_handle;
	struct wayland_buffer *sbuffer, *rbuffer;

	if (drawable->type == WSEGL_DRAWABLE_WINDOW) {
		struct wayland_window *window = &drawable->window;

		if (drawable->width != window->egl_window->width ||
		    drawable->height != window->egl_window->height) {
			dbg("window size changed, recreate drawable");
			return WSEGL_BAD_DRAWABLE;
		}

		rbuffer = window_get_render_buffer(drawable);
		if (!rbuffer)
			return WSEGL_OUT_OF_MEMORY;

		sbuffer = window->buffers[BUFFER_ID_FRONT];
		if (!sbuffer)
			sbuffer = rbuffer;

		dbg("render to %u", gma_gdl_pixmap_get_id(rbuffer->pixmap));

		window->buffers[BUFFER_ID_BACK] = rbuffer;
		window->egl_window->attached_width = drawable->width;
		window->egl_window->attached_height = drawable->height;

	} else {
		struct wayland_pixmap *pixmap = &drawable->pixmap;

		sbuffer = pixmap->buffer;
		rbuffer = pixmap->buffer;
	}

	get_buffer_parameters(rbuffer, render_params);
	get_buffer_parameters(sbuffer, source_params);

	return WSEGL_SUCCESS;
}

static WSEGLError
WSEGL_ConnectDrawable(WSEGLDrawableHandle hDrawable)
{
	return WSEGL_SUCCESS;
}

static WSEGLError
WSEGL_DisconnectDrawable(WSEGLDrawableHandle hDrawable)
{
	return WSEGL_SUCCESS;
}

WL_EXPORT const WSEGL_FunctionTable *
WSEGL_GetFunctionTablePointer(void)
{
	static const WSEGL_FunctionTable function_table = {
		WSEGL_VERSION,
		WSEGL_IsDisplayValid,
		WSEGL_InitialiseDisplay,
		WSEGL_CloseDisplay,
		WSEGL_CreateWindowDrawable,
		WSEGL_CreatePixmapDrawable,
		WSEGL_DeleteDrawable,
		WSEGL_SwapDrawable,
		WSEGL_SwapControlInterval,
		WSEGL_WaitNative,
		WSEGL_CopyFromDrawable,
		WSEGL_CopyFromPBuffer,
		WSEGL_GetDrawableParameters,
		WSEGL_ConnectDrawable,
		WSEGL_DisconnectDrawable,
	};

	return &function_table;
}