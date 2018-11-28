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

#include "bindings.h"
#include "client.h"
#include "output.h"

void bindings_cursor(struct jwc_server *server)
{
	if (server->meta_key_pressed) {

		/* move client */
		if (server->cursor_button_left_pressed) {
			/* TODO */
		}

		/* resize client */
		if (server->cursor_button_right_pressed) {
			/* TODO */
		}
	}
}

bool bindings_keyboard(struct jwc_server *server, xkb_keysym_t syms)
{
	bool handle = true;

	switch(syms) {

	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;

	case XKB_KEY_Return:
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", "weston-terminal", (void *)NULL);
		}
		break;

	default:
		handle = false;
		break;
	}

	return handle;
}
