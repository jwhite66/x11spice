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

#include <stdio.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xtest.h>
#include <xcb/xcb_aux.h>

#include "x11spice.h"
#include "session.h"
#include "scan.h"


void free_cursor_queue_item(gpointer data)
{
    // FIXME
}

void free_draw_queue_item(gpointer data)
{
    // FIXME
}

void *session_pop_draw(session_t *session)
{
    if (! session || ! session->running)
        return NULL;

    return g_async_queue_try_pop(session->draw_queue);
}

int session_draw_waiting(session_t *session)
{
    if (! session || ! session->running)
        return 0;

    return g_async_queue_length(session->draw_queue);
}

void session_handle_key(session_t *session, uint8_t keycode, int is_press)
{
    xcb_test_fake_input(session->display.c, is_press ? XCB_KEY_PRESS : XCB_KEY_RELEASE,
        keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    g_debug("key 0x%x, press %d", keycode, is_press);
    // FIXME - and maybe a sync too... xcb_flush(s->display.c);
    xcb_flush(session->display.c);
}

void session_handle_mouse_position(session_t *session, int x, int y, uint32_t buttons_state)
{
    xcb_test_fake_input(session->display.c, XCB_MOTION_NOTIFY, 0, XCB_CURRENT_TIME,
        session->display.screen->root, x, y, 0);
    xcb_flush(session->display.c);
}

#define BUTTONS 5
static void session_handle_button_change(session_t *s, uint32_t buttons_state)
{
    int i;
    for (i = 0; i < BUTTONS; i++) {
        if ((buttons_state ^ s->spice.buttons_state) & (1 << i)) {
            int action = (buttons_state & (1 << i));
            xcb_test_fake_input(s->display.c, action ? XCB_BUTTON_PRESS : XCB_BUTTON_RELEASE,
                i + 1, XCB_CURRENT_TIME, s->display.screen->root, 0, 0, 0);
        }
    }
    s->spice.buttons_state = buttons_state;
}

static uint32_t convert_spice_buttons(int wheel, uint32_t buttons_state)
{
    // For some reason spice switches the second and third button, undo that.
    // basically undo RED_MOUSE_STATE_TO_LOCAL
    buttons_state = (buttons_state & SPICE_MOUSE_BUTTON_MASK_LEFT) |
        ((buttons_state & SPICE_MOUSE_BUTTON_MASK_MIDDLE) << 1) |
        ((buttons_state & SPICE_MOUSE_BUTTON_MASK_RIGHT) >> 1) |
        (buttons_state & ~(SPICE_MOUSE_BUTTON_MASK_LEFT | SPICE_MOUSE_BUTTON_MASK_MIDDLE
                          |SPICE_MOUSE_BUTTON_MASK_RIGHT));
    return buttons_state | (wheel > 0 ? (1<<4) : 0)
                         | (wheel < 0 ? (1<<3) : 0);
}


void session_handle_mouse_wheel(session_t *session, int wheel_motion, uint32_t buttons_state)
{
    g_debug("mouse wheel: motion %d, buttons 0x%x", wheel_motion, buttons_state);

    session_handle_button_change(session, convert_spice_buttons(wheel_motion, buttons_state));
}

void session_handle_mouse_buttons(session_t *session, uint32_t buttons_state)
{
    g_debug("mouse button: buttons 0x%x", buttons_state);
    session_handle_button_change(session, convert_spice_buttons(0, buttons_state));
}

int session_start(session_t *s)
{
    int rc = 0;

    s->spice.session = s;
    s->display.session = s;
    s->scanner.session = s;

    rc = scanner_create(&s->scanner);
    if (rc)
        goto end;

    rc = display_start_event_thread(&s->display);
    if (rc)
        return rc;

    s->running = 1;

end:
    if (rc)
        session_end(s);
    return rc;
}

void session_end(session_t *s)
{
    s->running = 0;

    scanner_destroy(&s->scanner);

    display_destroy_fullscreen(&s->display);
}

int session_create(session_t *s)
{
    s->cursor_queue = g_async_queue_new_full(free_cursor_queue_item);
    s->draw_queue = g_async_queue_new_full(free_draw_queue_item);

    return 0;
}

void session_destroy(session_t *s)
{
    if (s->cursor_queue)
        g_async_queue_unref(s->cursor_queue);
    if (s->draw_queue)
        g_async_queue_unref(s->draw_queue);
    s->cursor_queue = NULL;
    s->draw_queue = NULL;
}

int session_alive(session_t *s)
{
    return s->running;
}
