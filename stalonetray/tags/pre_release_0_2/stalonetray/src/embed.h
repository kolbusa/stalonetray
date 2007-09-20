/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* embed.h
* Fri, 03 Sep 2004 20:38:55 +0700
* -------------------------------
* embedding cycle implementation
* -------------------------------*/

#ifndef _EMBED_H_

#define CM_KDE	1
#define CM_FDO	2

int embed_icon(Display *dpy, Window tray, Window w);
int unembed_icon(Display *dpy, Window w);

#endif
