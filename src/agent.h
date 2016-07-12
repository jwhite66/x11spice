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

#ifndef AGENT_H_
#define AGENT_H_

#include <linux/input.h>
#include "local_spice.h"

/*----------------------------------------------------------------------------
**  Structure definitions
**--------------------------------------------------------------------------*/
typedef struct
{
    SpiceCharDeviceInstance base;

    int virtio_listen_fd;
    int virtio_client_fd;
    int uinput_fd;

    char uinput_buffer[sizeof(struct input_event)];
    int uinput_offset;
    int uinput_buttons_state;
    int uinput_x;
    int uinput_y;

    int connected;

    SpiceWatch *virtio_listen_watch;
    SpiceWatch *virtio_client_watch;
    SpiceWatch *uinput_watch;

    spice_t *spice;
} agent_t;

/*----------------------------------------------------------------------------
**  Prototypes
**--------------------------------------------------------------------------*/
int agent_start(spice_t *spice, options_t *options, agent_t *agent);
void agent_stop(agent_t *agent);

#endif
