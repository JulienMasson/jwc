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
#include "utils.h"

struct jwc_output {
	/* pointer to compositor server */
	struct jwc_server *server;

	/* index in outputs list */
	struct wl_list link;

	/* Wayland listeners */
	struct wl_listener frame;
	struct wl_listener destroy;

	/* output ressources */
	struct wlr_output *wlr_output;
	bool enabled;
};

static void output_render(struct jwc_server *server, struct wlr_output *wlr_output)
{
	struct wlr_renderer *renderer = server->renderer;

	/* get current time */
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	/* make the output rendering context current */
	if (!wlr_output_make_current(wlr_output, NULL))
		return;

	/* start rendering on all output frame*/
	int width, height;
	wlr_output_effective_resolution(wlr_output, &width, &height);
	wlr_renderer_begin(renderer, width, height);

	/* default color in background */
	float color[4] = {0.2, 0.2, 0.2, 1.0};
	wlr_renderer_clear(renderer, color);

	/* update all client's surface of this output */
	client_render_all(server, wlr_output, &now);

	/* Finish rendering and swap buffers */
	wlr_renderer_end(renderer);
	wlr_output_swap_buffers(wlr_output, NULL, NULL);
}

static void output_frame(struct wl_listener *listener, void *data)
{
	struct jwc_output *output = wl_container_of(listener, output, frame);

	if (output->enabled)
		output_render(output->server, output->wlr_output);
}

static void output_destroy(struct wl_listener *listener, void *data)
{
	struct jwc_output *output = wl_container_of(listener, output, destroy);
	struct jwc_server *server = output->server;

	/* remove this output from the layout */
	wlr_output_layout_remove(server->output_layout, output->wlr_output);

	/* unregister listeners */
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->destroy.link);

	/* remove this output from the global list */
	wl_list_remove(&output->link);

	free(output);

	client_update_all(server);
}

static struct jwc_output *output_get_primary(struct jwc_server *server)
{
	struct wl_list *outputs = &server->outputs;
	if (wl_list_empty(outputs))
		return NULL;

	struct jwc_output *output;
	wl_list_for_each(output, outputs, link) {
		if (!strcmp(output->wlr_output->name, DEFAULT_PRIMARY_OUTPUT))
			return output;
	}

	return NULL;
}

static void output_auto_configure(struct jwc_server *server)
{
	struct wl_list *outputs = &server->outputs;
	if (wl_list_empty(outputs))
		return;

	/* make sure primary output is on top of the list */
	struct jwc_output *primary = output_get_primary(server);
	if (primary) {
		wl_list_remove(&primary->link);
		wl_list_insert(outputs, &primary->link);
	}

	/* auto detect if we need to add/remove output in layout */
	struct jwc_output *output;
	wl_list_for_each(output, outputs, link) {

		if (output->enabled) {
			/* check if this output is already in layout */
			if (wlr_output_layout_get(server->output_layout, output->wlr_output))
				continue;

			wlr_output_layout_add(server->output_layout, output->wlr_output,
					      0, 0);
		} else {
			/* check if this output is NOT in layout */
			if (!wlr_output_layout_get(server->output_layout, output->wlr_output))
				continue;

			wlr_output_layout_remove(server->output_layout, output->wlr_output);
		}
	}

	/* all the output are moved from the left to the right.
	 * the primary output is placed at the "most" right.
	 */
	int32_t width = 0;
	wl_list_for_each_reverse(output, outputs, link) {
		if (output->enabled) {
			wlr_output_layout_move(server->output_layout, output->wlr_output,
					       width, 0);
			width += output->wlr_output->width;
		}
	}

	/* update client coordinates if needed */
	client_update_all(server);
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

	/* create new output */
	struct jwc_output *output = calloc(1, sizeof(struct jwc_output));
	output->server = server;
	output->wlr_output = wlr_output;
	output->enabled = true;

	/* register callback when we get frame events from this output */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* register callback when we an output has been removed */
	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	/* add this output to the outputs server list */
	wl_list_insert(&server->outputs, &output->link);

	/* auto configure output */
	output_auto_configure(server);

	/* create a global of this output */
	wlr_output_create_global(wlr_output);
}

static struct wlr_output *output_get_layout_output_at(struct jwc_server *server, double x, double y)
{
	return wlr_output_layout_output_at(server->output_layout, x, y);
}

struct wlr_output *output_get_output_at(struct jwc_server *server, double x, double y)
{
	return output_get_layout_output_at(server, x, y);
}

void output_get_output_geo_at(struct jwc_server *server, double x, double y, struct wlr_box *box)
{
	struct wlr_output_layout_output *layout_output;
	struct wlr_output *output = output_get_layout_output_at(server, x, y);

	if (output) {
		layout_output = wlr_output_layout_get(server->output_layout, output);
		box->x = layout_output->x;
		box->y = layout_output->y;
		box->width = output->width;
		box->height = output->height;
	}
}

struct wlr_box *output_get_layout(struct jwc_server *server)
{
	return wlr_output_layout_get_box(server->output_layout, NULL);
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

void output_enable(struct jwc_server *server, const char *name, bool enabled)
{
	struct wl_list *outputs = &server->outputs;
	if (wl_list_empty(outputs))
		return;

	struct jwc_output *output;
	wl_list_for_each(output, outputs, link) {
		if (!strcmp(output->wlr_output->name, name)) {
			wlr_output_enable(output->wlr_output, enabled);
			output->enabled = enabled;
		}
	}

	output_auto_configure(server);
}
