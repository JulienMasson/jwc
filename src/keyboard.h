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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "server.h"

struct jwc_keyboard {
	/* pointer to compositor server */
	struct jwc_server *server;

	/* index in keyboards list */
	struct wl_list link;

	/* Wayland listeners */
	struct wl_listener key;
	struct wl_listener modifiers;

	/* input ressources */
	struct wlr_input_device *device;
};

/**
 * TODO
 */
void keyboard_init(struct jwc_server *server);

/**
 * TODO
 */
void keyboard_new(struct jwc_server *server, struct wlr_input_device *device);

/**
 * TODO
 */
void keyboard_enter(struct jwc_server *server, struct wlr_surface *surface);

#endif
