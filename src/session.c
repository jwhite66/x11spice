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
#include <stdlib.h>

#include <xcb/xcb.h>
#include <xcb/xtest.h>
#include <xcb/xcb_aux.h>
#include <xcb/xkb.h>

#include "x11spice.h"
#include "session.h"
#include "scan.h"


void free_cursor_queue_item(gpointer data)
{
    QXLCursorCmd  *ccmd = (QXLCursorCmd *) data;
    spice_free_release((spice_release_t *) ccmd->release_info.id);
}

void free_draw_queue_item(gpointer data)
{
    QXLDrawable *drawable = (QXLDrawable *) data;
    spice_free_release((spice_release_t *) drawable->release_info.id);
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

void *session_pop_cursor(session_t *session)
{
    if (! session || ! session->running)
        return NULL;

    return g_async_queue_try_pop(session->cursor_queue);
}

int session_cursor_waiting(session_t *session)
{
    if (! session || ! session->running)
        return 0;

    return g_async_queue_length(session->cursor_queue);
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

int session_push_cursor_image(session_t *s,
        int x, int y, int w, int h, int xhot, int yhot,
        int imglen, uint8_t *imgdata)
{
    QXLCursorCmd  *ccmd;
    QXLCursor     *cursor;

    ccmd = calloc(1, sizeof(*ccmd) + sizeof(*cursor) + imglen);
    if (! ccmd)
        return X11SPICE_ERR_MALLOC;;

    cursor = (QXLCursor *) (ccmd + 1);

    cursor->header.unique = 0;
    cursor->header.type = SPICE_CURSOR_TYPE_ALPHA;
    cursor->header.width = w;
    cursor->header.height = h;

    cursor->header.hot_spot_x = xhot;
    cursor->header.hot_spot_y = yhot;

    cursor->data_size = imglen;

    cursor->chunk.next_chunk = 0;
    cursor->chunk.prev_chunk = 0;
    cursor->chunk.data_size = imglen;

    memcpy(cursor->chunk.data, imgdata, imglen);

    ccmd->type = QXL_CURSOR_SET;
    ccmd->u.set.position.x = x + xhot;
    ccmd->u.set.position.y = y + yhot;
    ccmd->u.set.shape = (QXLPHYSICAL) cursor;
    ccmd->u.set.visible = TRUE;

    ccmd->release_info.id = (uint64_t) spice_create_release(&s->spice, RELEASE_MEMORY, ccmd);

    g_async_queue_push(s->cursor_queue, ccmd);

    return 0;
}

int session_get_one_led(session_t *session, const char *name)
{
    int ret;
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom_reply;
    xcb_xkb_get_named_indicator_cookie_t indicator_cookie;
    xcb_xkb_get_named_indicator_reply_t *indicator_reply;
    xcb_generic_error_t *error;

    atom_cookie = xcb_intern_atom(session->display.c, 0, strlen(name), name);
    atom_reply = xcb_intern_atom_reply(session->display.c, atom_cookie, &error);
    if (error)
    {
        g_warning("Could not get atom; type %d; code %d; major %d; minor %d",
            error->response_type, error->error_code, error->major_code, error->minor_code);
        return 0;
    }

    indicator_cookie = xcb_xkb_get_named_indicator(session->display.c,
                         XCB_XKB_ID_USE_CORE_KBD,
                         XCB_XKB_LED_CLASS_DFLT_XI_CLASS,
                         XCB_XKB_ID_DFLT_XI_ID,
                         atom_reply->atom);
    free(atom_reply);

    indicator_reply = xcb_xkb_get_named_indicator_reply(session->display.c, indicator_cookie, &error);
    if (error)
    {
        g_warning("Could not get indicator; type %d; code %d; major %d; minor %d",
            error->response_type, error->error_code, error->major_code, error->minor_code);
        return 0;
    }

    ret = indicator_reply->on;
    free(indicator_reply);
    return ret;
}

