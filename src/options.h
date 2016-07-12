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

#ifndef OPTIONS_H_
#define OPTIONS_H_

/*----------------------------------------------------------------------------
**  Structure definitions
**--------------------------------------------------------------------------*/
typedef struct {
    /* Both config and command line arguments */
    long timeout;
    int minimize;
    int viewonly;
    int generate_passcode;
    int hide;
    char *display;
    char *autouri;

    /* config only */
    char *spice_addr;
    int spice_port;
    char *spice_password;
    int disable_ticketing;
    int exit_on_disconnect;
    char *virtio_path;
    char *uinput_path;

    /* file names of config files */
    char *user_config_file;
    char *system_config_file;
} options_t;


/*----------------------------------------------------------------------------
**  Prototypes
**--------------------------------------------------------------------------*/
void options_init(options_t *options);
int options_parse_arguments(int argc, char *argv[], options_t *options);
void options_free(options_t *options);
void options_from_config(options_t *options);

#endif
