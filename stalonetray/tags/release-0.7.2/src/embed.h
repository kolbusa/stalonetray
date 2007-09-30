/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* embed.h
* Fri, 03 Sep 2004 20:38:55 +0700
* -------------------------------
* embedding cycle implementation
* -------------------------------*/

#ifndef _EMBED_H_

#include "icons.h"

/* Constants for compatibility modes */
/* KDE */
#define CM_KDE	1
/* Generic, freedesktop.org */
#define CM_FDO	2

/* Embed an icon */
int embedder_embed(struct TrayIcon *ti);

/* Unembed an icon */
int embedder_unembed(struct TrayIcon *ti);

/* If (forced) 
 * 		recalculate and update positions of all icons;
 * else
 * 		recalculate and update positions of all icons that have requested an update; */
int embedder_update_positions(int force);

/* Show the icon */
int embedder_show(struct TrayIcon *ti);

/* Hide the icon */
int embedder_hide(struct TrayIcon *ti);

/* Refresh icon and its parent */
int embedder_refresh(struct TrayIcon *ti);

/* (Re)set icon size according to the policy */
int embedder_reset_size(struct TrayIcon *ti);

#endif
