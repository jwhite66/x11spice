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
**  options.c
**      Code to handle options.  This includes command line arguments
**  as well as options that can be set in configuration files.
**--------------------------------------------------------------------------*/

#include <glib.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <spice/protocol.h>

#include "options.h"
#include "x11spice.h"

void options_init(options_t *options)
{
    memset(options, 0, sizeof(*options));
}

void options_free(options_t *options)
{
    if (options->display) {
        free(options->display);
        options->display = NULL;
    }

    g_free(options->spice_password);
    options->spice_password = NULL;

    g_free(options->virtio_path);
    options->virtio_path = NULL;
    g_free(options->uinput_path);
    options->uinput_path = NULL;

    if (options->listen)
        free(options->listen);
    options->listen = NULL;

    g_free(options->user_config_file);
    options->user_config_file = NULL;

    g_free(options->system_config_file);
    options->system_config_file = NULL;
}


static gchar *string_option(GKeyFile *u, GKeyFile *s, const gchar *section, const gchar *key)
{
    gchar *ret = NULL;
    GError *error = NULL;

    if (u)
        ret = g_key_file_get_string(u, section, key, &error);
    if ((!u || error) && s)
        ret = g_key_file_get_string(s, section, key, NULL);
    if (error)
        g_error_free(error);

    return ret;
}

static gint int_option(GKeyFile *u, GKeyFile *s, const gchar *section, const gchar *key)
{
    gint ret = 0;
    GError *error = NULL;

    if (u)
        ret = g_key_file_get_integer(u, section, key, &error);
    if ((!u || error) && s)
        ret = g_key_file_get_integer(s, section, key, NULL);
    if (error)
        g_error_free(error);

    return ret;
}

static gboolean bool_option(GKeyFile *u, GKeyFile *s, const gchar *section, const gchar *key)
{
    gboolean ret = FALSE;
    GError *error = NULL;

    if (u)
        ret = g_key_file_get_boolean(u, section, key, &error);
    if ((!u || error) && s)
        ret = g_key_file_get_boolean(s, section, key, NULL);
    if (error)
        g_error_free(error);

    return ret;
}

static void usage(options_t *options, char *argv0)
{
    char indent[256];

    snprintf(indent, sizeof(indent), "%*.*s ", (int) strlen(argv0), (int) strlen(argv0), "");
    printf("%s: [OPTIONS] [<listen-specification>]\n", argv0);
    printf("\n");
    printf("Starts a Spice server and connects it to an X11 display.\n");
    printf("\n");
    printf("The <listen-specification> is of the form:\n");
    printf("  [[host]:[port][-end-port]\n");
    printf("where host specifies the address to listen on.  Defaults to localhost\n");
    printf("      port specifies the port to listen to.  Defaults to 5900.\n");
    printf("      end-port, if given, will cause x11spice to scan from port to end-port\n");
    printf("      checking for an open port, and using the first one available.\n");
    printf("\n");
    printf("Options:\n");
    printf("%s [--viewonly]\n", indent);
    printf("%s [--timeout=<seconds>]\n", indent);
    printf("%s [--display=<DISPLAY>]\n", indent);
    printf("%s [--generate-password[=<len>]\n", indent);
    printf("%s [--password=<password>]\n", indent);
    printf("%s [--password-file={-|<password-file}]\n", indent);
    printf("%s [--hide]\n", indent);
    printf("%s [--minimize]\n", indent);
}

int options_parse_arguments(int argc, char *argv[], options_t *options)
{
    int rc;
    int longindex = 0;

    enum option_types {  OPTION_VIEWONLY, OPTION_TIMEOUT, OPTION_AUTO, OPTION_HIDE,
                         OPTION_PASSWORD, OPTION_PASSWORD_FILE,
                         OPTION_GENERATE_PASSWORD, OPTION_DISPLAY, OPTION_MINIMIZE,
                         OPTION_HELP
    };

    static struct option long_options[] =
    {
        {"viewonly",                 0, 0,       OPTION_VIEWONLY },
        {"timeout",                  1, 0,       OPTION_TIMEOUT  },
        {"auto",                     1, 0,       OPTION_AUTO },
        {"hide",                     0, 0,       OPTION_HIDE },
        {"password",                 1, 0,       OPTION_PASSWORD },
        {"password-file",            1, 0,       OPTION_PASSWORD_FILE },
        {"generate-password",        2, 0,       OPTION_GENERATE_PASSWORD },
        {"display",                  1, 0,       OPTION_DISPLAY },
        {"minimize",                 0, 0,       OPTION_MINIMIZE },
        {"help",                     0, 0,       OPTION_HELP},
        {0, 0, 0, 0}
    };

    while (1) {
        rc = getopt_long_only(argc, argv, "", long_options, &longindex);
        if (rc == -1) {
            rc = 0;
            break;
        }

        switch (rc) {
            case OPTION_VIEWONLY:
                options->viewonly = 1;
                break;

            case OPTION_TIMEOUT:
                options->timeout = atol(optarg);
                break;

            case OPTION_HIDE:
                options->hide = 1;
                break;

            case OPTION_PASSWORD:
                options->spice_password = strdup(optarg);
                break;

            case OPTION_PASSWORD_FILE:
                options->password_file = strdup(optarg);
                break;

            case OPTION_GENERATE_PASSWORD:
                options->generate_password = DEFAULT_PASSWORD_LENGTH;
                if (optarg)
                    options->generate_password = atol(optarg);
                break;

            case OPTION_DISPLAY:
                options->display = strdup(optarg);
                break;

            case OPTION_MINIMIZE:
                options->minimize = 1;
                break;

            default:
                usage(options, argv[0]);
                return X11SPICE_ERR_BADARGS;
        }
    }

    /* Make sure conflicting password options are not given */
    if (rc == 0) {
        int count = 0;
        count += options->password_file ? 1 : 0;
        count += options->spice_password ? 1 : 0;
        count += options->generate_password ? 1 : 0;
        if (count > 1) {
            fprintf(stderr, "Error: you can specify only one of password, password-file, "
                            "and generate-password\n");
            rc = X11SPICE_ERR_BADARGS;
        }
    }

    /* Grab the listen spec, if given */
    if (rc == 0) {
        if (optind >= argc) {
            /* Default */
            options->listen = strdup("5900");
        } else if (optind < (argc - 1)) {
            fprintf(stderr, "Error: too many arguments\n");
            rc = X11SPICE_ERR_BADARGS;
        } else {
            options->listen = strdup(argv[optind]);
        }
    }

    return rc;
}

void options_from_config(options_t *options)
{
    GKeyFile *userkey = g_key_file_new();
    GKeyFile *systemkey = g_key_file_new();

    options->user_config_file = g_build_filename(g_get_user_config_dir(), "x11spice", NULL);

    if (!g_key_file_load_from_file(userkey, options->user_config_file, G_KEY_FILE_NONE, NULL)) {
        g_key_file_free(userkey);
        userkey = NULL;
    }

    if (!g_key_file_load_from_dirs(systemkey, "x11spice",
                                   (const char **) g_get_system_config_dirs(),
                                   &options->system_config_file, G_KEY_FILE_NONE, NULL)) {
        g_key_file_free(systemkey);
        systemkey = NULL;
    }

    options->timeout = int_option(userkey, systemkey, "spice", "timeout");
    options->minimize = int_option(userkey, systemkey, "spice", "minimize");
    options->viewonly = int_option(userkey, systemkey, "spice", "viewonly");
    options->generate_password = int_option(userkey, systemkey, "spice", "generate-password");
    options->hide = int_option(userkey, systemkey, "spice", "hide");
    options->display = string_option(userkey, systemkey, "spice", "display");

    options->listen = string_option(userkey, systemkey, "spice", "listen");
    options->spice_password = string_option(userkey, systemkey, "spice", "password");
    options->password_file = string_option(userkey, systemkey, "spice", "password-file");
    options->disable_ticketing = bool_option(userkey, systemkey, "spice", "disable-ticketing");
    options->exit_on_disconnect = bool_option(userkey, systemkey, "spice", "exit-on-disconnect");
    options->virtio_path = string_option(userkey, systemkey, "spice", "virtio-path");
    options->uinput_path = string_option(userkey, systemkey, "spice", "uinput-path");

    if (systemkey)
        g_key_file_free(systemkey);
    if (userkey)
        g_key_file_free(userkey);

    g_debug("options listen '%s', disable_ticketing %d", options->listen,
            options->disable_ticketing);
}

static int process_password_file(options_t *options)
{
    int rc = 0;
    FILE *fp;
    char *p;
    char buf[SPICE_MAX_PASSWORD_LENGTH + 1];

    if (strcmp(options->password_file, "-") == 0) {
        printf("Enter password: ");
        fflush(stdout);
        fp = stdin;
    }
    else {
        fp = fopen(options->password_file, "r");
        if (!fp)
            return X11SPICE_ERR_OPEN;
    }
    if (!fgets(buf, sizeof(buf), fp))
        rc = X11SPICE_ERR_PARSE;

    if (strcmp(options->password_file, "-") != 0)
        fclose(fp);

    /* Strip a trailing \n */
    p = buf + strlen(buf);
    if (p > buf && *(p - 1) == '\n')
        *(p - 1) = '\0';

    options->spice_password = strdup(buf);

    return rc;
}

static int generate_password(options_t *options)
{
    int fd;
    int rc;
    char *p;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return X11SPICE_ERR_OPEN;

    p = options->spice_password = malloc(options->generate_password + 1);
    if (!p)
        return X11SPICE_ERR_MALLOC;

    while (p - options->spice_password < options->generate_password) {
        rc = read(fd, p, sizeof(*p));
        if (rc == 0 || (rc == -1 && errno != EINTR))
            return -1;

        if (isalnum(*p))
            p++;
    }
    *p = '\0';

    close(fd);

    return 0;
}

int options_process_io(options_t *options)
{
    int rc;
    if (options->password_file) {
        rc = process_password_file(options);
        if (rc)
            return rc;
    }

    if (options->generate_password) {
        rc = generate_password(options);
        if (rc)
            return rc;
        printf("PASSWORD=%s\n", options->spice_password);
        fflush(stdout);
    }

    return 0;
}

#if defined(OPTIONS_MAIN)
int main(int argc, char *argv[])
{
    options_t options;

    options_init(&options);
    options_parse_arguments(argc, argv, &options);
    options_from_config(&options);
    g_message("Options parsed");
    options_free(&options);
}
#endif
