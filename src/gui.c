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
**  gui.c
**      This file provides what modest user interface we have in x11spice.
**  The interface is all built using gtk, and all hand coded in this file.
**  This code does have the critical function of running the gtk main
**  loop, which is one of the main running threads of the x11spice program.
**--------------------------------------------------------------------------*/
#include "gui.h"
#include "session.h"
#include "x11spice.h"

gui_t *cached_gui;
void gui_sigterm(void)
{
    if (cached_gui)
        gtk_main_quit();
    cached_gui = NULL;
}

void gui_remote_connected(gui_t *gui, const char *details)
{
    gtk_label_set_text(GTK_LABEL(gui->status_label), "Connection established");
    gtk_widget_set_tooltip_text(gui->status_label, details);
    gtk_widget_set_sensitive(gui->disconnect_button, TRUE);
    if (gui->timeout_id > 0) {
        g_source_remove(gui->timeout_id);
        gui->timeout_id = 0;
    }
}

void gui_remote_disconnected(gui_t *gui)
{
    gtk_label_set_text(GTK_LABEL(gui->status_label), "Waiting for connection");
    gtk_widget_set_sensitive(gui->disconnect_button, FALSE);
}

void gui_disconnect_clicked(GtkWidget *widget G_GNUC_UNUSED, gpointer data)
{
    gui_t *gui = (gui_t *)data;
    session_disconnect_client(gui->session);
}

static gboolean timeout_if_no_connection(gpointer user_data G_GNUC_UNUSED)
{
    g_debug("Timeout waiting for connection.");
    gtk_main_quit();
    return FALSE;
}

int gui_create(gui_t *gui, session_t *session, int argc, char *argv[])
{
    if (!gtk_init_check(&argc, &argv))
        return X11SPICE_ERR_GTK_FAILED;

    gui->session = session;
    gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(gui->window, "destroy", G_CALLBACK(gui_sigterm), NULL);

    gui->button_box = gtk_vbutton_box_new();
    gtk_container_add(GTK_CONTAINER(gui->window), gui->button_box);

    gui->status_label = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(gui->button_box), gui->status_label);

    gui->disconnect_button = gtk_button_new_from_stock(GTK_STOCK_DISCONNECT);
    gtk_container_add(GTK_CONTAINER(gui->button_box), gui->disconnect_button);
    g_signal_connect(gui->disconnect_button, "clicked", G_CALLBACK(gui_disconnect_clicked), gui);
    gui_remote_disconnected(gui);

    gui->quit_button = gtk_button_new_from_stock(GTK_STOCK_QUIT);
    gtk_container_add(GTK_CONTAINER(gui->button_box), gui->quit_button);
    g_signal_connect_swapped(gui->quit_button, "clicked", G_CALLBACK(gtk_widget_destroy),
                             gui->window);

    gtk_widget_show(gui->status_label);
    gtk_widget_show(gui->disconnect_button);
    gtk_widget_show(gui->quit_button);
    gtk_widget_show(gui->button_box);

    if (!session->options.hide)
        gtk_widget_show(gui->window);
    if (session->options.minimize)
        gtk_window_iconify(GTK_WINDOW(gui->window));

    gui->timeout_id = 0;
    if (session->options.timeout)
        gui->timeout_id = g_timeout_add_seconds(session->options.timeout,
                                                timeout_if_no_connection, gui);

    return 0;
}

void gui_run(gui_t *gui)
{
    cached_gui = gui;
    gtk_main();
    cached_gui = NULL;
}

void gui_destroy(gui_t *gui)
{
    /* gtk destroys these windows on exit */
    gui->window = NULL;
}

void gui_report_error(gui_t *gui, const char *message)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW(gui->window),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_CLOSE,
                message);

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

#if defined(GUI_MAIN)
#include <locale.h>
#include <string.h>
void session_disconnect_client(session_t *session)
{
}

int main(int argc, char *argv[])
{
    gui_t gui;
    session_t session;

    setlocale(LC_ALL, "");
    memset(&session, 0, sizeof(session));
    gui_create(&gui, &session, argc, argv);
    gui_run(&gui);
    gui_destroy(&gui);
}
#endif
