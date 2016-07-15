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
#include <string.h>

#include <xcb/xcb.h>


static void lookup_color(xcb_connection_t *c, xcb_screen_t *screen, const char *color,
                         uint32_t *pixel)
{
    xcb_lookup_color_cookie_t cookie;
    xcb_lookup_color_reply_t *r;
    xcb_alloc_color_cookie_t acookie;
    xcb_alloc_color_reply_t *ar;

    cookie = xcb_lookup_color(c, screen->default_colormap, strlen(color), color);
    r = xcb_lookup_color_reply(c, cookie, NULL);

    acookie =
        xcb_alloc_color(c, screen->default_colormap, r->exact_red, r->exact_green, r->exact_blue);
    free(r);

    ar = xcb_alloc_color_reply(c, acookie, NULL);
    *pixel = ar->pixel;
    free(ar);
}

static void create_rectangles(xcb_rectangle_t * red, xcb_rectangle_t * green, int w, int h)
{
    int x, y;
    int r, g;
    int i;

    for (i = 0; i < 32; i++) {
        red[i].width = green[i].width = w / 8;
        red[i].height = green[i].height = h / 8;
    }

    r = g = 0;
    for (x = 0; x < 8; x++)
        for (y = 0; y < 8; y++) {
            if (((y * 8) + x) % 2 == y % 2) {
                red[r].x = x * (w / 8);
                red[r].y = y * (h / 8);
                r++;
            }
            else {
                green[g].x = x * (w / 8);
                green[g].y = y * (h / 8);
                g++;
            }
        }
}

int xcb_draw_grid(const char *display)
{
    uint32_t pixels[2];

    xcb_connection_t *c;
    xcb_screen_t *screen;
    xcb_gcontext_t red_fg;
    xcb_gcontext_t green_fg;

    xcb_rectangle_t red_rectangles[32];
    xcb_rectangle_t green_rectangles[32];

    /* Open the connection to the X server */
    c = xcb_connect(display, NULL);
    if (xcb_connection_has_error(c))
        return 1;

    /* Get the first screen */
    screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    red_fg = xcb_generate_id(c);
    lookup_color(c, screen, "red", &pixels[0]);
    pixels[1] = 0;
    xcb_create_gc(c, red_fg, screen->root, XCB_GC_FOREGROUND, pixels);

    green_fg = xcb_generate_id(c);
    lookup_color(c, screen, "green", &pixels[0]);
    pixels[1] = 0;
    xcb_create_gc(c, green_fg, screen->root, XCB_GC_FOREGROUND, pixels);

    create_rectangles(red_rectangles, green_rectangles, screen->width_in_pixels,
                      screen->height_in_pixels);

    /* We draw the rectangles */
    xcb_poly_fill_rectangle_checked(c, screen->root, red_fg, 32, red_rectangles);
    xcb_poly_fill_rectangle_checked(c, screen->root, green_fg, 32, green_rectangles);

    /* We flush the request */
    xcb_flush(c);

    xcb_disconnect(c);

    return 0;
}
