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
#include <rest-extras/facebook-proxy.h>
#include <rest/rest-xml-parser.h>
#include "service-info.h"
#include "bisho-pane-facebook.h"
#include "bisho-utils.h"

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

struct _BishoPaneFacebookPrivate {
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

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BISHO_TYPE_PANE_FACEBOOK, BishoPaneFacebookPrivate))
G_DEFINE_TYPE (BishoPaneFacebook, bisho_pane_facebook, BISHO_TYPE_PANE);

static void update_widgets (BishoPaneFacebook *pane, ButtonState state, const char *name);

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

/* TODO review this function */
static void
get_user_name (BishoPaneFacebook *pane, const char *uid)
{
  BishoPaneFacebookPrivate *priv = pane->priv;

  RestProxyCall *call;
  RestXmlNode *node;
  GError *error = NULL;

  g_assert (pane);
  g_assert (uid);

  call = rest_proxy_new_call (priv->proxy);
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
    update_widgets (pane, LOGGED_IN, rest_xml_node_find (node, "name")->content);
    rest_xml_node_unref (node);
  } else {
    update_widgets (pane, LOGGED_OUT, NULL);
  }
}

static void
got_token_cb (RestProxyCall *call,
              const GError  *error,
              GObject       *weak_object,
              gpointer      user_data)
{
  BishoPaneFacebook *pane = BISHO_PANE_FACEBOOK (user_data);
  BishoPaneFacebookPrivate *priv = pane->priv;
  RestXmlNode *node;
  char *url;

  if (error) {
    update_widgets (pane, LOGGED_OUT, NULL);

    g_message ("Error from Facebook: %s", error->message);
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    return;
  }

  node = get_xml (call);
  if (!node){
    update_widgets (pane, LOGGED_OUT, NULL);
    return;
  }

  priv->info->facebook.token = g_strdup (node->content);
  rest_xml_node_unref (node);

  url = facebook_proxy_build_login_url (FACEBOOK_PROXY (priv->proxy), priv->info->facebook.token);
  gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (pane)), url, GDK_CURRENT_TIME, NULL);

  update_widgets (pane, CONTINUE_AUTH, NULL);
}

/* TODO need review!!! */
static void
log_in_clicked (GtkWidget *button, gpointer user_data)
{
  BishoPaneFacebook *pane = BISHO_PANE_FACEBOOK (user_data);
  BishoPaneFacebookPrivate *priv = pane->priv;

  RestProxyCall *call;
  GError *error = NULL;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "auth.createToken");

  if (rest_proxy_call_async (call, got_token_cb, NULL, pane, &error)) {
    update_widgets (pane, WORKING, NULL);
  } else { 
    update_widgets (pane, LOGGED_OUT, NULL);
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    g_message ("Cannot get token: %s", error->message);
    g_error_free (error);
  }
}


static void
delete_done_cb (GnomeKeyringResult result, gpointer user_data)
{
  BishoPaneFacebook *pane = BISHO_PANE_FACEBOOK (user_data);
  MojitoClientService *service;
  BishoPane *generic_pane = BISHO_PANE (pane);

  if (result == GNOME_KEYRING_RESULT_OK){
    update_widgets (pane, LOGGED_OUT, NULL);
    service = mojito_client_get_service (generic_pane->mojito, generic_pane->info->name);
    mojito_client_service_credentials_updated (service);
  } else {
    update_widgets (pane, LOGGED_IN, NULL);
  }
}

static void
log_out_clicked (GtkButton *button, gpointer user_data)
{
  BishoPaneFacebook *pane = BISHO_PANE_FACEBOOK (user_data);
  BishoPaneFacebookPrivate *priv = pane->priv;

  update_widgets (pane, WORKING, NULL);

  gnome_keyring_delete_password (&facebook_schema, delete_done_cb, user_data, NULL,
                                 "server", FACEBOOK_SERVER,
                                 "api-key", priv->info->facebook.app_id,
                                 NULL);
}

static void
bisho_pane_facebook_continue_auth (BishoPane *_pane, GHashTable *params)
{
  BishoPaneFacebook *pane = BISHO_PANE_FACEBOOK (_pane);
  BishoPaneFacebookPrivate *priv = pane->priv;
  MojitoClientService *service;
  BishoPane *generic_pane = BISHO_PANE (pane);
  RestProxyCall *call;
  RestXmlNode *node;
  const char *session_key, *secret, *uid;
  char *password, *permission, *url;
  GError *error = NULL;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "auth.getSession");
  rest_proxy_call_add_param (call, "auth_token", priv->info->facebook.token);

  if (!rest_proxy_call_sync (call, NULL)){
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    g_message ("Cannot get session: %s", error->message);
    g_error_free (error);
    update_widgets (pane, LOGGED_OUT, NULL);
    return;
  }

  node = get_xml (call);

  if (node == NULL) {
    update_widgets (pane, LOGGED_OUT, NULL);
    return;
  }

  session_key = rest_xml_node_find (node, "session_key")->content;
  secret = rest_xml_node_find (node, "secret")->content;
  uid = rest_xml_node_find (node, "uid")->content;

  password = bisho_utils_encode_tokens (session_key, secret);
  facebook_proxy_set_session_key (FACEBOOK_PROXY (priv->proxy), session_key);
  facebook_proxy_set_app_secret (FACEBOOK_PROXY (priv->proxy), secret);

  get_user_name (pane, uid);

  rest_xml_node_unref (node);

  /* TODO async */
  GnomeKeyringResult result;
  GnomeKeyringAttributeList *attrs;
  guint32 id;
  attrs = gnome_keyring_attribute_list_new ();
  gnome_keyring_attribute_list_append_string (attrs, "server", FACEBOOK_SERVER);
  gnome_keyring_attribute_list_append_string (attrs, "api-key", priv->info->facebook.app_id);

  result = gnome_keyring_item_create_sync (NULL,
                                           GNOME_KEYRING_ITEM_GENERIC_SECRET,
                                           priv->info->display_name,
                                           attrs, password,
                                           TRUE, &id);

  if (result == GNOME_KEYRING_RESULT_OK) {
    gnome_keyring_item_grant_access_rights_sync (NULL,
                                                 "mojito",
                                                 LIBEXECDIR "/mojito-core",
                                                 id, GNOME_KEYRING_ACCESS_READ);

    service = mojito_client_get_service (generic_pane->mojito, generic_pane->info->name);
    mojito_client_service_credentials_updated (service);

    call = rest_proxy_new_call (priv->proxy);

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
      url = facebook_proxy_build_permission_url (FACEBOOK_PROXY (priv->proxy), "publish_stream");
      gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (priv->button)), url, GDK_CURRENT_TIME, NULL);
    }
  } else {
    update_widgets (pane, LOGGED_OUT, NULL);
  }
}

static void
continue_clicked (GtkWidget *button, gpointer user_data)
{
  bisho_pane_facebook_continue_auth (BISHO_PANE (user_data), NULL);
}

static void
update_widgets (BishoPaneFacebook *pane, ButtonState state, const char *name)
{
  BishoPaneFacebookPrivate *priv = pane->priv;
  GtkWidget *button = priv->button;
  char *str;

  g_signal_handlers_disconnect_by_func (button, log_out_clicked, pane);
  g_signal_handlers_disconnect_by_func (button, continue_clicked, pane);
  g_signal_handlers_disconnect_by_func (button, log_in_clicked, pane);

  switch (state) {
  case LOGGED_OUT:
    gtk_widget_show (button);
    bisho_pane_set_user (BISHO_PANE (pane), NULL, NULL);
    bisho_pane_set_banner (BISHO_PANE (pane), NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Log me in"));
    g_signal_connect (button, "clicked", G_CALLBACK (log_in_clicked), pane);
    break;
  case WORKING:
    bisho_pane_set_banner (BISHO_PANE (pane), NULL);
    gtk_widget_hide (button);
    gtk_button_set_label (GTK_BUTTON (button), _("Working..."));
    break;
  case CONTINUE_AUTH:
    gtk_widget_show (button);

    /* Use the same string in bisho-pane-oauth.c for translations. */
    str = g_strdup_printf (_("Once you have logged in to %s, press Continue."), "Facebook");

    bisho_pane_set_banner (BISHO_PANE (pane), str);
    g_free (str);

    gtk_button_set_label (GTK_BUTTON (button), _("Continue"));
    g_signal_connect (button, "clicked", G_CALLBACK (continue_clicked), pane);
    break;
  case LOGGED_IN:
    gtk_widget_show (button);
    bisho_pane_set_banner (BISHO_PANE (pane), _("Log in succeeded. You'll see new items in a couple of minutes."));
    bisho_pane_set_user (BISHO_PANE (pane), NULL, name);
    gtk_button_set_label (GTK_BUTTON (button), _("Log me out"));
    g_signal_connect (button, "clicked", G_CALLBACK (log_out_clicked), pane);
    break;
  }
}

static void
find_key_cb (GnomeKeyringResult result,
             const char *string,
             gpointer user_data)
{
  BishoPaneFacebook *pane = BISHO_PANE_FACEBOOK (user_data);
  BishoPaneFacebookPrivate *priv = pane->priv;

  if (result == GNOME_KEYRING_RESULT_OK) {
    RestProxyCall *call;
    RestXmlNode *node;
    char *secret, *session;
    GError *error = NULL;

    if (decode (string, &session, &secret)) {
      facebook_proxy_set_app_secret (FACEBOOK_PROXY (priv->proxy), secret);
      facebook_proxy_set_session_key (FACEBOOK_PROXY (priv->proxy), session);

      call = rest_proxy_new_call (priv->proxy);
      rest_proxy_call_set_function (call, "users.getLoggedInUser");

      /* TODO async */
      if (!rest_proxy_call_sync (call, &error)) {
        g_message ("Cannot get user: %s", error->message);
        g_error_free (error);
        return;
      }

      node = get_xml (call);
      if (node) {
        get_user_name (pane, node->content);
        rest_xml_node_unref (node);
      } else {
        /* The token isn't valid so fake a log out */
        log_out_clicked (NULL, pane);
      }
    } else {
      /* The token isn't valid so fake a log out */
      log_out_clicked (NULL, pane);
    }
  } else {
    update_widgets (pane, LOGGED_OUT, NULL);
  }
}

static void
bisho_pane_facebook_class_init (BishoPaneFacebookClass *klass)
{
  BishoPaneClass *pane_class = BISHO_PANE_CLASS (klass);

  pane_class->continue_auth = bisho_pane_facebook_continue_auth;

  g_type_class_add_private (klass, sizeof (BishoPaneFacebookPrivate));
}

static void
bisho_pane_facebook_init (BishoPaneFacebook *self)
{
  self->priv = GET_PRIVATE (self);
}

GtkWidget *
bisho_pane_facebook_new (MojitoClient *client, ServiceInfo *info)
{
  BishoPaneFacebook *pane;
  BishoPaneFacebookPrivate *priv;
  GtkWidget *content, *align, *box;

  g_assert (info);
  g_assert (info->auth == AUTH_FACEBOOK);
  g_assert (info->facebook.app_id);
  g_assert (info->facebook.secret);

  pane = g_object_new (BISHO_TYPE_PANE_FACEBOOK,
                       "mojito", client,
                       "service", info,
                       NULL);

  priv = pane->priv;
  priv->info = info;

  priv->proxy = facebook_proxy_new (info->facebook.app_id, info->facebook.secret);
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

  gnome_keyring_find_password (&facebook_schema, find_key_cb, pane, NULL,
                               "server", FACEBOOK_SERVER,
                               "api-key", info->facebook.app_id,
                               NULL);

  return (GtkWidget *)pane;
}
