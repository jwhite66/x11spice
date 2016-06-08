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

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <pixman.h>
#include <errno.h>

#include "x11spice.h"
#include "options.h"
#include "display.h"
#include "session.h"
#include "scan.h"


static xcb_screen_t *screen_of_display (xcb_connection_t *c, int screen)
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
          fmt.rem;
          xcb_format_next(&fmt))
        if (fmt.data->depth == d->screen->root_depth)
            return fmt.data->bits_per_pixel;

     return 0;
}

static void * handle_xevents(void *opaque)
{
    display_t *display = (display_t *) opaque;
    xcb_generic_event_t *ev;
    int i, n;
    pixman_box16_t *p;
    pixman_region16_t damage_region;

    pixman_region_init(&damage_region);

    while ((ev = xcb_wait_for_event(display->c)))
    {
        xcb_damage_notify_event_t *dev;

        if (ev->response_type != display->damage_ext->first_event + XCB_DAMAGE_NOTIFY)
        {
            g_debug("Unexpected X event %d", ev->response_type);
            continue;;
        }
        dev = (xcb_damage_notify_event_t *) ev;

        g_debug("Damage Notify [seq %d|level %d|more %d|area (%dx%d)@%dx%d|geo (%dx%d)@%dx%d",
            dev->sequence, dev->level, dev->level & 0x80,
            dev->area.width, dev->area.height, dev->area.x, dev->area.y,
            dev->geometry.width, dev->geometry.height, dev->geometry.x, dev->geometry.y);

        pixman_region_union_rect(&damage_region, &damage_region,
            dev->area.x, dev->area.y, dev->area.width, dev->area.height);

        /* The MORE flag is 0x80 on the level field; the proto documentation
           is wrong on this point.  Check the xorg server code to see */
        if (dev->level & 0x80)
            continue;

        xcb_damage_subtract(display->c, display->damage,
            XCB_XFIXES_REGION_NONE, XCB_XFIXES_REGION_NONE);

        p = pixman_region_rectangles(&damage_region, &n);

        for (i = 0; i < n; i++)
            scanner_push(&display->session->scanner, DAMAGE_SCAN_REPORT,
                    p[i].x1, p[i].y1, p[i].x2 - p[i].x1, p[i].y2 - p[i].y1);

        pixman_region_clear(&damage_region);
    }

    pixman_region_clear(&damage_region);

    return NULL;
}



int display_open(display_t *d, options_t *options)
{
    int scr;
    xcb_damage_query_version_cookie_t dcookie;

    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    d->c = xcb_connect(options->display, &scr);
    if (! d->c)
    {
        fprintf(stderr, "Error:  could not open display %s\n", options->display ? options->display : "");
        return X11SPICE_ERR_NODISPLAY;
    }

    d->screen = screen_of_display(d->c, scr);
    if (!d->screen)
    {
        fprintf(stderr, "Error:  could not get screen for display %s\n", options->display ? options->display : "");
        return X11SPICE_ERR_NODISPLAY;
    }

    d->damage_ext = xcb_get_extension_data(d->c, &xcb_damage_id);
    if (! d->damage_ext)
    {
        fprintf(stderr, "Error:  XDAMAGE not found on display %s\n", options->display ? options->display : "");
        return X11SPICE_ERR_NODAMAGE;
    }

    dcookie = xcb_damage_query_version(d->c, XCB_DAMAGE_MAJOR_VERSION, XCB_DAMAGE_MINOR_VERSION);
    xcb_damage_query_version_reply(d->c, dcookie, &error);
    if (error)
    {
        fprintf(stderr, "Error:  Could not query damage; type %d; code %d; major %d; minor %d\n",
            error->response_type, error->error_code, error->major_code, error->minor_code);
        return X11SPICE_ERR_NODAMAGE;
    }

    d->damage = xcb_generate_id(d->c);
    cookie = xcb_damage_create_checked(d->c, d->damage, d->screen->root, XCB_DAMAGE_REPORT_LEVEL_RAW_RECTANGLES);
    error = xcb_request_check(d->c, cookie);
    if (error)
    {
        fprintf(stderr, "Error:  Could not create damage; type %d; code %d; major %d; minor %d\n",
            error->response_type, error->error_code, error->major_code, error->minor_code);
        return X11SPICE_ERR_NODAMAGE;
    }

    d->shm_ext = xcb_get_extension_data(d->c, &xcb_shm_id);
    if (! d->shm_ext)
    {
        fprintf(stderr, "Error:  XSHM not found on display %s\n", options->display ? options->display : "");
        return X11SPICE_ERR_NOSHM;
    }

    g_message("Display %s opened", options->display ? options->display : "");

    return 0;
}

shm_image_t * create_shm_image(display_t *d, int w, int h)
{
    shm_image_t *shmi;
    int imgsize;
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    shmi = calloc(1, sizeof(*shmi));
    if (! shmi)
        return shmi;

    shmi->w = w ? w : d->screen->width_in_pixels;
    shmi->h = h ? h : d->screen->height_in_pixels;

    shmi->bytes_per_line = (bits_per_pixel(d) / 8)  * shmi->w;
    imgsize = shmi->bytes_per_line * shmi->h;

    shmi->shmid = shmget(IPC_PRIVATE, imgsize, IPC_CREAT | 0700);
    if (shmi->shmid != -1)
        shmi->shmaddr = shmat(shmi->shmid, 0, 0);
    if (shmi->shmid == -1 || shmi->shmaddr == (void *) -1)
    {
        g_error("Cannot get shared memory of size %d; errno %d", imgsize, errno);
        free(shmi);
        return NULL;
    }
    /* We tell shmctl to detach now; that prevents us from holding this
       shared memory segment forever in case of abnormal process exit. */
    shmctl(shmi->shmid, IPC_RMID, NULL);

    shmi->shmseg = xcb_generate_id(d->c);
    cookie = xcb_shm_attach_checked(d->c, shmi->shmseg, shmi->shmid, 0);
    error = xcb_request_check(d->c, cookie);
    if (error)
    {
        g_error("Could not attach; type %d; code %d; major %d; minor %d\n",
            error->response_type, error->error_code, error->major_code, error->minor_code);
        return NULL;
    }

    shmi->pid = xcb_generate_id(d->c);
    cookie = xcb_shm_create_pixmap_checked(d->c, shmi->pid, d->screen->root,
            shmi->w, shmi->h, d->screen->root_depth, shmi->shmseg, 0);
    error = xcb_request_check(d->c, cookie);
    if (error)
    {
        g_error("Could not create pixmap; type %d; code %d; major %d; minor %d\n",
            error->response_type, error->error_code, error->major_code, error->minor_code);
        return NULL;
    }

    return shmi;
}

int read_shm_image(display_t *d, shm_image_t *shmi, int x, int y)
{
    xcb_shm_get_image_cookie_t cookie;
    xcb_generic_error_t *e;

    cookie = xcb_shm_get_image(d->c, d->screen->root, x, y, shmi->w, shmi->h,
                ~0, XCB_IMAGE_FORMAT_Z_PIXMAP, shmi->shmseg, 0);

    xcb_shm_get_image_reply(d->c, cookie, &e);
    if (e)
    {
        g_error("xcb_shm_get_image from %dx%d into size %dx%d failed", x, y, shmi->w, shmi->h);
        return X11SPICE_ERR_NOSHM;
    }

    return 0;
}

void destroy_shm_image(display_t *d, shm_image_t *shmi)
{
    xcb_free_pixmap(d->c, shmi->pid);
    xcb_shm_detach(d->c, shmi->shmseg);
    shmdt(shmi->shmaddr);
    shmctl(shmi->shmid, IPC_RMID, NULL);
    if (shmi->drawable_ptr)
        free(shmi->drawable_ptr);
}

// FIXME - is this necessary?  And/or can we modify our pushing
//         to spice to use it?
int display_create_fullscreen(display_t *d)
{
    d->fullscreen = create_shm_image(d, 0, 0);
    if (!d->fullscreen)
        return X11SPICE_ERR_NOSHM;

    return 0;
}

void display_destroy_fullscreen(display_t *d)
{
    if (d->fullscreen)
    {
        destroy_shm_image(d, d->fullscreen);
        d->fullscreen = NULL;
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
    display_destroy_fullscreen(d);
    xcb_disconnect(d->c);
}

