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

int main(int argc, char *argv[])
{
    int rc;

    options_t   options;
    display_t * display = NULL;
    gui_t       gui;
    spice_t     s;

    /*------------------------------------------------------------------------
    **  Parse arguments
    **----------------------------------------------------------------------*/
    options_init(&options);
    rc = options_parse_arguments(argc, argv, &options);
    if (rc)
        goto exit;
    options_from_config(&options);

    /*------------------------------------------------------------------------
    **  Open the display
    **----------------------------------------------------------------------*/
    display = display_open(&options);
    if (! display)
    {
        rc = X11SPICE_ERR_NODISPLAY;
        goto exit;
    }

    /*------------------------------------------------------------------------
    **  Initialize the GUI
    **----------------------------------------------------------------------*/
    rc = gui_init(&gui, argc, argv);
    if (rc)
        goto exit;

    /*------------------------------------------------------------------------
    **  Start up a spice server
    **----------------------------------------------------------------------*/
    rc = spice_start(&s, &options);
    if (rc)
        goto exit;

    gui_run(&gui);

    spice_end(&s);

    /*------------------------------------------------------------------------
    **  Close the display, go home
    **----------------------------------------------------------------------*/
exit:
    options_free(&options);

    if (display)
        display_close(display);

    return rc;
}
