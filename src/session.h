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
#include "agent.h"
#include "gui.h"
#include "scan.h"

/*----------------------------------------------------------------------------
**  Structure definitions
**--------------------------------------------------------------------------*/
typedef struct session_struct {
    options_t options;
    display_t display;
    spice_t spice;
    agent_t agent;
    gui_t gui;
    scanner_t scanner;
    int running;

    int connected;
    int connect_pid;
    int disconnect_pid;

    GMutex *lock;
    int draw_command_in_progress;

    GAsyncQueue *cursor_queue;
    GAsyncQueue *draw_queue;
} session_t;

/*----------------------------------------------------------------------------
**  Prototypes
**--------------------------------------------------------------------------*/
int session_create(session_t *s);
void session_destroy(session_t *s);
int session_start(session_t *s);
void session_end(session_t *s);
int session_alive(session_t *s);

void session_handle_resize(session_t *s);

void *session_pop_draw(session_t *session);
int session_draw_waiting(session_t *session);

void *session_pop_cursor(session_t *session);
int session_cursor_waiting(session_t *session);

void session_handle_key(session_t *session, uint8_t keycode, int is_press);
void session_handle_mouse_position(session_t *session, int x, int y, uint32_t buttons_state);
void session_handle_mouse_buttons(session_t *session, uint32_t buttons_state);
void session_handle_mouse_wheel(session_t *session, int wheel_motion, uint32_t buttons_state);

int session_get_one_led(session_t *session, const char *name);

int session_push_cursor_image(session_t *s,
                              int x, int y, int w, int h, int xhot, int yhot,
                              int imglen, uint8_t *imgdata);

void session_remote_connected(const char *from);
void session_remote_disconnected(void);

void session_disconnect_client(session_t *session);
#endif
