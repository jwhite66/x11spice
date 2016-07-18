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

/*----------------------------------------------------------------------------
**  display.c
**      This file provides functions to interact with the X11 display.
**  The concept is that the bulk of the connection to the X server is done
**  here, using xcb.
**--------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xkb.h>
#include <pixman.h>
#include <errno.h>

#include "x11spice.h"
#include "options.h"
#include "display.h"
#include "session.h"
#include "scan.h"


static xcb_screen_t *screen_of_display(xcb_connection_t *c, int screen)
{
    xcb_screen_iterator_t iter;

    iter = xcb_setup_roots_iterator(xcb_get_setup(c));
    for (; iter.rem; --screen, xcb_screen_next(&iter))
        if (screen == 0)
            return iter.data;

    return NULL;
}

static int bits_per_pixel(display_t *d)
{
    xcb_format_iterator_t fmt;

    for (fmt = xcb_setup_pixmap_formats_iterator(xcb_get_setup(d->c));
         fmt.rem; xcb_format_next(&fmt))
        if (fmt.data->depth == d->depth)
            return fmt.data->bits_per_pixel;

    return 0;
}


static void handle_cursor_notify(display_t *display, xcb_xfixes_cursor_notify_event_t *cev)
{
    xcb_xfixes_get_cursor_image_cookie_t icookie;
    xcb_xfixes_get_cursor_image_reply_t *ir;
    xcb_generic_error_t *error;
    int imglen;
    uint32_t *imgdata;

    g_debug("Cursor Notify [seq %d|subtype %d|serial %u]",
            cev->sequence, cev->subtype, cev->cursor_serial);

    icookie = xcb_xfixes_get_cursor_image(display->c);

    ir = xcb_xfixes_get_cursor_image_reply(display->c, icookie, &error);
    if (error) {
        g_warning("Could not get cursor_image_reply; type %d; code %d; major %d; minor %d\n",
                error->response_type, error->error_code, error->major_code, error->minor_code);
        return;
    }

    imglen = xcb_xfixes_get_cursor_image_cursor_image_length(ir);
    imgdata = xcb_xfixes_get_cursor_image_cursor_image(ir);

    session_push_cursor_image(display->session,
                              ir->x, ir->y, ir->width, ir->height, ir->xhot, ir->yhot,
                              imglen * sizeof(*imgdata), (uint8_t *) imgdata);

    free(ir);
}

static void handle_damage_notify(display_t *display, xcb_damage_notify_event_t *dev,
                                 pixman_region16_t * damage_region)
{
    int i, n;
    pixman_box16_t *p;

    g_debug("Damage Notify [seq %d|level %d|more %d|area (%dx%d)@%dx%d|geo (%dx%d)@%dx%d",
            dev->sequence, dev->level, dev->level & 0x80,
            dev->area.width, dev->area.height, dev->area.x, dev->area.y,
            dev->geometry.width, dev->geometry.height, dev->geometry.x, dev->geometry.y);

    pixman_region_union_rect(damage_region, damage_region,
                             dev->area.x, dev->area.y, dev->area.width, dev->area.height);

    /* The MORE flag is 0x80 on the level field; the proto documentation
       is wrong on this point.  Check the xorg server code to see */
    if (dev->level & 0x80)
        return;

    xcb_damage_subtract(display->c, display->damage,
                        XCB_XFIXES_REGION_NONE, XCB_XFIXES_REGION_NONE);

    p = pixman_region_rectangles(damage_region, &n);

    for (i = 0; i < n; i++)
        scanner_push(&display->session->scanner, DAMAGE_SCAN_REPORT,
                     p[i].x1, p[i].y1, p[i].x2 - p[i].x1, p[i].y2 - p[i].y1);

    pixman_region_clear(damage_region);
}

static void handle_configure_notify(display_t *display, xcb_configure_notify_event_t *cev)
{
    g_debug
        ("%s:[event %u|window %u|above_sibling %u|x %d|y %d|width %d|height %d|border_width %d|override_redirect %d]",
         __func__, cev->event, cev->window, cev->above_sibling, cev->x, cev->y, cev->width,
         cev->height, cev->border_width, cev->override_redirect);
    if (cev->window != display->root) {
        g_debug("not main window; skipping.");
        return;
    }

    display->width = cev->width;
    display->height = cev->height;
    session_handle_resize(display->session);
}

static void *handle_xevents(void *opaque)
{
    display_t *display = (display_t *) opaque;
    xcb_generic_event_t *ev = NULL;
    pixman_region16_t damage_region;

    pixman_region_init(&damage_region);

    // FIXME - we do not have a good way to cause this thread to exit gracefully
    while ((ev = xcb_wait_for_event(display->c))) {
        if (ev->response_type == display->xfixes_ext->first_event + XCB_XFIXES_CURSOR_NOTIFY)
            handle_cursor_notify(display, (xcb_xfixes_cursor_notify_event_t *) ev);

        else if (ev->response_type == display->damage_ext->first_event + XCB_DAMAGE_NOTIFY)
            handle_damage_notify(display, (xcb_damage_notify_event_t *) ev, &damage_region);

        else if (ev->response_type == XCB_CONFIGURE_NOTIFY)
            handle_configure_notify(display, (xcb_configure_notify_event_t *) ev);

        else
            g_debug("Unexpected X event %d", ev->response_type);

        free(ev);

        if (display->session && !session_alive(display->session))
            break;
    }

    while ((ev = xcb_poll_for_event(display->c)))
        free(ev);

    pixman_region_clear(&damage_region);

    return NULL;
}

static int register_for_events(display_t *d)
{
    uint32_t events = XCB_EVENT_MASK_STRUCTURE_NOTIFY;  // FIXME - do we need this? | XCB_EVENT_MASK_POINTER_MOTION;
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    cookie = xcb_change_window_attributes_checked(d->c, d->root, XCB_CW_EVENT_MASK, &events);
    error = xcb_request_check(d->c, cookie);
    if (error) {
        fprintf(stderr,
                "Error:  Could not register normal events; type %d; code %d; major %d; minor %d\n",
                error->response_type, error->error_code, error->major_code, error->minor_code);
        return X11SPICE_ERR_NOEVENTS;
    }

    return 0;
}


int display_open(display_t *d, session_t *session)
{
    int scr;
    int rc;
    xcb_damage_query_version_cookie_t dcookie;
    xcb_damage_query_version_reply_t *damage_version;
    xcb_xkb_use_extension_cookie_t use_cookie;
    xcb_xkb_use_extension_reply_t *use_reply;

    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;
    xcb_screen_t *screen;

    d->session = session;

    d->c = xcb_connect(session->options.display, &scr);
    if (!d->c || xcb_connection_has_error(d->c)) {
        fprintf(stderr, "Error:  could not open display %s\n",
                session->options.display ? session->options.display : "");
        return X11SPICE_ERR_NODISPLAY;
    }

    screen = screen_of_display(d->c, scr);
    if (!screen) {
        fprintf(stderr, "Error:  could not get screen for display %s\n",
                session->options.display ? session->options.display : "");
        return X11SPICE_ERR_NODISPLAY;
    }
    d->root = screen->root;
    d->width = screen->width_in_pixels;
    d->height = screen->height_in_pixels;
    d->depth = screen->root_depth;

    d->damage_ext = xcb_get_extension_data(d->c, &xcb_damage_id);
    if (!d->damage_ext) {
        fprintf(stderr, "Error:  XDAMAGE not found on display %s\n",
                session->options.display ? session->options.display : "");
        return X11SPICE_ERR_NODAMAGE;
    }

    dcookie = xcb_damage_query_version(d->c, XCB_DAMAGE_MAJOR_VERSION, XCB_DAMAGE_MINOR_VERSION);
    damage_version = xcb_damage_query_version_reply(d->c, dcookie, &error);
    if (error) {
        fprintf(stderr, "Error:  Could not query damage; type %d; code %d; major %d; minor %d\n",
                error->response_type, error->error_code, error->major_code, error->minor_code);
        return X11SPICE_ERR_NODAMAGE;
    }
    free(damage_version);

    d->damage = xcb_generate_id(d->c);
    cookie =
        xcb_damage_create_checked(d->c, d->damage, d->root, XCB_DAMAGE_REPORT_LEVEL_RAW_RECTANGLES);
    error = xcb_request_check(d->c, cookie);
    if (error) {
        fprintf(stderr, "Error:  Could not create damage; type %d; code %d; major %d; minor %d\n",
                error->response_type, error->error_code, error->major_code, error->minor_code);
        return X11SPICE_ERR_NODAMAGE;
    }

    d->shm_ext = xcb_get_extension_data(d->c, &xcb_shm_id);
    if (!d->shm_ext) {
        fprintf(stderr, "Error:  XSHM not found on display %s\n",
                session->options.display ? session->options.display : "");
        return X11SPICE_ERR_NOSHM;
    }

    d->xfixes_ext = xcb_get_extension_data(d->c, &xcb_xfixes_id);
    if (!d->xfixes_ext) {
        fprintf(stderr, "Error:  XFIXES not found on display %s\n",
                session->options.display ? session->options.display : "");
        return X11SPICE_ERR_NOXFIXES;
    }

    xcb_xfixes_query_version(d->c, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);

    cookie =
        xcb_xfixes_select_cursor_input_checked(d->c, d->root,
                                               XCB_XFIXES_CURSOR_NOTIFY_MASK_DISPLAY_CURSOR);
    error = xcb_request_check(d->c, cookie);
    if (error) {
        fprintf(stderr,
                "Error:  Could not select cursor input; type %d; code %d; major %d; minor %d\n",
                error->response_type, error->error_code, error->major_code, error->minor_code);
        return X11SPICE_ERR_NOXFIXES;
    }

    use_cookie = xcb_xkb_use_extension(d->c, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
    use_reply = xcb_xkb_use_extension_reply(d->c, use_cookie, &error);
    if (error) {
        fprintf(stderr, "Error: could not get use reply; type %d; code %d; major %d; minor %d\n",
                error->response_type, error->error_code, error->major_code, error->minor_code);
        return X11SPICE_ERR_NO_XKB;
    }
    free(use_reply);


    rc = register_for_events(d);
    if (rc)
        return rc;

    rc = display_create_screen_images(d);

    g_message("Display %s opened", session->options.display ? session->options.display : "");

    return rc;
}

shm_image_t *create_shm_image(display_t *d, int w, int h)
{
    shm_image_t *shmi;
    int imgsize;
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    shmi = calloc(1, sizeof(*shmi));
    if (!shmi)
        return shmi;

    shmi->w = w ? w : d->width;
    shmi->h = h ? h : d->height;

    shmi->bytes_per_line = (bits_per_pixel(d) / 8) * shmi->w;
    imgsize = shmi->bytes_per_line * shmi->h;

    shmi->shmid = shmget(IPC_PRIVATE, imgsize, IPC_CREAT | 0700);
    if (shmi->shmid != -1)
        shmi->shmaddr = shmat(shmi->shmid, 0, 0);
    if (shmi->shmid == -1 || shmi->shmaddr == (void *) -1) {
        g_warning("Cannot get shared memory of size %d; errno %d", imgsize, errno);
        free(shmi);
        return NULL;
    }
    /* We tell shmctl to detach now; that prevents us from holding this
       shared memory segment forever in case of abnormal process exit. */
    shmctl(shmi->shmid, IPC_RMID, NULL);

    shmi->shmseg = xcb_generate_id(d->c);
    cookie = xcb_shm_attach_checked(d->c, shmi->shmseg, shmi->shmid, 0);
    error = xcb_request_check(d->c, cookie);
    if (error) {
        g_warning("Could not attach; type %d; code %d; major %d; minor %d\n",
                error->response_type, error->error_code, error->major_code, error->minor_code);
        return NULL;
    }

    return shmi;
}

int read_shm_image(display_t *d, shm_image_t *shmi, int x, int y)
{
    xcb_shm_get_image_cookie_t cookie;
    xcb_generic_error_t *e;
    xcb_shm_get_image_reply_t *reply;

    cookie = xcb_shm_get_image(d->c, d->root, x, y, shmi->w, shmi->h,
                               ~0, XCB_IMAGE_FORMAT_Z_PIXMAP, shmi->shmseg, 0);

    reply = xcb_shm_get_image_reply(d->c, cookie, &e);
    if (e) {
        g_warning("xcb_shm_get_image from %dx%d into size %dx%d failed", x, y, shmi->w, shmi->h);
        return -1;
    }
    free(reply);

    return 0;
}

int display_find_changed_tiles(display_t *d, int row, int *tiles, int tiles_across)
{
    int ret;
    int len;
    int i;

    memset(tiles, 0, sizeof(*tiles) * tiles_across);
    ret = read_shm_image(d, d->scanline, 0, row);
    if (ret == 0) {
        uint32_t *old = ((uint32_t *) d->fullscreen->shmaddr) + row * d->fullscreen->w;
        uint32_t *new = ((uint32_t *) d->scanline->shmaddr);
        if (memcmp(old, new, sizeof(*old) * d->scanline->w) == 0)
            return 0;

        len = d->scanline->w / tiles_across;
        for (i = 0; i < tiles_across; i++, old += len, new += len) {
            if (i == tiles_across - 1)
                len = d->scanline->w - (i * len);
            if (memcmp(old, new, sizeof(*old) * len)) {
                ret++;
                tiles[i]++;
            }
        }
    }

#if defined(DEBUG_SCANLINES)
    fprintf(stderr, "%d: ", row);
    for (i = 0; i < tiles_across; i++)
        fprintf(stderr, "%c", tiles[i] ? 'X' : '-');
    fprintf(stderr, "\n");
    fflush(stderr);
#endif

    return ret;
}

void display_copy_image_into_fullscreen(display_t *d, shm_image_t *shmi, int x, int y)
{
    uint32_t *to = ((uint32_t *) d->fullscreen->shmaddr) + (y * d->fullscreen->w) + x;
    uint32_t *from = ((uint32_t *) shmi->shmaddr);
    int i;

    /* Ignore invalid draws.  This can happen if the screen is resized after a scan
       has been qeueued */
    if (x + shmi->w > d->fullscreen->w)
        return;
    if (y + shmi->h > d->fullscreen->h)
        return;

    for (i = 0; i < shmi->h; i++) {
        memcpy(to, from, sizeof(*to) * shmi->w);
        from += shmi->w;
        to += d->fullscreen->w;
    }
}


void destroy_shm_image(display_t *d, shm_image_t *shmi)
{
    xcb_shm_detach(d->c, shmi->shmseg);
    shmdt(shmi->shmaddr);
    shmctl(shmi->shmid, IPC_RMID, NULL);
    if (shmi->drawable_ptr)
        free(shmi->drawable_ptr);
    free(shmi);
}

// FIXME - Can we / should we do pushing to spice using the fullscreen?
int display_create_screen_images(display_t *d)
{
    d->fullscreen = create_shm_image(d, 0, 0);
    if (!d->fullscreen)
        return X11SPICE_ERR_NOSHM;

    d->scanline = create_shm_image(d, 0, 1);
    if (!d->scanline) {
        destroy_shm_image(d, d->fullscreen);
        d->fullscreen = NULL;
        return X11SPICE_ERR_NOSHM;
    }

    return 0;
}

void display_destroy_screen_images(display_t *d)
{
    if (d->fullscreen) {
        destroy_shm_image(d, d->fullscreen);
        d->fullscreen = NULL;
    }

    if (d->scanline) {
        destroy_shm_image(d, d->scanline);
        d->scanline = NULL;
    }
}

int display_start_event_thread(display_t *d)
{
    // FIXME - gthread?
    return pthread_create(&d->event_thread, NULL, handle_xevents, d);
}


void display_close(display_t *d)
{
    xcb_damage_destroy(d->c, d->damage);
    display_destroy_screen_images(d);
    xcb_disconnect(d->c);
}
