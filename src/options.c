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


#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "options.h"
#include "x11spice.h"

void options_init(options_t *options)
{
    memset(options, 0, sizeof(*options));
}

void options_free(options_t *options)
{
    if (options->display)
    {
        free(options->display);
        options->display = NULL;
    }
}

static void usage(char *argv0)
{
    fprintf(stderr, "%s: \n", argv0);
    // FIXME - write usage
}

int options_parse_arguments(int argc, char *argv[], options_t *options)
{
    int rc;
    int longindex = 0;

    enum option_types {  OPTION_VIEWONLY, OPTION_TIMEOUT, OPTION_AUTOPORT,
                         OPTION_GENERATE_PASSCODE, OPTION_DISPLAY, OPTION_HELP
    };

    static struct option long_options[] =
    {
        {"viewonly",                 0, 0,       OPTION_VIEWONLY },
        {"timeout",                  1, 0,       OPTION_TIMEOUT  },
        {"autoport",                 0, 0,       OPTION_AUTOPORT },
        {"generate-passcode",        0, 0,       OPTION_GENERATE_PASSCODE},
        {"display",                  1, 0,       OPTION_DISPLAY },
        {"help",                     0, 0,       OPTION_HELP},
        {0, 0, 0, 0}
    };

    while (1)
    {
        rc = getopt_long_only(argc, argv, "", long_options, &longindex);
        if (rc == -1)
        {
            rc = 0;
            break;
        }

        switch (rc)
        {
            case OPTION_TIMEOUT:
                options->timeout = atol(optarg);
                break;

            case OPTION_DISPLAY:
                options->display = strdup(optarg);
                break;

            default:
                usage(argv[0]);
                rc = X11SPICE_ERR_BADARGS;
                break;
        }
    }

    return rc;
}
