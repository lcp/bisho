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

#ifndef __BISHO_WEBKIT_H__
#define __BISHO_WEBKIT_H__

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "bisho-pane.h"

G_BEGIN_DECLS

typedef struct {
  BishoPane *pane;
  GtkWidget *main_window;
  WebKitWebView *web_view;
  char *main_title;
  int load_progress;
  char *stop_url;
  char *session_url;
  void (*session_handler)(gpointer);
} BrowserInfo;

void bisho_webkit_open_url (GdkScreen *screen, BrowserInfo *info, const char* url);

G_BEGIN_DECLS

#endif /* __BISHO_WEBKIT_H__ */
