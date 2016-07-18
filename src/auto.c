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
**  auto.c
**      This file provides functions to implement the '--auto' option
**  for x11spice.  This mostly involves trying to find an open port we can use
**  for our Xserver
**--------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#include "auto.h"
#include "x11spice.h"

#define SPICE_URI_PREFIX    "spice://"

int auto_parse(const char *auto_spec, char **addr, int *port_start, int *port_end)
{
    int leading = 0;
    int trailing = 0;
    int hyphen = 0;
    const char *p;
    int len;

    *port_start = *port_end = -1;
    *addr = NULL;

    /* Allow form of spice:// */
    if (strlen(auto_spec) > strlen(SPICE_URI_PREFIX))
        if (memcmp(auto_spec, SPICE_URI_PREFIX, strlen(SPICE_URI_PREFIX)) == 0)
            auto_spec += strlen(SPICE_URI_PREFIX);

    p = auto_spec + strlen(auto_spec) - 1;
    /* Look for a form of NNNN-NNNN at the end of the line */
    for (; p >= auto_spec && *p; p--) {
        /* Skip trailing white space */
        if (isspace(*p) && !hyphen && !trailing)
            continue;

        /* We're looking for only digits and a hyphen */
        if (*p != '-' && !isdigit(*p))
            break;

        if (*p == '-') {
            if (hyphen)
                return X11SPICE_ERR_PARSE;
            hyphen++;
            if (trailing > 0)
                *port_end = strtol(p + 1, NULL, 0);
            continue;
        }

        if (hyphen)
            leading++;
        else
            trailing++;
    }

    if (leading && hyphen)
        *port_start = strtol(p + 1, NULL, 0);

    if (trailing && !hyphen)
        *port_end = strtol(p + 1, NULL, 0);

    /* If we had a hyphen, make sure we had a NNNN-NNN pattern too... */
    if (hyphen && (!leading || !trailing))
        return X11SPICE_ERR_PARSE;

    /* If we got a port range, make sure we had either no address provided,
       or a clear addr:NNNN-NNNN specficiation */
    if (leading || trailing)
        if (p > auto_spec && *p != ':')
            return X11SPICE_ERR_PARSE;

    if (p > auto_spec && *p == ':')
        p--;

    len = p - auto_spec + 1;
    if (len > 0) {
        *addr = calloc(1, len + 1);
        memcpy(*addr, auto_spec, len);
    }

    return 0;
}

static int try_port(const char *addr, int port)
{
    static const int on = 1, off = 0;
    struct addrinfo ai, *res, *e;
    char portbuf[33];
    int sock, rc;

    memset(&ai, 0, sizeof(ai));
    ai.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_family = 0;

    snprintf(portbuf, sizeof(portbuf), "%d", port);
    rc = getaddrinfo(addr && strlen(addr) ? addr : NULL, portbuf, &ai, &res);
    if (rc != 0)
        return -1;

    for (e = res; e != NULL; e = e->ai_next) {
        sock = socket(e->ai_family, e->ai_socktype, e->ai_protocol);
        if (sock < 0)
            continue;

        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &on, sizeof(on));
        /* listen on both ipv4 and ipv6 */
        if (e->ai_family == PF_INET6)
            setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (void *) &off, sizeof(off));

        if (bind(sock, e->ai_addr, e->ai_addrlen) == 0) {
            char uaddr[INET6_ADDRSTRLEN + 1];
            char uport[33];
            rc = getnameinfo((struct sockaddr *) e->ai_addr, e->ai_addrlen,
                             uaddr, INET6_ADDRSTRLEN, uport, sizeof(uport) - 1,
                             NI_NUMERICHOST | NI_NUMERICSERV);
            if (rc == 0)
                printf("bound to %s:%s\n", uaddr, uport);
            else
                printf("cannot resolve address spice-server is bound to\n");

            freeaddrinfo(res);
            goto listen;
        }
        close(sock);

        /*
        **  Oddly, you seem to get situations where the ipv6 bind will fail,
        **   with address in use; you can then try again and bind to the ipv4,
        **   but you then go on to get other failures.
        */
        break;
    }

    freeaddrinfo(res);
    return -1;

listen:
    if (listen(sock, SOMAXCONN) != 0) {
        perror("Error in listen");
        close(sock);
        return -1;
    }

    return sock;
}

int auto_listen_port_fd(const char *addr, int start, int end)
{
    int i;
    int rc;

    if (start == -1)
        start = end;
    if (end == -1)
        end = start;

    for (i = start; i <= end; i++) {
        rc = try_port(addr, i);
        if (rc >= 0) {
            printf("URI=%s:%d\n", addr ? addr : "", i);
            return rc;
        }
    }

    return -1;
}

int auto_listen(char *auto_spec)
{
    char *addr = NULL;
    int start;
    int end;
    int rc;

    if (auto_parse(auto_spec, &addr, &start, &end))
        return -1;

    rc = auto_listen_port_fd(addr, start, end);
    if (addr)
        free(addr);

    fflush(stdout);

    return rc;
}
