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