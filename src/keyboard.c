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

#include "keyboard.h"

struct jwc_keyboard {
	/* pointer to compositor server */
	struct jwc_server *server;

	/* index in keyboards list */
	struct wl_list link;

	/* Wayland listeners */
	struct wl_listener key;

	/* input ressources */
	struct wlr_input_device *device;
};

static void keyboard_handle_key(struct wl_listener *listener, void *data)
{
	struct jwc_keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct jwc_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat;

	wlr_seat_set_keyboard(seat, keyboard->device);
	wlr_seat_keyboard_notify_key(seat, event->time_msec,
				     event->keycode, event->state);
}

static void keyboard_set_keymap(struct wlr_keyboard *keyboard)
{
	struct xkb_rule_names rules = { 0 };
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
							   XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(keyboard, keymap);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
}

void keyboard_init(struct jwc_server *server)
{
	wl_list_init(&server->keyboards);
}

void keyboard_new(struct jwc_server *server, struct wlr_input_device *device)
{
	/* create new keyboard */
	struct jwc_keyboard *keyboard = calloc(1, sizeof(struct jwc_keyboard));
	keyboard->server = server;
	keyboard->device = device;

	/* set keymap of this keyboard */
	keyboard_set_keymap(device->keyboard);

	/* set repeat info: 25 repeats each 600 milliseconds*/
	wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

	/* register callback when we receive key event */
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);

	/* set this keyboard as the active keyboard for the seat. */
	wlr_seat_set_keyboard(server->seat, device);

	/* add this keyboard to the keyboards server list */
	wl_list_insert(&server->keyboards, &keyboard->link);
}

void keyboard_enter(struct wlr_seat *seat, struct wlr_surface *surface)
{
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

	wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes,
				       keyboard->num_keycodes, &keyboard->modifiers);
}
