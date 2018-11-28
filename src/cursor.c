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

#include "cursor.h"
#include "bindings.h"

static void server_cursor_motion(struct wl_listener *listener, void *data)
{
	struct jwc_server *server = wl_container_of(listener, server, cursor_motion);
	struct wlr_event_pointer_motion *event = data;

	/* set default cursor image */
	wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->cursor);

	/* move the cursor in direction following the delta (x, y) */
	wlr_cursor_move(server->cursor, event->device, event->delta_x, event->delta_y);

	bindings_cursor(server);
}

static void cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct jwc_server *server = wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;

	/* set default cursor image */
	wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->cursor);

	/* move the cursor at the coordinates (x, y) */
	wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);

	bindings_cursor(server);
}

static void cursor_button(struct wl_listener *listener, void *data)
{
	struct jwc_server *server = wl_container_of(listener, server, cursor_button);
	struct wlr_event_pointer_button *event = data;

	server->cursor_button_left_pressed = false;
	server->cursor_button_right_pressed = false;

	if (event->state == WLR_BUTTON_PRESSED) {
		if (event->button == BTN_LEFT)
			server->cursor_button_left_pressed = true;
		else if (event->button == BTN_RIGHT)
			server->cursor_button_right_pressed = true;
	}
}

void cursor_init(struct jwc_server *server)
{
	server->cursor = wlr_cursor_create();
	server->cursor_button_left_pressed = false;
	server->cursor_button_right_pressed = false;

	/* attach the created cursor to the layout */
	wlr_cursor_attach_output_layout(server->cursor, server->output_layout);

	/* creates a new XCursor manager (size 24) and load it */
	server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server->cursor_mgr, 1);

	/* register callback when we receive relative motion event */
	server->cursor_motion.notify = cursor_motion;
	wl_signal_add(&server->cursor->events.motion, &server->cursor_motion);

	/* register callback when we receive absolute motion event */
	server->cursor_motion_absolute.notify = cursor_motion_absolute;
	wl_signal_add(&server->cursor->events.motion_absolute,
		      &server->cursor_motion_absolute);

	/* register callback when we receive cursor button event */
	server->cursor_button.notify = cursor_button;
	wl_signal_add(&server->cursor->events.button, &server->cursor_button);
}

void cursor_new(struct jwc_server *server, struct wlr_input_device *device)
{
	server->cursor_input = device;

	/* attaches this input device to the cursor */
	wlr_cursor_attach_input_device(server->cursor, device);
}

void cursor_set_image(struct jwc_server *server, const char *name)
{
	wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, name, server->cursor);
}

void cursor_move(struct jwc_server *server, double x, double y)
{
	if (server->cursor_input)
		wlr_cursor_warp_closest(server->cursor, server->cursor_input, x, y);
}
