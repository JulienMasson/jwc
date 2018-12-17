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

static void client_setup(struct jwc_client *client)
{
	/* focus and show on toplevel */
	client_set_focus(client);
	client_set_on_toplevel(client);

	/* move client to have the current pointer center on this client */
	struct wlr_box box;
	client_get_geometry(client, &box);
	client_move(client,
		    client->server->cursor->x - (box.width / 2),
		    client->server->cursor->y - (box.height / 2));

	client->mapped = true;
}

static void client_destroy_event(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, destroy);
	wl_list_remove(&client->link);
	free(client);
}

void xdg_surface_v6_close(struct jwc_client *client)
{
	wlr_xdg_surface_v6_send_close(client->xdg_surface_v6);
}

void xdg_surface_v6_move(struct jwc_client *client, double x, double y)
{
	/* TODO: set geometry to 0,0 instead of shifting coordinates */
	client->x = x - client->xdg_surface_v6->geometry.x;
	client->y = y - client->xdg_surface_v6->geometry.y;
}

void xdg_surface_v6_resize(struct jwc_client *client, double width, double height)
{
	wlr_xdg_toplevel_v6_set_size(client->xdg_surface_v6, width, height);
}

void xdg_surface_v6_move_resize(struct jwc_client *client, double x, double y,
				  double width, double height)
{
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface_v6;
	uint32_t serial = wlr_xdg_toplevel_v6_set_size(surface, width, height);
	if (serial > 0) {
		client->pending_geo.x = x;
		client->pending_geo.y = y;
		client->pending_geo.width = width;
		client->pending_geo.height = height;
		client->pending_serial = serial;
	} else if (client->pending_serial == 0) {
		client->move(client, x, y);
	}
}

void xdg_surface_v6_set_activated(struct jwc_client *client, bool activated)
{
	wlr_xdg_toplevel_v6_set_activated(client->xdg_surface_v6, activated);
}

void xdg_surface_v6_set_maximized(struct jwc_client *client, bool maximized)
{
	wlr_xdg_toplevel_v6_set_maximized(client->xdg_surface_v6, maximized);
}

void xdg_surface_v6_set_fullscreen(struct jwc_client *client, bool fullscreen)
{
	wlr_xdg_toplevel_v6_set_fullscreen(client->xdg_surface_v6, fullscreen);
}

void xdg_surface_v6_get_geometry(struct jwc_client *client, struct wlr_box *box)
{
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface_v6;
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

struct wlr_surface *xdg_surface_v6_surface_at(struct jwc_client *client,
						double sx, double sy,
						double *sub_x, double *sub_y)
{
	return wlr_xdg_surface_v6_surface_at(client->xdg_surface_v6, sx, sy,
					     sub_x, sub_y);
}

void xdg_surface_v6_for_each_surface(struct jwc_client *client,
				       wlr_surface_iterator_func_t iterator,
				       void *user_data)
{
	wlr_xdg_surface_v6_for_each_surface(client->xdg_surface_v6,
					    render_surface, user_data);
}

static void xdg_surface_v6_commit_event(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, surface_commit);
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface_v6;

	uint32_t pending_serial = client->pending_serial;

	if (pending_serial > 0 && pending_serial >= surface->configure_serial) {

		client_move(client, client->pending_geo.x, client->pending_geo.y);

		if (pending_serial == surface->configure_serial)
			client->pending_serial = 0;
	}
}

static void xdg_surface_v6_map_event(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, map);

	/* save surface */
	client->surface = client->xdg_surface_v6->surface;

	/* reference client features */
	client->close = xdg_surface_v6_close;
	client->move = xdg_surface_v6_move;
	client->resize = xdg_surface_v6_resize;
	client->move_resize = xdg_surface_v6_move_resize;
	client->set_activated = xdg_surface_v6_set_activated;
	client->set_maximized = xdg_surface_v6_set_maximized;
	client->set_fullscreen = xdg_surface_v6_set_fullscreen;
	client->get_geometry = xdg_surface_v6_get_geometry;
	client->surface_at = xdg_surface_v6_surface_at;
	client->for_each_surface = xdg_surface_v6_for_each_surface;

	/* register callback for surface commit event */
	client->surface_commit.notify = xdg_surface_v6_commit_event;
	wl_signal_add(&client->surface->events.commit,
		      &client->surface_commit);

	client_setup(client);
}

static void xdg_shell_v6_new_surface_event(struct wl_listener *listener, void *data)
{
	struct jwc_server *server = wl_container_of(listener, server,
						    xdg_shell_v6_new_surface);
	struct wlr_xdg_surface_v6 *xdg_surface_v6 = data;

	/* only toplevel surface are accepted */
	if (xdg_surface_v6->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL)
		return;

	/* create new client */
	INFO("New XDG shell V6 client: %s", xdg_surface_v6->toplevel->title);
	wlr_xdg_surface_v6_ping(xdg_surface_v6);

	struct jwc_client *client = calloc(1, sizeof(struct jwc_client));
	client->server = server;
	client->xdg_surface_v6 = xdg_surface_v6;

	/* register callbacks when we get events from this client */
	client->map.notify = xdg_surface_v6_map_event;
	wl_signal_add(&xdg_surface_v6->events.map, &client->map);

	client->destroy.notify = client_destroy_event;
	wl_signal_add(&xdg_surface_v6->events.destroy, &client->destroy);

	/* add this client to the clients server list */
	wl_list_insert(&server->clients, &client->link);
}

void xwayland_surface_close(struct jwc_client *client)
{
	wlr_xwayland_surface_close(client->xwayland_surface);
}

void xwayland_surface_move(struct jwc_client *client, double x, double y)
{
	struct wlr_xwayland_surface *xwayland_surface = client->xwayland_surface;
	wlr_xwayland_surface_configure(xwayland_surface, x, y,
				       xwayland_surface->width,
				       xwayland_surface->height);
	client->x = x;
	client->y = y;
}

void xwayland_surface_resize(struct jwc_client *client, double width, double height)
{
	wlr_xwayland_surface_configure(client->xwayland_surface, client->x, client->y,
				       width, height);
}

void xwayland_surface_move_resize(struct jwc_client *client, double x, double y,
				  double width, double height)
{
	struct wlr_xwayland_surface *xwayland_surface = client->xwayland_surface;
	wlr_xwayland_surface_configure(xwayland_surface, x, y, width, height);

	client->pending_geo.x = x;
	client->pending_geo.y = y;
	client->pending_geo.width = width;
	client->pending_geo.height = height;
	client->pending_serial = 1;
}

void xwayland_surface_set_activated(struct jwc_client *client, bool activated)
{
	wlr_xwayland_surface_activate(client->xwayland_surface, activated);
}

void xwayland_surface_set_maximized(struct jwc_client *client, bool maximized)
{
	wlr_xwayland_surface_set_maximized(client->xwayland_surface, maximized);
}

void xwayland_surface_set_fullscreen(struct jwc_client *client, bool fullscreen)
{
	wlr_xwayland_surface_set_fullscreen(client->xwayland_surface, fullscreen);
}

void xwayland_surface_get_geometry(struct jwc_client *client, struct wlr_box *box)
{
	struct wlr_xwayland_surface *surface = client->xwayland_surface;

	box->x = surface->x;
	box->y = surface->y;
	box->width = surface->width;
	box->height = surface->height;
}

struct wlr_surface *xwayland_surface_surface_at(struct jwc_client *client,
						double sx, double sy,
						double *sub_x, double *sub_y)
{
	return wlr_surface_surface_at(client->surface, sx, sy, sub_x, sub_y);

}

void xwayland_surface_for_each_surface(struct jwc_client *client,
				       wlr_surface_iterator_func_t iterator,
				       void *user_data)
{
	wlr_surface_for_each_surface(client->surface, render_surface, user_data);
}

static void xwayland_surface_commit_event(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, surface_commit);
	uint32_t pending_serial = client->pending_serial;

	if (pending_serial > 0) {
		client_move(client, client->pending_geo.x, client->pending_geo.y);
		client->pending_serial = 0;
	}
}

static void xwayland_map_event(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, map);

	/* save surface */
	struct wlr_xwayland_surface *xwayland_surface = data;
	client->surface = xwayland_surface->surface;
	client->x = xwayland_surface->x;
	client->y = xwayland_surface->y;

	/* reference client features */
	client->close = xwayland_surface_close;
	client->move = xwayland_surface_move;
	client->resize = xwayland_surface_resize;
	client->move_resize = xwayland_surface_move_resize;
	client->set_activated = xwayland_surface_set_activated;
	client->set_maximized = xwayland_surface_set_maximized;
	client->set_fullscreen = xwayland_surface_set_fullscreen;
	client->get_geometry = xwayland_surface_get_geometry;
	client->surface_at = xwayland_surface_surface_at;
	client->for_each_surface = xwayland_surface_for_each_surface;

	/* register callback for surface commit event */
	client->surface_commit.notify = xwayland_surface_commit_event;
	wl_signal_add(&client->surface->events.commit,
		      &client->surface_commit);

	client_setup(client);
}

static void xwayland_request_configure_event(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, request_configure);
	struct wlr_xwayland_surface *xwayland_surface = client->xwayland_surface;
	struct wlr_xwayland_surface_configure_event *event = data;

	client->x = event->x;
	client->y = event->y;

	wlr_xwayland_surface_configure(xwayland_surface, event->x, event->y,
				       event->width, event->height);
}

static void xwayland_new_surface_event(struct wl_listener *listener, void *data)
{
	struct jwc_server *server = wl_container_of(listener, server,
						    xwayland_new_surface);
	struct wlr_xwayland_surface *xwayland_surface = data;

	wlr_xwayland_surface_ping(xwayland_surface);

	/* create new client */
	INFO("New xwayland client: %s", xwayland_surface->title);
	struct jwc_client *client = calloc(1, sizeof(struct jwc_client));
	if (!client)
		return;
	client->server = server;
	client->xwayland_surface = xwayland_surface;

	/* register callbacks when we get events from this client */
	client->map.notify = xwayland_map_event;
	wl_signal_add(&xwayland_surface->events.map, &client->map);

	client->destroy.notify = client_destroy_event;
	wl_signal_add(&xwayland_surface->events.destroy, &client->destroy);

	client->request_configure.notify = xwayland_request_configure_event;
	wl_signal_add(&xwayland_surface->events.request_configure, &client->request_configure);

	/* add this client to the clients server list */
	wl_list_insert(&server->clients, &client->link);
}

void client_init(struct jwc_server *server)
{
	wl_list_init(&server->clients);

	/* support xdg shell v6 wayland client */
	server->xdg_shell_v6 = wlr_xdg_shell_v6_create(server->wl_display);

	server->xdg_shell_v6_new_surface.notify = xdg_shell_v6_new_surface_event;
	wl_signal_add(&server->xdg_shell_v6->events.new_surface,
		      &server->xdg_shell_v6_new_surface);

	/* support xwayland client */
	server->xwayland = wlr_xwayland_create(server->wl_display,
						server->compositor, false);
	wlr_xwayland_set_seat(server->xwayland, server->seat);

	server->xwayland_new_surface.notify = xwayland_new_surface_event;
	wl_signal_add(&server->xwayland->events.new_surface,
		      &server->xwayland_new_surface);
}

static struct jwc_client *client_get_from_surface(struct jwc_server *server,
						  struct wlr_surface *surface)
{
	struct wl_list *clients = &server->clients;
	if (wl_list_empty(clients))
		return NULL;

	struct jwc_client *client;
	wl_list_for_each(client, clients, link) {
		if (client->surface == surface)
			return client;
	}

	return NULL;
}

void client_set_focus(struct jwc_client *client)
{
	struct wlr_surface *surface = client->surface;
	struct wlr_seat *seat = client->server->seat;

	/* already focused */
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface)
		return;

	/* set unactivated the previous focus */
	if (prev_surface) {
		struct jwc_client *prev_client;
		prev_client = client_get_from_surface(client->server, prev_surface);
		if (prev_client)
			prev_client->set_activated(prev_client, false);
	}

	/* set activated the current focus */
	client->set_activated(client, true);

	/* notify keyboard enter */
	keyboard_enter(client->server, surface);
}

struct wlr_surface *client_surface_at(struct jwc_client *client, double sx, double sy,
				      double *sub_x, double *sub_y)
{
	return client->surface_at(client, sx, sy, sub_x, sub_y);
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
	client->get_geometry(client, box);
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
	struct wlr_box box;
	wl_list_for_each(client, clients, link) {

		if (!client->mapped)
			continue;

		/* intersect */
		client_get_geometry(client, &box);
		if ((cursor_x > box.x) && (cursor_y > box.y) &&
		    (cursor_x < box.x + box.width) &&
		    (cursor_y < box.y + box.height))
			return client;
	}

	return NULL;
}

void client_close(struct jwc_client *client)
{
	client->close(client);
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

	client->set_maximized(client, maximized);
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

	client->set_fullscreen(client, fullscreen);
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

	client->move(client, x, y);
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

	client->resize(client, width, height);
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

	client->move_resize(client, x, y, width, height);
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

		/* update surface of the client */
		rdata.client = client;
		client->for_each_surface(client, render_surface, &rdata);
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
