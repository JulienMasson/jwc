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

#include "input.h"
#include "cursor.h"
#include "keyboard.h"

static void new_input(struct wl_listener *listener, void *data)
{
	struct jwc_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	static uint32_t caps;

	switch (device->type) {

	case WLR_INPUT_DEVICE_KEYBOARD:
		/* new keyboard detected */
		keyboard_new(server, device);
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
		break;

	case WLR_INPUT_DEVICE_POINTER:
		/* new cursor detected */
		cursor_new(server, device);
		caps |= WL_SEAT_CAPABILITY_POINTER;
		break;

	default:
		break;
	}

	/* updates the capabilities available on this seat */
	wlr_seat_set_capabilities(server->seat, caps);
}

void input_init(struct jwc_server *server)
{
	/* register callback when we get new input */
	server->new_input.notify = new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);
}
