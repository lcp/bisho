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
#include <rest-extras/flickr-proxy.h>
#include <rest/rest-xml-parser.h>
#include "service-info.h"
#include "bisho-pane-flickr.h"
#include "bisho-utils.h"

/* TODO: use mojito-keyring */
static const GnomeKeyringPasswordSchema flickr_schema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET,
  {
    { "server", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "api-key", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL, 0 }
  }
};

#define FLICKR_SERVER "http://flickr.com/"

struct _BishoPaneFlickrPrivate {
  ServiceInfo *info;
  RestProxy *proxy;
  GtkWidget *button;
};

typedef enum {
  LOGGED_OUT,
  WORKING,
  CONTINUE_AUTH,
  LOGGED_IN,
} ButtonState;

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BISHO_TYPE_PANE_FLICKR, BishoPaneFlickrPrivate))
G_DEFINE_TYPE (BishoPaneFlickr, bisho_pane_flickr, BISHO_TYPE_PANE);

static void update_widgets (BishoPaneFlickr *data, ButtonState state, const char *name);

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
    g_message ("Invalid XML from Flickr:\n%s",
               rest_proxy_call_get_payload (call));
    goto done;
  }

  if (strcmp (root->name, "rsp") != 0) {
    g_message ("Unexpected response from Flickr:\n%s",
               rest_proxy_call_get_payload (call));
    rest_xml_node_unref (root);
    root = NULL;
    goto done;
  }

  if (strcmp (rest_xml_node_get_attr (root, "stat"), "ok") != 0) {
    RestXmlNode *node;
    const char *msg;

    node = rest_xml_node_find (root, "err");
    msg = rest_xml_node_get_attr (node, "msg");

    /* TODO bisho_utils_message (NULL, "Flickr", msg); */
    g_message ("Error from Flickr: %s", msg);

    rest_xml_node_unref (root);
    root = NULL;
    goto done;
  }

  done:
  g_object_unref (call);
  return root;
}

static void
log_in_clicked (GtkWidget *button, gpointer user_data)
{
  BishoPaneFlickr *pane = BISHO_PANE_FLICKR (user_data);
  BishoPaneFlickrPrivate *priv = pane->priv;
  char *url;
  RestProxyCall *call;
  RestXmlNode *node;

  update_widgets (pane, WORKING, NULL);

  /* TODO: async */
  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "flickr.auth.getFrob");

  if (!rest_proxy_call_run (call, NULL, NULL))
    g_error ("Cannot get frob");

  node = get_xml (call);

  priv->info->flickr.frob = g_strdup (rest_xml_node_find (node, "frob")->content);
  rest_xml_node_unref (node);

  url = flickr_proxy_build_login_url (FLICKR_PROXY (priv->proxy), priv->info->flickr.frob);
  gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (button)), url, GDK_CURRENT_TIME, NULL);

  /* TODO wait for dbus call from callback */
  update_widgets (pane, CONTINUE_AUTH, NULL);
}


static void
delete_done_cb (GnomeKeyringResult result, gpointer user_data)
{
  BishoPaneFlickr *pane = BISHO_PANE_FLICKR (user_data);

  if (result == GNOME_KEYRING_RESULT_OK)
    update_widgets (pane, LOGGED_OUT, NULL);
  else
    update_widgets (pane, LOGGED_IN, NULL);
}

static void
log_out_clicked (GtkButton *button, gpointer user_data)
{
  BishoPaneFlickr *pane = BISHO_PANE_FLICKR (user_data);
  BishoPaneFlickrPrivate *priv = pane->priv;

  gnome_keyring_delete_password (&flickr_schema, delete_done_cb, user_data, NULL,
                                 "server", FLICKR_SERVER,
                                 "api-key", priv->info->flickr.api_key,
                                 NULL);

  update_widgets (pane, LOGGED_OUT, NULL);
}

static void
got_auth (RestXmlNode *node, BishoPaneFlickr *pane)
{
  RestXmlNode *user;
  const char *name;

  user = rest_xml_node_find (node, "user");
  name = rest_xml_node_get_attr (user, "fullname");
  if (name == NULL || name[0] == '\0')
    name = rest_xml_node_get_attr (user, "username");

  update_widgets (pane, LOGGED_IN, name);
}

static void
continue_clicked (GtkWidget *button, gpointer user_data)
{
  BishoPaneFlickr *pane = BISHO_PANE_FLICKR (user_data);
  BishoPaneFlickrPrivate *priv = pane->priv;
  RestProxyCall *call;
  RestXmlNode *node;
  const char *token;
  GError *error = NULL;

//  if (params == NULL || g_hash_table_lookup (params, "frob") == NULL) {
//    g_message ("Frob not provided in callback, cannot continue");
    /* TODO bisho_utils_message (NULL, "Flickr", NULL); */
//    update_widgets (pane, LOGGED_OUT, NULL);
//    return;
//  }

  update_widgets (pane, WORKING, NULL);

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "flickr.auth.getToken");
  rest_proxy_call_add_param (call, "frob", priv->info->flickr.frob);

  if (!rest_proxy_call_sync (call, &error)) {
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    g_message ("Cannot get token: %s", error->message);
    g_error_free (error);
    update_widgets (pane, LOGGED_OUT, NULL);
    return;
  }

  node = get_xml (call);

  if (node == NULL) {
    update_widgets (pane, LOGGED_OUT, NULL);
    return;
  }

  token = rest_xml_node_find (node, "token")->content;
  flickr_proxy_set_token (FLICKR_PROXY (priv->proxy), token);

  got_auth (node, pane);

  /* TODO async */
  GnomeKeyringResult result;
  GnomeKeyringAttributeList *attrs;
  guint32 id;
  attrs = gnome_keyring_attribute_list_new ();
  gnome_keyring_attribute_list_append_string (attrs, "server", FLICKR_SERVER);
  gnome_keyring_attribute_list_append_string (attrs, "api-key", priv->info->flickr.api_key);

  result = gnome_keyring_item_create_sync (NULL,
                                           GNOME_KEYRING_ITEM_GENERIC_SECRET,
                                           priv->info->display_name,
                                           attrs, token,
                                           TRUE, &id);

  if (result == GNOME_KEYRING_RESULT_OK) {
    gnome_keyring_item_grant_access_rights_sync (NULL,
                                                 "mojito",
                                                 LIBEXECDIR "/mojito-core",
                                                 id, GNOME_KEYRING_ACCESS_READ);
  } else {
    g_message ("Cannot update keyring: %s", gnome_keyring_result_to_message (result));
    update_widgets (pane, LOGGED_OUT, NULL);
  }

  rest_xml_node_unref (node);
}

static void
update_widgets (BishoPaneFlickr *pane, ButtonState state, const char *name)
{
  BishoPaneFlickrPrivate *priv = pane->priv;

  g_signal_handlers_disconnect_by_func (priv->button, log_out_clicked, pane);
  g_signal_handlers_disconnect_by_func (priv->button, continue_clicked, pane);
  g_signal_handlers_disconnect_by_func (priv->button, log_in_clicked, pane);

  switch (state) {
  case LOGGED_OUT:
    bisho_pane_set_user (BISHO_PANE (pane), NULL, NULL);
    bisho_pane_set_banner (BISHO_PANE (pane), NULL);
    gtk_widget_set_sensitive (priv->button, TRUE);
    gtk_button_set_label (GTK_BUTTON (priv->button), _("Log me in"));
    g_signal_connect (priv->button, "clicked", G_CALLBACK (log_in_clicked), pane);
    break;
  case WORKING:
    bisho_pane_set_banner (BISHO_PANE (pane), NULL);
    gtk_widget_set_sensitive (priv->button, FALSE);
    gtk_button_set_label (GTK_BUTTON (priv->button), _("Working..."));
    break;
  case CONTINUE_AUTH:
    gtk_widget_set_sensitive (priv->button, TRUE);
    bisho_pane_set_banner (BISHO_PANE (pane), _("Once you have logged in to Flickr, press Continue."));
    gtk_button_set_label (GTK_BUTTON (priv->button), _("Continue"));
    g_signal_connect (priv->button, "clicked", G_CALLBACK (continue_clicked), pane);
    break;
  case LOGGED_IN:
    bisho_pane_set_banner (BISHO_PANE (pane), _("Log in succeeded. You'll see new items in a couple of minutes."));
    gtk_widget_set_sensitive (priv->button, TRUE);
    bisho_pane_set_user (BISHO_PANE (pane), NULL, name);
    gtk_button_set_label (GTK_BUTTON (priv->button), _("Log me out"));
    g_signal_connect (priv->button, "clicked", G_CALLBACK (log_out_clicked), pane);
    break;
  }
}

static void
check_token_cb (RestProxyCall *call, const GError *error, GObject *weak_object, gpointer user_data)
{
  BishoPaneFlickr *pane = BISHO_PANE_FLICKR (user_data);
  RestXmlNode *node;

  if (error) {
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    g_message ("Cannot check token: %s", error->message);
  } else {
    node = get_xml (call);
    if (node) {
      got_auth (node, pane);
      rest_xml_node_unref (node);
    } else {
      /* The token isn't valid so fake a log out */
      log_out_clicked (NULL, pane);
    }
  }
}

static void
find_key_cb (GnomeKeyringResult result,
             const char *string,
             gpointer user_data)
{
  BishoPaneFlickr *pane = BISHO_PANE_FLICKR (user_data);
  BishoPaneFlickrPrivate *priv = pane->priv;

  if (result == GNOME_KEYRING_RESULT_OK) {
    GError *error = NULL;
    RestProxyCall *call;

    flickr_proxy_set_token (FLICKR_PROXY (priv->proxy), string);

    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "flickr.auth.checkToken");

    if (rest_proxy_call_async (call, check_token_cb, NULL, pane, &error)) {
      update_widgets (pane, WORKING, NULL);
    } else {
      bisho_pane_set_banner_error (BISHO_PANE (pane), error);
      g_message ("Cannot check token: %s", error->message);
      g_error_free (error);
    }
  } else {
    update_widgets (pane, LOGGED_OUT, NULL);
  }
}

static void
bisho_pane_flickr_class_init (BishoPaneFlickrClass *klass)
{
  g_type_class_add_private (klass, sizeof (BishoPaneFlickrPrivate));
}

static void
bisho_pane_flickr_init (BishoPaneFlickr *self)
{
  self->priv = GET_PRIVATE (self);
}

GtkWidget *
bisho_pane_flickr_new (MojitoClient *client, ServiceInfo *info)
{
  BishoPaneFlickr *pane;
  BishoPaneFlickrPrivate *priv;
  GtkWidget *content, *align, *box;

  g_assert (info);
  g_assert (info->auth == AUTH_FLICKR);
  g_assert (info->flickr.api_key);
  g_assert (info->flickr.shared_secret);

  pane = g_object_new (BISHO_TYPE_PANE_FLICKR,
                       "mojito", client,
                       "service", info,
                       NULL);

  priv = pane->priv;
  priv->info = info;

  priv->proxy = flickr_proxy_new (info->flickr.api_key, info->flickr.shared_secret);
  rest_proxy_set_user_agent (priv->proxy, "Bisho/" VERSION);

  content = BISHO_PANE (pane)->content;

  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_widget_show (align);
  gtk_container_add (GTK_CONTAINER (content), align);

  box = gtk_hbox_new (FALSE, 8);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (align), box);

  priv->button = gtk_button_new ();
  gtk_widget_show (priv->button);
  bisho_pane_follow_connected (BISHO_PANE (pane), priv->button);
  gtk_box_pack_start (GTK_BOX (box), priv->button, FALSE, FALSE, 0);

  update_widgets (pane, LOGGED_OUT, NULL);

  gnome_keyring_find_password (&flickr_schema, find_key_cb, pane, NULL,
                               "server", FLICKR_SERVER,
                               "api-key", info->flickr.api_key,
                               NULL);

  return (GtkWidget *)pane;
}
