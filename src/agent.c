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
**  agent.c
**      Provide an interface to support the Virtual Desktop Agent (vdagent).
**--------------------------------------------------------------------------*/

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "x11spice.h"
#include "agent.h"
#include "session.h"

static void uinput_handle_key(agent_t *agent, struct input_event *ev)
{
    int button = 0;

    switch (ev->code) {
        case BTN_LEFT:
            button = 1 << 0;
            break;
        case BTN_MIDDLE:
            button = 1 << 2;
            break;
        case BTN_RIGHT:
            button = 1 << 1;
            break;
    }

    if (ev->value > 0)
        agent->uinput_buttons_state |= button;
    else
        agent->uinput_buttons_state &= ~button;

    session_handle_mouse_buttons(agent->spice->session, agent->uinput_buttons_state);
}

static void uinput_handle_relative(agent_t *agent, struct input_event *ev)
{
    int button = 1;
    if (ev->value == 1)
        button = 1 << 3;
    else
        button = 1 << 4;

    agent->uinput_buttons_state |= button;
    session_handle_mouse_buttons(agent->spice->session, agent->uinput_buttons_state);
    agent->uinput_buttons_state &= ~button;
    session_handle_mouse_buttons(agent->spice->session, agent->uinput_buttons_state);
}

static void uinput_handle_absolute(agent_t *agent, struct input_event *ev)
{
    switch (ev->code) {
        case ABS_X:
            agent->uinput_x = ev->value;
            break;
        case ABS_Y:
            agent->uinput_y = ev->value;
            break;
        default:
            g_debug("%s: unknown axis %d, ignoring\n", __func__, ev->code);
            return;
            break;
    }
    session_handle_mouse_position(agent->spice->session, agent->uinput_x, agent->uinput_y,
                                  agent->uinput_buttons_state);
}

static void uinput_read_cb(int fd, int event, void *opaque)
{
    agent_t *agent = (agent_t *) opaque;
    struct input_event *ev;
    int rc;

    rc = read(agent->uinput_fd, agent->uinput_buffer + agent->uinput_offset,
              sizeof(agent->uinput_buffer) - agent->uinput_offset);
    if (rc == -1) {
        if (errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK) {
            perror("Error - x11spice uinput read failed");
        }
        return;
    }

    agent->uinput_offset += rc;

    while (agent->uinput_offset >= sizeof(*ev)) {
        ev = (struct input_event *) agent->uinput_buffer;
        agent->uinput_offset -= sizeof(*ev);
        if (agent->uinput_offset > 0)
            memmove(agent->uinput_buffer, agent->uinput_buffer + sizeof(*ev), agent->uinput_offset);

        switch (ev->type) {
            case EV_KEY:
                uinput_handle_key(agent, ev);
                break;

            case EV_REL:
                uinput_handle_relative(agent, ev);
                break;

            case EV_ABS:
                uinput_handle_absolute(agent, ev);
                break;
        }
    }

    if (rc == 0)
        close(agent->uinput_fd);
}

static int start_uinput(agent_t *agent, const char *uinput_filename)
{
    int rc;

    rc = mkfifo(uinput_filename, 0666);
    if (rc != 0) {
        fprintf(stderr, "Error: failed to create uinput fifo %s: %s\n",
                uinput_filename, strerror(errno));
        return -1;
    }

    agent->uinput_fd = open(uinput_filename, O_RDONLY | O_NONBLOCK, 0666);
    if (agent->uinput_fd == -1) {
        fprintf(stderr, "Error: failed creating uinput file %s: %s\n",
                uinput_filename, strerror(errno));
        return -1;
    }

    agent->uinput_watch = agent->spice->core->watch_add(agent->uinput_fd,
                                                        SPICE_WATCH_EVENT_READ, uinput_read_cb,
                                                        agent);

    return 0;
}

static void stop_uinput(agent_t *agent)
{
    if (agent->uinput_watch)
        agent->spice->core->watch_remove(agent->uinput_watch);
    agent->uinput_watch = NULL;

    if (agent->uinput_fd != -1)
        close(agent->uinput_fd);
    agent->uinput_fd = -1;
}

static void stop_virtio(agent_t *agent)
{
    if (agent->virtio_client_watch)
        agent->spice->core->watch_remove(agent->virtio_client_watch);
    agent->virtio_client_watch = NULL;

    if (agent->virtio_listen_watch)
        agent->spice->core->watch_remove(agent->virtio_listen_watch);
    agent->virtio_listen_watch = NULL;

    if (agent->virtio_client_fd != -1) {
        spice_server_remove_interface(&agent->base.base);
        close(agent->virtio_client_fd);
    }
    if (agent->virtio_listen_fd != -1)
        close(agent->virtio_listen_fd);
    agent->virtio_client_fd = agent->virtio_listen_fd - 1;
}


static int agent_char_write(SpiceCharDeviceInstance *sin, const uint8_t *buf, int len)
{
    agent_t *agent = SPICE_CONTAINEROF(sin, agent_t, base);
    int written;

    if (agent->virtio_client_fd == -1)
        return 0;

    written = send(agent->virtio_client_fd, buf, len, 0);
    if (written != len)
        g_warning("%s: ERROR: short write to vdagentd - TODO buffering\n", __func__);

    return written;
}

static int agent_char_read(SpiceCharDeviceInstance *sin, uint8_t *buf, int len)
{
    agent_t *agent = SPICE_CONTAINEROF(sin, agent_t, base);
    int nbytes;

    if (agent->virtio_client_fd == -1) {
        return 0;
    }
    nbytes = recv(agent->virtio_client_fd, buf, len, 0);
    if (nbytes <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        g_warning("ERROR: vdagent died\n");

        stop_virtio(agent);
        stop_uinput(agent);
    }
    return nbytes;
}

#if SPICE_SERVER_VERSION >= 0x000c02
static void agent_char_event(SpiceCharDeviceInstance *sin, uint8_t event)
{
    g_debug("agent event %d", event);
}
#endif

static void agent_char_state(SpiceCharDeviceInstance *sin, int connected)
{
    agent_t *agent = SPICE_CONTAINEROF(sin, agent_t, base);
    agent->connected = connected;
    g_debug("agent state %d", connected);
}

static void on_read_available(int fd, int event, void *opaque)
{
    agent_t *agent = (agent_t *) opaque;
    if (agent->virtio_client_fd == -1) {
        return;
    }
    spice_server_char_device_wakeup(&agent->base);
}

static void on_accept(int fd, int event, void *opaque)
{
    agent_t *agent = (agent_t *) opaque;
    struct sockaddr_un address;
    socklen_t length = sizeof(address);
    int flags;

    if (agent->virtio_client_fd != -1) {
        fprintf(stderr, "Error: cannot accept multiple agent connection.\n");
        close(fd);
        return;
    }

    agent->virtio_client_fd =
        accept(agent->virtio_listen_fd, (struct sockaddr *) &address, &length);
    if (agent->virtio_client_fd == -1) {
        perror("Error - accepting on unix domain socket");
        return;
    }

    flags = fcntl(agent->virtio_client_fd, F_GETFL);
    if (flags == -1) {
        perror("Error - getting flags from uds client fd");
        close(agent->virtio_client_fd);
        return;
    }
    if (fcntl(agent->virtio_client_fd, F_SETFL, flags | O_NONBLOCK | O_CLOEXEC) == -1) {
        perror("Error setting CLOEXEC & NONBLOCK flags from uds client fd");
        close(agent->virtio_client_fd);
        return;
    }

    agent->virtio_client_watch =
        agent->spice->core->watch_add(agent->virtio_client_fd, SPICE_WATCH_EVENT_READ,
                                      on_read_available, agent);

    spice_server_add_interface(agent->spice->server, &agent->base.base);
}

static int start_virtio(agent_t *agent, const char *virtio_path)
{
    struct sockaddr_un address;
    int rc;

    agent->virtio_listen_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (agent->virtio_listen_fd == -1) {
        perror("Error creating unix domain socket");
        return X11SPICE_ERR_NO_SOCKET;
    }
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", virtio_path);
    rc = bind(agent->virtio_listen_fd, (struct sockaddr *) &address, sizeof(address));
    if (rc != 0) {
        fprintf(stderr, "Error binding unix domain socket to %s: %s\n",
                virtio_path, strerror(errno));
        return X11SPICE_ERR_BIND;
    }

    rc = listen(agent->virtio_listen_fd, 1);
    if (rc != 0) {
        perror("Error listening to unix domain socket");
        return X11SPICE_ERR_LISTEN;
    }

    agent->virtio_listen_watch = agent->spice->core->watch_add(agent->virtio_listen_fd,
                                                               SPICE_WATCH_EVENT_READ, on_accept,
                                                               agent);

    return 0;

}

int agent_start(spice_t *spice, options_t *options, agent_t *agent)
{
    int rc;

    const static SpiceCharDeviceInterface agent_sif = {
        .base = {
                 .type = SPICE_INTERFACE_CHAR_DEVICE,
                 .description = "x11spice vdagent",
                 .major_version = SPICE_INTERFACE_CHAR_DEVICE_MAJOR,
                 .minor_version = SPICE_INTERFACE_CHAR_DEVICE_MINOR,
                 },
        .state = agent_char_state,
        .write = agent_char_write,
        .read = agent_char_read,
#if SPICE_SERVER_VERSION >= 0x000c02
        .event = agent_char_event,
#endif
    };

    memset(agent, 0, sizeof(*agent));
    agent->spice = spice;
    agent->base.base.sif = &agent_sif.base;
    agent->base.subtype = "vdagent";
    agent->virtio_listen_fd = agent->virtio_client_fd = agent->uinput_fd = -1;

    if (!options->virtio_path || !options->uinput_path)
        return 0;

    rc = start_virtio(agent, options->virtio_path);
    if (rc)
        return rc;

    rc = start_uinput(agent, options->uinput_path);
    if (rc) {
        stop_virtio(agent);
        return rc;
    }

    return 0;
}

void agent_stop(agent_t *agent)
{
    stop_uinput(agent);
    stop_virtio(agent);
}
