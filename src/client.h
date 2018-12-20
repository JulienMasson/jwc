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

#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"

struct jwc_client {
	/* pointer to compositor server */
	struct jwc_server *server;

	/* index in clients list */
	struct wl_list link;

	/* surface ressources */
	struct wlr_surface *surface;
	union {
		struct wlr_xdg_surface_v6 *xdg_surface_v6;
		struct wlr_xwayland_surface *xwayland_surface;
	};

	/* surface features */
	void (*close)(struct jwc_client *client);
	void (*move)(struct jwc_client *client, double x, double y);
	void (*resize)(struct jwc_client *client, double width, double height);
	void (*move_resize)(struct jwc_client *client, double x, double y,
			    double width, double height);
	void (*set_activated)(struct jwc_client *client, bool activated);
	void (*set_maximized)(struct jwc_client *client, bool maximized);
	void (*set_fullscreen)(struct jwc_client *client, bool fullscreen);
	void (*get_geometry)(struct jwc_client *client, struct wlr_box *box);
	struct wlr_surface *(*surface_at)(struct jwc_client *client,
					 double sx, double sy,
					 double *sub_x, double *sub_y);
	void (*for_each_surface)(struct jwc_client *client,
				 wlr_surface_iterator_func_t iterator,
				 void *user_data);
	bool (*is_focusable)(struct jwc_client *client);

	/* Wayland listeners */
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener surface_commit;
	struct wl_listener request_configure;

	/* client ressources */
	double x, y;
	struct wlr_box orig;
	struct wlr_box pending_geo;
	uint32_t pending_serial;
	bool mapped, maximized, fullscreen;
	float alpha;
};

/**
 * Init several wayland protocols to accept new client
 */
void client_init(struct jwc_server *server);
void client_setup(struct jwc_client *client);
void client_center_on_cursor(struct jwc_client *client);
void client_destroy_event(struct wl_listener *listener, void *data);

/**
 * TODO
 */
struct jwc_client *client_get_focus(struct jwc_server *server);
void client_set_focus(struct jwc_client *client);

/**
 * TODO
 */
struct wlr_surface *client_surface_at(struct jwc_client *client, double sx, double sy,
				      double *sub_x, double *sub_y);

/**
 * TODO
 */
struct jwc_client *client_get_on_toplevel(struct jwc_server *server);
void client_set_on_toplevel(struct jwc_client *client);

/**
 * TODO
 */
struct jwc_client *client_get_last(struct jwc_server *server);

/**
 * TODO
 */
void client_unmap(struct jwc_client *client);

/**
 * TODO
 */
void client_get_geometry(struct jwc_client *client, struct wlr_box *box);

/**
 * TODO
 */
void client_close(struct jwc_client *client);

/**
 * TODO
 */
void client_set_maximazed(struct jwc_client *client, bool maximized);
void client_set_fullscreen(struct jwc_client *client, bool fullscreen);

/**
 * TODO
 */
void client_move(struct jwc_client *client, double x, double y);
void client_resize(struct jwc_client *client, double width, double height);
void client_move_resize(struct jwc_client *client, double x, double y,
			double width, double height);

/**
 * TODO
 */
void client_render_all(struct jwc_server *server, struct wlr_output *output,
		       struct timespec *when);
void client_update_all(struct jwc_server *server);

#endif
