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

#include "options.h"
#include "display.h"


display_t *display_open(options_t *options)
{
    display_t *d = malloc(sizeof(*d));
    if (! d)
        return NULL;

    d->xdisplay = XOpenDisplay(options->display);
    // FIXME g_x_error_handler = XSetErrorHandler(handle_xerrors);
    if (! d->xdisplay)
    {
        fprintf(stderr, "Error:  could not open display %s\n", options->display ? options->display : "");
        return NULL;
    }

    return d;
}

void display_close(display_t *display)
{
    if (display->xdisplay)
    {
        XCloseDisplay(display->xdisplay);
        display->xdisplay = NULL;
    }
}

