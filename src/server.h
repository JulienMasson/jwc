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

#ifndef SERVER_H
#define SERVER_H

#include <assert.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>

struct jwc_server {
	/* Wayland resources */
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	/* wlroots resources */
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_compositor *compositor;
	struct wlr_seat *seat;

	/* Output resources */
	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;

	/* input ressources */
	struct wl_listener new_input;

	/* cursor ressources */
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	bool cursor_button_left_pressed;
	bool cursor_button_right_pressed;
	struct wlr_input_device *cursor_input;

	/* keyboard ressources */
	struct wl_list keyboards;
	bool meta_key_pressed;

	/* clients resources */
	struct wl_list clients;
	struct wlr_xdg_shell_v6 *xdg_shell_v6;
	struct wlr_xwayland *xwayland;
	struct wl_listener xdg_shell_v6_new_surface;
	struct wl_listener xwayland_new_surface;
};

#endif
