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

struct jwc_output {
	struct wl_list link;
	struct jwc_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
};

static void output_frame(struct wl_listener *listener, void *data)
{
	struct jwc_output *output = wl_container_of(listener, output, frame);
	struct wlr_renderer *renderer = output->server->renderer;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (!wlr_output_make_current(output->wlr_output, NULL)) {
		return;
	}
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	wlr_renderer_begin(renderer, width, height);

	float color[4] = {0.3, 0.3, 0.3, 1.0};
	wlr_renderer_clear(renderer, color);

	wlr_renderer_end(renderer);
	wlr_output_swap_buffers(output->wlr_output, NULL, NULL);
}

void output_notify_new(struct wl_listener *listener, void *data)
{
	struct jwc_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	struct jwc_output *output = calloc(1, sizeof(struct jwc_output));
	output->wlr_output = wlr_output;
	output->server = server;
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);

	wlr_output_layout_add_auto(server->output_layout, wlr_output);

	wlr_output_create_global(wlr_output);
}
