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

#include "gui.h"
#include "x11spice.h"

int gui_create(gui_t *gui, int argc, char *argv[])
{
    if (! gtk_init_check(&argc, &argv))
        return X11SPICE_ERR_GTK_FAILED;

    gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(gui->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show(gui->window);

    return 0;
}

void gui_run(gui_t *gui)
{
    gtk_main();
}

void gui_destroy(gui_t *gui)
{
    if (gui->window)
        gtk_widget_destroy(gui->window);
    gui->window = NULL;
}
