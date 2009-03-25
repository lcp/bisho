#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include "service-info.h"

static GConfClient *gconf = NULL;

static void
set_gconf_key (ServiceInfo *info, const char *key_suffix, const char *value)
{
  char *key;

  key = g_strdup_printf ("/apps/mojito/services/%s/%s",
                         info->name, key_suffix);

  /* TODO: block gconf notify when we have one */
  gconf_client_set_string (gconf, key, value, NULL);

  g_free (key);
}

static gboolean
on_user_entry_left (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
  ServiceInfo *info = user_data;

  set_gconf_key (info, "user", gtk_entry_get_text (GTK_ENTRY (widget)));

  return FALSE;
}

static char *
get_gconf_key (ServiceInfo *info, const char *key_suffix)
{
  char *key, *value;
  GError *error = NULL;

  key = g_strdup_printf ("/apps/mojito/services/%s/%s",
                         info->name, key_suffix);

  value = gconf_client_get_string (gconf, key, &error);
  if (error) {
    g_message ("Cannot get key %s: %s", key, error->message);
    g_error_free (error);
  }

  g_free (key);

  return value;
}

GtkWidget *
new_entry_from_gconf (ServiceInfo *info, const char *key)
{
  GtkWidget *entry;

  g_assert (info);
  g_assert (key);

  if (!gconf) gconf = gconf_client_get_default ();

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), get_gconf_key (info, key));
  /* TODO: connect to gconf notify */
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (on_user_entry_left), info);

  return entry;
}