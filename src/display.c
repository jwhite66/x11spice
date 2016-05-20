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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>


#include "x11spice.h"
#include "options.h"
#include "display.h"


int display_open(display_t *d, options_t *options)
{
    d->xdisplay = XOpenDisplay(options->display);
    // FIXME - do we care? - g_x_error_handler = XSetErrorHandler(handle_xerrors);
    if (! d->xdisplay)
    {
        fprintf(stderr, "Error:  could not open display %s\n", options->display ? options->display : "");
        return X11SPICE_ERR_NODISPLAY;
    }

    if (! XDamageQueryExtension(d->xdisplay, &d->xd_event_base, &d->xd_error_base))
    {
        fprintf(stderr, "Error:  XDAMAGE not found on display %s\n", options->display ? options->display : "");
        return X11SPICE_ERR_NODAMAGE;
    }

    if (! XShmQueryExtension(d->xdisplay))
    {
        fprintf(stderr, "Error:  XSHM not found on display %s\n", options->display ? options->display : "");
        return X11SPICE_ERR_NOSHM;
    }

    d->xdamage = XDamageCreate(d->xdisplay, DefaultRootWindow(d->xdisplay), XDamageReportRawRectangles);

    g_info("Display %s opened", options->display ? options->display : "");

    return 0;
}

shm_image_t * create_shm_image(display_t *d, int w, int h)
{
    shm_image_t *shmi;
    int scr = DefaultScreen(d->xdisplay);
    int imgsize;

    shmi = calloc(1, sizeof(*shmi));
    if (! shmi)
        return shmi;

    shmi->img = XShmCreateImage(d->xdisplay,
        DefaultVisual(d->xdisplay, scr),
        DefaultDepth(d->xdisplay, scr),
        ZPixmap /* FIXME - format we want? */, NULL /* data? */,
        &shmi->info,
        w ? w : DisplayWidth(d->xdisplay, scr),
        h ? h : DisplayHeight(d->xdisplay, scr));
    if (! shmi->img)
    {
        free(shmi);
        return NULL;
    }

    imgsize = shmi->img->bytes_per_line * shmi->img->height;

    shmi->info.shmid = shmget(IPC_PRIVATE, imgsize, IPC_CREAT | 0700);
    if (shmi->info.shmid == -1)
    {
        g_error("Cannot get shared memory of size %d", imgsize);
        XDestroyImage(shmi->img);
        free(shmi);
        return NULL;
    }

    shmi->info.shmaddr = shmi->img->data = shmat(shmi->info.shmid, 0, 0);
    shmi->info.readOnly = False;

    if (!XShmAttach(d->xdisplay, &shmi->info))
    {
        g_error("Failed to attach shared memory");
        shmdt(shmi->info.shmaddr);
        shmctl(shmi->info.shmid, IPC_RMID, NULL);
        XDestroyImage(shmi->img);
        free(shmi);
        return NULL;
    }

    return shmi;
}

int read_shm_image(display_t *d, shm_image_t *shmi, int x, int y)
{
    if (!XShmGetImage(d->xdisplay, DefaultRootWindow(d->xdisplay), shmi->img,
            x, y, AllPlanes))
    {
        g_error("XShmGetImage from %dx%d into size %dx%d failed",
            x, y, shmi->img->width, shmi->img->height);
        return X11SPICE_ERR_NOSHM;
    }
    return 0;
}

void destroy_shm_image(display_t *d, shm_image_t *shmi)
{
    XShmDetach(d->xdisplay, &shmi->info);
    shmdt(shmi->info.shmaddr);
    shmctl(shmi->info.shmid, IPC_RMID, NULL);
    XDestroyImage(shmi->img);
    if (shmi->drawable_ptr)
        free(shmi->drawable_ptr);
}

void display_close(display_t *d)
{
    XDamageDestroy(d->xdisplay, d->xdamage);
    XCloseDisplay(d->xdisplay);
    if (d->fullscreen)
    {
        destroy_shm_image(d, d->fullscreen);
        d->fullscreen = NULL;
    }
}

