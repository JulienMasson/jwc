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

static void xwayland_surface_close(struct jwc_client *client)
{
	wlr_xwayland_surface_close(client->xwayland_surface);
}

static void xwayland_surface_move(struct jwc_client *client, double x, double y)
{
	struct wlr_xwayland_surface *xwayland_surface = client->xwayland_surface;
	wlr_xwayland_surface_configure(xwayland_surface, x, y,
				       xwayland_surface->width,
				       xwayland_surface->height);
	client->x = x;
	client->y = y;
}

static void xwayland_surface_resize(struct jwc_client *client, double width, double height)
{
	wlr_xwayland_surface_configure(client->xwayland_surface, client->x, client->y,
				       width, height);
}

static void xwayland_surface_move_resize(struct jwc_client *client, double x, double y,
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

static void xwayland_surface_set_activated(struct jwc_client *client, bool activated)
{
	wlr_xwayland_surface_activate(client->xwayland_surface, activated);
}

static void xwayland_surface_set_maximized(struct jwc_client *client, bool maximized)
{
	wlr_xwayland_surface_set_maximized(client->xwayland_surface, maximized);
}

static void xwayland_surface_set_fullscreen(struct jwc_client *client, bool fullscreen)
{
	wlr_xwayland_surface_set_fullscreen(client->xwayland_surface, fullscreen);
}

static void xwayland_surface_get_geometry(struct jwc_client *client, struct wlr_box *box)
{
	struct wlr_xwayland_surface *surface = client->xwayland_surface;

	box->x = surface->x;
	box->y = surface->y;
	box->width = surface->width;
	box->height = surface->height;
}

static struct wlr_surface *xwayland_surface_surface_at(struct jwc_client *client,
						       double sx, double sy,
						       double *sub_x, double *sub_y)
{
	return wlr_surface_surface_at(client->surface, sx, sy, sub_x, sub_y);

}

static void xwayland_surface_for_each_surface(struct jwc_client *client,
					      wlr_surface_iterator_func_t iterator,
					      void *user_data)
{
	wlr_surface_for_each_surface(client->surface, iterator, user_data);
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

static void xwayland_unmap_event(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, unmap);
	client->mapped = false;
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

	/* register callback for destroy and surface commit event */
	client->surface_commit.notify = xwayland_surface_commit_event;
	wl_signal_add(&client->surface->events.commit,
		      &client->surface_commit);

	client->destroy.notify = client_destroy_event;
	client->unmap.notify = xwayland_unmap_event;
	wl_signal_add(&xwayland_surface->events.unmap, &client->unmap);
	wl_signal_add(&xwayland_surface->events.destroy, &client->destroy);

	client_setup(client);
	if (!xwayland_surface->override_redirect)
		client_center_on_cursor(client);
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

	client->request_configure.notify = xwayland_request_configure_event;
	wl_signal_add(&xwayland_surface->events.request_configure, &client->request_configure);
}

void xwayland_init(struct jwc_server *server)
{
	server->xwayland = wlr_xwayland_create(server->wl_display,
					       server->compositor, false);
	wlr_xwayland_set_seat(server->xwayland, server->seat);

	server->xwayland_new_surface.notify = xwayland_new_surface_event;
	wl_signal_add(&server->xwayland->events.new_surface,
		      &server->xwayland_new_surface);
}
