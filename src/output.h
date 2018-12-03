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

#ifndef OUTPUT_H
#define OUTPUT_H

#include "server.h"

/**
 * TODO
 */
void output_init(struct jwc_server *server);

/**
 * TODO
 */
struct wlr_box *output_get_layout(struct jwc_server *server);

/**
 * TODO
 */
void output_get_output_geo_at(struct jwc_server *server, double x, double y,
			      struct wlr_box *box);

#endif
