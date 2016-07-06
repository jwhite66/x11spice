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
#include <stdio.h>
#include <string.h>

#include "local_spice.h"
#include "x11spice.h"
#include "display.h"
#include "session.h"
#include "auto.h"

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
    /* This event, as far as I can tell, is fairly useless.
       You don't get any real information in the pointer, so
       it's not a good opportunity to act on connect/disconnect,
       for example */
    g_debug("channel event [connection_id %d|type %d|id %d]",
        info->connection_id, info->type, info->id);
}

static void attach_worker(QXLInstance *qin, QXLWorker *qxl_worker)
{
    static int count = 0;
    spice_t *s = SPICE_CONTAINEROF(qin, spice_t, display_sin);

    static QXLDevMemSlot slot = {
        .slot_group_id = 0,
        .slot_id = 0,
        .generation = 0,
        .virt_start = 0,
        .virt_end = ~0,
        .addr_delta = 0,
        .qxl_ram_size = ~0,
        };

    if (++count > 1)
    {
        g_message("Ignoring worker %d", count);
        return;
    }

    spice_qxl_add_memslot(qin, &slot);
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
    spice_t *s = SPICE_CONTAINEROF(qin, spice_t, display_sin);
    QXLDrawable *drawable;

    drawable = session_pop_draw(s->session);
    if (! drawable)
        return 0;

    cmd->group_id = 0;
    cmd->flags = 0;
    cmd->cmd.type = QXL_CMD_DRAW;
    cmd->cmd.padding = 0;
    cmd->cmd.data = (QXLPHYSICAL) drawable;

    return 1;
}

static int req_cmd_notification(QXLInstance *qin)
{
    spice_t *s = SPICE_CONTAINEROF(qin, spice_t, display_sin);

    if (session_draw_waiting(s->session) > 0)
        return 0;

    return 1;
}

static void release_resource(QXLInstance *qin, struct QXLReleaseInfoExt release_info)
{
    spice_free_release((spice_release_t *) release_info.info->id);
}

static int get_cursor_command(QXLInstance *qin, struct QXLCommandExt *cmd)
{
    spice_t *s = SPICE_CONTAINEROF(qin, spice_t, display_sin);
    struct QXLCursorCmd *cursor;

    cursor = session_pop_cursor(s->session);
    if (! cursor)
        return 0;

    cmd->group_id = 0;
    cmd->flags = 0;
    cmd->cmd.type = QXL_CMD_CURSOR;
    cmd->cmd.padding = 0;
    cmd->cmd.data = (QXLPHYSICAL) cursor;

    return 1;
}

static int req_cursor_notification(QXLInstance *qin)
{
    spice_t *s = SPICE_CONTAINEROF(qin, spice_t, display_sin);

    if (session_cursor_waiting(s->session) > 0)
        return 0;

    return 1;
}

static void notify_update(QXLInstance *qin, uint32_t update_id)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static int flush_resources(QXLInstance *qin)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
    // Return 0 to direct the server to flush resources
    return 1;
}

static void async_complete(QXLInstance *qin, uint64_t cookie)
{
    g_debug("%s: cookie 0x%lx", __FUNCTION__, cookie);
    spice_free_release((spice_release_t *) cookie);
}

static void update_area_complete(QXLInstance *qin, uint32_t surface_id,
                                 struct QXLRect *updated_rects,
                                 uint32_t num_updated_rects)
{
    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
}

static int client_monitors_config(QXLInstance *qin,
                                  VDAgentMonitorsConfig *monitors_config)
{
    int i;
    if (! monitors_config)
    {
        /* a NULL is used as a test to see if we support this function */
        g_debug("%s: NULL monitors_config", __func__);
        return TRUE;
    }

    g_debug("%s: [num %d|flags 0x%x]", __func__, monitors_config->num_of_monitors, monitors_config->flags);
    for (i = 0; i < monitors_config->num_of_monitors; i++)
        g_debug("  %d:[height %d|width %d|depth %d|x %d|y %d]", i,
            monitors_config->monitors[i].height,
            monitors_config->monitors[i].width,
            monitors_config->monitors[i].depth,
            monitors_config->monitors[i].x,
            monitors_config->monitors[i].y);

    g_debug("FIXME! UNIMPLEMENTED! %s", __func__);
    return FALSE;
}

/* spice sends AT scancodes (with a strange escape).
 * But xf86PostKeyboardEvent expects scancodes. Apparently most of the time
 * you just need to add MIN_KEYCODE, see xf86-input-keyboard/src/atKeynames
 * and xf86-input-keyboard/src/kbd.c:PostKbdEvent:
 *   xf86PostKeyboardEvent(device, scanCode + MIN_KEYCODE, down); */
#define MIN_KEYCODE     8

static uint8_t escaped_map[256] = {
    [0x1c] = 104, //KEY_KP_Enter,
    [0x1d] = 105, //KEY_RCtrl,
    [0x2a] = 0,//KEY_LMeta, // REDKEY_FAKE_L_SHIFT
    [0x35] = 106,//KEY_KP_Divide,
    [0x36] = 0,//KEY_RMeta, // REDKEY_FAKE_R_SHIFT
    [0x37] = 107,//KEY_Print,
    [0x38] = 108,//KEY_AltLang,
    [0x46] = 127,//KEY_Break,
    [0x47] = 110,//KEY_Home,
    [0x48] = 111,//KEY_Up,
    [0x49] = 112,//KEY_PgUp,
    [0x4b] = 113,//KEY_Left,
    [0x4d] = 114,//KEY_Right,
    [0x4f] = 115,//KEY_End,
    [0x50] = 116,//KEY_Down,
    [0x51] = 117,//KEY_PgDown,
    [0x52] = 118,//KEY_Insert,
    [0x53] = 119,//KEY_Delete,
    [0x5b] = 133,//0, // REDKEY_LEFT_CMD,
    [0x5c] = 134,//0, // REDKEY_RIGHT_CMD,
    [0x5d] = 135,//KEY_Menu,
};

static void kbd_push_key(SpiceKbdInstance *sin, uint8_t frag)
{
    spice_t *s = SPICE_CONTAINEROF(sin, spice_t, keyboard_sin);
    int is_down;

    if (frag == 224) {
        s->escape = frag;
        return;
    }
    is_down = frag & 0x80 ? FALSE : TRUE;
    frag = frag & 0x7f;
    if (s->escape == 224) {
        s->escape = 0;
        if (escaped_map[frag] == 0) {
            g_warning("spiceqxl_inputs.c: kbd_push_key: escaped_map[%d] == 0", frag);
        }
        frag = escaped_map[frag];
    } else {
        frag += MIN_KEYCODE;
    }

    session_handle_key(s->session, frag, is_down);
}

static uint8_t kbd_get_leds(SpiceKbdInstance *sin)
{
    spice_t *s = SPICE_CONTAINEROF(sin, spice_t, keyboard_sin);
    uint8_t ret = 0;

    if (session_get_one_led(s->session, "Caps Lock"))
        ret |= SPICE_KEYBOARD_MODIFIER_FLAGS_CAPS_LOCK;
    if (session_get_one_led(s->session, "Scroll Lock"))
        ret |= SPICE_KEYBOARD_MODIFIER_FLAGS_SCROLL_LOCK;
    if (session_get_one_led(s->session, "Num Lock"))
        ret |= SPICE_KEYBOARD_MODIFIER_FLAGS_NUM_LOCK;

    return ret;
}

void tablet_set_logical_size(SpiceTabletInstance* tablet, int width, int height)
{
    g_debug("FIXME! UNIMPLEMENTED! %s (width %dx%d)", __func__, width, height);
}

void tablet_position(SpiceTabletInstance* tablet, int x, int y, uint32_t buttons_state)
{
    spice_t *s = SPICE_CONTAINEROF(tablet, spice_t, tablet_sin);
    session_handle_mouse_position(s->session, x, y, buttons_state);
}

void tablet_wheel(SpiceTabletInstance* tablet, int wheel_motion, uint32_t buttons_state)
{
    spice_t *s = SPICE_CONTAINEROF(tablet, spice_t, tablet_sin);
    session_handle_mouse_wheel(s->session, wheel_motion, buttons_state);
}

void tablet_buttons(SpiceTabletInstance* tablet, uint32_t buttons_state)
{
    spice_t *s = SPICE_CONTAINEROF(tablet, spice_t, tablet_sin);
    session_handle_mouse_buttons(s->session, buttons_state);
}

static int send_monitors_config(spice_t *s, int w, int h)
{
    spice_release_t *release;

    QXLMonitorsConfig *monitors = calloc(1, sizeof(QXLMonitorsConfig) + sizeof(QXLHead));
    if (! monitors)
        return X11SPICE_ERR_MALLOC;
    release = spice_create_release(s, RELEASE_MEMORY, monitors);

    monitors->count = 1;
    monitors->max_allowed = 1;
    monitors->heads[0].id = 0;
    monitors->heads[0].surface_id = 0;
    monitors->heads[0].width = w;
    monitors->heads[0].height = h;

    spice_qxl_monitors_config_async(&s->display_sin, (QXLPHYSICAL) monitors, 0, (uint64_t) release);

    return 0;
}

int spice_create_primary(spice_t *s, int w, int h, int bytes_per_line, void *shmaddr)
{
    QXLDevSurfaceCreate surface;

    memset(&surface, 0, sizeof(surface));
    surface.height     = h;
    surface.width      = w;
    // FIXME - negative stride?
    surface.stride     = bytes_per_line;
    surface.type       = QXL_SURF_TYPE_PRIMARY;
    surface.flags      = 0;
    surface.group_id   = 0;
    surface.mouse_mode = TRUE;

    // Position appears to be completely unused
    surface.position   = 0;

    // FIXME - compute this dynamically?
    surface.format     = SPICE_SURFACE_FMT_32_xRGB;
    surface.mem        = (QXLPHYSICAL) shmaddr;

    s->width = w;
    s->height = h;

    spice_qxl_create_primary_surface(&s->display_sin, 0, &surface);

    return send_monitors_config(s, w, h);
}

void spice_destroy_primary(spice_t *s)
{
    spice_qxl_destroy_primary_surface(&s->display_sin, 0);
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
        .set_client_capabilities = NULL, /* Allowed to be unset */
    };

    static const SpiceKbdInterface keyboard_sif = {
        .base.type          = SPICE_INTERFACE_KEYBOARD,
        .base.description   = "x11spice keyboard",
        .base.major_version = SPICE_INTERFACE_KEYBOARD_MAJOR,
        .base.minor_version = SPICE_INTERFACE_KEYBOARD_MINOR,
        .push_scan_freg     = kbd_push_key,
        .get_leds           = kbd_get_leds,
    };

    static const SpiceTabletInterface tablet_sif = {
        .base.type          = SPICE_INTERFACE_TABLET,
        .base.description   = "x11spice tablet",
        .base.major_version = SPICE_INTERFACE_TABLET_MAJOR,
        .base.minor_version = SPICE_INTERFACE_TABLET_MINOR,
        .set_logical_size   = tablet_set_logical_size,
        .position           = tablet_position,
        .wheel              = tablet_wheel,
        .buttons            = tablet_buttons,
    };

    s->core = &core;
    s->display_sin.base.sif = &display_sif.base;
    s->display_sin.id = id++;

    s->keyboard_sin.base.sif = &keyboard_sif.base;
    s->tablet_sin.base.sif = &tablet_sif.base;

}

static void set_options(spice_t *s, options_t *options)
{
    if (options->disable_ticketing)
        spice_server_set_noauth(s->server);

    if (! options->autouri)
    {
        spice_server_set_addr(s->server, options->spice_addr ?
                options->spice_addr : "", 0);
        if (options->spice_port)
            spice_server_set_port(s->server, options->spice_port);
    }

    if (options->spice_password)
        spice_server_set_ticket(s->server, options->spice_password, 0, 0, 0);

    spice_server_set_exit_on_disconnect(s->server, options->exit_on_disconnect);

}

static int try_auto(spice_t *s, options_t *options)
{
    int fd;
    fd = auto_listen(options->autouri);
    if (fd < 0)
        return X11SPICE_ERR_AUTO_FAILED;

    return spice_server_set_listen_socket_fd(s->server, fd);
}

int spice_start(spice_t *s, options_t *options, shm_image_t *fullscreen)
{
    int rc;

    memset(s, 0, sizeof(*s));

    s->server = spice_server_new();
    if (! s->server)
        return X11SPICE_ERR_SPICE_INIT_FAILED;

    initialize_spice_instance(s);

    set_options(s, options);

    if (options->autouri)
    {
        int rc = try_auto(s, options);
        if (rc)
            return rc;
    }

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

    if (spice_server_add_interface(s->server, &s->keyboard_sin.base))
    {
        spice_server_destroy(s->server);
        return X11SPICE_ERR_SPICE_INIT_FAILED;
    }

    if (spice_server_add_interface(s->server, &s->tablet_sin.base))
    {
        spice_server_destroy(s->server);
        return X11SPICE_ERR_SPICE_INIT_FAILED;
    }

    spice_server_vm_start(s->server);

    rc = spice_create_primary(s, fullscreen->w, fullscreen->h,
                                 fullscreen->bytes_per_line, fullscreen->shmaddr);

    return rc;
}

void spice_end(spice_t *s)
{
    spice_server_remove_interface(&s->tablet_sin.base);
    spice_server_remove_interface(&s->keyboard_sin.base);
    spice_server_remove_interface(&s->display_sin.base);

    spice_server_destroy(s->server);

    // FIXME - can't always destroy...
    spice_destroy_primary(s);
}

spice_release_t *spice_create_release(spice_t *s, release_type_t type, void *data)
{
    spice_release_t *r = malloc(sizeof(*r));
    if (r)
    {
        r->s = s;
        r->type = type;
        r->data = data;
    }

    return r;
}

void spice_free_release(spice_release_t *r)
{
    if (!r)
        return;

    switch (r->type)
    {
        case RELEASE_SHMI:
            destroy_shm_image(&r->s->session->display, (shm_image_t *) r->data);
            break;

        case RELEASE_MEMORY:
            free(r->data);
            break;
    }

    free(r);
}
