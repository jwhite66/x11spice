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

static void session_handle_xevent(int fd, int event, void *opaque)
{
    session_t *s = (session_t *) opaque;
    XEvent xev;
    int rc;
    XDamageNotifyEvent *dev = (XDamageNotifyEvent *) &xev;;

    rc = XNextEvent(s->display.xdisplay, &xev);
    if (rc == 0)
    {
        shm_image_t shmi;

        if (xev.type != s->display.xd_event_base + XDamageNotify)
        {
            g_debug("Unexpected X event %d", xev.type);
            return;
        }

        g_debug("XDamageNotify [ser %ld|send_event %d|level %d|more %d|area (%dx%d)@%dx%d|geo (%dx%d)@%dx%d",
            dev->serial, dev->send_event, dev->level, dev->more,
            dev->area.width, dev->area.height, dev->area.x, dev->area.y,
            dev->geometry.width, dev->geometry.height, dev->geometry.x, dev->geometry.y);
        if (create_shm_image(&s->display, &shmi, dev->area.width, dev->area.height) == 0)
        {
            if (read_shm_image(&s->display, &shmi, dev->area.x, dev->area.y) == 0)
            {
                //save_ximage_pnm(shmi.img);
                spice_qxl_wakeup(&s->spice.display_sin);
            }
            destroy_shm_image(&s->display, &shmi);
        }

    }
}

static int create_primary(session_t *s)
{
    int rc;
    int scr = DefaultScreen(s->display.xdisplay);
    QXLDevSurfaceCreate surface;

    rc = create_shm_image(&s->display, &s->display.fullscreen, 0, 0);
    if (rc)
        return rc;

    memset(&surface, 0, sizeof(surface));
    surface.height     = DisplayHeight(s->display.xdisplay, scr);
    surface.width      = DisplayWidth(s->display.xdisplay, scr);
    surface.stride     = -s->display.fullscreen.img->bytes_per_line;
    surface.type       = QXL_SURF_TYPE_PRIMARY;
    surface.flags      = 0;
    surface.group_id   = 0;
    surface.mouse_mode = TRUE;

    // Position appears to be completely unused
    surface.position   = 0;

    // FIXME - compute this dynamically?
    surface.format     = SPICE_SURFACE_FMT_32_xRGB;
    surface.mem        = (QXLPHYSICAL) s->display.fullscreen.img->data;

    spice_qxl_create_primary_surface(&s->spice.display_sin, 0, &surface);

    return 0;
}

int session_start(session_t *s)
{
    int rc;
    s->xwatch = s->spice.core->watch_add(ConnectionNumber(s->display.xdisplay),
                    SPICE_WATCH_EVENT_READ, session_handle_xevent, s);
    if (! s->xwatch)
        return X11SPICE_ERR_NOWATCH;

    /* In order for the watch to function,
        we seem to have to request at least one event */
    // FIXME - not sure I know why...
    XPending(s->display.xdisplay);

    rc = create_primary(s);
    if (rc)
        return rc;

    return 0;
}

void session_end(session_t *s)
{
    s->spice.core->watch_remove(s->xwatch);
}
