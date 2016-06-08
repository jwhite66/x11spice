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

#include "x11spice.h"
#include "session.h"
#include "scan.h"

#include <stdlib.h>
#include <pthread.h>
#include <glib.h>

// FIXME - refactor and move this...
static QXLDrawable *shm_image_to_drawable(shm_image_t *shmi, int x, int y)
{
    QXLDrawable *drawable;
    QXLImage *qxl_image;
    int i;

    drawable = calloc(1, sizeof(*drawable) + sizeof(*qxl_image));
    if (! drawable)
        return NULL;
    qxl_image = (QXLImage *) (drawable + 1);

    drawable->release_info.id = (uint64_t) shmi;
    shmi->drawable_ptr = drawable;

    drawable->surface_id = 0;
    drawable->type = QXL_DRAW_COPY;
    drawable->effect = QXL_EFFECT_OPAQUE;
    drawable->clip.type = SPICE_CLIP_TYPE_NONE;
    drawable->bbox.left = x;
    drawable->bbox.top = y;
    drawable->bbox.right = x + shmi->w;
    drawable->bbox.bottom = y + shmi->h;

    /*
     * surfaces_dest[i] should apparently be filled out with the
     * surfaces that we depend on, and surface_rects should be
     * filled with the rectangles of those surfaces that we
     * are going to use.
     *  FIXME - explore this instead of blindly copying...
     */
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

    // FIXME - be a bit more dynamic...
    qxl_image->bitmap.format = SPICE_BITMAP_FMT_RGBA;
    qxl_image->bitmap.flags = SPICE_BITMAP_FLAGS_TOP_DOWN | QXL_BITMAP_DIRECT;
    qxl_image->bitmap.x = shmi->w;
    qxl_image->bitmap.y = shmi->h;
    qxl_image->bitmap.stride = shmi->bytes_per_line;
    qxl_image->bitmap.palette = 0;
    qxl_image->bitmap.data = (QXLPHYSICAL) shmi->shmaddr;

    // FIXME - cache images at all?

    return drawable;
}

static guint64 get_timeout(scanner_t *scanner)
{
    // FIXME - make this a bit smarter...
    
    return G_USEC_PER_SEC / 30;
}

static void save_ximage_pnm(shm_image_t *shmi)
{
    int x,y;
    guint32 *pixel;
    static int count = 0;
    char fname[200];
    FILE *fp;
    sprintf(fname, "ximage%04d.ppm", count++);
    fp = fopen(fname, "w");

    pixel = (guint32 *) shmi->shmaddr;

    fprintf(fp,"P3\n%d %d\n255\n", shmi->w, shmi->h);
    for (y=0; y<shmi->h; y++)
    {
        for (x=0; x<shmi->w; x++)
        {
            fprintf(fp,"%u %u %u\n",
                ((*pixel)&0x0000ff)>>0,
                ((*pixel)&0x00ff00)>>8,
                ((*pixel)&0xff0000)>>16
                );
            pixel++;
        }
    }
    fclose(fp);
}


static void handle_damage_report(session_t *session, scan_report_t *r)
{
    shm_image_t *shmi;

    shmi = create_shm_image(&session->display, r->w, r->h);
    if (!shmi)
    {
        g_debug("Unexpected failure to create_shm_image of area %dx%d", r->w, r->h);
        return;
    }

    if (read_shm_image(&session->display, shmi, r->x, r->y) == 0)
    {
        //save_ximage_pnm(shmi);
        QXLDrawable *drawable = shm_image_to_drawable(shmi, r->x, r->y);
        if (drawable)
        {
            g_async_queue_push(session->draw_queue, drawable);
            spice_qxl_wakeup(&session->spice.display_sin);
            // FIXME - Note that shmi is not cleaned up at this point
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


static void * scanner_run(void *opaque)
{
    scanner_t *scanner = (scanner_t *) opaque;
    while (1)
    {
        scan_report_t *r;
        r = (scan_report_t *) g_async_queue_timeout_pop(scanner->queue, get_timeout(scanner));
        if (! r)
            continue;

        switch(r->type)
        {
            case EXIT_SCAN_REPORT:
                return 0;

            case DAMAGE_SCAN_REPORT:
                handle_damage_report(scanner->session, r);
                break;

            // FIXME - implement hint + scan
        }
    }

    return 0;
}

static void free_queue_item(gpointer data)
{
    free(data);
    // FIXME - test this...
}

int scanner_create(scanner_t *scanner)
{
    scanner->queue = g_async_queue_new_full(free_queue_item);
    // FIXME - gthread?
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

    if (scanner->queue)
    {
        g_async_queue_unref(scanner->queue);
        scanner->queue = NULL;
    }

    return rc;
}

int scanner_push(scanner_t *scanner, scan_type_t type, int x, int y, int w, int h)
{
    scan_report_t *r = malloc(sizeof(*r));
    if (r)
    {
        r->type = type;
        r->x = x;
        r->y = y;
        r->w = w;
        r->h = h;
        g_async_queue_push(scanner->queue, r);
        return 0;
    }
    return X11SPICE_ERR_MALLOC;
}

