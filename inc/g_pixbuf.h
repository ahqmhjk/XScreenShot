#ifndef _G_PIXBUF_H
#define _G_PIXBUF_H
#pragma once
#include "g_def.h"

typedef enum {
	ICO,
	BMP,
	PNG,
	TIFF0,
	JPG,
	JPEG
}g_save_type;


typedef struct _GPixbuf {
	int byte_order;

	int bytes_per_line;

	int depth;

    /* Number of channels, alpha included */
    int n_channels;

    /* Bits per channel */
    int bits_per_sample;

    /* Size */
    int width, height;

    /* Offset between rows */
    int rowstride;

    /* The pixel array */
    unsigned char *pixels;

    /* Do we have an alpha channel? */
    unsigned int has_alpha : 1;
}GPixbuf;

GPixbuf *g_pixbuf_new_from_data (const unsigned char *data, int depth, int b_order, int has_alpha, int bits_per_sample, int width, int height, int rowstride);
GPixbuf *g_pixbuf_new (int depth, int b_order, int has_alpha, int bits_per_sample, int width, int height);
GPixbuf *g_pixbuf_x_get_from_drawable (Display *dpy, Drawable src, int src_x, int src_y, int width, int height);
int g_pixbuf_save(GPixbuf *pixbuf, FILE *fp, g_save_type type);

void grab_window(const char *fileName, g_save_type type);

#endif
