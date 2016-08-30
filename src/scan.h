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

#ifndef SCAN_H_
#define SCAN_H_

#include <pixman.h>

/*----------------------------------------------------------------------------
**  Definitions and simple types
**--------------------------------------------------------------------------*/
typedef enum { DAMAGE_SCAN_REPORT, SCANLINE_SCAN_REPORT, EXIT_SCAN_REPORT } scan_type_t;

typedef struct session_struct session_t;
/*----------------------------------------------------------------------------
**  Structure definitions
**--------------------------------------------------------------------------*/

typedef struct {
    scan_type_t type;
    int x;
    int y;
    int w;
    int h;
} scan_report_t;

typedef struct {
    pthread_t thread;
    GAsyncQueue *queue;
    session_t *session;
    GMutex lock;
    int current_scanline;
    pixman_region16_t region;
    int target_fps;
} scanner_t;


/*----------------------------------------------------------------------------
**  Prototypes
**--------------------------------------------------------------------------*/
int scanner_create(scanner_t *scanner);
int scanner_destroy(scanner_t *scanner);

int scanner_push(scanner_t *scanner, scan_type_t type, int x, int y, int w, int h);

#endif
