/*
 * This file is part of the jwm distribution:
 * https://github.com/JulienMasson/jwc
 *
 * Copyright (c) 2018 Julien Masson.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "client.h"
#include "keyboard.h"
#include "output.h"
#include "utils.h"

struct render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct jwc_client *client;
	struct timespec *when;
};

static void xdg_surface_map(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, map);
	client->mapped = true;

	/* focus and show on toplevel */
	client_set_focus(client);
	client_set_on_toplevel(client);

	/* move client to have the current pointer center on this client */
	struct wlr_box box;
	client_get_geometry(client, &box);
	client_move(client,
		    client->server->cursor->x - (box.width / 2),
		    client->server->cursor->y - (box.height / 2));
}

static void xdg_surface_destroy(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, destroy);
	wl_list_remove(&client->link);
	free(client);
}

static void xdg_surface_commit(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, surface_commit);
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface;

	uint32_t pending_serial = client->pending_serial;

	if (pending_serial > 0 && pending_serial >= surface->configure_serial) {

		client_move(client, client->pending_geo.x, client->pending_geo.y);

		if (pending_serial == surface->configure_serial)
			client->pending_serial = 0;
	}
}

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data)
{
	struct render_data *rdata = data;
	struct jwc_client *client = rdata->client;
	struct wlr_output *output = rdata->output;
	struct wlr_output_layout *output_layout = client->server->output_layout;

	/* get texture of the surface */
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture)
		return;

	/* calculate origin coordinates */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(output_layout, output, &ox, &oy);
	ox += client->x + sx;
	oy += client->y + sy;

	wlr_render_texture(rdata->renderer, texture, output->transform_matrix, ox, oy, 1);

	wlr_surface_send_frame_done(surface, rdata->when);
}

static void client_notify_new(struct wl_listener *listener, void *data)
{
	struct jwc_server *server =
		wl_container_of(listener, server, xdg_shell_v6_surface);
	struct wlr_xdg_surface_v6 *xdg_surface = data;

	/* only toplevel surface are accepted */
	if (xdg_surface->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL)
		return;

	/* create new client */
	struct jwc_client *client = calloc(1, sizeof(struct jwc_client));
	client->server = server;
	client->xdg_surface = xdg_surface;

	/* register callbacks when we get events from this client */
	client->map.notify = xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &client->map);

	client->destroy.notify = xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &client->destroy);

	client->surface_commit.notify = xdg_surface_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &client->surface_commit);

	/* add this client to the clients server list */
	wl_list_insert(&server->clients, &client->link);
}

void client_init(struct jwc_server *server)
{
	wl_list_init(&server->clients);

	/* create xdg shell v6 wayland protocol */
	server->xdg_shell_v6 = wlr_xdg_shell_v6_create(server->wl_display);

	/* register callback when we have a new client */
	server->xdg_shell_v6_surface.notify = client_notify_new;
	wl_signal_add(&server->xdg_shell_v6->events.new_surface,
		      &server->xdg_shell_v6_surface);
}

void client_set_focus(struct jwc_client *client)
{
	struct wlr_xdg_surface_v6 *xdg_surface = client->xdg_surface;
	struct wlr_surface *surface = xdg_surface->surface;
	struct wlr_seat *seat = client->server->seat;

	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		return;
	}
	if (prev_surface) {
		struct wlr_xdg_surface_v6 *previous = wlr_xdg_surface_v6_from_wlr_surface(
			seat->keyboard_state.focused_surface);
		wlr_xdg_toplevel_v6_set_activated(previous, false);
	}

	wlr_xdg_toplevel_v6_set_activated(xdg_surface, true);

	keyboard_enter(client->server, surface);
}

struct jwc_client *client_get_on_toplevel(struct jwc_server *server)
{
	struct wl_list *clients = &server->clients;
	struct jwc_client *client;

	if (wl_list_empty(clients))
		return NULL;

	return wl_container_of(clients->next, client, link);
}

void client_set_on_toplevel(struct jwc_client *client)
{
	/* put this client on top of the list */
	wl_list_remove(&client->link);
	wl_list_insert(&client->server->clients, &client->link);
}

struct jwc_client *client_get_last(struct jwc_server *server)
{
	struct wl_list *clients = &server->clients;
	struct jwc_client *client;

	if (wl_list_empty(clients))
		return NULL;

	wl_list_for_each_reverse(client, clients, link) {
		if (client->mapped == true)
			return client;
	}

	return NULL;
}

void client_unmap(struct jwc_client *client)
{
	client->mapped = false;

	struct jwc_client *last = client_get_last(client->server);
	if (last) {
		wl_list_remove(&client->link);
		wl_list_insert(&last->link, &client->link);
	}
}

void client_get_geometry(struct jwc_client *client, struct wlr_box *box)
{
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface;
	uint32_t pending_serial = client->pending_serial;

	if (pending_serial > 0 && pending_serial >= surface->configure_serial) {
		box->x = client->pending_geo.x;
		box->y = client->pending_geo.y;
		box->width = client->pending_geo.width;
		box->height = client->pending_geo.height;
	} else {
		/* TODO: set geometry to 0,0 instead of shifting coordinates */
		box->x = client->x + surface->geometry.x;
		box->y = client->y + surface->geometry.y;
		box->width = surface->geometry.width;
		box->height = surface->geometry.height;
	}
}

struct jwc_client *client_get_focus(struct jwc_server *server)
{
	double cursor_x = server->cursor->x;
	double cursor_y = server->cursor->y;

	/* check if we have clients */
	struct wl_list *clients = &server->clients;
	if (wl_list_empty(clients))
		return NULL;

	/* loop through clients */
	struct jwc_client *client;
	double client_x, client_y;
	int geo_x, geo_y, geo_width, geo_height;

	wl_list_for_each(client, clients, link) {

		if (!client->mapped)
			continue;

		/* TODO: set geometry to 0,0 instead of shifting coordinates */
		geo_x = client->xdg_surface->geometry.x;
		geo_y = client->xdg_surface->geometry.y;
		geo_width = client->xdg_surface->geometry.width;
		geo_height = client->xdg_surface->geometry.height;

		client_x = client->x + geo_x;
		client_y = client->y + geo_y;

		/* intersect */
		if ((cursor_x > client_x) && (cursor_y > client_y) &&
		    (cursor_x < client_x + geo_width) &&
		    (cursor_y < client_y + geo_height))
			return client;
	}

	return NULL;
}

void client_close(struct jwc_client *client)
{
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface;
	if (surface)
		wlr_xdg_surface_v6_send_close(surface);
}

void client_set_maximazed(struct jwc_client *client, bool maximized)
{
	if (maximized == true) {
		/* save current coordinates */
		client_get_geometry(client, &client->orig);

		/* get the current output geometry of the client */
		struct wlr_box output_geo;
		output_get_output_geo_at(client->server, client->orig.x, client->orig.y,
					 &output_geo);

		/* maximized client to the corresponding output */
		client_move_resize(client, 0, 0, output_geo.width, output_geo.height);
	} else {
		/* restore origin coordinates */
		client_move_resize(client, client->orig.x, client->orig.y,
				   client->orig.width, client->orig.height);
	}

	wlr_xdg_toplevel_v6_set_maximized(client->xdg_surface, maximized);
	client->maximized = maximized;
}

void client_set_fullscreen(struct jwc_client *client, bool fullscreen)
{
	if (fullscreen == true) {
		/* save current coordinates */
		client_get_geometry(client, &client->orig);

		/* get the current output geometry of the client */
		struct wlr_box output_geo;
		output_get_output_geo_at(client->server, client->orig.x, client->orig.y,
					 &output_geo);

		/* maximized client to the corresponding output */
		client_move_resize(client, output_geo.x, output_geo.y,
				   output_geo.width, output_geo.height);
	} else {
		/* restore origin coordinates */
		client_move_resize(client, client->orig.x, client->orig.y,
				   client->orig.width, client->orig.height);
	}

	wlr_xdg_toplevel_v6_set_fullscreen(client->xdg_surface, fullscreen);
	client->fullscreen = fullscreen;
}

void client_move(struct jwc_client *client, double x, double y)
{
	struct wlr_box box;
	struct wlr_box *layout;

	/* check if the client don't cross layout */
	client_get_geometry(client, &box);
	layout = output_get_layout(client->server);
	if (x < layout->x)
		x = layout->x;
	if ((x + box.width) > (layout->x + layout->width))
		x = layout->x + layout->width - box.width;
	if (y < layout->y)
		y = layout->y;
	if ((y + box.height) > (layout->y + layout->height))
		y = layout->y + layout->height - box.height;

	/* TODO: set geometry to 0,0 instead of shifting coordinates */
	client->x = x - client->xdg_surface->geometry.x;
	client->y = y - client->xdg_surface->geometry.y;
}

void client_resize(struct jwc_client *client, double width, double height)
{
	struct wlr_box box;
	struct wlr_box *layout;

	/* check if the client don't cross layout */
	client_get_geometry(client, &box);
	layout = output_get_layout(client->server);
	if ((box.x + width) > (layout->x + layout->width))
		width = layout->x + layout->width - box.x;
	if ((box.y + height) > (layout->y + layout->height))
		height = layout->y + layout->height - box.y;

	/* request this toplevel surface be the given size */
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface;
	wlr_xdg_toplevel_v6_set_size(surface, width, height);
}

void client_move_resize(struct jwc_client *client, double x, double y,
			double width, double height)
{
	struct wlr_box *layout;

	/* check if the client don't cross layout */
	layout = output_get_layout(client->server);
	if (x < layout->x)
		x = layout->x;
	if ((x + width) > (layout->x + layout->width))
		width = layout->x + layout->width - x;
	if (y < layout->y)
		y = layout->y;
	if ((y + height) > (layout->y + layout->height))
		height = layout->y + layout->height + height - y;

	/* TODO */
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface;
	uint32_t serial = wlr_xdg_toplevel_v6_set_size(surface, width, height);
	if (serial > 0) {
		client->pending_geo.x = x;
		client->pending_geo.y = y;
		client->pending_geo.width = width;
		client->pending_geo.height = height;
		client->pending_serial = serial;
	} else if (client->pending_serial == 0) {
		client_move(client, x, y);
	}
}

void client_render_all(struct jwc_server *server, struct wlr_output *output,
		       struct timespec *when)
{
	struct wl_list *clients = &server->clients;
	if (wl_list_empty(clients))
		return;

	/* render all clients expect toplevel */
	struct jwc_client *client;
	struct render_data rdata = {
		.output = output,
		.renderer = server->renderer,
		.when = when,
	};

	wl_list_for_each_reverse(client, clients, link) {

		/* if client is not mapped, no need to render it */
		if (!client->mapped)
			continue;

		/* update xdg shell v6 surface of the client */
		rdata.client = client;
		wlr_xdg_surface_v6_for_each_surface(client->xdg_surface,
						    render_surface, &rdata);
	}
}

void client_update_all(struct jwc_server *server)
{
	struct wl_list *clients = &server->clients;
	if (wl_list_empty(clients))
		return;

	struct jwc_client *client;
	struct wlr_box box;
	struct wlr_output *output;

	wl_list_for_each(client, clients, link) {

		client_get_geometry(client, &box);

		output = output_get_output_at(server, box.x, box.y);

		if (output == NULL)
			client_move(client, 0, 0);
	}
}
