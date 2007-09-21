/* -------------------------------
 * vim:tabstop=4:shiftwidth=4
 * image.h
 * Fri, 22 Jun 2007 23:32:27 +0700
 * -------------------------------
 * Simple XImage manipulation 
 * interface
 * -------------------------------*/

#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <X11/Xmd.h>
#include <X11/Xlib.h>

/* outstanding TODO (for 0.8): use Xrender when available */

/* WARNING: works with ZPixmaps only */

/* Creates alpha channel mask with specified fade-out order */
CARD8 *image_create_alpha_mask(int ord, int w, int h);

/* Alpha-tint image using color. */
int image_tint(XImage *image, XColor *color, CARD8 alpha);

/* Compose image stored in tgt with image stored in bg. 
 * Alpha of each pixel is defined by mask, which should */
int image_compose(XImage *tgt, XImage *bg, CARD8 *mask);

#endif
