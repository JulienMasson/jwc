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
#include "bindings.h"

#define META_KEY		XKB_KEY_Alt_L
#define META_MODIFIER_KEY	WLR_MODIFIER_ALT

static uint32_t keyboard_get_modifiers(struct jwc_keyboard *keyboard)
{
	struct wlr_keyboard *wlr_kb = keyboard->device->keyboard;

	return wlr_keyboard_get_modifiers(wlr_kb);
}

static xkb_keysym_t keyboard_get_keysym(struct jwc_keyboard *keyboard, uint32_t keycode)
{
	struct wlr_keyboard *wlr_kb = keyboard->device->keyboard;

	return xkb_state_key_get_one_sym(wlr_kb->xkb_state, keycode + 8);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data)
{
	struct jwc_keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct jwc_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	bool handle;

	/* Apply actions following the key event:
	 *
	 * - if the META key is pressed or release with no modifier:
	 *   --> set meta_key_pressed to the corresponding value
	 *
	 * - if the META key is pressed as a modifier:
	 *   --> handle if there is a keybindings associated
	 */
	uint32_t modifiers = keyboard_get_modifiers(keyboard);
	xkb_keysym_t sym = keyboard_get_keysym(keyboard, event->keycode);

	if (event->state == WLR_KEY_PRESSED) {

		if (sym == META_KEY)
			server->meta_key_pressed = true;

		if (modifiers & META_MODIFIER_KEY)
			handle = bindings_keyboard(server, sym);

	} else if (event->state == WLR_KEY_RELEASED) {

		if (sym == META_KEY)
			server->meta_key_pressed = false;
	}

	/* if no bindings is associated to the keycode, notify the seat that
	 * a key has been pressed on the keyboard.
	 */
	if (handle == false) {
		struct wlr_seat *seat = server->seat;
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
					     event->keycode, event->state);
	}
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
	server->meta_key_pressed = false;
}

void keyboard_new(struct jwc_server *server, struct wlr_input_device *device)
{
	/* create new keyboard */
	struct jwc_keyboard *keyboard = calloc(1, sizeof(struct jwc_keyboard));
	keyboard->server = server;
	keyboard->device = device;

	/* set keymap of this keyboard */
	keyboard_set_keymap(device->keyboard);

	/* set repeat info: 25 repeats each 600 milliseconds */
	wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

	/* register callback when we receive key event */
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);

	/* set this keyboard as the active keyboard for the seat */
	wlr_seat_set_keyboard(server->seat, device);

	/* add this keyboard to the keyboards server list */
	wl_list_insert(&server->keyboards, &keyboard->link);
}

void keyboard_enter(struct jwc_server *server, struct wlr_surface *surface)
{
	struct wlr_seat *seat = server->seat;
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

	wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes,
				       keyboard->num_keycodes, &keyboard->modifiers);
}
