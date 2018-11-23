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
#include "client.h"

struct jwc_server server;

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

int main(void)
{
	/* init logging */
	redirect_stdio();
	wlr_log_init(WLR_INFO, NULL);

	/* init server: wayland ressources */
	server.wl_display = wl_display_create();
	server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
	assert(server.wl_display && server.wl_event_loop);

	/* init server: wlroots resources */
	server.backend = wlr_backend_autocreate(server.wl_display, NULL);
	assert(server.backend);
	server.renderer = wlr_backend_get_renderer(server.backend);
	assert(server.renderer);
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	/* init server: outputs */
	wl_list_init(&server.outputs);
	server.new_output.notify = output_notify_new;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);
	server.output_layout = wlr_output_layout_create();

	/* init server: clients */
	wl_list_init(&server.clients);
	client_init(&server);

	/* create wlroots compositor */
	wlr_compositor_create(server.wl_display, server.renderer);

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
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);

	return 0;
}
