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
	client_move(client, 50, 50);
	client_focus(client);
	client_show_on_toplevel(client);
}

static void xdg_surface_unmap(struct wl_listener *listener, void *data)
{
	/* TODO */
}

static void xdg_surface_destroy(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, destroy);
	wl_list_remove(&client->link);
	free(client);
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
	/* TODO */
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data)
{
	/* TODO */
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

	/* create matrix box */
	float matrix[9];
	struct wlr_box box = {
		.x = ox * output->scale,
		.y = oy * output->scale,
		.width = surface->current.width * output->scale,
		.height = surface->current.height * output->scale,
	};

	wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, 0,
		output->transform_matrix);

	/* render the matrix texture and send the surface */
	wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);
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
	struct wlr_xdg_toplevel_v6 *toplevel = xdg_surface->toplevel;

	/* create new client */
	struct jwc_client *client = calloc(1, sizeof(struct jwc_client));
	client->server = server;
	client->xdg_surface = xdg_surface;

	/* register callbacks when we get events from this client */
	client->map.notify = xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &client->map);

	client->unmap.notify = xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &client->unmap);

	client->destroy.notify = xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &client->destroy);

	client->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &client->request_move);

	client->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &client->request_resize);

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

void client_focus(struct jwc_client *client)
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

void client_show_on_toplevel(struct jwc_client *client)
{
	/* put this client on top of the list */
	wl_list_remove(&client->link);
	wl_list_insert(&client->server->clients, &client->link);
}

void client_get_geometry(struct jwc_client *client, struct wlr_box *box)
{
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface;
	struct wlr_surface_state current = surface->surface->current;

	box->x = client->x;
	box->y = client->y;
	box->width = current.width;
	box->height = current.height;
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
	struct wlr_surface_state current;
	wl_list_for_each(client, clients, link) {

		/* current surface */
		current = client->xdg_surface->surface->current;

		/* intersect */
		if ((cursor_x > client->x) && (cursor_y > client->y) &&
		    (cursor_x < client->x + current.width) &&
		    (cursor_y < client->y + current.height))
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


void client_move(struct jwc_client *client, double x, double y)
{
	client->x = x;
	client->y = y;
}

void client_resize(struct jwc_client *client, uint32_t width, uint32_t height)
{
	struct wlr_xdg_surface_v6 *surface = client->xdg_surface;

	wlr_xdg_toplevel_v6_set_size(surface, width, height);
}

void client_update_all_surface(struct wl_list *clients, struct wlr_output *output,
			       struct timespec *when)
{
	struct jwc_client *client;

	if (wl_list_empty(clients))
		return;

	wl_list_for_each_reverse(client, clients, link) {

		/* if client is not mapped, no need to render it */
		if (!client->mapped)
			continue;

		/* update xdg shell v6 surface of the client */
		struct jwc_server *server = client->server;
		struct render_data rdata = {
			.output = output,
			.client = client,
			.renderer = server->renderer,
			.when = when,
		};
		wlr_xdg_surface_v6_for_each_surface(client->xdg_surface,
						    render_surface, &rdata);
	}
}
