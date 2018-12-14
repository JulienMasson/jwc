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

static void move_cursor_center_client(struct jwc_client *client)
{
	struct wlr_box box;
	client_get_geometry(client, &box);
	cursor_move(client->server, box.x + (box.width / 2), box.y + (box.height / 2));
}

bool bindings_cursor(struct jwc_server *server, double x, double y)
{
	static struct jwc_client *focus;
	static bool action_ongoing = false;
	struct wlr_box box;

	/* if the meta key has been pressed and
	 * the client is not fullscreen, take some actions.
	 */
	if (server->meta_key_pressed) {

		/* move client */
		if (server->cursor_button_left_pressed) {

			if (action_ongoing == false) {
				/* get focus client */
				focus = client_get_focus(server);
				if (focus == NULL || focus->fullscreen)
					return false;
			}
			action_ongoing = true;

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

			/* reset some client ressources */
			focus->maximized = false;

			return true;
		}

		/* resize client */
		if (server->cursor_button_right_pressed) {

			if (action_ongoing == false) {
				/* get focus client */
				focus = client_get_focus(server);
				if (focus == NULL || focus->fullscreen)
					return false;
			}
			action_ongoing = true;

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
			cursor_move(server, x - 1, y - 1);

			/* reset some client ressources */
			focus->maximized = false;

			return true;
		}
	} else
		action_ongoing = false;

	return false;
}

bool bindings_keyboard(struct jwc_server *server, xkb_keysym_t syms)
{
	struct jwc_client *focus;
	bool handle = true;

	switch(syms) {

	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		handle = true;
		break;

	case XKB_KEY_Return:
		if (fork() == 0)
			execl("/bin/sh", "/bin/sh", "-c", "weston-terminal", (void *)NULL);
		break;

	case XKB_KEY_Right:
		focus = client_get_focus(server);
		if (focus != NULL && !focus->fullscreen) {
			/* get the current output geometry of the focus */
			struct wlr_box box, output_geo;
			client_get_geometry(focus, &box);
			output_get_output_geo_at(server, box.x, box.y, &output_geo);

			/* move and resize client to right output */
			client_move_resize(focus, output_geo.x + (output_geo.width / 2), 0,
					   output_geo.width / 2, output_geo.height);
			move_cursor_center_client(focus);

			/* reset some client ressources */
			focus->maximized = false;
		}
		break;

	case XKB_KEY_Left:
		focus = client_get_focus(server);
		if (focus != NULL && !focus->fullscreen) {
			/* get the current output geometry of the focus */
			struct wlr_box box, output_geo;
			client_get_geometry(focus, &box);
			output_get_output_geo_at(server, box.x, box.y, &output_geo);

			/* move and resize client to left output */
			client_move_resize(focus, output_geo.x, 0,
					   output_geo.width / 2, output_geo.height);
			move_cursor_center_client(focus);

			/* reset some client ressources */
			focus->maximized = false;
		}
		break;

	case XKB_KEY_Tab:
		focus = client_get_last(server);
		if (focus != NULL) {
			client_set_focus(focus);
			client_set_on_toplevel(focus);
		}
		break;

	case XKB_KEY_a:
		wl_list_for_each(focus, &server->clients, link) {
			focus->mapped = true;
		}
		break;

	case XKB_KEY_c:
		focus = client_get_focus(server);
		if (focus != NULL)
			client_close(focus);
		break;

	case XKB_KEY_e:
		if (fork() == 0)
			execl("/bin/sh", "/bin/sh", "-c", "emacs", (void *)NULL);
		break;

	case XKB_KEY_f:
		focus = client_get_focus(server);
		if (focus != NULL) {
			client_set_fullscreen(focus, !focus->fullscreen);
			move_cursor_center_client(focus);
			return true;
		}
		break;

	case XKB_KEY_h:
		focus = client_get_focus(server);
		if (focus != NULL) {
			client_unmap(focus);
			focus = client_get_on_toplevel(server);
			if (focus != NULL) {
				client_set_focus(focus);
				client_set_on_toplevel(focus);
			}
		}
		break;

	case XKB_KEY_m:
		focus = client_get_focus(server);
		if (focus != NULL && !focus->fullscreen) {
			client_set_maximazed(focus, !focus->maximized);
			move_cursor_center_client(focus);
		}
		break;

	default:
		handle = false;
		break;
	}

	return handle;
}
