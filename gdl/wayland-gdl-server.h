#ifndef WAYLAND_GDL_H_
# define WAYLAND_GDL_H_

#include <gdl_types.h>
#include <wayland-server.h>
#include "wayland-gdl-server-protocol.h"

struct wl_gdl_buffer;

int wl_display_init_gdl(struct wl_display *display);

struct wl_gdl_buffer *wl_gdl_buffer_get(struct wl_resource *resource);

gdl_surface_info_t *
wl_gdl_buffer_get_surface_info(struct wl_gdl_buffer *buffer);

#endif /* !WAYLAND_GDL_H_ */
