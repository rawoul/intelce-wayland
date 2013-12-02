#include <stdio.h>
#include <stdlib.h>
#include <gdl.h>

#include "wayland-gdl-server.h"

struct wl_gdl_buffer {
	struct wl_resource *resource;
	gdl_surface_info_t surface_info;
};

static void
destroy_buffer(struct wl_resource *resource)
{
	struct wl_gdl_buffer *buffer = wl_resource_get_user_data(resource);

	free(buffer);
}

static void
buffer_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_buffer_interface gdl_buffer_interface = {
	buffer_destroy,
};

static void
create_buffer(struct wl_client *client, struct wl_resource *resource,
	      uint32_t id, uint32_t name)
{
	struct wl_gdl_buffer *buffer;

	buffer = malloc(sizeof (*buffer));
	if (!buffer) {
		wl_resource_post_no_memory(resource);
		return;
	}

	if (gdl_get_surface_info(name, &buffer->surface_info) != GDL_SUCCESS) {
		wl_resource_post_error(resource, WL_GDL_ERROR_INVALID_NAME,
				       "invalid surface id %u", name);
		free(buffer);
		return;
	}

	buffer->resource =
		wl_resource_create(client, &wl_buffer_interface, 1, id);
	if (buffer->resource == NULL) {
		wl_resource_post_no_memory(resource);
		free(buffer);
		return;
	}

	wl_resource_set_implementation(buffer->resource,
				       &gdl_buffer_interface,
				       buffer, destroy_buffer);
}

static const struct wl_gdl_interface gdl_interface = {
	create_buffer,
};

static void
bind_gdl(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &wl_gdl_interface, 1, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &gdl_interface, data, NULL);
}

int
wl_display_init_gdl(struct wl_display *display)
{
	if (!wl_display_add_global(display, &wl_gdl_interface, NULL, bind_gdl))
		return -1;

	return 0;
}

struct wl_gdl_buffer *
wl_gdl_buffer_get(struct wl_resource *resource)
{
	if (wl_resource_instance_of(resource, &wl_buffer_interface,
				    &gdl_buffer_interface))
		return wl_resource_get_user_data(resource);
	else
		return NULL;
}

gdl_surface_info_t *
wl_gdl_buffer_get_surface_info(struct wl_gdl_buffer *buffer)
{
	return &buffer->surface_info;
}
