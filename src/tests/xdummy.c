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
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>


#include "xdummy.h"

static void write_xorg_conf(FILE *fp, xdummy_t *server, long vram)
{

    fprintf(fp,
        "# This xorg configuration file is meant to be used by x11spice\n"
        "# to start a dummy X11 server.\n"
        "\n"
        "Section \"ServerFlags\"\n"
        "  Option \"DontVTSwitch\" \"true\"\n"
        "  Option \"AllowMouseOpenFail\" \"true\"\n"
        "  Option \"PciForceNone\" \"true\"\n"
        "  Option \"AutoEnableDevices\" \"false\"\n"
        "  Option \"AutoAddDevices\" \"false\"\n"
        "EndSection\n"
        "\n"
        "Section \"InputDevice\"\n"
        "  Identifier \"dummy_mouse\"\n"
        "  Option \"CorePointer\" \"true\"\n"
        "  Driver \"void\"\n"
        "EndSection\n"
        "\n"
        "Section \"InputDevice\"\n"
        "  Identifier \"dummy_keyboard\"\n"
        "  Option \"CoreKeyboard\" \"true\"\n"
        "  Driver \"void\"\n"
        "EndSection\n"
        "\n"
        "Section \"Device\"\n"
        "  Identifier \"dummy_videocard\"\n"
        "  Driver \"dummy\"\n"
        "  VideoRam %ld\n"
        "EndSection\n"
        "\n"
        "Section \"Screen\"\n"
        "  Identifier \"dummy_screen\"\n"
        "  Device \"dummy_videocard\"\n"
        "  Monitor \"dummy_monitor\"\n"
        "  DefaultDepth 24\n"
        "EndSection\n"
        "\n"
        "Section \"ServerLayout\"\n"
        "  Identifier   \"dummy_layout\"\n"
        "  Screen       \"dummy_screen\"\n"
        "  InputDevice  \"dummy_mouse\"\n"
        "  InputDevice  \"dummy_keyboard\"\n"
        "EndSection\n", vram);
}

static int generate_paths(xdummy_t *server, gconstpointer user_data)
{
    gchar *p = g_test_build_filename(G_TEST_BUILT, "run", NULL);
    if (!p)
        return -1;

    if (mkdir(p, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) && errno != EEXIST)
        return -1;
    g_free(p);

    p = g_test_build_filename(G_TEST_BUILT, "run", user_data, NULL);
    if (!p)
        return -1;

    if (mkdir(p, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) && errno != EEXIST)
        return -1;
    g_free(p);


    server->xorg_fname = g_test_build_filename(G_TEST_BUILT, "run", user_data, "xorg.conf", NULL);
    if (! server->xorg_fname)
        return -1;

    server->logfile = g_test_build_filename(G_TEST_BUILT, "run", user_data, "xorg.log", NULL);
    if (! server->logfile)
        return -1;

    server->outfile = g_test_build_filename(G_TEST_BUILT, "run", user_data, "xorg.out", NULL);
    if (! server->outfile)
        return -1;

    server->spicefile = g_test_build_filename(G_TEST_BUILT, "run", user_data, "spice.log", NULL);
    if (! server->outfile)
        return -1;

    return 0;
}

static int exec_xorg(xdummy_t *server, gconstpointer user_data)
{
    FILE *fp;
    char fdbuf[100];

    fp = fopen(server->xorg_fname, "w");
    if (! fp)
        return -1;

    write_xorg_conf(fp, server, 192000L);
    fclose(fp);

    if (redirect(server->outfile))
        return -1;

    snprintf(fdbuf, sizeof(fdbuf), "%d", server->pipe);

    return execlp("Xorg", "Xorg", "-ac",
            "-config", server->xorg_fname,
            "-logfile", server->logfile,
            "-displayfd", fdbuf, NULL);
}

int redirect(gchar *fname)
{
    int fd;
    fd = open(fname, O_CREAT | O_WRONLY, S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH);
    if (fd <= 0)
        return -1;
   
    dup2(fd, fileno(stdout));
    dup2(fd, fileno(stderr));

    return 0;
}


void start_server(xdummy_t *server, gconstpointer user_data)
{
    int fd[2];
    char buf[200];
    int rc;
    int pos = 0;
    char *p;

    server->running = FALSE;
    if (generate_paths(server, user_data))
        return;

    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd))
        return;

    server->pid = fork();
    if (server->pid == 0)
    {
        close(fd[0]);
        server->pipe = fd[1];
        exec_xorg(server, user_data);
        g_warning("server exec failed.");
        exit(-1);
    }
    else {
        server->pipe = fd[0];
        close(fd[1]);

        if (server->pid == -1)
            return;
    }

    while (1)
    {
        rc = read(server->pipe, buf + pos, sizeof(buf) - pos);
        if (rc == -1 && errno == EINTR)
            continue;
    
        if (rc <= 0)
        {
            g_warning("server failed to start.");
            return;
        }
        pos += rc;

        for (p = buf; p < buf + pos; p++)
            if (*p == '\n')
                break;

        if (p >= buf + sizeof(buf))
            return;

        if (*p != '\n')
            continue;

        server->display = g_memdup(buf, p - buf + 1);
        server->display[p - buf] = '\0';
        break;
    }

    server->running = TRUE;
    g_message("server started; display %s", server->display);
}

int still_alive(int pid)
{
    return !waitpid(pid, NULL, WNOHANG);
}

void stop_server(xdummy_t *server, gconstpointer user_data)
{
    g_message("server stopping; display %s", server->display);
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

    g_free(server->xorg_fname);
    g_free(server->logfile);
    g_free(server->outfile);
    g_free(server->spicefile);
    g_free(server->display);
}
