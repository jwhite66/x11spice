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
**  scan.c
**      This code is meant to handle the logic around knowing which portions
**  of the display to push along to the spice server.  There are two key ways
**  we detect these changes - through periodic scans of the screen
**  (see scanner_periodic) and through XDAMAGE reports
**  (see display/handle_damage_notify)
**--------------------------------------------------------------------------*/

#include <stdlib.h>
#include <pthread.h>
#include <glib.h>
#include <pixman.h>

#include "x11spice.h"
#include "session.h"
#include "scan.h"

/*----------------------------------------------------------------------------
**  We will scan over the screen by breaking it into a grid of tiles, each
**   NUM_SCANLINES x NUM_HORIZONTAL_TILES.  We try to scan in a fashion designed
**   to catch changes with a fairly modest set of scans; this scan pattern is
**   taken from the x11vnc project.
**--------------------------------------------------------------------------*/
#define NUM_SCANLINES               32
#define NUM_HORIZONTAL_TILES        NUM_SCANLINES
#define MAX_SCAN_FPS                30
#define MIN_SCAN_FPS                 1

/* If we have more than this number of changes in any given row, we just
   copy the whole row */
#define SCAN_ROW_THRESHOLD          (NUM_HORIZONTAL_TILES / 2)

static int scanlines[NUM_SCANLINES] = {
    0, 16, 8, 24, 4, 20, 12, 28,
    10, 26, 18, 2, 22, 6, 30, 14,
    1, 17, 9, 25, 7, 23, 15, 31,
    19, 3, 27, 11, 29, 13, 5, 21
};


static QXLDrawable *shm_image_to_drawable(spice_t *s, shm_image_t *shmi, int x, int y)
{
    QXLDrawable *drawable;
    QXLImage *qxl_image;
    int i;

    drawable = calloc(1, sizeof(*drawable) + sizeof(*qxl_image));
    if (!drawable)
        return NULL;
    qxl_image = (QXLImage *) (drawable + 1);

    drawable->release_info.id = (uint64_t) spice_create_release(s, RELEASE_SHMI, shmi);
    shmi->drawable_ptr = drawable;

    drawable->surface_id = 0;
    drawable->type = QXL_DRAW_COPY;
    drawable->effect = QXL_EFFECT_OPAQUE;
    drawable->clip.type = SPICE_CLIP_TYPE_NONE;
    drawable->bbox.left = x;
    drawable->bbox.top = y;
    drawable->bbox.right = x + shmi->w;
    drawable->bbox.bottom = y + shmi->h;

    for (i = 0; i < 3; ++i)
        drawable->surfaces_dest[i] = -1;

    drawable->u.copy.src_area.left = 0;
    drawable->u.copy.src_area.top = 0;
    drawable->u.copy.src_area.right = shmi->w;
    drawable->u.copy.src_area.bottom = shmi->h;
    drawable->u.copy.rop_descriptor = SPICE_ROPD_OP_PUT;

    drawable->u.copy.src_bitmap = (QXLPHYSICAL) qxl_image;

    qxl_image->descriptor.id = 0;
    qxl_image->descriptor.type = SPICE_IMAGE_TYPE_BITMAP;

    qxl_image->descriptor.flags = 0;
    qxl_image->descriptor.width = shmi->w;
    qxl_image->descriptor.height = shmi->h;

    qxl_image->bitmap.format = SPICE_BITMAP_FMT_RGBA;
    qxl_image->bitmap.flags = SPICE_BITMAP_FLAGS_TOP_DOWN | QXL_BITMAP_DIRECT;
    qxl_image->bitmap.x = shmi->w;
    qxl_image->bitmap.y = shmi->h;
    qxl_image->bitmap.stride = shmi->bytes_per_line;
    qxl_image->bitmap.palette = 0;
    qxl_image->bitmap.data = (QXLPHYSICAL) shmi->shmaddr;

    return drawable;
}

static guint64 get_timeout(scanner_t *scanner)
{
    return G_USEC_PER_SEC / scanner->target_fps / NUM_SCANLINES;
}

static void scan_update_fps(scanner_t *scanner, int increment)
{
    scanner->target_fps += increment;
    if (scanner->target_fps > MAX_SCAN_FPS)
        scanner->target_fps = MAX_SCAN_FPS;

    if (scanner->target_fps < MIN_SCAN_FPS)
        scanner->target_fps = MIN_SCAN_FPS;
}

static void handle_scan_report(session_t *session, scan_report_t *r)
{
    shm_image_t *shmi;

    shmi = create_shm_image(&session->display, r->w, r->h);
    if (!shmi) {
        g_debug("Unexpected failure to create_shm_image of area %dx%d", r->w, r->h);
        return;
    }

    if (read_shm_image(&session->display, shmi, r->x, r->y) == 0) {
        //save_ximage_pnm(shmi);
        g_mutex_lock(session->lock);
        display_copy_image_into_fullscreen(&session->display, shmi, r->x, r->y);
        g_mutex_unlock(session->lock);

        QXLDrawable *drawable = shm_image_to_drawable(&session->spice, shmi, r->x, r->y);
        if (drawable) {
            g_async_queue_push(session->draw_queue, drawable);
            spice_qxl_wakeup(&session->spice.display_sin);
            /*
            **  NOTE: the shmi is intentionally not freed at this point.
            **        The call path will take care of that once it's been
            **        pushed to Spice.
            */
            return;
        }
        else
            g_debug("Unexpected failure to create drawable");
    }
    else
        g_debug("Unexpected failure to read shm of area %dx%d", r->w, r->h);

    if (shmi)
        destroy_shm_image(&session->display, shmi);
}


static void free_queue_item(gpointer data)
{
    free(data);
}

/* Note: session lock must be held by caller */
static void push_tiles_report(scanner_t *scanner, int start_row, int start_col, int end_row,
                              int end_col)
{
    int x = (scanner->session->display.fullscreen->w / NUM_HORIZONTAL_TILES) * start_col;
    int w = (scanner->session->display.fullscreen->w / NUM_HORIZONTAL_TILES) * (end_col - start_col + 1);

    int y = (scanner->session->display.fullscreen->h / NUM_SCANLINES) * start_row;
    int h = scanner->session->display.fullscreen->h / NUM_SCANLINES * (end_row - start_row + 1);

    if (x + w > scanner->session->display.fullscreen->w)
        w = scanner->session->display.fullscreen->w - x;

    if (y + h > scanner->session->display.fullscreen->h)
        h = scanner->session->display.fullscreen->h - y;

    scanner_push(scanner, SCANLINE_SCAN_REPORT, x, y, w, h);
}

static void grow_changed_tiles(scanner_t *scanner, int *tiles_changed_in_row,
                               int tiles_changed[][NUM_HORIZONTAL_TILES])
{
    int i;
    int j;
    for (i = 0; i < NUM_SCANLINES; i++) {
        if (!tiles_changed_in_row[i] || tiles_changed_in_row[i] == NUM_HORIZONTAL_TILES)
            continue;

        if (tiles_changed_in_row[i] > SCAN_ROW_THRESHOLD) {
            tiles_changed_in_row[i] = NUM_HORIZONTAL_TILES;
            continue;
        }

        for (j = 0; j < NUM_HORIZONTAL_TILES; j++) {
            if (!tiles_changed[i][j]) {
                int grow = 0;

                /* You get good optimzations from having multiple rows,
                   so be more aggressive in growing the first and last tile;
                   just require a neighbor be set */
                if (j == 0 && tiles_changed[i][1])
                    grow++;
                else if (j == NUM_HORIZONTAL_TILES - 1 && tiles_changed[i][j - 1])
                    grow++;

                /* Otherwise, require that growing 'fills' a gap */
                else if (j > 0 && j < (NUM_HORIZONTAL_TILES - 1) &&
                         tiles_changed[i][j - 1] && tiles_changed[i][j + 1])
                    grow++;

                if (grow) {
                    tiles_changed[i][j]++;
                    tiles_changed_in_row[i]++;
                }
            }
        }

        /* Recheck, in case our growth algorithm pushed this
           into the 'scan the whole row' category */
        if (tiles_changed_in_row[i] > SCAN_ROW_THRESHOLD)
            tiles_changed_in_row[i] = NUM_HORIZONTAL_TILES;
    }
}

static void push_changes_across_rows(scanner_t *scanner, int *tiles_changed_in_row)
{
    int i = 0;
    int start_row = -1;
    int current_row = -1;

    for (i = 0; i < NUM_SCANLINES; i++) {
        if (tiles_changed_in_row[i] == NUM_HORIZONTAL_TILES) {
            if (start_row == -1)
                start_row = i;
            current_row = i;
        }
        else {
            if (current_row != -1) {
                push_tiles_report(scanner, start_row, 0, current_row, NUM_HORIZONTAL_TILES - 1);
                start_row = current_row = -1;
            }
            continue;
        }
    }

    if (current_row != -1)
        push_tiles_report(scanner, start_row, 0, current_row, NUM_HORIZONTAL_TILES - 1);
}

static void push_changes_in_one_row(scanner_t *scanner, int row, int *tiles_changed)
{
    int i = 0;
    int start_tile = -1;
    int current_tile = -1;

    for (i = 0; i < NUM_HORIZONTAL_TILES; i++) {
        if (tiles_changed[i] == 0) {
            if (current_tile != -1) {
                push_tiles_report(scanner, row, start_tile, row, current_tile);
                start_tile = current_tile = -1;
            }
            continue;
        }
        if (start_tile == -1)
            start_tile = i;
        current_tile = i;
    }

    if (current_tile != -1)
        push_tiles_report(scanner, row, start_tile, row, current_tile);
}

static void push_changed_tiles(scanner_t *scanner, int *tiles_changed_in_row,
                               int tiles_changed[][NUM_HORIZONTAL_TILES])
{
    int i = 0;

    push_changes_across_rows(scanner, tiles_changed_in_row);

    for (i = 0; i < NUM_SCANLINES; i++)
        if (tiles_changed_in_row[i] > 0 && tiles_changed_in_row[i] < NUM_HORIZONTAL_TILES)
            push_changes_in_one_row(scanner, i, tiles_changed[i]);
}


static void scanner_remove_region(scanner_t *scanner, scan_report_t *r)
{
    pixman_region16_t remove;
    pixman_region_init_rect(&remove, r->x, r->y, r->w, r->h);

    g_mutex_lock(scanner->lock);
    pixman_region_subtract(&scanner->region, &scanner->region, &remove);
    g_mutex_unlock(scanner->lock);

    pixman_region_clear(&remove);
}

static void scanner_periodic(scanner_t *scanner)
{
    int i;
    int tiles_changed_in_row[NUM_SCANLINES];
    int tiles_changed[NUM_SCANLINES][NUM_HORIZONTAL_TILES];
    int h;
    int y;
    int offset;
    int rc;

    g_mutex_lock(scanner->session->lock);
    h = scanner->session->display.fullscreen->h / NUM_SCANLINES;

    offset = scanlines[scanner->current_scanline++];
    scanner->current_scanline %= NUM_SCANLINES;

    for (y = offset, i = 0; i < NUM_SCANLINES; i++, y += h) {
        if (y >= scanner->session->display.fullscreen->h)
            y = scanner->session->display.fullscreen->h - 1;

        rc = display_find_changed_tiles(&scanner->session->display,
                                        y, tiles_changed[i], NUM_HORIZONTAL_TILES);
        if (rc < 0) {
            g_mutex_unlock(scanner->session->lock);
            return;
        }

        tiles_changed_in_row[i] = rc;
    }
    grow_changed_tiles(scanner, tiles_changed_in_row, tiles_changed);
    push_changed_tiles(scanner, tiles_changed_in_row, tiles_changed);

    g_mutex_unlock(scanner->session->lock);
}

#if ! GLIB_CHECK_VERSION(2, 31, 18)
static gpointer g_async_queue_timeout_pop(GAsyncQueue *queue, guint64 t)
{
    GTimeVal end;
    g_get_current_time(&end);
    g_time_val_add(&end, t);
    return g_async_queue_timed_pop(queue, &end);
}
#endif

static void *scanner_run(void *opaque)
{
    scanner_t *scanner = (scanner_t *) opaque;
    while (session_alive(scanner->session)) {
        scan_report_t *r;
        r = (scan_report_t *) g_async_queue_timeout_pop(scanner->queue, get_timeout(scanner));
        if (!r) {
            scan_update_fps(scanner, -1);
            scanner_periodic(scanner);
            continue;
        }
        scan_update_fps(scanner, 1);

        if (r->type == EXIT_SCAN_REPORT) {
            free_queue_item(r);
            break;
        }

        scanner_remove_region(scanner, r);

        handle_scan_report(scanner->session, r);
        free_queue_item(r);
    }

    return 0;
}


int scanner_create(scanner_t *scanner)
{
    scanner->queue = g_async_queue_new_full(free_queue_item);
    scanner->lock = g_mutex_new();
    scanner->current_scanline = 0;
    pixman_region_init(&scanner->region);
    scanner->target_fps = MIN_SCAN_FPS;
    return pthread_create(&scanner->thread, NULL, scanner_run, scanner);
}

int scanner_destroy(scanner_t *scanner)
{
    void *err;
    int rc;

    scanner_push(scanner, EXIT_SCAN_REPORT, 0, 0, 0, 0);
    rc = pthread_join(scanner->thread, &err);
    if (rc == 0)
        rc = (int) (long) err;

    g_mutex_lock(scanner->lock);
    if (scanner->queue) {
        g_async_queue_unref(scanner->queue);
        scanner->queue = NULL;
    }
    pixman_region_clear(&scanner->region);

    g_mutex_unlock(scanner->lock);
    g_mutex_free(scanner->lock);
    scanner->lock = NULL;

    return rc;
}

int scanner_push(scanner_t *scanner, scan_type_t type, int x, int y, int w, int h)
{
    int rc = X11SPICE_ERR_MALLOC;
    scan_report_t *r = malloc(sizeof(*r));

    if (r) {
        r->type = type;
        r->x = x;
        r->y = y;
        r->w = w;
        r->h = h;

        g_mutex_lock(scanner->lock);
        if (scanner->queue) {
            pixman_box16_t rect;
            rect.x1 = x;
            rect.x2 = x + w;
            rect.y1 = y;
            rect.y2 = y + h;

            if (!pixman_region_contains_rectangle(&scanner->region, &rect)) {
                pixman_region_union_rect(&scanner->region, &scanner->region, x, y, w, h);

                g_async_queue_push(scanner->queue, r);
            }
            else {
                free(r);
            }
            rc = 0;
        }
        else {
            free(r);
            rc = X11SPICE_ERR_SHUTTING_DOWN;
        }
        g_mutex_unlock(scanner->lock);
    }

#if defined(DEBUG_SCANLINES)
    fprintf(stderr, "scan: %dx%d @ %dx%d\n", w, h, x, y);
    fflush(stderr);
#endif

    return rc;
}
