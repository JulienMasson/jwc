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

#include "server.h"
#include "output.h"
#include "input.h"
#include "cursor.h"
#include "keyboard.h"
#include "client.h"

static void redirect_stdio(void)
{
	char log_file[256];
	memset(log_file, '\0', 256);
	snprintf(log_file, 256, "%s/.jwc.log", getenv("HOME"));

	/* redirect stdout */
	freopen(log_file, "a", stdout);
	setbuf(stdout, NULL);

	/* redirect stderr */
	freopen(log_file, "a", stderr);
	setbuf(stderr, NULL);
}

static void wlroots_init(struct jwc_server *server)
{
	/* automatically initializes the most suitable backend */
	server->backend = wlr_backend_autocreate(server->wl_display, NULL);
	assert(server->backend);

	/* create a wl data device manager global for this display */
	wlr_data_device_manager_create(server->wl_display);

	/* get the renderer of the backend used */
	server->renderer = wlr_backend_get_renderer(server->backend);
	assert(server->renderer);

	/* init the wayland display from the renderer */
	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	/* allocate new compositor and add global to the display*/
	server->compositor = wlr_compositor_create(server->wl_display,
						   server->renderer);

	/* allocate new seat and add global to the display*/
	server->seat = wlr_seat_create(server->wl_display, "seat0");
}

int main(void)
{
	struct jwc_server server;

	/* init logging */
	redirect_stdio();
	wlr_log_init(WLR_INFO, NULL);

	/* init server: wayland */
	server.wl_display = wl_display_create();
	server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
	assert(server.wl_display && server.wl_event_loop);

	/* init server: wlroots */
	wlroots_init(&server);

	/* init server: modules */
	output_init(&server);
	input_init(&server);
	cursor_init(&server);
	keyboard_init(&server);
	client_init(&server);

	/* open wayland socket */
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}
	setenv("WAYLAND_DISPLAY", socket, true);

	/* start wlroots backend */
	if (!wlr_backend_start(server.backend)) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

	/* loop */
	wl_display_run(server.wl_display);

	/* free resources */
	wlr_xwayland_destroy(server.xwayland);
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);

	return 0;
}
