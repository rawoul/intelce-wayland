#ifndef WAYLAND_GDL_SERVER_H
# define WAYLAND_GDL_SERVER_H

#include <gdl.h>
#include <wayland-server.h>
#include "wayland-gdl-server-protocol.h"

struct wl_gdl;

struct wl_gdl_callbacks {
	void (*buffer_created)(struct wl_buffer *buffer, void *data);
	void (*buffer_destroyed)(struct wl_buffer *buffer, void *data);
};

struct wl_gdl *wl_gdl_init(struct wl_display *display,
			   const struct wl_gdl_callbacks *callbacks,
			   void *data);

void wl_gdl_finish(struct wl_gdl *gdl);

int wl_buffer_is_gdl(struct wl_buffer *buffer);

gdl_surface_info_t *wl_gdl_buffer_get_surface_info(struct wl_buffer *buffer);

#endif /* !WAYLAND_GDL_SERVER_H */
