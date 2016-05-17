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

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "local_spice.h"
#include "x11spice.h"

struct SpiceTimer {
    SpiceTimerFunc func;
    void *opaque;
    GSource *source;
};

static SpiceTimer *timer_add(SpiceTimerFunc func, void *opaque)
{
    SpiceTimer *timer = (SpiceTimer *) calloc(1, sizeof(SpiceTimer));

    timer->func = func;
    timer->opaque = opaque;

    return timer;
}

static gboolean timer_func(gpointer user_data)
{
    SpiceTimer *timer = user_data;

    timer->func(timer->opaque);
    /* timer might be free after func(), don't touch */

    return FALSE;
}

static void timer_cancel(SpiceTimer *timer)
{
    if (timer->source) {
        g_source_destroy(timer->source);
        g_source_unref(timer->source);
        timer->source = NULL;
    }
}

static void timer_start(SpiceTimer *timer, uint32_t ms)
{
    timer_cancel(timer);

    timer->source = g_timeout_source_new(ms);

    g_source_set_callback(timer->source, timer_func, timer, NULL);

    g_source_attach(timer->source, g_main_context_default());

}

static void timer_remove(SpiceTimer *timer)
{
    timer_cancel(timer);
    free(timer);
}

struct SpiceWatch {
    void *opaque;
    GSource *source;
    GIOChannel *channel;
    SpiceWatchFunc func;
};

static GIOCondition spice_event_to_giocondition(int event_mask)
{
    GIOCondition condition = 0;

    if (event_mask & SPICE_WATCH_EVENT_READ)
        condition |= G_IO_IN;
    if (event_mask & SPICE_WATCH_EVENT_WRITE)
        condition |= G_IO_OUT;

    return condition;
}

static int giocondition_to_spice_event(GIOCondition condition)
{
    int event = 0;

    if (condition & G_IO_IN)
        event |= SPICE_WATCH_EVENT_READ;
    if (condition & G_IO_OUT)
        event |= SPICE_WATCH_EVENT_WRITE;

    return event;
}

static gboolean watch_func(GIOChannel *source, GIOCondition condition,
                           gpointer data)
{
    SpiceWatch *watch = data;
    int fd = g_io_channel_unix_get_fd(source);

    watch->func(fd, giocondition_to_spice_event(condition), watch->opaque);

    return TRUE;
}

static void watch_update_mask(SpiceWatch *watch, int event_mask)
{
    if (watch->source) {
        g_source_destroy(watch->source);
        g_source_unref(watch->source);
        watch->source = NULL;
    }

    if (!event_mask)
        return;

    watch->source = g_io_create_watch(watch->channel, spice_event_to_giocondition(event_mask));
    g_source_set_callback(watch->source, (GSourceFunc)watch_func, watch, NULL);
    g_source_attach(watch->source, g_main_context_default());
}

static SpiceWatch *watch_add(int fd, int event_mask, SpiceWatchFunc func, void *opaque)
{
    SpiceWatch *watch;

    watch = calloc(1, sizeof(SpiceWatch));
    watch->channel = g_io_channel_unix_new(fd);
    watch->func = func;
    watch->opaque = opaque;

    watch_update_mask(watch, event_mask);

    return watch;
}

static void watch_remove(SpiceWatch *watch)
{
    watch_update_mask(watch, 0);

    g_io_channel_unref(watch->channel);
    free(watch);
}

static void channel_event(int event, SpiceChannelEventInfo *info)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static void attach_worker(QXLInstance *qin, QXLWorker *qxl_worker)
{
    static int count = 0;
    spice_t *s = SPICE_CONTAINEROF(qin, spice_t, display_sin);

    if (++count > 1)
    {
        g_info("Ignoring worker %d", count);
        return;
    }

    // FIXME - spice_qxl_add_memslot(qin, &slot);
    // FIXME - do we ever need the worker?
    s->worker = qxl_worker;
}

static void set_compression_level(QXLInstance *qin, int level)
{
    spice_t *s = SPICE_CONTAINEROF(qin, spice_t, display_sin);
    // FIXME - compression level unused?
    s->compression_level = level;
}

// FIXME - deprecated?
static void set_mm_time(QXLInstance *qin, uint32_t mm_time)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static void get_init_info(QXLInstance *qin, QXLDevInitInfo *info)
{
    memset(info, 0, sizeof(*info));
    info->num_memslots = 1;
    info->num_memslots_groups = 1;
    info->memslot_id_bits = 1;
    info->memslot_gen_bits = 1;
    info->n_surfaces = 1;
    // FIXME - think about surface count, and no thoughtfulness on this
    // uint32_t qxl_ram_size;
    // uint8_t internal_groupslot_id;
}

static int get_command(QXLInstance *qin, struct QXLCommandExt *cmd)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static int req_cmd_notification(QXLInstance *qin)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static void release_resource(QXLInstance *qin, struct QXLReleaseInfoExt release_info)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static int get_cursor_command(QXLInstance *qin, struct QXLCommandExt *cmd)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static int req_cursor_notification(QXLInstance *qin)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static void notify_update(QXLInstance *qin, uint32_t update_id)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static int flush_resources(QXLInstance *qin)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static void async_complete(QXLInstance *qin, uint64_t cookie)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static void update_area_complete(QXLInstance *qin, uint32_t surface_id,
                                 struct QXLRect *updated_rects,
                                 uint32_t num_updated_rects)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static void set_client_capabilities(QXLInstance *qin,
                                    uint8_t client_present,
                                    uint8_t caps[SPICE_CAPABILITIES_SIZE])
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static int client_monitors_config(QXLInstance *qin,
                                  VDAgentMonitorsConfig *monitors_config)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

void initialize_spice_instance(spice_t *s)
{
    static int id = 0;

    static SpiceCoreInterface core = {
        .base = {
            .major_version = SPICE_INTERFACE_CORE_MAJOR,
            .minor_version = SPICE_INTERFACE_CORE_MINOR,
        },
        .timer_add = timer_add,
        .timer_start = timer_start,
        .timer_cancel = timer_cancel,
        .timer_remove = timer_remove,
        .watch_add = watch_add,
        .watch_update_mask = watch_update_mask,
        .watch_remove = watch_remove,
        .channel_event = channel_event
    };

    const static QXLInterface display_sif = {
        .base = {
            .type = SPICE_INTERFACE_QXL,
            .description = "x11spice qxl",
            .major_version = SPICE_INTERFACE_QXL_MAJOR,
            .minor_version = SPICE_INTERFACE_QXL_MINOR
        },
        .attache_worker = attach_worker,
        .set_compression_level = set_compression_level,
        .set_mm_time = set_mm_time,
        .get_init_info = get_init_info,

        /* the callbacks below are called from spice server thread context */
        .get_command = get_command,
        .req_cmd_notification = req_cmd_notification,
        .release_resource = release_resource,
        .get_cursor_command = get_cursor_command,
        .req_cursor_notification = req_cursor_notification,
        .notify_update = notify_update,
        .flush_resources = flush_resources,
        .async_complete = async_complete,
        .update_area_complete = update_area_complete,
        .client_monitors_config = client_monitors_config,
        .set_client_capabilities = set_client_capabilities,
    };

    s->core = &core;
    s->display_sin.base.sif = &display_sif.base;
    s->display_sin.id = id++;

    // FIXME - this cast seems dubious to me
    s->display_sin.st = (struct QXLState*) s;
}

int spice_start(spice_t *s, options_t *options)
{
    s->server = spice_server_new();
    if (! s->server)
        return X11SPICE_ERR_SPICE_INIT_FAILED;

    initialize_spice_instance(s);

    if (spice_server_init(s->server, s->core) < 0)
    {
        spice_server_destroy(s->server);
        return X11SPICE_ERR_SPICE_INIT_FAILED;
    }

    if (spice_server_add_interface(s->server, &s->display_sin.base))
    {
        spice_server_destroy(s->server);
        return X11SPICE_ERR_SPICE_INIT_FAILED;
    }

    return 0;
}

void spice_end(spice_t *s)
{
    spice_server_destroy(s->server);
}

#if defined(HACK_REFERENCE)

void xspice_set_spice_server_options(OptionInfoPtr options)
{
    /* environment variables take precedense. If not then take
     * parameters from the config file. */
    int addr_flags;
    int len;
    spice_image_compression_t compression;
    spice_wan_compression_t wan_compr;
    int port = get_int_option(options, OPTION_SPICE_PORT, "XSPICE_PORT");
    int tls_port =
        get_int_option(options, OPTION_SPICE_TLS_PORT, "XSPICE_TLS_PORT");
    const char *password =
        get_str_option(options, OPTION_SPICE_PASSWORD, "XSPICE_PASSWORD");
    int disable_ticketing =
        get_bool_option(options, OPTION_SPICE_DISABLE_TICKETING, "XSPICE_DISABLE_TICKETING");
    const char *x509_dir =
        get_str_option(options, OPTION_SPICE_X509_DIR, "XSPICE_X509_DIR");
    int sasl = get_bool_option(options, OPTION_SPICE_SASL, "XSPICE_SASL");
    const char *x509_key_file_base =
        get_str_option(options, OPTION_SPICE_X509_KEY_FILE,
                       "XSPICE_X509_KEY_FILE");
    char *x509_key_file = NULL;
    const char *x509_cert_file_base =
        get_str_option(options, OPTION_SPICE_X509_CERT_FILE,
                       "XSPICE_X509_CERT_FILE");
    char *x509_cert_file = NULL;
    const char *x509_key_password =
        get_str_option(options, OPTION_SPICE_X509_KEY_PASSWORD,
                    "XSPICE_X509_KEY_PASSWORD");
    const char *tls_ciphers =
        get_str_option(options, OPTION_SPICE_TLS_CIPHERS,
                    "XSPICE_TLS_CIPHERS");
    const char *x509_cacert_file_base =
        get_str_option(options, OPTION_SPICE_CACERT_FILE,
                    "XSPICE_CACERT_FILE");
    char *x509_cacert_file = NULL;
    const char * addr =
        get_str_option(options, OPTION_SPICE_ADDR, "XSPICE_ADDR");
    int ipv4 =
        get_bool_option(options, OPTION_SPICE_IPV4_ONLY, "XSPICE_IPV4_ONLY");
    int ipv6 =
        get_bool_option(options, OPTION_SPICE_IPV6_ONLY, "XSPICE_IPV6_ONLY");
    const char *x509_dh_file =
        get_str_option(options, OPTION_SPICE_DH_FILE, "XSPICE_DH_FILE");
    int disable_copy_paste =
        get_bool_option(options, OPTION_SPICE_DISABLE_COPY_PASTE,
                        "XSPICE_DISABLE_COPY_PASTE");
    int exit_on_disconnect =
        get_bool_option(options, OPTION_SPICE_EXIT_ON_DISCONNECT,
                        "XSPICE_EXIT_ON_DISCONNECT");
    const char *image_compression =
        get_str_option(options, OPTION_SPICE_IMAGE_COMPRESSION,
                       "XSPICE_IMAGE_COMPRESSION");
    const char *jpeg_wan_compression =
        get_str_option(options, OPTION_SPICE_JPEG_WAN_COMPRESSION,
                       "XSPICE_JPEG_WAN_COMPRESSION");
    const char *zlib_glz_wan_compression =
        get_str_option(options, OPTION_SPICE_ZLIB_GLZ_WAN_COMPRESSION,
                       "XSPICE_ZLIB_GLZ_WAN_COMPRESSION");
    const char *streaming_video =
        get_str_option(options, OPTION_SPICE_STREAMING_VIDEO,
                       "XSPICE_STREAMING_VIDEO");
    const char *video_codecs =
        get_str_option(options, OPTION_SPICE_VIDEO_CODECS,
                       "XSPICE_VIDEO_CODECS");
    int agent_mouse =
        get_bool_option(options, OPTION_SPICE_AGENT_MOUSE,
                        "XSPICE_AGENT_MOUSE");
    int playback_compression =
        get_bool_option(options, OPTION_SPICE_PLAYBACK_COMPRESSION,
                        "XSPICE_PLAYBACK_COMPRESSION");

    SpiceServer *spice_server = xspice_get_spice_server();

    if (!port && !tls_port) {
        printf("one of tls-port or port must be set\n");
        exit(1);
    }
    printf("xspice: port = %d, tls_port = %d\n", port, tls_port);
    if (disable_ticketing) {
        spice_server_set_noauth(spice_server);
    }
    if (tls_port) {
        if (NULL == x509_dir) {
            x509_dir = ".";
        }
        len = strlen(x509_dir) + 32;

        if (x509_key_file_base) {
            x509_key_file = strdup(x509_key_file_base);
        } else {
            x509_key_file = malloc(len);
            snprintf(x509_key_file, len, "%s/%s", x509_dir, X509_SERVER_KEY_FILE);
        }

        if (x509_cert_file_base) {
            x509_cert_file = strdup(x509_cert_file_base);
        } else {
            x509_cert_file = malloc(len);
            snprintf(x509_cert_file, len, "%s/%s", x509_dir, X509_SERVER_CERT_FILE);
        }

        if (x509_cacert_file_base) {
            x509_cacert_file = strdup(x509_cert_file_base);
        } else {
            x509_cacert_file = malloc(len);
            snprintf(x509_cacert_file, len, "%s/%s", x509_dir, X509_CA_CERT_FILE);
        }
    }

    addr_flags = 0;
    if (ipv4) {
        addr_flags |= SPICE_ADDR_FLAG_IPV4_ONLY;
    } else if (ipv6) {
        addr_flags |= SPICE_ADDR_FLAG_IPV6_ONLY;
    }

    spice_server_set_addr(spice_server, addr ? addr : "", addr_flags);
    if (port) {
        spice_server_set_port(spice_server, port);
    }
    if (tls_port) {
        spice_server_set_tls(spice_server, tls_port,
                             x509_cacert_file,
                             x509_cert_file,
                             x509_key_file,
                             x509_key_password,
                             x509_dh_file,
                             tls_ciphers);
    }
    if (password) {
        spice_server_set_ticket(spice_server, password, 0, 0, 0);
    }
    if (sasl) {
#if SPICE_SERVER_VERSION >= 0x000802 /* 0.8.2 */
        if (spice_server_set_sasl_appname(spice_server, "xspice") == -1 ||
            spice_server_set_sasl(spice_server, 1) == -1) {
            fprintf(stderr, "spice: failed to enable sasl\n");
            exit(1);
        }
#else
        fprintf(stderr, "spice: sasl is not available (spice >= 0.8.2 required)\n");
        exit(1);
#endif
    }

#if SPICE_SERVER_VERSION >= 0x000801
    /* we still don't actually support agent in xspice, but this
     * can't hurt for later, just copied from qemn/ui/spice-core.c */
    if (disable_copy_paste) {
        spice_server_set_agent_copypaste(spice_server, 0);
    }
#endif

    if (exit_on_disconnect) {
#if SPICE_SERVER_VERSION >= 0x000b04 /* 0.11.4 */
        spice_server_set_exit_on_disconnect(spice_server, exit_on_disconnect);
#else
        fprintf(stderr, "spice: cannot set exit_on_disconnect (spice >= 0.11.4 required)\n");
        exit(1);
#endif
    }

    compression = SPICE_IMAGE_COMPRESS_AUTO_GLZ;
    if (image_compression) {
        compression = parse_compression(image_compression);
    }
    spice_server_set_image_compression(spice_server, compression);

    wan_compr = SPICE_WAN_COMPRESSION_AUTO;
    if (jpeg_wan_compression) {
        wan_compr = parse_wan_compression(jpeg_wan_compression);
    }
    spice_server_set_jpeg_compression(spice_server, wan_compr);

    wan_compr = SPICE_WAN_COMPRESSION_AUTO;
    if (zlib_glz_wan_compression) {
        wan_compr = parse_wan_compression(zlib_glz_wan_compression);
    }
    spice_server_set_zlib_glz_compression(spice_server, wan_compr);

    if (streaming_video) {
        int streaming_video_opt = parse_stream_video(streaming_video);
        spice_server_set_streaming_video(spice_server, streaming_video_opt);
    }

    if (video_codecs) {
#if SPICE_SERVER_VERSION >= 0x000c06 /* 0.12.6 */
        if (spice_server_set_video_codecs(spice_server, video_codecs)) {
            fprintf(stderr, "spice: invalid video encoder %s\n", video_codecs);
            exit(1);
        }
#else
        fprintf(stderr, "spice: video_codecs are not available (spice >= 0.12.6 required)\n");
        exit(1);
#endif
    }

    spice_server_set_agent_mouse(spice_server, agent_mouse);
    spice_server_set_playback_compression(spice_server, playback_compression);

    free(x509_key_file);
    free(x509_cert_file);
    free(x509_cacert_file);
}

#endif



