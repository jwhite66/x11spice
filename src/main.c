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

#include "x11spice.h"
#include "options.h"
#include "local_spice.h"
#include "display.h"
#include "gui.h"
#include "session.h"

int main(int argc, char *argv[])
{
    int rc;

    session_t   session;

    int         display_opened = 0;
    int         spice_started = 0;

    /*------------------------------------------------------------------------
    **  Parse arguments
    **----------------------------------------------------------------------*/
    options_init(&session.options);
    rc = options_parse_arguments(argc, argv, &session.options);
    if (rc)
        goto exit;
    options_from_config(&session.options);

    /*------------------------------------------------------------------------
    **  Open the display
    **----------------------------------------------------------------------*/
    rc = display_open(&session.display, &session.options);
    if (rc)
        goto exit;
    display_opened = 1;

    /*------------------------------------------------------------------------
    **  Initialize the GUI
    **----------------------------------------------------------------------*/
    rc = gui_init(&session.gui, argc, argv);
    if (rc)
        goto exit;

    /*------------------------------------------------------------------------
    **  Start up a spice server
    **----------------------------------------------------------------------*/
    rc = spice_start(&session.spice, &session.options);
    if (rc)
        goto exit;
    spice_started = 1;

    /*------------------------------------------------------------------------
    **  Leave the GUI running until we have a reason to quit
    **----------------------------------------------------------------------*/
    rc = session_start(&session);
    if (rc)
        goto exit;

    gui_run(&session.gui);
    session_end(&session);

    /*------------------------------------------------------------------------
    **  Clean up, go home
    **----------------------------------------------------------------------*/
exit:
    if (spice_started)
        spice_end(&session.spice);

    if (display_opened)
        display_close(&session.display);

    options_free(&session.options);

    return rc;
}
