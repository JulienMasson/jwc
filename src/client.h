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

/**
 * Init several wayland protocols to accept new client
 */
void client_init(struct jwc_server *server);

/**
 * TODO
 */
struct jwc_client *client_get_focus(struct jwc_server *server);

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
void client_move(struct jwc_client *client, double x, double y);

/**
 * TODO
 */
void client_resize(struct jwc_client *client, uint32_t width, uint32_t height);

/**
 * Update all surface of the client's list
 */
void client_update_all_surface(struct wl_list *clients, struct wlr_output *output,
			       struct timespec *when);

#endif
