/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * icons.h
 * Tue, 24 Aug 2004 12:05:38 +0700
 * -------------------------------
 * Manipulations with reparented 
 * windows --- tray icons 
 * -------------------------------*/

#ifndef _ICONS_H_
#define _ICONS_H_

#include "layout.h"

typedef struct _TrayIcon {
	struct _TrayIcon *next;
	struct _TrayIcon *prev;

	Window w;  /* the window itself */
	
	int cmode; /* compatibility mode --- KDE\XEMBED\etc */
	int embeded; /* just a flag */
	
	Layout l;  /* icon's layout info */
} TrayIcon;

/* adds new icon to the list */
int add_icon(Window w, int cmode);
/* deletes icon from the list */
int del_icon(Window w);
/* finds a list item corresponding to w */
TrayIcon *find_icon(Window w);

#endif
