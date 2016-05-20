/*
    Copyright (C) 2016  Jeremy White <jwhite@codeweavers.com>
    All rights reserved.

    This file is part of x11spice

    x11spice is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    x11spice is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with x11spice.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SESSION_H_
#define SESSION_H_

#include "options.h"
#include "display.h"
#include "local_spice.h"
#include "gui.h"

/*----------------------------------------------------------------------------
**  Structure definitions
**--------------------------------------------------------------------------*/
typedef struct
{
    options_t   options;
    display_t   display;
    spice_t     spice;
    gui_t       gui;
    SpiceWatch  *xwatch;

    GAsyncQueue *cursor_queue;
    GAsyncQueue *draw_queue;
} session_t;

/*----------------------------------------------------------------------------
**  Prototypes
**--------------------------------------------------------------------------*/
int session_start(session_t *s);
void session_end(session_t *s);

void *session_pop_draw(void *session_ptr);
int session_draw_waiting(void *session_ptr);
#endif
