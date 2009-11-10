/*
 * Copyright (C) 2009 Intel Corporation.
 *
 * Author: Ross Burton <ross@linux.intel.com>
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

#include <config.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gnome-keyring.h>
#include <libsoup/soup.h>
#include <rest/facebook-proxy.h>
#include <rest/rest-xml-parser.h>
#include "service-info.h"
#include "bisho-pane.h"
#include "bisho-utils.h"
#include "bisho-facebook-pane.h"

/* TODO: use mojito-keyring */
static const GnomeKeyringPasswordSchema facebook_schema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET,
  {
    { "server", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "api-key", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL, 0 }
  }
};

#define FACEBOOK_SERVER "http://facebook.com/"

/* TODO: make this a widget subclass and this private data */
typedef struct {
  ServiceInfo *info;
  RestProxy *proxy;
  GtkWidget *label;
  GtkWidget *button;
} WidgetData;

typedef enum {
  LOGGED_OUT,
  WORKING,
  CONTINUE_AUTH,
  LOGGED_IN,
} ButtonState;

static void update_widgets (WidgetData *data, ButtonState state, const char *name);

/* TODO: move to mojito */
static gboolean
decode (const char *string, char **token, char **token_secret)
{
  char **encoded_keys;
  gboolean ret = FALSE;
  gsize len;

  g_assert (string);
  g_assert (token);
  g_assert (token_secret);

  encoded_keys = g_strsplit (string, " ", 2);

  if (encoded_keys[0] && encoded_keys[1]) {
    *token = (char*)g_base64_decode (encoded_keys[0], &len);
    *token_secret = (char*)g_base64_decode (encoded_keys[1], &len);
    ret = TRUE;
  }

  g_strfreev (encoded_keys);

  return ret;
}

static RestXmlNode *
get_xml (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Invalid XML from Facebook:\n%s\n",
               rest_proxy_call_get_payload (call));
    goto done;
  }

  if (strcmp (root->name, "error_response") == 0) {
    RestXmlNode *node;
    node = rest_xml_node_find (root, "error_msg");
    g_message ("Error response from Facebook: %s\n", node->content);
    rest_xml_node_unref (root);
    root = NULL;
    goto done;
  }

 done:
  g_object_unref (call);
  return root;
}

static void
get_user_name (WidgetData *data, const char *uid)
{
  RestProxyCall *call;
  RestXmlNode *node;
  GError *error = NULL;

  g_assert (data);
  g_assert (uid);

  call = rest_proxy_new_call (data->proxy);
  rest_proxy_call_set_function (call, "users.getInfo");
  rest_proxy_call_add_param (call, "uids", uid);
  rest_proxy_call_add_param (call, "fields", "name");

  if (!rest_proxy_call_sync (call, &error)) {
    g_message ("Cannot get user info: %s", error->message);
    g_error_free (error);
    return;
  }

  node = get_xml (call);
  if (node) {
    update_widgets (data, LOGGED_IN, rest_xml_node_find (node, "name")->content);
    rest_xml_node_unref (node);
  } else {
    update_widgets (data, LOGGED_OUT, NULL);
  }
}

static void
log_in_clicked (GtkWidget *button, gpointer user_data)
{
  WidgetData *data = user_data;
  char *url;
  RestProxyCall *call;
  RestXmlNode *node;
  GError *error = NULL;

  update_widgets (data, WORKING, NULL);

  /* TODO: async */
  call = rest_proxy_new_call (data->proxy);
  rest_proxy_call_set_function (call, "auth.createToken");

  if (!rest_proxy_call_sync (call, &error)) {
    g_message ("Cannot get token: %s", error->message);
    g_error_free (error);
    update_widgets (data, LOGGED_OUT, NULL);
    return;
  }

  node = get_xml (call);

  data->info->facebook.token = g_strdup (node->content);
  rest_xml_node_unref (node);

  url = facebook_proxy_build_login_url (FACEBOOK_PROXY (data->proxy), data->info->facebook.token);
  gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (button)), url, GDK_CURRENT_TIME, NULL);

  /* TODO wait for dbus call from callback */
  update_widgets (data, CONTINUE_AUTH, NULL);
}


static void
delete_done_cb (GnomeKeyringResult result, gpointer user_data)
{
  WidgetData *data = user_data;

  if (result == GNOME_KEYRING_RESULT_OK)
    update_widgets (data, LOGGED_OUT, NULL);
  else
    update_widgets (data, LOGGED_IN, NULL);
}

static void
log_out_clicked (GtkButton *button, gpointer user_data)
{
  WidgetData *data = user_data;

  gnome_keyring_delete_password (&facebook_schema, delete_done_cb, user_data, NULL,
                                 "server", FACEBOOK_SERVER,
                                 "api-key", data->info->facebook.app_id,
                                 NULL);

  update_widgets (data, LOGGED_OUT, NULL);
}

static void
continue_clicked (GtkWidget *button, gpointer user_data)
{
  WidgetData *data = user_data;
  RestProxyCall *call;
  RestXmlNode *node;
  const char *session_key, *secret, *uid;
  char *password, *permission, *url;

  update_widgets (data, WORKING, NULL);

  call = rest_proxy_new_call (data->proxy);
  rest_proxy_call_set_function (call, "auth.getSession");
  rest_proxy_call_add_param (call, "auth_token", data->info->facebook.token);

  if (!rest_proxy_call_sync (call, NULL))
    g_message ("Cannot get session");

  node = get_xml (call);

  if (node == NULL) {
    update_widgets (data, LOGGED_OUT, NULL);
    return;
  }

  session_key = rest_xml_node_find (node, "session_key")->content;
  secret = rest_xml_node_find (node, "secret")->content;
  uid = rest_xml_node_find (node, "uid")->content;

  password = bisho_utils_encode_tokens (session_key, secret);
  facebook_proxy_set_session_key (FACEBOOK_PROXY (data->proxy), session_key);
  facebook_proxy_set_app_secret (FACEBOOK_PROXY (data->proxy), secret);

  get_user_name (data, uid);

  rest_xml_node_unref (node);

  /* TODO async */
  GnomeKeyringResult result;
  GnomeKeyringAttributeList *attrs;
  guint32 id;

  attrs = gnome_keyring_attribute_list_new ();
  gnome_keyring_attribute_list_append_string (attrs, "server", FACEBOOK_SERVER);
  gnome_keyring_attribute_list_append_string (attrs, "api-key", data->info->facebook.app_id);

  result = gnome_keyring_item_create_sync (NULL,
                                           GNOME_KEYRING_ITEM_GENERIC_SECRET,
                                           data->info->display_name,
                                           attrs, password,
                                           TRUE, &id);

  if (result == GNOME_KEYRING_RESULT_OK) {
    gnome_keyring_item_grant_access_rights_sync (NULL,
                                                 "mojito",
                                                 LIBEXECDIR "/mojito-core",
                                                 id, GNOME_KEYRING_ACCESS_READ);

    call = rest_proxy_new_call (data->proxy);

    rest_proxy_call_set_function (call, "Users.hasAppPermission");
    rest_proxy_call_add_param (call, "ext_perm", "publish_stream");

    if (!rest_proxy_call_sync (call, NULL))
      return;

    node = get_xml (call);
    if (!node)
      return;

    permission = g_strdup (node->content);
    rest_xml_node_unref (node);

    if (g_strcmp0 (permission, "0") == 0) {
      url = facebook_proxy_build_permission_url (FACEBOOK_PROXY (data->proxy), "publish_stream");
      gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (button)), url, GDK_CURRENT_TIME, NULL);
    }
  } else {
    update_widgets (data, LOGGED_OUT, NULL);
  }
}

static void
update_widgets (WidgetData *data, ButtonState state, const char *name)
{
  g_signal_handlers_disconnect_by_func (data->button, log_out_clicked, data);
  g_signal_handlers_disconnect_by_func (data->button, continue_clicked, data);
  g_signal_handlers_disconnect_by_func (data->button, log_in_clicked, data);

  /* TODO: display user name */

  switch (state) {
  case LOGGED_OUT:
    gtk_widget_set_sensitive (data->button, TRUE);
    gtk_label_set_text (GTK_LABEL (data->label), _("Log in pending"));
    gtk_button_set_label (GTK_BUTTON (data->button), _("Log me in"));
    g_signal_connect (data->button, "clicked", G_CALLBACK (log_in_clicked), data);
    break;
  case WORKING:
    gtk_widget_set_sensitive (data->button, FALSE);
    gtk_label_set_text (GTK_LABEL (data->label), _("Log in pending..."));
    gtk_button_set_label (GTK_BUTTON (data->button), _("Working..."));
    break;
  case CONTINUE_AUTH:
    gtk_widget_set_sensitive (data->button, TRUE);
    gtk_label_set_text (GTK_LABEL (data->label), _("Press Continue to continue the log in."));
    gtk_button_set_label (GTK_BUTTON (data->button), _("Continue"));
    g_signal_connect (data->button, "clicked", G_CALLBACK (continue_clicked), data);
    break;
  case LOGGED_IN:
    gtk_widget_set_sensitive (data->button, TRUE);
    if (name) {
      char *s;
      s = g_strdup_printf (_("Logged in as %s"), name);
      gtk_label_set_text (GTK_LABEL (data->label), s);
      g_free (s);
    } else {
      gtk_label_set_text (GTK_LABEL (data->label), _("Logged in"));
    }
    gtk_button_set_label (GTK_BUTTON (data->button), _("Log me out"));
    g_signal_connect (data->button, "clicked", G_CALLBACK (log_out_clicked), data);
    break;
  }
}

static void
find_key_cb (GnomeKeyringResult result,
             const char *string,
             gpointer user_data)
{
  WidgetData *data = user_data;

  if (result == GNOME_KEYRING_RESULT_OK) {
    RestProxyCall *call;
    RestXmlNode *node;
    char *secret, *session;
    GError *error = NULL;

    if (decode (string, &session, &secret)) {
      facebook_proxy_set_app_secret (FACEBOOK_PROXY (data->proxy), secret);
      facebook_proxy_set_session_key (FACEBOOK_PROXY (data->proxy), session);

      call = rest_proxy_new_call (data->proxy);
      rest_proxy_call_set_function (call, "users.getLoggedInUser");

      /* TODO async */
      if (!rest_proxy_call_sync (call, &error)) {
        g_message ("Cannot get user: %s", error->message);
        g_error_free (error);
        return;
      }

      node = get_xml (call);
      if (node) {
        get_user_name (data, node->content);
        rest_xml_node_unref (node);
      } else {
        /* The token isn't valid so fake a log out */
        log_out_clicked (NULL, data);
      }
    } else {
      /* The token isn't valid so fake a log out */
      log_out_clicked (NULL, data);
    }
  } else {
    update_widgets (data, LOGGED_OUT, NULL);
  }
}

GtkWidget *
bisho_facebook_pane_new (ServiceInfo *info)
{
  GtkWidget *table, *label;
  WidgetData *data;

  g_assert (info);
  g_assert (info->auth == AUTH_FACEBOOK);
  g_assert (info->facebook.app_id);
  g_assert (info->facebook.secret);

  data = g_slice_new0 (WidgetData);
  data->info = info;
  data->proxy = facebook_proxy_new (info->facebook.app_id, info->facebook.secret);
  rest_proxy_set_user_agent (data->proxy, "Bisho/" VERSION);

  table = gtk_table_new (2, 2, TRUE);

  label = gtk_label_new (_("<b>Status:</b>"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

  data->label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (data->label), 0.0, 0.5);
  gtk_widget_show (data->label);
  gtk_table_attach (GTK_TABLE (table), data->label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

  data->button = gtk_button_new ();
  gtk_widget_show (data->button);
  gtk_table_attach (GTK_TABLE (table), data->button, 1, 2, 0, 1, 0, 0, 0, 0);

  label = bisho_pane_make_disclaimer_label (info);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

  update_widgets (data, LOGGED_OUT, NULL);

  gnome_keyring_find_password (&facebook_schema, find_key_cb, data, NULL,
                               "server", FACEBOOK_SERVER,
                               "api-key", info->facebook.app_id,
                               NULL);

  return table;
}
