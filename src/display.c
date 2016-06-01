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

#include "x11spice.h"
#include "options.h"
#include "display.h"


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
        g_error("Cannot get shared memory of size %d", imgsize);
        free(shmi);
        return NULL;
    }

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

void display_close(display_t *d)
{
    xcb_damage_destroy(d->c, d->damage);
    if (d->fullscreen)
    {
        destroy_shm_image(d, d->fullscreen);
        d->fullscreen = NULL;
    }
    xcb_disconnect(d->c);
}

