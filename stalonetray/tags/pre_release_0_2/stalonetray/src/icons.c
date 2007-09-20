/* -------------------------------
* vim:tabstop=4:shiftwidth=4
* icons.h
* Tue, 24 Aug 2004 12:05:38 +0700
* -------------------------------
* Manipulations with reparented 
* windows --- tray icons 
* -------------------------------*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "icons.h"

#include "debug.h"
#include "layout.h"
#include "xembed.h"
#include "list.h"

TrayIcon *icons_head = NULL;

#ifdef DEBUG
void print_icon_list()
{
	TrayIcon *tmp;
	
	DBG(5, "print_icon_list()\n");
	DBG(5, "========================\n");

	for (tmp = icons_head; tmp != NULL; tmp = tmp->next) {
		DBG(11, "(prev=0x%x, self=0x%x, next=0x%x, w=0x%x, l->p=0x%x)\n",
				tmp->prev, tmp, tmp->next, tmp->w, tmp->l.p);
	}

	if (icons_head == NULL) {
		DBG(11, "<no icons>\n");
	}

	DBG(5, "========================\n");
	DBG(5, "\n");
	
}
#endif

int add_icon(Window w, int cmode)
{
	TrayIcon *new_icon, *tmp;
	
#ifdef DEBUG
	print_icon_list();
#endif

	for (tmp = icons_head; tmp != NULL; tmp = tmp->next)
		if (tmp->w == w) { /* I dunno why, but this sh.t happens */
			return 0;
		}

	if ((new_icon = (TrayIcon *) malloc(sizeof(TrayIcon))) == NULL) {
		DBG(0, "Out of memory");
		return 0;
	}

	new_icon->w = w;
	new_icon->l.layed_out = 0;
	new_icon->embeded = 0;
	new_icon->l.p = 0;
	new_icon->cmode = cmode;

	LIST_ADD_ITEM(icons_head, new_icon);
	
#ifdef DEBUG
	print_icon_list();
#endif

	return 1;
}

int del_icon(Window w)
{
	TrayIcon *tmp;

#ifdef DEBUG
	print_icon_list();
#endif

	tmp = find_icon(w);

	if (tmp == NULL) {
		return 0;
	}

	LIST_DEL_ITEM(icons_head, tmp);

	free(tmp);
	
#ifdef DEBUG
	print_icon_list();
#endif
	return 1;
}

TrayIcon *find_icon(Window w)
{
	TrayIcon *tmp;
	
	for (tmp = icons_head; tmp != NULL; tmp = tmp->next)
		if (tmp->w == w)
			return tmp;

	return NULL;
}

