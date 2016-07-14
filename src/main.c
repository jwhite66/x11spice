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

/*----------------------------------------------------------------------------
**  main.c
**      The main program for x11spice.  The hope is that this code
**  remains simple, and easy to follow, and that logic is broken neatly
**  down in subordinate modules.
**--------------------------------------------------------------------------*/


#include <stdio.h>
#include <signal.h>

#include "x11spice.h"
#include "options.h"
#include "local_spice.h"
#include "display.h"
#include "agent.h"
#include "gui.h"
#include "session.h"


static void sigterm_handler(int arg)
{
    gui_sigterm();
}

static void handle_sigterm(void)
{
    struct sigaction act = { };
    act.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &act, NULL);
}

int main(int argc, char *argv[])
{
    int rc;

    session_t session;

    int display_opened = 0;
    int spice_started = 0;
    int gui_created = 0;
    int session_started = 0;

    /*------------------------------------------------------------------------
    **  Parse arguments
    **----------------------------------------------------------------------*/
    options_init(&session.options);
    options_from_config(&session.options);
    rc = options_parse_arguments(argc, argv, &session.options);
    if (rc)
        goto exit;

    rc = options_process_io(&session.options);
    if (rc)
        goto exit;

    /*------------------------------------------------------------------------
    **  Create the session
    **----------------------------------------------------------------------*/
    rc = session_create(&session);
    if (rc)
        goto exit;

    /*------------------------------------------------------------------------
    **  Open the display
    **----------------------------------------------------------------------*/
    rc = display_open(&session.display, &session);
    if (rc)
        goto exit;
    display_opened = 1;

    /*------------------------------------------------------------------------
    **  Initialize the GUI
    **----------------------------------------------------------------------*/
    rc = gui_create(&session.gui, &session, argc, argv);
    if (rc)
        goto exit;
    gui_created = 1;

    /*------------------------------------------------------------------------
    **  Start up a spice server
    **----------------------------------------------------------------------*/
    rc = spice_start(&session.spice, &session.options, session.display.fullscreen);
    if (rc)
        goto exit;
    spice_started = 1;
    agent_start(&session.spice, &session.options, &session.agent);

    /*------------------------------------------------------------------------
    **  Start our session and leave the GUI running until we have
    **   a reason to quit
    **----------------------------------------------------------------------*/
    rc = session_start(&session);
    if (rc)
        goto exit;
    session_started = 1;

    handle_sigterm();

    gui_run(&session.gui);

    /*------------------------------------------------------------------------
    **  Clean up, go home
    **----------------------------------------------------------------------*/
exit:
    if (session_started)
        session_end(&session);

    if (spice_started) {
        agent_stop(&session.agent);
        spice_end(&session.spice);
    }

    if (gui_created)
        gui_destroy(&session.gui);

    if (display_opened)
        display_close(&session.display);

    options_free(&session.options);

    return rc;
}
