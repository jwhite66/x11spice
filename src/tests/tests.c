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


#include "xdummy.h"

#include "tests.h"
#include "xcb.h"
#include "x11spice_test.h"

static int test_common_start(test_t * test, x11spice_server_t * server,
                             xdummy_t *xserver, gconstpointer user_data)
{
    int rc;

    if (!xserver->running) {
        g_test_skip("No server");
        return -1;
    }

    test->xserver = xserver;
    test->name = user_data;

    test->logfile = g_test_build_filename(G_TEST_BUILT, "run", test->name, "test.log", NULL);
    if (!test->logfile) {
        g_warning("Failed to create logfile");
        g_test_fail();
        return -1;
    }

    memset(server, 0, sizeof(*server));
    rc = x11spice_start(server, test);
    if (rc) {
        g_warning("Failed to start x11spice");
        g_test_fail();
        return rc;
    }

    return 0;
}

static void test_common_stop(test_t * test, x11spice_server_t * server)
{
    x11spice_stop(server);
}

void test_basic(xdummy_t *xserver, gconstpointer user_data)
{
    test_t test;
    x11spice_server_t server;
    int rc;
    gchar *screencap;
    char buf[4096];
    int needs_prefix;

    rc = test_common_start(&test, &server, xserver, user_data);
    if (rc)
        return;

    snprintf(buf, sizeof(buf), ":%s", xserver->display);
    if (xcb_draw_grid(buf)) {
        g_warning("Could not draw the grid");
        g_test_fail();
    }
    else {

        screencap = g_test_build_filename(G_TEST_BUILT, "run", test.name, "screencap.ppm", NULL);
        needs_prefix = 1;
        if (strlen(server.uri) >= 8 && memcmp(server.uri, "spice://", 8) == 0)
            needs_prefix = 0;

        snprintf(buf, sizeof(buf), "spicy-screenshot --uri=%s%s --out-file=%s",
                 needs_prefix ? "spice://" : "", server.uri, screencap);
        system(buf);

        snprintf(buf, sizeof(buf), "md5sum basic.expected.ppm | "
                 "sed -e 's!basic.expected.ppm!%s!' |" "md5sum -c", screencap);
        if (system(buf)) {
            snprintf(buf, sizeof(buf), "xwd -display :%s -root -out %s.xwd",
                     xserver->display, screencap);
            system(buf);

            g_warning("%s does not match basic.expected.ppm", screencap);
            g_warning("xwud -in %s.xwd should show you the current X screen.", screencap);
            g_test_fail();
        }
    }

    test_common_stop(&test, &server);
}
