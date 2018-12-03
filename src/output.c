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

#include "output.h"
#include "client.h"

struct jwc_output {
	/* pointer to compositor server */
	struct jwc_server *server;

	/* index in outputs list */
	struct wl_list link;

	/* Wayland listeners */
	struct wl_listener frame;

	/* output ressources */
	struct wlr_output *wlr_output;
};

static void output_frame(struct wl_listener *listener, void *data)
{
	struct jwc_output *output = wl_container_of(listener, output, frame);
	struct wlr_renderer *renderer = output->server->renderer;

	/* get current time */
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	/* make the output rendering context current */
	if (!wlr_output_make_current(output->wlr_output, NULL))
		return;

	/* start rendering on all output frame*/
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	wlr_renderer_begin(renderer, width, height);

	/* default color in background */
	float color[4] = {0.2, 0.2, 0.2, 1.0};
	wlr_renderer_clear(renderer, color);

	/* update all client's surface of this output */
	client_update_all_surface(&output->server->clients, output->wlr_output, &now);

	/* Finish rendering and swap buffers */
	wlr_renderer_end(renderer);
	wlr_output_swap_buffers(output->wlr_output, NULL, NULL);
}

static void output_notify_new(struct wl_listener *listener, void *data)
{
	struct jwc_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/* set default Modesetting */
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		wlr_output_set_mode(wlr_output, mode);
	}

	wlr_log(WLR_INFO, "New output: %s %dx%d", wlr_output->name, wlr_output->width,
		wlr_output->height);

	/* create new output */
	struct jwc_output *output = calloc(1, sizeof(struct jwc_output));
	output->wlr_output = wlr_output;
	output->server = server;

	/* register callback when we get frame events from this output */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* add this output to the outputs server list */
	wl_list_insert(&server->outputs, &output->link);

	/* Set custom layout otherwise add an auto configured output to the layout */
	if (!strcmp(wlr_output->name, "eDP-1"))
		wlr_output_layout_add(server->output_layout, wlr_output, 2560, 0);
	else if (!strcmp(wlr_output->name, "DP-3"))
		wlr_output_layout_add(server->output_layout, wlr_output, 0, 0);
	else
		wlr_output_layout_add_auto(server->output_layout, wlr_output);

	/* create a global of this output */
	wlr_output_create_global(wlr_output);
}

void output_init(struct jwc_server *server)
{
	wl_list_init(&server->outputs);

	/* register callback when we have new output */
	server->new_output.notify = output_notify_new;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	/* create layout, it will be used to describe how the screens are organized */
	server->output_layout = wlr_output_layout_create();
}
