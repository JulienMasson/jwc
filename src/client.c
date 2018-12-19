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

/* client init function declaration */
void xdg_shell_v6_init(struct jwc_server *server);
void xwayland_init(struct jwc_server *server);

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

	/* render the client texture */
	wlr_render_texture(rdata->renderer, texture, output->transform_matrix, ox, oy,
			   client->alpha);

	wlr_surface_send_frame_done(surface, rdata->when);
}

static bool client_exist(struct jwc_server *server, struct jwc_client *client)
{
	struct wl_list *clients = &server->clients;
	if (wl_list_empty(clients))
		return NULL;

	struct jwc_client *tmp;
	wl_list_for_each(tmp, clients, link) {
		if (&tmp->link == &client->link)
			return true;
	}

	return false;
}

void client_setup(struct jwc_client *client)
{
	struct jwc_server *server = client->server;

	/* by default client has no transparency */
	client->alpha = 1;

	/* add this client to the clients server list */
	if (!client_exist(server, client))
		wl_list_insert(&server->clients, &client->link);

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

static void client_safe_remove(struct jwc_client *client)
{
	if (client_exist(client->server, client))
		wl_list_remove(&client->link);
	else
		INFO("Client not found");
}

void client_destroy_event(struct wl_listener *listener, void *data)
{
	struct jwc_client *client = wl_container_of(listener, client, destroy);
	client_safe_remove(client);
	free(client);
}

void client_init(struct jwc_server *server)
{
	wl_list_init(&server->clients);

	/* init all client type */
	xdg_shell_v6_init(server);
	xwayland_init(server);
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
