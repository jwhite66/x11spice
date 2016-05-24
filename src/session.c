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
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#include "x11spice.h"
#include "session.h"


static void save_ximage_pnm(XImage *img)
{
    int x,y;
    unsigned long pixel;
    static int count = 0;
    char fname[200];
    FILE *fp;
    sprintf(fname, "ximage%04d.ppm", count++);
    fp = fopen(fname, "w");

    fprintf(fp,"P3\n%d %d\n255\n",img->width, img->height);
    for (y=0; y<img->height; y++)
    {
        for (x=0; x<img->width; x++)
        {
            pixel=XGetPixel(img,x,y);
            fprintf(fp,"%ld %ld %ld\n",
                pixel>>16,(pixel&0x00ff00)>>8,pixel&0x0000ff);
        }
    }
    fclose(fp);
}

static QXLDrawable *shm_image_to_drawable(shm_image_t *shmi, int x, int y)
{
    QXLDrawable *drawable;
    QXLImage *qxl_image;
    int i;

    drawable = calloc(1, sizeof(*drawable) + sizeof(*qxl_image));
    if (! drawable)
        return NULL;
    qxl_image = (QXLImage *) (drawable + 1);

    drawable->release_info.id = (uint64_t) shmi;
    shmi->drawable_ptr = drawable;

    drawable->surface_id = 0;
    drawable->type = QXL_DRAW_COPY;;
    drawable->effect = QXL_EFFECT_OPAQUE;
    drawable->clip.type = SPICE_CLIP_TYPE_NONE;
    drawable->bbox.left = x;
    drawable->bbox.top = y;
    drawable->bbox.right = x + shmi->img->width;
    drawable->bbox.bottom = y + shmi->img->height;

    /*
     * surfaces_dest[i] should apparently be filled out with the
     * surfaces that we depend on, and surface_rects should be
     * filled with the rectangles of those surfaces that we
     * are going to use.
     *  FIXME - explore this instead of blindly copying...
     */
    for (i = 0; i < 3; ++i)
	drawable->surfaces_dest[i] = -1;

    drawable->u.copy.src_area = drawable->bbox;
    drawable->u.copy.rop_descriptor = SPICE_ROPD_OP_PUT;

    drawable->u.copy.src_bitmap = (QXLPHYSICAL) qxl_image;

    qxl_image->descriptor.id = 0;
    qxl_image->descriptor.type = SPICE_IMAGE_TYPE_BITMAP;

    qxl_image->descriptor.flags = 0;
    qxl_image->descriptor.width = shmi->img->width;
    qxl_image->descriptor.height = shmi->img->height;

    // FIXME - be a bit more dynamic...
    qxl_image->bitmap.format = SPICE_BITMAP_FMT_RGBA;
    qxl_image->bitmap.flags = SPICE_BITMAP_FLAGS_TOP_DOWN | QXL_BITMAP_DIRECT;
    qxl_image->bitmap.x = shmi->img->width;
    qxl_image->bitmap.y = shmi->img->height;
    qxl_image->bitmap.stride = shmi->img->bytes_per_line;
    qxl_image->bitmap.palette = 0;
    qxl_image->bitmap.data = (QXLPHYSICAL) shmi->img->data;

    // FIXME - cache images at all?

    return drawable;
}

static void session_handle_xevent(int fd, int event, void *opaque)
{
    session_t *s = (session_t *) opaque;
    XEvent xev;
    int rc;
    XDamageNotifyEvent *dev = (XDamageNotifyEvent *) &xev;;
    shm_image_t *shmi = NULL;

    rc = XNextEvent(s->display.xdisplay, &xev);
    if (rc)
        return;

    if (xev.type != s->display.xd_event_base + XDamageNotify)
    {
        g_debug("Unexpected X event %d", xev.type);
        return;
    }

    g_debug("XDamageNotify [ser %ld|send_event %d|level %d|more %d|area (%dx%d)@%dx%d|geo (%dx%d)@%dx%d",
        dev->serial, dev->send_event, dev->level, dev->more,
        dev->area.width, dev->area.height, dev->area.x, dev->area.y,
        dev->geometry.width, dev->geometry.height, dev->geometry.x, dev->geometry.y);
// FIXME - HACK!
    dev->area.width = s->display.fullscreen->img->width;
    dev->area.height = s->display.fullscreen->img->height;
    dev->area.x = dev->area.y = 0;
    shmi = create_shm_image(&s->display, dev->area.width, dev->area.height);
    if (!shmi)
    {
        g_debug("Unexpected failure to create_shm_image of area %dx%d", dev->area.width, dev->area.width);
        return;
    }

    if (read_shm_image(&s->display, shmi, dev->area.x, dev->area.y) == 0)
    {
        //save_ximage_pnm(shmi.img);
        QXLDrawable *drawable = shm_image_to_drawable(shmi, dev->area.x, dev->area.y);
        if (drawable)
        {
            g_async_queue_push(s->draw_queue, drawable);
            spice_qxl_wakeup(&s->spice.display_sin);
            // FIXME - Note that shmi is not cleaned up at this point
            return;
        }
        else
            g_debug("Unexpected failure to create drawable");
    }
    else
        g_debug("Unexpected failure to read shm of area %dx%d", dev->area.width, dev->area.width);


    if (shmi)
        destroy_shm_image(&s->display, shmi);
}


// FIXME - this is not really satisfying.  It'd be
//         nicer over in spice.c.  But spice.c needs
//         access to the display info if it can.
static int create_primary(session_t *s)
{
    int scr = DefaultScreen(s->display.xdisplay);
    QXLDevSurfaceCreate surface;

    s->display.fullscreen = create_shm_image(&s->display, 0, 0);
    if (!s->display.fullscreen)
        return X11SPICE_ERR_NOSHM;

    memset(&surface, 0, sizeof(surface));
    surface.height     = DisplayHeight(s->display.xdisplay, scr);
    surface.width      = DisplayWidth(s->display.xdisplay, scr);
    // FIXME - negative stride?
    surface.stride     = s->display.fullscreen->img->bytes_per_line;
    surface.type       = QXL_SURF_TYPE_PRIMARY;
    surface.flags      = 0;
    surface.group_id   = 0;
    surface.mouse_mode = TRUE;

    // Position appears to be completely unused
    surface.position   = 0;

    // FIXME - compute this dynamically?
    surface.format     = SPICE_SURFACE_FMT_32_xRGB;
    surface.mem        = (QXLPHYSICAL) s->display.fullscreen->img->data;

    spice_qxl_create_primary_surface(&s->spice.display_sin, 0, &surface);

    return 0;
}

void free_cursor_queue_item(gpointer data)
{
    // FIXME
}

void free_draw_queue_item(gpointer data)
{
    // FIXME
}

void *session_pop_draw(void *session_ptr)
{
    session_t *s = (session_t *) session_ptr;

    return g_async_queue_try_pop(s->draw_queue);
}

int session_draw_waiting(void *session_ptr)
{
    session_t *s = (session_t *) session_ptr;

    return g_async_queue_length(s->draw_queue);
}

void session_handle_key(void *session_ptr, uint8_t keycode, int is_press)
{
    int rc;
    session_t *s = (session_t *) session_ptr;
    rc = XTestFakeKeyEvent(s->display.xdisplay, keycode, is_press ? True : False, CurrentTime);
    g_debug("key 0x%x, press %d, rc %d (t %d, f %d)", keycode, is_press, rc, True, False);
    XFlush(s->display.xdisplay);
}

void session_handle_mouse_position(void *session_ptr, int x, int y, uint32_t buttons_state)
{
    session_t *s = (session_t *) session_ptr;
    int scr = DefaultScreen(s->display.xdisplay);
    XFlush(s->display.xdisplay);
    g_debug("mouse position: x %d, y %d, buttons 0x%x", x, y, buttons_state);
    XTestFakeMotionEvent(s->display.xdisplay, scr, x, y, CurrentTime);
    XFlush(s->display.xdisplay);
}

#define BUTTONS 5
static void session_handle_button_change(session_t *s, uint32_t buttons_state)
{
    int i;
    for (i = 0; i < BUTTONS; i++) {
        if ((buttons_state ^ s->spice.buttons_state) & (1 << i)) {
            int action = (buttons_state & (1 << i));
            XTestFakeButtonEvent(s->display.xdisplay, i + 1, action, CurrentTime);
        }
    }
    s->spice.buttons_state = buttons_state;
    XFlush(s->display.xdisplay);
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


void session_handle_mouse_wheel(void *session_ptr, int wheel_motion, uint32_t buttons_state)
{
    session_t *s = (session_t *) session_ptr;
    g_debug("mouse wheel: motion %d, buttons 0x%x", wheel_motion, buttons_state);

    session_handle_button_change(s, convert_spice_buttons(wheel_motion, buttons_state));
}

void session_handle_mouse_buttons(void *session_ptr, uint32_t buttons_state)
{
    session_t *s = (session_t *) session_ptr;
    g_debug("mouse button: buttons 0x%x", buttons_state);
    session_handle_button_change(s, convert_spice_buttons(0, buttons_state));
}

int session_start(session_t *s)
{
    int rc = 0;

    s->spice.session_ptr = s;
    s->xwatch = s->spice.core->watch_add(ConnectionNumber(s->display.xdisplay),
                    SPICE_WATCH_EVENT_READ, session_handle_xevent, s);
    if (! s->xwatch)
        return(X11SPICE_ERR_NOWATCH);

    s->cursor_queue = g_async_queue_new_full(free_cursor_queue_item);
    s->draw_queue = g_async_queue_new_full(free_draw_queue_item);

    /* In order for the watch to function,
        we seem to have to request at least one event */
    // FIXME - not sure I know why...
    XPending(s->display.xdisplay);

    rc = create_primary(s);
    if (rc)
        session_end(s);

    return rc;
}

void session_end(session_t *s)
{
    s->spice.core->watch_remove(s->xwatch);
    if (s->cursor_queue)
        g_async_queue_unref(s->cursor_queue);
    if (s->draw_queue)
        g_async_queue_unref(s->draw_queue);
}
