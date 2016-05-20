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

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XShm.h>

/*----------------------------------------------------------------------------
**  Structure definitions
**--------------------------------------------------------------------------*/
typedef struct
{
    XImage *img;
    XShmSegmentInfo info;
    void *drawable_ptr;
}shm_image_t;

typedef struct
{
    Display *xdisplay;
    Damage xdamage;
    int xd_event_base;
    int xd_error_base;
    shm_image_t *fullscreen;
} display_t;


/*----------------------------------------------------------------------------
**  Prototypes
**--------------------------------------------------------------------------*/
int display_open(display_t *display, options_t *options);
void display_close(display_t *display);
shm_image_t * create_shm_image(display_t *d, int w, int h);
int read_shm_image(display_t *d, shm_image_t *shmi, int x, int y);
void destroy_shm_image(display_t *d, shm_image_t *shmi);

#endif
