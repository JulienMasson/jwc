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

#include <wlr/types/wlr_xdg_shell_v6.h>
#include "client.h"

struct jwc_client {
	/* pointer to global data structure (server) */
	struct jwc_server *server;

	/* index in clients list */
	struct wl_list link;

	/* xdg shell v6 surface */
	struct wlr_xdg_surface_v6 *xdg_surface;

	/* Wayland listeners */
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;

	/* client ressources */
	bool mapped;
	int x, y;
};

struct protocol_backend {
	/* pointer to global data structure (server) */
	struct jwc_server *server;

	/* xdg shell v6 resources */
	struct wlr_xdg_shell_v6 *xdg_shell_v6;
	struct wl_listener xdg_shell_v6_surface;
};

struct render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct jwc_client *client;
	struct timespec *when;
};

/* internal data */
static struct protocol_backend client_backend;

static void xdg_surface_map(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, map);
	client->mapped = true;
}

static void xdg_surface_unmap(struct wl_listener *listener, void *data)
{
	/* TODO */
}

static void xdg_surface_destroy(struct wl_listener *listener, void *data)
{
	/* TODO */
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
	struct protocol_backend *client_backend =
		wl_container_of(listener, client_backend, xdg_shell_v6_surface);
	struct jwc_server *server = client_backend->server;
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
	client_backend.server = server;

	/* create xdg shell v6 wayland protocol */
	client_backend.xdg_shell_v6 = wlr_xdg_shell_v6_create(server->wl_display);

	/* register callback when we have a new client */
	client_backend.xdg_shell_v6_surface.notify = client_notify_new;
	wl_signal_add(&client_backend.xdg_shell_v6->events.new_surface,
		      &client_backend.xdg_shell_v6_surface);
}

void client_update_all_surface(struct wl_list *clients, struct wlr_output *output,
			       struct timespec *when)
{
	struct jwc_client *client;

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
