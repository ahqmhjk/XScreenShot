#include "g_pixbuf.h"


static unsigned int mask_table[] = {
	0x00000000, 0x00000001, 0x00000003, 0x00000007,
	0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
	0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
	0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
	0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
	0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
	0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
	0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
	0xffffffff
};

typedef struct xlib_colormap_struct xlib_colormap;
struct xlib_colormap_struct {
	int size;
	XColor *colors;
	Visual *visual;
	Colormap colormap;
};

typedef void (* cfunc) (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *cmap);

static xlib_colormap *xlib_get_colormap (Display *dpy, Colormap id, Visual *visual);
static void xlib_colormap_free (xlib_colormap *xc);
static void rgbconvert (XImage *image, unsigned char *pixels, int rowstride, int alpha, xlib_colormap *cmap);
static void rgb1(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb1a(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb8(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb8a(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb555lsb(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb555msb(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb555alsb(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb555amsb(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb565lsb(XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb565msb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb565msb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb565alsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb565amsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb888alsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb888lsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb888amsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);
static void rgb888msb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap);


static void visual_decompose_mask (unsigned long  mask, int *shift, int *prec);
static void convert_real_slow (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *cmap, int alpha);

static cfunc convert_map[] = {
	rgb1,rgb1,rgb1a,rgb1a,
	rgb8,rgb8,rgb8a,rgb8a,
	rgb555lsb,rgb555msb,rgb555alsb,rgb555amsb,
	rgb565lsb,rgb565msb,rgb565alsb,rgb565amsb,
	rgb888lsb,rgb888msb,rgb888alsb,rgb888amsb
};

static xlib_colormap *xlib_get_colormap (Display *dpy, Colormap id, Visual *visual)
{
	int i;
	xlib_colormap *xc = (xlib_colormap *)malloc(sizeof(xlib_colormap));

	xc->size = visual->map_entries;
	xc->colors = (XColor*)malloc(sizeof(XColor) * xc->size);
	xc->visual = visual;
	xc->colormap = id;

	for (i = 0; i < xc->size; i++) {
		xc->colors[i].pixel = i;
		xc->colors[i].flags = DoRed | DoGreen | DoBlue;
	}

	XQueryColors (dpy, xc->colormap, xc->colors, xc->size);

	return xc;
}

GPixbuf *g_pixbuf_new_from_data (const unsigned char *data, int depth, int b_order, int has_alpha, int bits_per_sample, int width, int height, int rowstride)
{
	GPixbuf *pixbuf;

	pixbuf = (GPixbuf*)malloc(sizeof(GPixbuf));
	pixbuf->depth = depth;
	pixbuf->byte_order = b_order;
	pixbuf->n_channels = has_alpha ? 4 : 3;
	pixbuf->bits_per_sample = bits_per_sample;
	pixbuf->has_alpha = has_alpha ? 1: 0;
	pixbuf->width = width;
	pixbuf->height = height;
	pixbuf->rowstride = rowstride;
	pixbuf->bytes_per_line = ((width * depth + 31) >> 5) << 2;
	pixbuf->pixels = (unsigned char *)data;

	return pixbuf;
}


GPixbuf *g_pixbuf_new (int depth, int b_order, int has_alpha, int bits_per_sample, int width, int height)
{
	unsigned char *buf;
	int channels;
	int rowstride;
	int bytes;

	channels = has_alpha ? 4 : 3;
    rowstride = width * channels;
    if (rowstride / channels != width || rowstride + 3 < 0) /* overflow */
                return NULL;
        
	/* Always align rows to 32-bit boundaries */
	rowstride = (rowstride + 3) & ~3;

    bytes = height * rowstride;
    if (bytes / rowstride !=  height) /* overflow */
        return NULL;

	buf = (unsigned char*)malloc (height * rowstride);
	if (!buf)
		return NULL;

	return g_pixbuf_new_from_data (buf,depth, b_order, has_alpha, bits_per_sample, width, height, rowstride);
}

GPixbuf *g_pixbuf_x_get_from_drawable (Display *dpy, Drawable src, int src_x, int src_y, int width, int height)
{
	XImage *image;
	int rowstride, bpp, alpha;
	XWindowAttributes wa;
	xlib_colormap *x_cmap;

	XGetWindowAttributes (dpy, src, &wa);
	int src_width = wa.width;
	int src_height = wa.height;

	int ret;
	int src_xorigin, src_yorigin;
	int screen_width, screen_height;
	int screen_srcx, screen_srcy;
	
	Window child;
	ret =XTranslateCoordinates (dpy, src, DefaultRootWindow(dpy), 0, 0, &src_xorigin, &src_yorigin, &child);

	screen_width = DisplayWidth (dpy, 0);
	screen_height = DisplayHeight (dpy, 0);

	if (src_x < 0 || src_x >= screen_width)
		screen_srcx = src_xorigin;
	else
		screen_srcx = src_xorigin + src_x;
	if (src_y < 0 || src_y >= screen_height)
		screen_srcy = src_yorigin;
	else
		screen_srcy = src_yorigin + src_y;

	if (width + screen_srcx > screen_width)
		width = screen_width - screen_srcx;
	if (height + screen_srcy > screen_height)
		height = screen_height - screen_srcy;
 
	/* Get Image in ZPixmap format (packed bits). */
	image = XGetImage (dpy, src, screen_srcx, screen_srcy, width, height, AllPlanes, ZPixmap);
	GPixbuf *dest = g_pixbuf_new (image->depth, image->byte_order, 0, 8, width, height);
	if (!dest) {
		XDestroyImage (image);
		return NULL;
	}
	/* Get the colormap if needed */
	Colormap cmap = wa.colormap;
	Visual *visual = wa.visual;

	x_cmap = xlib_get_colormap (dpy, cmap, visual);

	alpha = dest->has_alpha;
	rowstride = dest->rowstride;
	bpp = alpha ? 4 : 3;

	/* we offset into the image data based on the position we are retrieving from */
	rgbconvert (image, dest->pixels ,rowstride, alpha, x_cmap);

	xlib_colormap_free (x_cmap);
	XDestroyImage (image);

	return dest;
}


static void xlib_colormap_free (xlib_colormap *xc)
{
	free(xc->colors);
	free(xc);
}



static void rgbconvert (XImage *image, unsigned char *pixels, int rowstride, int alpha, xlib_colormap *cmap)
{
	int index = (image->byte_order == MSBFirst) | (alpha != 0) << 1;
	int bank=5;		/* default fallback converter */
	Visual *v = cmap->visual;

	switch (v->class) {
				/* I assume this is right for static & greyscale's too? */
	case StaticGray:
	case GrayScale:
	case StaticColor:
	case PseudoColor:
		switch (image->bits_per_pixel) {
		case 1:
			bank = 0;
			break;
		case 8:
			bank = 1;
			break;
		}
		break;
	case TrueColor:
		switch (image->depth) {
		case 15:
			if (v->red_mask == 0x7c00 && v->green_mask == 0x3e0 && v->blue_mask == 0x1f
			    && image->bits_per_pixel == 16)
				bank = 2;
			break;
		case 16:
			if (v->red_mask == 0xf800 && v->green_mask == 0x7e0 && v->blue_mask == 0x1f
			    && image->bits_per_pixel == 16)
				bank = 3;
			break;
		case 24:
		case 32:
			if (v->red_mask == 0xff0000 && v->green_mask == 0xff00 && v->blue_mask == 0xff
			    && image->bits_per_pixel == 32)
				bank = 4;
			break;
		}
		break;
	case DirectColor:
		/* always use the slow version */
		break;
	}

	if (bank==5) {
		convert_real_slow(image, pixels, rowstride, cmap, alpha);
	} else {
		index |= bank << 2;
		(* convert_map[index]) (image, pixels, rowstride, cmap);
	}
}


static void rgb1 (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned char *s;
	register unsigned char data;
	unsigned char *o;
	unsigned char *srow = image->data, *orow = pixels;


	/* convert upto 8 pixels/time */
	/* its probably not worth trying to make this run very fast, who uses
	   1 bit displays anymore? */
	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;

		for (xx = 0; xx < width; xx ++) {
			data = srow[xx >> 3] >> (7 - (xx & 7)) & 1;
			*o++ = colormap->colors[data].red;
			*o++ = colormap->colors[data].green;
			*o++ = colormap->colors[data].blue;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 1 bits/pixel data
  with alpha
*/
static void rgb1a (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned char *s;
	register unsigned char data;
	unsigned char *o;
	unsigned char *srow = image->data, *orow = pixels;
	unsigned int remap[2];


	/* convert upto 8 pixels/time */
	/* its probably not worth trying to make this run very fast, who uses
	   1 bit displays anymore? */
	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (xx = 0; xx < 2; xx++) {
#ifdef LITTLE
		remap[xx] = 0xff000000
			| colormap->colors[xx].blue << 16
			| colormap->colors[xx].green << 8
			| colormap->colors[xx].red;
#else
		remap[xx] = 0xff
			| colormap->colors[xx].red << 24
			| colormap->colors[xx].green << 16
			| colormap->colors[xx].blue << 8;
#endif
	}

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;

		for (xx = 0; xx < width; xx ++) {
			data = srow[xx >> 3] >> (7 - (xx & 7)) & 1;
			*o++ = remap[data];
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 8 bits/pixel data
  no alpha
*/
static void rgb8 (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned int mask;
	register unsigned int data;
	unsigned char *srow = image->data, *orow = pixels;
	register unsigned char *s;
	register unsigned char *o;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;


	mask = mask_table[image->depth];

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			data = *s++ & mask;
			*o++ = colormap->colors[data].red;
			*o++ = colormap->colors[data].green;
			*o++ = colormap->colors[data].blue;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 8 bits/pixel data
  with alpha
*/
static void rgb8a (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned int mask;
	register unsigned int data;
	unsigned int remap[256];
	register unsigned char *s;	/* read 2 pixels at once */
	register unsigned int *o;
	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	mask = mask_table[image->depth];

	for (xx = 0; xx < colormap->size; xx++) {
#ifdef LITTLE
		remap[xx] = 0xff000000
			| colormap->colors[xx].blue << 16
			| colormap->colors[xx].green << 8
			| colormap->colors[xx].red;
#else
		remap[xx] = 0xff
			| colormap->colors[xx].red << 24
			| colormap->colors[xx].green << 16
			| colormap->colors[xx].blue << 8;
#endif
	}

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = (unsigned int *) orow;
		for (xx = 0; xx < width; xx ++) {
			data = *s++ & mask;
			*o++ = remap[data];
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 16 bits/pixel data
  no alpha
  data in lsb format
*/
static void rgb565lsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned int *s;	/* read 2 pixels at once */
#else
	register unsigned char *s;	/* read 2 pixels at once */
#endif
	register unsigned short *o;
	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned int *) srow;
#else
		s = srow;
#endif
		o = (unsigned short *) orow;
		for (xx = 1; xx < width; xx += 2) {
			register unsigned int data;
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0xf800) >> 8 | (data & 0xe000) >> 13
				| (data & 0x7e0) << 5 | (data & 0x600) >> 1;
			*o++ = (data & 0x1f) << 3 | (data & 0x1c) >> 2
				| (data & 0xf8000000) >> 16 | (data & 0xe0000000) >> 21;
			*o++ = (data & 0x7e00000) >> 19 | (data & 0x6000000) >> 25
				| (data & 0x1f0000) >> 5 | (data & 0x1c0000) >> 10;
#else
			/* swap endianness first */
			data = s[1] | s[0] << 8 | s[3] << 16 | s[2] << 24;
			s += 4;
			*o++ = (data & 0xf800) | (data & 0xe000) >> 5
				| (data & 0x7e0) >> 3 | (data & 0x600) >> 9;
			*o++ = (data & 0x1f) << 11 | (data & 0x1c) << 6
				| (data & 0xf8000000) >> 24 | (data & 0xe0000000) >> 29;
			*o++ = (data & 0x7e00000) >> 11 | (data & 0x6000000) >> 17
				| (data & 0x1f0000) >> 13 | (data & 0x1c0000) >> 18;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register unsigned short data;
#ifdef LITTLE
			data = *((short *) s);
#else
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
#endif
			((char *) o)[0] = ((data >> 8) & 0xf8) | ((data >> 13) & 0x7);
			((char *) o)[1] = ((data >> 3) & 0xfc) | ((data >> 9) & 0x3);
			((char *) o)[2] = ((data << 3) & 0xf8) | ((data >> 2) & 0x7);
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 16 bits/pixel data
  no alpha
  data in msb format
*/
static void rgb565msb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned char *s;	/* need to swap data order */
#else
	register unsigned int *s;	/* read 2 pixels at once */
#endif
	register unsigned short *o;
	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = srow;
#else
		s = (unsigned int *) srow;
#endif
		o = (unsigned short *) orow;
		for (xx = 1; xx < width; xx += 2) {
			register unsigned int data;
#ifdef LITTLE
			/* swap endianness first */
			data = s[1] | s[0] << 8 | s[3] << 16 | s[2] << 24;
			s += 4;
			*o++ = (data & 0xf800) >> 8 | (data & 0xe000) >> 13
				| (data & 0x7e0) << 5 | (data & 0x600) >> 1;
			*o++ = (data & 0x1f) << 3 | (data & 0x1c) >> 2
				| (data & 0xf8000000) >> 16 | (data & 0xe0000000) >> 21;
			*o++ = (data & 0x7e00000) >> 19 | (data & 0x6000000) >> 25
				| (data & 0x1f0000) >> 5 | (data & 0x1c0000) >> 10;
#else
			data = *s++;
			*o++ = (data & 0xf800) | (data & 0xe000) >> 5
				| (data & 0x7e0) >> 3 | (data & 0x600) >> 9;
			*o++ = (data & 0x1f) << 11 | (data & 0x1c) << 6
				| (data & 0xf8000000) >> 24 | (data & 0xe0000000) >> 29;
			*o++ = (data & 0x7e00000) >> 11 | (data & 0x6000000) >> 17
				| (data & 0x1f0000) >> 13 | (data & 0x1c0000) >> 18;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register unsigned short data;
#ifdef LITTLE
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
#else
			data = *((short *) s);
#endif
			((char *) o)[0] = ((data >> 8) & 0xf8) | ((data >> 13) & 0x7);
			((char *) o)[1] = ((data >> 3) & 0xfc) | ((data >> 9) & 0x3);
			((char *) o)[2] = ((data << 3) & 0xf8) | ((data >> 2) & 0x7);
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 16 bits/pixel data
  with alpha
  data in lsb format
*/
static void rgb565alsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned short *s;	/* read 1 pixels at once */
#else
	register unsigned char *s;
#endif
	register unsigned int *o;

	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned short *) srow;
#else
		s = (unsigned char *) srow;
#endif
		o = (unsigned int *) orow;
		for (xx = 0; xx < width; xx ++) {
			register unsigned int data;
			/*  rrrrrggg gggbbbbb -> rrrrrRRR ggggggGG bbbbbBBB aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbbBBB ggggggGG rrrrrRRR */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0xf800) >> 8 | (data & 0xe000) >> 13
				| (data & 0x7e0) << 5 | (data & 0x600) >> 1
				| (data & 0x1f) << 19 | (data & 0x1c) << 14
				| 0xff000000;
#else
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0xf800) << 16 | (data & 0xe000) << 11
				| (data & 0x7e0) << 13 | (data & 0x600) << 7
				| (data & 0x1f) << 11 | (data & 0x1c) << 6
				| 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 16 bits/pixel data
  with alpha
  data in msb format
*/
static void rgb565amsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned char *s;
#else
	register unsigned short *s;	/* read 1 pixels at once */
#endif
	register unsigned int *o;

	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = (unsigned int *) orow;
		for (xx = 0; xx < width; xx ++) {
			register unsigned int data;
			/*  rrrrrggg gggbbbbb -> rrrrrRRR gggggg00 bbbbbBBB aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbbBBB gggggg00 rrrrrRRR */
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0xf800) >> 8 | (data & 0xe000) >> 13
				| (data & 0x7e0) << 5 | (data & 0x600) >> 1
				| (data & 0x1f) << 19 | (data & 0x1c) << 14
				| 0xff000000;
#else
			data = *s++;
			*o++ = (data & 0xf800) << 16 | (data & 0xe000) << 11
				| (data & 0x7e0) << 13 | (data & 0x600) << 7
				| (data & 0x1f) << 11 | (data & 0x1c) << 6
				| 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 15 bits/pixel data
  no alpha
  data in lsb format
*/
static void rgb555lsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned int *s;	/* read 2 pixels at once */
#else
	register unsigned char *s;	/* read 2 pixels at once */
#endif
	register unsigned short *o;
	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned int *) srow;
#else
		s = srow;
#endif
		o = (unsigned short *) orow;
		for (xx = 1; xx < width; xx += 2) {
			register unsigned int data;
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12
				| (data & 0x3e0) << 6 | (data & 0x380) << 1;
			*o++ = (data & 0x1f) << 3 | (data & 0x1c) >> 2
				| (data & 0x7c000000) >> 15 | (data & 0x70000000) >> 20;
			*o++ = (data & 0x3e00000) >> 18 | (data & 0x3800000) >> 23
				| (data & 0x1f0000) >> 5 | (data & 0x1c0000) >> 10;
#else
			/* swap endianness first */
			data = s[1] | s[0] << 8 | s[3] << 16 | s[2] << 24;
			s += 4;
			*o++ = (data & 0x7c00) << 1 | (data & 0x7000) >> 4
				| (data & 0x3e0) >> 2 | (data & 0x380) >> 7;
			*o++ = (data & 0x1f) << 11 | (data & 0x1c) << 6
				| (data & 0x7c000000) >> 23 | (data & 0x70000000) >> 28;
			*o++ = (data & 0x3e00000) >> 10 | (data & 0x3800000) >> 15
				| (data & 0x1f0000) >> 13 | (data & 0x1c0000) >> 18;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register unsigned short data;
#ifdef LITTLE
			data = *((short *) s);
#else
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
#endif
			((char *) o)[0] = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12;
			((char *) o)[1] = (data & 0x3e0) >> 2 | (data & 0x380) >> 7;
			((char *) o)[2] = (data & 0x1f) << 3 | (data & 0x1c) >> 2;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 15 bits/pixel data
  no alpha
  data in msb format
*/
static void rgb555msb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned char *s;	/* read 2 pixels at once */
#else
	register unsigned int *s;	/* read 2 pixels at once */
#endif
	register unsigned short *o;
	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = (unsigned short *) orow;
		for (xx = 1; xx < width; xx += 2) {
			register unsigned int data;
#ifdef LITTLE
			/* swap endianness first */
			data = s[1] | s[0] << 8 | s[3] << 16 | s[2] << 24;
			s += 4;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12
				| (data & 0x3e0) << 6 | (data & 0x380) << 1;
			*o++ = (data & 0x1f) << 3 | (data & 0x1c) >> 2
				| (data & 0x7c000000) >> 15 | (data & 0x70000000) >> 20;
			*o++ = (data & 0x3e00000) >> 18 | (data & 0x3800000) >> 23
				| (data & 0x1f0000) >> 5 | (data & 0x1c0000) >> 10;
#else
			data = *s++;
			*o++ = (data & 0x7c00) << 1 | (data & 0x7000) >> 4
				| (data & 0x3e0) >> 2 | (data & 0x380) >> 7;
			*o++ = (data & 0x1f) << 11 | (data & 0x1c) << 6
				| (data & 0x7c000000) >> 23 | (data & 0x70000000) >> 28;
			*o++ = (data & 0x3e00000) >> 10 | (data & 0x3800000) >> 15
				| (data & 0x1f0000) >> 13 | (data & 0x1c0000) >> 18;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register unsigned short data;
#ifdef LITTLE
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
#else
			data = *((short *) s);
#endif
			((char *) o)[0] = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12;
			((char *) o)[1] = (data & 0x3e0) >> 2 | (data & 0x380) >> 7;
			((char *) o)[2] = (data & 0x1f) << 3 | (data & 0x1c) >> 2;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 15 bits/pixel data
  with alpha
  data in lsb format
*/
static void rgb555alsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned short *s;	/* read 1 pixels at once */
#else
	register unsigned char *s;
#endif
	register unsigned int *o;

	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned short *) srow;
#else
		s = srow;
#endif
		o = (unsigned int *) orow;
		for (xx = 0; xx < width; xx++) {
			register unsigned int data;
			/*  rrrrrggg gggbbbbb -> rrrrrRRR gggggGGG bbbbbBBB aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbbBBB gggggGGG rrrrrRRR */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12
				| (data & 0x3e0) << 6 | (data & 0x380) << 1
				| (data & 0x1f) << 19 | (data & 0x1c) << 14
				| 0xff000000;
#else
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0x7c00) << 17 | (data & 0x7000) << 12
				| (data & 0x3e0) << 14 | (data & 0x380) << 9
				| (data & 0x1f) << 11 | (data & 0x1c) << 6
				| 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 15 bits/pixel data
  with alpha
  data in msb format
*/
static void rgb555amsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned short *s;	/* read 1 pixels at once */
#else
	register unsigned char *s;
#endif
	register unsigned int *o;

	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned short *) srow;
#else
		s = srow;
#endif
		o = (unsigned int *) orow;
		for (xx = 0; xx < width; xx++) {
			register unsigned int data;
			/*  rrrrrggg gggbbbbb -> rrrrrRRR gggggGGG bbbbbBBB aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbbBBB gggggGGG rrrrrRRR */
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12
				| (data & 0x3e0) << 6 | (data & 0x380) << 1
				| (data & 0x1f) << 19 | (data & 0x1c) << 14
				| 0xff000000;
#else
			data = *s++;
			*o++ = (data & 0x7c00) << 17 | (data & 0x7000) << 12
				| (data & 0x3e0) << 14 | (data & 0x380) << 9
				| (data & 0x1f) << 11 | (data & 0x1c) << 6
				| 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}


static void rgb888alsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	unsigned char *s;	/* for byte order swapping */
	unsigned char *o;
	unsigned char *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;


	/* lsb data */
	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			*o++ = s[2];
			*o++ = s[1];
			*o++ = s[0];
			*o++ = 0xff;
			s += 4;
		}
		srow += bpl;
		orow += rowstride;
	}
}

static void rgb888lsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	unsigned char *srow = image->data, *orow = pixels;
	unsigned char *o, *s;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;


	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			*o++ = s[2];
			*o++ = s[1];
			*o++ = s[0];
			s += 4;
		}
		srow += bpl;
		orow += rowstride;
	}
}

static void rgb888amsb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	unsigned char *srow = image->data, *orow = pixels;
#ifdef LITTLE
	unsigned int *o;
	unsigned int *s;
#else
	unsigned char *s;	/* for byte order swapping */
	unsigned char *o;
#endif


	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	/* msb data */
	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned int *) srow;
		o = (unsigned int *) orow;
#else
		s = srow;
		o = orow;
#endif
		for (xx = 0; xx < width; xx++) {
#ifdef LITTLE
			*o++ = s[1];
			*o++ = s[2];
			*o++ = s[3];
			*o++ = 0xff;
			s += 4;
#else
			*o++ = (*s << 8) | 0xff; /* untested */
			s++;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

static void rgb888msb (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	unsigned char *srow = image->data, *orow = pixels;
	unsigned char *s;
	unsigned char *o;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			*o++ = s[1];
			*o++ = s[2];
			*o++ = s[3];
			s += 4;
		}
		srow += bpl;
		orow += rowstride;
	}
}

static void visual_decompose_mask (unsigned long  mask, int *shift, int *prec)
{
	*shift = 0;
	*prec = 0;

	while (!(mask & 0x1)) {
		(*shift)++;
		mask >>= 1;
	}

	while (mask & 0x1) {
		(*prec)++;
		mask >>= 1;
	}
}

/*
  This should work correctly with any display/any endianness, but will probably
  run quite slow
*/
static void convert_real_slow (XImage *image, unsigned char *pixels, int rowstride, xlib_colormap *cmap, int alpha)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned char *srow = image->data, *orow = pixels;
	unsigned char *s;
	unsigned char *o;
	unsigned int pixel;
	Visual *v;
	unsigned char component;
	int i;
	int red_shift, red_prec, green_shift, green_prec, blue_shift, blue_prec;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;
	v = cmap->visual;

	visual_decompose_mask (v->red_mask, &red_shift, &red_prec);
	visual_decompose_mask (v->green_mask, &green_shift, &green_prec);
	visual_decompose_mask (v->blue_mask, &blue_shift, &blue_prec);

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			pixel = XGetPixel (image, xx, yy);
			switch (v->class) {
				/* I assume this is right for static & greyscale's too? */
			case StaticGray:
			case GrayScale:
			case StaticColor:
			case PseudoColor:
				*o++ = cmap->colors[pixel].red;
				*o++ = cmap->colors[pixel].green;
				*o++ = cmap->colors[pixel].blue;
				break;
			case TrueColor:
				/* This is odd because it must sometimes shift left (otherwise
				   I'd just shift >> (*_shift - 8 + *_prec + <0-7>). This logic
				   should work for all bit sizes/shifts/etc. */
				component = 0;
				for (i = 24; i < 32; i += red_prec)
					component |= ((pixel & v->red_mask) << (32 - red_shift - red_prec)) >> i;
				*o++ = component;
				component = 0;
				for (i = 24; i < 32; i += green_prec)
					component |= ((pixel & v->green_mask) << (32 - green_shift - green_prec)) >> i;
				*o++ = component;
				component = 0;
				for (i = 24; i < 32; i += blue_prec)
					component |= ((pixel & v->blue_mask) << (32 - blue_shift - blue_prec)) >> i;
				*o++ = component;
				break;
			case DirectColor:
				*o++ = cmap->colors[((pixel & v->red_mask) << (32 - red_shift - red_prec)) >> 24].red;
				*o++ = cmap->colors[((pixel & v->green_mask) << (32 - green_shift - green_prec)) >> 24].green;
				*o++ = cmap->colors[((pixel & v->blue_mask) << (32 - blue_shift - blue_prec)) >> 24].blue;
				break;
			}
			if (alpha)
				*o++ = 0xff;
		}
		srow += bpl;
		orow += rowstride;
	}
}

#if 0
int main()
{
	dpy = XOpenDisplay(NULL);
	GPixbuf *dest = g_pixbuf_x_get_from_drawable(DefaultRootWindow(dpy), 0, 0, 77, 90);
	printf("%d, %d\n", dest->width, dest->rowstride);
	printf("%s\n", dest->pixels);
	XCloseDisplay(dpy);
	return 0;
}
#endif
