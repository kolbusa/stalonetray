/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * list.h
 * Fri, 10 Sep 2004 23:34:48 +0700
 * -------------------------------
 * Simple two-linked list
 * -------------------------------*/

#ifndef _LIST_H_
#define _LIST_H_

#define LIST_ADD_ITEM(h_, i_) { \
	i_->prev = NULL;            \
	i_->next = NULL;            \
    if (h_ != NULL) {           \
		i_->next = h_;          \
		h_->prev = i_;          \
	}                           \
    h_ = i_;                    \
}

#define LIST_DEL_ITEM(h_, i_) {    \
	if (i_->prev != NULL)          \
		i_->prev->next = i_->next; \
	if (i_->next != NULL)          \
    	i_->next->prev = i_->prev; \
    if (i_ == h_)                  \
		h_ = i_->next;             \
}

#endif
