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

#include <glib.h>
#include <mojito-keystore/mojito-keystore.h>
#include "service-info.h"

#define GROUP "MojitoService"
#define GROUP_OAUTH "OAuth"

ServiceAuthType
service_info_authtype_from_string (const char *s)
{
  if (s == NULL) {
    return AUTH_INVALID;
  } else if (g_ascii_strcasecmp (s, "username") == 0) {
    return AUTH_USERNAME;
  } else if (g_ascii_strcasecmp (s, "password") == 0) {
    return AUTH_USERNAME_PASSWORD;
  } else if (g_ascii_strcasecmp (s, "oauth") == 0) {
    return AUTH_OAUTH;
  } else if (g_ascii_strcasecmp (s, "flickr") == 0) {
    return AUTH_FLICKR;
  } else if (g_ascii_strcasecmp (s, "facebook") == 0) {
    return AUTH_FACEBOOK;
  } else {
    g_message ("Unknown authentication type '%s'", s);
    return AUTH_INVALID;
  }
}

ServiceInfo *
get_info_for_service (const char *name)
{
  char *filename, *path, *real_path, *authstring;
  GKeyFile *keys;
  ServiceInfo *info;
  ServiceAuthType auth;

  g_assert (name);

  filename = g_strconcat (name, ".keys", NULL);
  path = g_build_filename ("mojito", "services", filename, NULL);
  g_free (filename);

  keys = g_key_file_new ();

  if (!g_key_file_load_from_data_dirs (keys, path, &real_path, G_KEY_FILE_NONE, NULL)) {
    g_free (path);
    g_key_file_free (keys);
    return NULL;
  }
  g_free (path);

  /* Sanity check for required keys */
  if (!g_key_file_has_key (keys, GROUP, "Name", NULL) ||
      !g_key_file_has_key (keys, GROUP, "AuthType", NULL)) {
    g_key_file_free (keys);
    return NULL;
  }

  authstring = g_key_file_get_string (keys, GROUP, "AuthType", NULL);
  auth = service_info_authtype_from_string (authstring);
  g_free (authstring);

  if (auth == AUTH_INVALID) {
    g_key_file_free (keys);
    return NULL;
  }

  info = g_slice_new0 (ServiceInfo);
  info->name = g_strdup (name);
  info->display_name = g_key_file_get_locale_string (keys, GROUP, "Name", NULL, NULL);
  info->description = g_key_file_get_locale_string (keys, GROUP, "Description", NULL, NULL);
  info->link = g_key_file_get_string (keys, GROUP, "Link", NULL);
  info->auth = auth;

  switch (auth) {
  case AUTH_OAUTH:
    {
      const char *key, *secret;

      if (mojito_keystore_get_key_secret (info->name, &key, &secret)) {
        info->oauth.consumer_key = g_strdup (key);
        info->oauth.consumer_secret = g_strdup (secret);
      } else {
        g_message ("Cannot find keys for %s", info->name);
        /* Yes, we're leaking.  Live with it */
        return NULL;
      }
      info->oauth.base_url = g_key_file_get_string (keys, GROUP_OAUTH, "BaseURL", NULL);
      info->oauth.request_token_function = g_key_file_get_string (keys, GROUP_OAUTH, "RequestTokenFunction", NULL);
      info->oauth.authorize_function = g_key_file_get_string (keys, GROUP_OAUTH, "AuthoriseFunction", NULL);
      info->oauth.access_token_function = g_key_file_get_string (keys, GROUP_OAUTH, "AccessTokenFunction", NULL);
      info->oauth.callback = g_key_file_get_string (keys, GROUP_OAUTH, "Callback", NULL);
    }
    break;
  case AUTH_FLICKR:
    {
      const char *key, *secret;

      if (mojito_keystore_get_key_secret (info->name, &key, &secret)) {
        info->flickr.api_key = g_strdup (key);
        info->flickr.shared_secret = g_strdup (secret);
      } else {
        g_message ("Cannot find keys for %s", info->name);
        /* Yes, we're leaking.  Live with it */
        return NULL;
      }
    }
    break;
  case AUTH_FACEBOOK:
    {
      const char *key, *secret;

      if (mojito_keystore_get_key_secret (info->name, &key, &secret)) {
        info->facebook.app_id = g_strdup (key);
        info->facebook.secret = g_strdup (secret);
      } else {
        g_message ("Cannot find API keys for %s", info->name);
        /* Yes, we're leaking.  Live with it */
        return NULL;
      }
    }
    break;
  case AUTH_USERNAME:
  case AUTH_USERNAME_PASSWORD:
  case AUTH_INVALID:
    /* Nothing to do */
    break;
  }

  g_key_file_free (keys);

  /* TODO: this should be specified in the key file or something */
  path = g_path_get_dirname (real_path);
  g_free (real_path);
  filename = g_strconcat (name, ".png", NULL);
  info->icon = g_build_filename (path, filename, NULL);
  g_free (filename);

  if (!g_file_test (info->icon, G_FILE_TEST_EXISTS)) {
    g_free (info->icon);
    info->icon = NULL;
  }

  return info;
}

