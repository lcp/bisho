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

#ifndef __BISHO_PANE_FACEBOOK_H__
#define __BISHO_PANE_FACEBOOK_H__

#include "bisho-pane.h"
#include "service-info.h"

G_BEGIN_DECLS

#define BISHO_TYPE_PANE_FACEBOOK (bisho_pane_facebook_get_type())
#define BISHO_PANE_FACEBOOK(obj)                                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               BISHO_TYPE_PANE_FACEBOOK,                  \
                               BishoPaneFacebook))
#define BISHO_PANE_FACEBOOK_CLASS(klass)                                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            BISHO_TYPE_PANE_FACEBOOK,                     \
                            BishoPaneFacebookClass))
#define BISHO_IS_PANE_FACEBOOK(obj)                               \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BISHO_TYPE_PANE_FACEBOOK))
#define BISHO_IS_PANE_FACEBOOK_CLASS(klass)                       \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), BISHO_TYPE_PANE_FACEBOOK))
#define BISHO_PANE_FACEBOOK_GET_CLASS(obj)                                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              BISHO_TYPE_PANE_FACEBOOK,                   \
                              BishoPaneFacebookClass))

typedef struct _BishoPaneFacebookPrivate BishoPaneFacebookPrivate;
typedef struct _BishoPaneFacebook        BishoPaneFacebook;
typedef struct _BishoPaneFacebookClass   BishoPaneFacebookClass;

struct _BishoPaneFacebook {
  BishoPane parent;
  BishoPaneFacebookPrivate *priv;
};

struct _BishoPaneFacebookClass {
  BishoPaneClass parent_class;
};

GType bisho_pane_facebook_get_type (void) G_GNUC_CONST;

GtkWidget * bisho_pane_facebook_new (MojitoClient *client, ServiceInfo *info);

G_END_DECLS

#endif /* __BISHO_PANE_FACEBOOK_H__ */
