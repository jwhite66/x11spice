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
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>


#include "xdummy.h"
#include "util.h"

static void write_xorg_conf(FILE * fp, xdummy_t *server)
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
            "Section \"Monitor\"\n"
            "    Identifier     \"dummy_monitor\"\n"
            "    VendorName     \"Unknown\"\n"
            "    ModelName      \"Unknown\"\n"
            "    HorizSync       30.0 - 130.0\n"
            "    VertRefresh     50.0 - 250.0\n"
            "    Option         \"DPMS\"\n"
            "    Option         \"ReducedBlanking\"\n"
            "    # Fedora 24 xorg did not have a default 1920x1080 mode.\n"
            "    # 1920x1080 @ 60.00 Hz (GTF) hsync: 67.08 kHz; pclk: 172.80 MHz\n"
            "    Modeline \"1920x1080\" 172.80 1920 2040 2248 2576 1080 1081 1084 1118 -HSync +Vsync\n"
            "EndSection\n"
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
            "SubSection \"Display\"\n"
            "  Modes %s\n"
            "EndSubSection\n"
            "EndSection\n"
            "\n"
            "Section \"ServerLayout\"\n"
            "  Identifier   \"dummy_layout\"\n"
            "  Screen       \"dummy_screen\"\n"
            "  InputDevice  \"dummy_mouse\"\n"
            "  InputDevice  \"dummy_keyboard\"\n"
            "EndSection\n", server->desired_vram, server->modes);
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
    if (!server->xorg_fname)
        return -1;

    server->logfile = g_test_build_filename(G_TEST_BUILT, "run", user_data, "xorg.log", NULL);
    if (!server->logfile)
        return -1;

    server->outfile = g_test_build_filename(G_TEST_BUILT, "run", user_data, "xorg.out", NULL);
    if (!server->outfile)
        return -1;

    server->spicefile = g_test_build_filename(G_TEST_BUILT, "run", user_data, "spice.log", NULL);
    if (!server->spicefile)
        return -1;

    return 0;
}

static int exec_xorg(xdummy_t *server, gconstpointer user_data G_GNUC_UNUSED)
{
    FILE *fp;
    char fdbuf[100];
    char xorg_binary[100];

    fp = fopen(server->xorg_fname, "w");
    if (!fp)
        return -1;

    write_xorg_conf(fp, server);
    fclose(fp);

    if (redirect(server->outfile))
        return -1;

    snprintf(fdbuf, sizeof(fdbuf), "%d", server->pipe);

    strcpy(xorg_binary, "Xorg");
    if (access("/usr/libexec/Xorg", X_OK) == 0)
        strcpy(xorg_binary, "/usr/libexec/Xorg");

    return execlp(xorg_binary, xorg_binary, "-ac",
                  "-config", server->xorg_fname,
                  "-logfile", server->logfile, "-displayfd", fdbuf, NULL);
}

static void configure_xorg_parameters(xdummy_t *server, gconstpointer user_data)
{
    server->desired_vram = ((1024 * 768 * 4) + 1023)/ 1024;
    server->modes = "\"1024x768\"";

    if (strcmp(user_data , "resize") == 0) {
        server->desired_vram = ((1920 * 1080 * 4) + 1023) / 1024;
        server->modes = "\"1920x1080\"";
    }

    if (strlen(user_data) > 7 && memcmp(user_data, "client_", 7) == 0) {
        server->desired_vram = ((1280 * 1024 * 4) + 1023) / 1024;
        server->modes = "\"1280x1024\"";
    }
}

void start_server(xdummy_t *server, gconstpointer user_data)
{
    int fd[2];
    char buf[200];
    int rc;
    int pos = 0;
    char *p;

    server->running = FALSE;

    configure_xorg_parameters(server, user_data);

    if (generate_paths(server, user_data))
        return;

    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd))
        return;

    server->pid = fork();
    if (server->pid == 0) {
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

    while (1) {
        rc = read(server->pipe, buf + pos, sizeof(buf) - pos);
        if (rc == -1 && errno == EINTR)
            continue;

        if (rc <= 0) {
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

void stop_server(xdummy_t *server, gconstpointer user_data G_GNUC_UNUSED)
{
    g_message("server stopping; display %s", server->display);
    if (server->running) {
        if (still_alive(server->pid)) {
            kill(server->pid, SIGTERM);
            usleep(50 * 1000);
        }

        if (still_alive(server->pid)) {
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
