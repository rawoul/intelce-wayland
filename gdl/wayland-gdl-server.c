#include <stdio.h>
#include <stdlib.h>

#include <wayland-util.h>

#include "wayland-gdl-server.h"

struct wl_gdl {
	const struct wl_gdl_callbacks *callbacks;
	struct wl_display *display;
	struct wl_global *global;
	void *user_data;
};

struct wl_gdl_buffer {
	struct wl_buffer buffer;
	struct wl_gdl *gdl;
	gdl_surface_info_t surface_info;
};

static inline struct wl_gdl_buffer *wl_gdl_buffer(struct wl_buffer *buffer)
{
	return container_of(buffer, struct wl_gdl_buffer, buffer);
}

static void
destroy_buffer(struct wl_resource *resource)
{
	struct wl_gdl_buffer *buffer = wl_gdl_buffer(resource->data);

	buffer->gdl->callbacks->buffer_destroyed(&buffer->buffer,
						 buffer->gdl->user_data);

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
	struct wl_gdl *gdl = resource->data;
	struct wl_gdl_buffer *buffer;

	buffer = calloc(1, sizeof (*buffer));
	if (buffer == NULL)
		return;

	if (gdl_get_surface_info(name, &buffer->surface_info) != GDL_SUCCESS) {
		wl_resource_post_error(resource, WL_GDL_ERROR_INVALID_NAME,
				       "invalid surface id");
		free(buffer);
		return;
	}

	buffer->buffer.width = buffer->surface_info.width;
	buffer->buffer.height = buffer->surface_info.height;

	buffer->buffer.resource.object.id = id;
	buffer->buffer.resource.object.interface = &wl_buffer_interface;
	buffer->buffer.resource.object.implementation = (void (**)(void))
		&gdl_buffer_interface;

	buffer->buffer.resource.data = buffer;
	buffer->buffer.resource.client = client;
	buffer->buffer.resource.destroy = destroy_buffer;

	buffer->gdl = gdl;

	buffer->gdl->callbacks->buffer_created(&buffer->buffer,
					       buffer->gdl->user_data);

	wl_client_add_resource(client, &buffer->buffer.resource);
}

static const struct wl_gdl_interface gdl_interface = {
	create_buffer,
};

static void
bind_gdl(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	wl_client_add_object(client, &wl_gdl_interface,
			     &gdl_interface, id, data);
}

WL_EXPORT struct wl_gdl *
wl_gdl_init(struct wl_display *display,
	    const struct wl_gdl_callbacks *callbacks, void *data)
{
	struct wl_gdl *gdl;

	gdl = malloc(sizeof (*gdl));
	if (gdl == NULL)
		return NULL;

	gdl->global = wl_display_add_global(display, &wl_gdl_interface,
					    gdl, bind_gdl);

	if (gdl->global == NULL) {
		free(gdl);
		return NULL;
	}

	gdl->display = display;
	gdl->callbacks = callbacks;
	gdl->user_data = data;

	return gdl;
}

WL_EXPORT void
wl_gdl_finish(struct wl_gdl *gdl)
{
	wl_display_remove_global(gdl->display, gdl->global);
	free(gdl);
}

WL_EXPORT int
wl_buffer_is_gdl(struct wl_buffer *buffer)
{
	return buffer->resource.object.implementation ==
		(void (**)(void)) &gdl_buffer_interface;
}

WL_EXPORT gdl_surface_info_t *
wl_gdl_buffer_get_surface_info(struct wl_buffer *wl_buffer)
{
	struct wl_gdl_buffer *buffer = wl_gdl_buffer(wl_buffer);

	return &buffer->surface_info;
}
