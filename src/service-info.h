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

#ifndef _SERVICE_INFO_H
#define _SERVICE_INFO_H

typedef enum {
  AUTH_INVALID = 0,
  AUTH_USERNAME,
  AUTH_USERNAME_PASSWORD,
  AUTH_OAUTH,
  AUTH_FLICKR,
} ServiceAuthType;

typedef struct {
  char *name;
  char *display_name;
  char *description;
  char *link;
  ServiceAuthType auth;
  char *icon;
  union {
    struct {
      char *consumer_key;
      char *consumer_secret;
      char *base_url;
      char *request_token_function;
      char *authorize_function;
      char *access_token_function;
      char *callback;
    } oauth;
    struct {
      char *api_key;
      char *shared_secret;
      char *frob;
    } flickr;
  };
} ServiceInfo;

ServiceInfo * get_info_for_service (const char *name);

ServiceAuthType service_info_authtype_from_string (const char *s);

#endif /* _SERVICE_INFO_H */
