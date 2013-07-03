#ifndef WAYLAND_EGL_PRIV_H
# define WAYLAND_EGL_PRIV_H

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

#endif /* !WAYLAND_EGL_PRIV_H */
