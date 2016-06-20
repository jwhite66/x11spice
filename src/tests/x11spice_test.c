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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "xdummy.h"
#include "x11spice_test.h"


static int exec_x11spice(x11spice_server_t *server, gchar *display)
{
    char buf[256];

    /* Redirect stderr and stdout to our pipe */
    dup2(server->pipe, fileno(stdout));
    dup2(server->pipe, fileno(stderr));

    snprintf(buf, sizeof(buf), "../x11spice --display :%s --auto localhost:5900-5999 --hide", display);

    return execl("/bin/sh", "sh", "-c", buf, NULL);

    return -1;
}


static void * flush_output(void *opaque)
{
    x11spice_server_t *server = (x11spice_server_t *) opaque;
    int rc;
    char buf[4096];

    while (1)
    {
        rc = read(server->pipe, buf, sizeof(buf));
        if (rc == -1 && errno == EINTR)
            continue;
    
        if (rc <= 0)
            break;

        write(server->logfd, buf, rc);
    }

    close(server->logfd);
    close(server->pipe);

    return NULL;
}

static int get_a_line(char *buf, int len, x11spice_server_t *server)
{
    char *p;

    for (p = buf; p < buf + len; p++)
        if (*p == '\n')
        {
            if (p - buf > 4 && memcmp(buf, "URI=", 4) == 0)
            {
                server->uri = g_memdup(buf + 4, p - buf - 4 + 1);
                server->uri[p - buf - 4] = '\0';
                return len;
            }
            return p - buf + 1;
        }

    return 0;
}

int x11spice_start(x11spice_server_t *server, test_t *test)
{
    int fd[2];
    char buf[4096];
    int rc;
    int pos = 0;
    int flush;

    server->running = FALSE;

    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd))
        return -1;

    server->pid = fork();
    if (server->pid == 0)
    {
        close(fd[0]);
        server->pipe = fd[1];
        exec_x11spice(server, test->xserver->display);
        g_warning("x11spice server exec failed.");
        return -1;
    }
    else
    {
        server->pipe = fd[0];
        close(fd[1]);

        if (server->pid == -1)
            return -1;
    }

    server->logfd = open(test->logfile, O_CREAT | O_WRONLY, S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH);
    if (server->logfd <= 0)
    {
        x11spice_stop(server);
        return -1;
    }
   
    memset(buf, 0, sizeof(buf));
    while (! server->uri)
    {
        rc = read(server->pipe, buf + pos, sizeof(buf) - pos);
        if (rc == -1 && errno == EINTR)
            continue;

        if (rc <= 0)
        {
            g_warning("x11spice server failed to send signal line.  rc %d, errno %d", rc, errno);
            return -1;
        }

        pos += rc;

        while ((flush = get_a_line(buf, pos, server)))
        {
            write(server->logfd, buf, flush);
            if (flush < pos)
                memmove(buf, buf + flush, pos - flush);
            pos -= flush;
        }
    }

    pthread_create(&server->flush_thread, NULL, flush_output, server);

    server->running = TRUE;
    g_message("x11spice started; pid %d", server->pid);

    return 0;
}

void x11spice_stop(x11spice_server_t *server)
{
    g_message("server stopping; pid %d", server->pid);
    if (server->running)
    {
        if (still_alive(server->pid))
        {
            kill(server->pid, SIGTERM);
            usleep(50 * 1000);
        }

        if (still_alive(server->pid))
        {
            sleep(1);
            kill(server->pid, SIGKILL);
        }
    }
}
