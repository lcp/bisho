/*
 * Copyright (C) 2009 Novell Inc.
 *
 * Author: Gary Ching-Pang Lin <glin@novell.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "bisho-webkit.h"

static void
update_title (GtkWindow* window, BrowserInfo *info)
{
  GString* string = g_string_new (info->main_title);
  if (info->load_progress < 100)
    g_string_append_printf (string, " (%d%%)", info->load_progress);
  gchar* title = g_string_free (string, FALSE);
  gtk_window_set_title (window, title);
  g_free (title);
}

static void
title_change_cb (WebKitWebView* page, WebKitWebFrame* web_frame, const gchar* title, gpointer data)
{
  BrowserInfo *info = (BrowserInfo*) data;
  if (info->main_title)
      g_free (info->main_title);
  info->main_title = g_strdup (title);
  update_title (GTK_WINDOW (info->main_window), info);
}

static void
progress_change_cb (WebKitWebView* page, gint progress, gpointer data)
{
  BrowserInfo *info = (BrowserInfo*) data;
  info->load_progress = progress;
  update_title (GTK_WINDOW (info->main_window), info);
}

static void
load_commit_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data)
{
  BrowserInfo *info = (BrowserInfo*) data;
  const gchar* uri = webkit_web_frame_get_uri(frame);

  if (g_strrstr (uri, info->stop_url)){
    webkit_web_view_stop_loading (page);
    gtk_widget_hide (GTK_WIDGET (info->main_window));
    if (info->session_handler != NULL){
        info->session_url = uri;
        info->session_handler (info);
    }
  }
}

static GtkWidget*
create_browser (BrowserInfo *info)
{
  GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  WebKitWebView *web_view;
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  info->web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  web_view = info->web_view;
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

  g_signal_connect (G_OBJECT (web_view), "title-changed", G_CALLBACK (title_change_cb), info);
  g_signal_connect (G_OBJECT (web_view), "load-progress-changed", G_CALLBACK (progress_change_cb), info);
  g_signal_connect (G_OBJECT (web_view), "load-committed", G_CALLBACK (load_commit_cb), info);

  return scrolled_window;
}

static GtkWidget*
create_window (GdkScreen *screen)
{
  GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 500);
  gtk_widget_set_name (window, "AuthBrowser");
  gtk_window_set_screen (window, screen);
  /* Set modal so the browser window will show in the same zone of the main window */
  gtk_window_set_modal (window, TRUE);

  return window;
}

/* Create an authentication browser */
void
bisho_webkit_open_url (GdkScreen *screen, BrowserInfo *info, const char *url)
{
  GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), create_browser (info), TRUE, TRUE, 0);

  info->main_window = create_window (screen);
  gtk_container_add (GTK_CONTAINER (info->main_window), vbox);

  webkit_web_view_open (info->web_view, url);

  gtk_widget_grab_focus (GTK_WIDGET (info->web_view));
  gtk_widget_show_all (info->main_window);
}

