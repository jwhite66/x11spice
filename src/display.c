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
#include <glib.h>

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

    d->xdamage = XDamageCreate(d->xdisplay, DefaultRootWindow(d->xdisplay), XDamageReportRawRectangles);

    g_info("Display %s opened", options->display ? options->display : "");

    return 0;
}

void display_close(display_t *d)
{
    XDamageDestroy(d->xdisplay, d->xdamage);
    XCloseDisplay(d->xdisplay);
}

