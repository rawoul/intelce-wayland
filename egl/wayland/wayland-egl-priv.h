#ifndef WAYLAND_EGL_PRIV_H
# define WAYLAND_EGL_PRIV_H

#include <stdint.h>

#include "wayland-egl.h"

struct wl_egl_window {
	struct wl_surface *surface;
	int width;
	int height;
	int dx;
	int dy;
	int attached_width;
	int attached_height;
};

struct wl_egl_pixmap {
	struct wl_buffer *buffer;
	int width;
	int height;
	uint32_t flags;
};

#endif /* !WAYLAND_EGL_PRIV_H */
