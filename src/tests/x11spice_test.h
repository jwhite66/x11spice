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

#ifndef X11SPICE_TEST_H_
#define X11SPICE_TEST_H_

#include <glib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "tests.h"

/*----------------------------------------------------------------------------
**  Structure definitions
**--------------------------------------------------------------------------*/
typedef struct
{
    int pid;
    int pipe;
    int logfd;
    pthread_t flush_thread;
    gboolean running;
    gchar *uri;
} x11spice_server_t;

/*----------------------------------------------------------------------------
**  Prototypes
**--------------------------------------------------------------------------*/
int x11spice_start(x11spice_server_t *server, test_t *test);
void x11spice_stop(x11spice_server_t *server);

#endif
