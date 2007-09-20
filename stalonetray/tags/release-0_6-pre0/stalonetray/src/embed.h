/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* embed.h
* Fri, 03 Sep 2004 20:38:55 +0700
* -------------------------------
* embedding cycle implementation
* -------------------------------*/

#ifndef _EMBED_H_

#include "icons.h"

#define CM_KDE	1
#define CM_FDO	2

int embed_window(struct TrayIcon *ti);
int unembed_window(struct TrayIcon *ti);
int update_positions();

int show_window(struct TrayIcon *ti);
int hide_window(struct TrayIcon *ti);

#endif
