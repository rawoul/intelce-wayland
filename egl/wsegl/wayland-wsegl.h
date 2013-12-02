#ifndef WAYLAND_WSEGL_H
# define WAYLAND_WSEGL_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include <EGL/eglplatform.h>
#include <wsegl.h>
#include <pvr2d.h>
#include <wayland-client.h>

#include <libgdl.h>
#include <libgma.h>

#include "wayland-gdl.h"
#include "wayland-egl-priv.h"

#undef DEBUG

#ifdef DEBUG
# define dbg(fmt, ...)	fprintf(stderr, "[%llu] EGL/wayland: "fmt"\n", \
				get_time_ms(), ##__VA_ARGS__)
#else
# define dbg(fmt, ...)	{ }
#endif
# define err(fmt, ...)	fprintf(stderr, "EGL/wayland: "fmt"\n", ##__VA_ARGS__)

#define BUFFER_COUNT	4

struct wayland_display {
	struct wl_display *wl_display;
	struct wl_event_queue *wl_queue;
	struct wl_gdl *wl_gdl;
	struct wl_shm *wl_shm;
	bool gdl_init;
	PVR2DCONTEXTHANDLE pvr2d_context;
	pthread_mutex_t pvr2d_lock;
};

enum wayland_buffer_id {
	BUFFER_ID_FRONT,
	BUFFER_ID_BACK,
	BUFFER_ID_MAX,
};

struct wayland_pixel_format {
	const char *name;
	gdl_pixel_format_t gdl_pf;
	gma_pixel_format_t gma_pf;
	enum wl_shm_format wl_pf;
	WSEGLPixelFormat wsegl_pf;
	int bpp;
	bool has_alpha;
	bool renderable;
};

struct wayland_buffer {
	int id;
	int width;
	int height;
	int pitch;
	bool lock;
	struct wl_buffer *wl_buffer;
	const struct wayland_pixel_format *format;
	PVR2DMEMINFO *meminfo;
	gma_pixmap_t pixmap;
};

struct wayland_pixmap {
	struct wayland_buffer *buffer;
};

struct wayland_window {
	struct wayland_buffer *buffers[BUFFER_ID_MAX];
	struct wayland_buffer *bufferpool[BUFFER_COUNT];
	struct wl_callback *throttle_cb;
	struct wl_egl_window *egl_window;
	int num_buffers;
	int max_buffers;
	int swap_interval;
};

struct wayland_drawable {
	struct wayland_display *display;
	const struct wayland_pixel_format *format;
	int width;
	int height;
	int type;
	union {
		struct wayland_window window;
		struct wayland_pixmap pixmap;
	};
};

static inline int align(int value, int alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

bool debug_get_bool_option(const char *name, bool dfault);
const char *pvr2d_strerror(PVR2DERROR err);
uint64_t get_time_ms(void);

/* pixel format conversion functions */
const struct wayland_pixel_format *
convert_gdl_pixel_format(gdl_pixel_format_t pf);
const struct wayland_pixel_format *
convert_gma_pixel_format(gma_pixel_format_t pf);
const struct wayland_pixel_format *
convert_wsegl_pixel_format(WSEGLPixelFormat pf);

/* buffer functions */
WSEGLError wayland_alloc_buffer(struct wayland_display *display,
				int width, int height,
				const struct wayland_pixel_format *format,
				struct wayland_buffer **out_buffer);

void wayland_destroy_buffer(struct wayland_display *display,
			    struct wayland_buffer *buffer);

WSEGLError wayland_bind_gma_buffer(struct wayland_display *display,
				   struct wayland_buffer *buffer,
				   gma_pixmap_t pixmap);

void wayland_unbind_buffer(struct wayland_display *display,
			   struct wayland_buffer *buffer);

#endif /* !WAYLAND_WSEGL_H */
