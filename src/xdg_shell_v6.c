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
#include "utils.h"

static void xdg_surface_v6_close(struct jwc_client *client)
{
	wlr_xdg_surface_v6_send_close(client->xdg_surface_v6);
}

static void xdg_surface_v6_move(struct jwc_client *client, double x, double y)
{
	/* TODO: set geometry to 0,0 instead of shifting coordinates */
	client->x = x - client->xdg_surface_v6->geometry.x;
	client->y = y - client->xdg_surface_v6->geometry.y;
}

static void xdg_surface_v6_resize(struct jwc_client *client, double width, double height)
{
	wlr_xdg_toplevel_v6_set_size(client->xdg_surface_v6, width, height);
}

static void xdg_surface_v6_move_resize(struct jwc_client *client, double x, double y,
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

static void xdg_surface_v6_set_activated(struct jwc_client *client, bool activated)
{
	wlr_xdg_toplevel_v6_set_activated(client->xdg_surface_v6, activated);
}

static void xdg_surface_v6_set_maximized(struct jwc_client *client, bool maximized)
{
	wlr_xdg_toplevel_v6_set_maximized(client->xdg_surface_v6, maximized);
}

static void xdg_surface_v6_set_fullscreen(struct jwc_client *client, bool fullscreen)
{
	wlr_xdg_toplevel_v6_set_fullscreen(client->xdg_surface_v6, fullscreen);
}

static void xdg_surface_v6_get_geometry(struct jwc_client *client, struct wlr_box *box)
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

static struct wlr_surface *xdg_surface_v6_surface_at(struct jwc_client *client,
						     double sx, double sy,
						     double *sub_x, double *sub_y)
{
	return wlr_xdg_surface_v6_surface_at(client->xdg_surface_v6, sx, sy,
					     sub_x, sub_y);
}

static void xdg_surface_v6_for_each_surface(struct jwc_client *client,
					    wlr_surface_iterator_func_t iterator,
					    void *user_data)
{
	wlr_xdg_surface_v6_for_each_surface(client->xdg_surface_v6,
					    iterator, user_data);
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
}

void xdg_shell_v6_init(struct jwc_server *server)
{
	server->xdg_shell_v6 = wlr_xdg_shell_v6_create(server->wl_display);

	server->xdg_shell_v6_new_surface.notify = xdg_shell_v6_new_surface_event;
	wl_signal_add(&server->xdg_shell_v6->events.new_surface,
		      &server->xdg_shell_v6_new_surface);
}
