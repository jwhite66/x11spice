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

#ifndef LOCAL_SPICE_H_
#define LOCAL_SPICE_H_

#include <spice.h>

#include "options.h"

/*----------------------------------------------------------------------------
**  Structure definitions
**--------------------------------------------------------------------------*/
typedef struct
{
    SpiceServer *server;
    SpiceCoreInterface *core;
    QXLInstance display_sin;

    SpiceKbdInstance keyboard_sin;
    uint8_t escape;

    QXLWorker *worker;
    int compression_level;

    void *session_ptr;
} spice_t;

/*----------------------------------------------------------------------------
**  Prototypes
**--------------------------------------------------------------------------*/
int spice_start(spice_t *s, options_t *options);
void spice_end(spice_t *s);

#endif
