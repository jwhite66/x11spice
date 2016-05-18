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

#include "x11spice.h"
#include "session.h"

static void session_handle_xevent(int fd, int event, void *opaque)
{
    session_t *s = (session_t *) opaque;
    XEvent xev;
    int rc;
    XDamageNotifyEvent *dev = (XDamageNotifyEvent *) &xev;;

    rc = XNextEvent(s->display.xdisplay, &xev);
    if (rc == 0)
    {
        if (xev.type != s->display.xd_event_base + XDamageNotify)
        {
            g_debug("Unexpected X event %d", xev.type);
            return;
        }

        g_debug("XDamageNotify [ser %ld|send_event %d|level %d|more %d|area (%dx%d)@%dx%d|geo (%dx%d)@%dx%d",
            dev->serial, dev->send_event, dev->level, dev->more,
            dev->area.width, dev->area.height, dev->area.x, dev->area.y,
            dev->geometry.width, dev->geometry.height, dev->geometry.x, dev->geometry.y);
    }
}

int session_start(session_t *s)
{
    s->xwatch = s->spice.core->watch_add(ConnectionNumber(s->display.xdisplay),
                    SPICE_WATCH_EVENT_READ, session_handle_xevent, s);
    if (! s->xwatch)
        return X11SPICE_ERR_NOWATCH;

    /* In order for the watch to function,
        we seem to have to request at least one event */
    // FIXME - not sure I know why...
    XPending(s->display.xdisplay);

    return 0;
}

void session_end(session_t *s)
{
    s->spice.core->watch_remove(s->xwatch);
}
