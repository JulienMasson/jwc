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
#include "cursor.h"
#include "utils.h"

bool bindings_cursor(struct jwc_server *server, double x, double y)
{
	struct jwc_client *focus;
	struct wlr_box box;

	if (server->meta_key_pressed) {

		/* move client */
		if (server->cursor_button_left_pressed) {
			focus = client_get_focus(server);
			if (focus != NULL) {
				/* calculate focus coordinates based on
				 * client geometry.
				 */
				double focus_x, focus_y;
				client_get_geometry(focus, &box);
				focus_x = x - (box.width / 2);
				focus_y = y - (box.height / 2);

				/* move client/cursor */
				client_move(focus, focus_x, focus_y);
				cursor_set_image(server, "all-scroll");
				cursor_move(server, x, y);

				return true;
			}
		}

		/* resize client */
		if (server->cursor_button_right_pressed) {
			focus = client_get_focus(server);
			if (focus != NULL) {
				/* calculate focus coordinates based on
				 * client geometry.
				 */
				double focus_width, focus_height;
				client_get_geometry(focus, &box);
				focus_width = x - box.x;
				focus_height = y - box.y;

				/* resize client and move cursor */
				client_resize(focus, focus_width, focus_height);
				cursor_set_image(server, "bottom_right_corner");

				/* TODO: need a grab mechanism instead of shifting by 40
				 * to make sure we don't loose the focus when resizing
				 */
				cursor_move(server, x - 40, y - 40);

				return true;
			}
		}
	}

	return false;
}

bool bindings_keyboard(struct jwc_server *server, xkb_keysym_t syms)
{
	struct jwc_client *focus;
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

	case XKB_KEY_c:
		focus = client_get_focus(server);
		if (focus != NULL)
			client_close(focus);
		return true;

	default:
		handle = false;
		break;
	}

	return handle;
}
