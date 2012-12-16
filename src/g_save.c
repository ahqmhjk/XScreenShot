#include "g_pixbuf.h"
#include <png.h>
#ifndef png_jmpbuf					/* pngconf.h (libpng 1.0.6 or later) */
# define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf)
#endif

#ifndef _SETJMP_H
#include <setjmp.h>
#endif
#include <jpeglib.h>
#include <tiffio.h>

#include "list.h"

static int g_pixbuf_png_image_save(FILE *f, GPixbuf * pixbuf);
static int g_pixbuf_jpeg_image_save(FILE * f, GPixbuf *pixbuf);
static int g_pixbuf_bmp_image_save(FILE * f, GPixbuf *pixbuf);
static int g_pixbuf_ico_image_save(FILE * f, GPixbuf *pixbuf);
static int g_pixbuf_tiff_image_save(FILE * f, GPixbuf *pixbuf);


int g_pixbuf_save(GPixbuf *pixbuf, FILE *fp, g_save_type type)
{
	switch(type) {
		case ICO:
			return g_pixbuf_ico_image_save(fp, pixbuf);
		case BMP:
			return g_pixbuf_bmp_image_save(fp, pixbuf);
		case PNG:
			return g_pixbuf_png_image_save(fp, pixbuf);
		case TIFF0:
			return g_pixbuf_tiff_image_save(fp, pixbuf);
		case JPG:
		case JPEG:
			return g_pixbuf_jpeg_image_save(fp, pixbuf);
		default:
			return g_pixbuf_jpeg_image_save(fp, pixbuf);
	}
}


struct error_handler_data {
  struct jpeg_error_mgr pub;    /* "public" fields */
  jmp_buf setjmp_buffer;        /* for return to caller */
};

static int g_pixbuf_jpeg_image_save(FILE * f, GPixbuf *pixbuf) {
	struct jpeg_compress_struct cinfo;
	unsigned char *buf;
	unsigned char *ptr;
	unsigned char *pixels;
	JSAMPROW *jbuf;
	int y = 0;
	int i, j;
	int w, h = 0;
	int rowstride = 0;
	struct error_handler_data jerr;

	w = pixbuf->width;
	h = pixbuf->height;
	rowstride = pixbuf->rowstride;
	pixels = pixbuf->pixels;
	if (!pixels)
		return 0;
	/* allocate a small buffer to convert image data */
	buf = malloc(w * 3 * sizeof(unsigned char));
	if (!buf)
		return 0;

	cinfo.err = jpeg_std_error(&(jerr.pub));
	if (sigsetjmp(jerr.setjmp_buffer, 1)) {
		jpeg_destroy_compress(&cinfo);
		free(buf);
		fclose(f);
		return 1;
	}

	/* setup compress params */
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, f);
	cinfo.image_width = w;
	cinfo.image_height = h;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;


	/* set up jepg compression parameters */
	jpeg_set_defaults(&cinfo);
	jpeg_start_compress(&cinfo, TRUE);
	/* get the start pointer */
	ptr = pixels;
	/* go one scanline at a time... and save */
	i = 0;
	while (cinfo.next_scanline < cinfo.image_height) {
		/* convert scanline from ARGB to RGB packed */
		for (j = 0; j < w; j++)
			memcpy(&(buf[j * 3]), &(ptr[i * rowstride + j * 3]), 3);

		/* write scanline */
		jbuf = (JSAMPROW *) (&buf);
		jpeg_write_scanlines(&cinfo, jbuf, 1);
		i++;
		y++;
	}
	/* finish off */
	jpeg_finish_compress(&cinfo);
	free(buf);
	return 0;
}

static int g_pixbuf_png_image_save(FILE *f, GPixbuf * pixbuf) {
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned char *ptr;
	unsigned char *pixels;
	int x, y, j;
	png_bytep row_ptr, data = NULL;
	png_color_8 sig_bit;
	int w, h, rowstride;
	int has_alpha;
	int bpc;

	bpc = pixbuf->bits_per_sample;
	w = pixbuf->width;
	h = pixbuf->height;
	rowstride = pixbuf->rowstride;
	has_alpha = pixbuf->has_alpha;
	pixels = pixbuf->pixels;
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
									  NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(f);
		return 0;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fclose(f);
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		return 0;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(f);
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		return 0;
	}
	png_init_io(png_ptr, f);
	if (has_alpha) {
		png_set_IHDR(png_ptr, info_ptr, w, h, bpc,
					 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
					 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
#ifndef LITTLE
		png_set_swap_alpha(png_ptr);
#else
		png_set_bgr(png_ptr);
#endif
	} else {
		png_set_IHDR(png_ptr, info_ptr, w, h, bpc,
					 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
					 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
		data = malloc(w * 3 * sizeof(char));
	}
	sig_bit.red = bpc;
	sig_bit.green = bpc;
	sig_bit.blue = bpc;
	sig_bit.alpha = bpc;
	png_set_sBIT(png_ptr, info_ptr, &sig_bit);
	png_write_info(png_ptr, info_ptr);
	png_set_shift(png_ptr, &sig_bit);
	png_set_packing(png_ptr);

	ptr = pixels;
	for (y = 0; y < h; y++) {
		if (has_alpha)
			row_ptr = (png_bytep) ptr;
		else {
			for (j = 0, x = 0; x < w; x++)
				memcpy(&(data[x * 3]), &(ptr[x * 3]), 3);

			row_ptr = (png_bytep) data;
		}
		png_write_rows(png_ptr, &row_ptr, 1);
		ptr += rowstride;
	}
	if (data)
		free(data);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
	return 0;
}

#define BI_RGB 0

#ifndef UINT16_TO_LE
#define UINT16_TO_LE(val)	((unsigned short) (val))
#endif

#ifndef UINT32_TO_LE
#define UINT32_TO_LE(val)	((unsigned int) (val))
#endif

#define put16(buf,data)	{ unsigned short x; \
			  x = UINT16_TO_LE (data); \
			  memcpy(buf, &x, 2); \
			  buf += 2; }
#define put32(buf,data)	{ unsigned int x; \
			  x = UINT32_TO_LE (data); \
			  memcpy(buf, &x, 4); \
			  buf += 4; }


static int save_to_file_cb (const char *buf, unsigned long count,FILE *f)
{
	int bytes;
	
	while (count > 0) {
		bytes = fwrite (buf, sizeof (char), count, f);
		if (bytes <= 0)
			break;
		count -= bytes;
		buf += bytes;
	}

	if (count) {
		return -1;
	}
	
	return 0;
}


static int g_pixbuf_bmp_image_save(FILE *f, GPixbuf *pixbuf)
{
	unsigned int width, height, channel, size, stride, src_stride, x, y;
	unsigned char BFH_BIH[54], *pixels, *buf, *src, *dst, *dst_line;
	int ret;

	width = pixbuf->width;
	height = pixbuf->height;
	channel = pixbuf->n_channels;
	pixels = pixbuf->pixels;
	src_stride = pixbuf->rowstride;
	stride = (width * 3 + 3) & ~3;
	size = stride * height;

	/* filling BFH */
	dst = BFH_BIH;
	*dst++ = 'B';			/* bfType */
	*dst++ = 'M';
	put32 (dst, size + 14 + 40);	/* bfSize */
	put32 (dst, 0);			/* bfReserved1 + bfReserved2 */
	put32 (dst, 14 + 40);		/* bfOffBits */

	/* filling BIH */
	put32 (dst, 40);		/* biSize */
	put32 (dst, width);		/* biWidth */
	put32 (dst, height);		/* biHeight */
	put16 (dst, 1);			/* biPlanes */
	put16 (dst, 24);		/* biBitCount */
	put32 (dst, BI_RGB);		/* biCompression */
	put32 (dst, size);		/* biSizeImage */
	put32 (dst, 0);			/* biXPelsPerMeter */
	put32 (dst, 0);			/* biYPelsPerMeter */
	put32 (dst, 0);			/* biClrUsed */
	put32 (dst, 0);			/* biClrImportant */

	if (save_to_file_cb((char *)BFH_BIH, 14 + 40, f) < 0)
		return -1;

	dst_line = buf = (unsigned char*)malloc(size * sizeof(unsigned char));
	if (!buf) {
		return -1;
	}

	/* saving as a bottom-up bmp */
	pixels += (height - 1) * src_stride;
	for (y = 0; y < height; ++y, pixels -= src_stride, dst_line += stride) {
		dst = dst_line;
		src = pixels;
		for (x = 0; x < width; ++x, dst += 3, src += channel) {
			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
		}
	}
	ret = save_to_file_cb ((char *)buf, size, f);
	free (buf);

	return ret;
}


typedef struct _IconEntry IconEntry;
struct _IconEntry {
	int width;
	int height;
	int depth;
	int hot_x;
	int hot_y;

	unsigned char n_colors;
	unsigned int *colors;
	unsigned int xor_rowstride;
	unsigned char *xor;
	unsigned int and_rowstride;
	unsigned char *and;
};

static int write8 (FILE  *f, unsigned char   *data, int      count)
{
  int bytes;
  int written;

  written = 0;
  while (count > 0)
    {
      bytes = fwrite ((char*) data, sizeof (char), count, f);
      if (bytes <= 0)
        break;
      count -= bytes;
      data += bytes;
      written += bytes;
    }

  return written;
}

static int write16 (FILE   *f, unsigned short  *data, int  count)
{
  int i;

  for (i = 0; i < count; i++)
	  data[i] = UINT16_TO_LE (data[i]);

  return write8 (f, (unsigned char*) data, count * 2);
}

static int write32 (FILE     *f, unsigned int  *data, int  count)
{
  int i;

  for (i = 0; i < count; i++)
	  data[i] = UINT32_TO_LE (data[i]);
  
  return write8 (f, (unsigned char*) data, count * 4);
}


static int fill_entry (IconEntry *icon, GPixbuf *pixbuf, int  hot_x, int  hot_y){
	unsigned char *p, *pixels, *and, *xor;
	int n_channels, v, x, y;

	if (icon->width > 255 || icon->height > 255) {
		return -1;
	} 
	
	if (hot_x > -1 && hot_y > -1) {
		icon->hot_x = hot_x;
		icon->hot_y = hot_y;
		if (icon->hot_x >= icon->width || icon->hot_y >= icon->height) {
			return -1;
		}
	}
	else {
		icon->hot_x = -1;
		icon->hot_y = -1;
	}
	
	switch (icon->depth) {
	case 32:
		icon->xor_rowstride = icon->width * 4;
		break;
	case 24:
		icon->xor_rowstride = icon->width * 3;
		break;
	case 16:
		icon->xor_rowstride = icon->width * 2;
		break;
	default:
		return -1;
	}

	if ((icon->xor_rowstride % 4) != 0)
		icon->xor_rowstride = 4 * ((icon->xor_rowstride / 4) + 1);
	icon->xor = (unsigned char*)malloc(icon->xor_rowstride * icon->height * sizeof(unsigned char));
	
	icon->and_rowstride = (icon->width + 7) / 8;
	if ((icon->and_rowstride % 4) != 0)
		icon->and_rowstride = 4 * ((icon->and_rowstride / 4) + 1);
	icon->and = (unsigned char*)malloc(icon->and_rowstride * icon->height * sizeof(unsigned char));

	pixels = pixbuf->pixels;
	n_channels = pixbuf->n_channels;
	for (y = 0; y < icon->height; y++) {
		p = pixels + pixbuf->rowstride * (icon->height - 1 - y);
		and = icon->and + icon->and_rowstride * y;
		xor = icon->xor + icon->xor_rowstride * y;
		for (x = 0; x < icon->width; x++) {
			switch (icon->depth) {
			case 32:
				xor[0] = p[2];
				xor[1] = p[1];
				xor[2] = p[0];
				xor[3] = 0xff;
				if (n_channels == 4) {
					xor[3] = p[3];
					if (p[3] < 0x80)
						*and |= 1 << (7 - x % 8);
				}
				xor += 4;
				break;
			case 24:
				xor[0] = p[2];
				xor[1] = p[1];
				xor[2] = p[0];
				if (n_channels == 4 && p[3] < 0x80)
					*and |= 1 << (7 - x % 8);
				xor += 3;
				break;
			case 16:
				v = ((p[0] >> 3) << 10) | ((p[1] >> 3) << 5) | (p[2] >> 3);
				xor[0] = v & 0xff;
				xor[1] = v >> 8;
				if (n_channels == 4 && p[3] < 0x80)
					*and |= 1 << (7 - x % 8);
				xor += 2;
				break;
			}
			
			p += n_channels;
			if (x % 8 == 7) 
				and++;
		}
	}

	return 0;
}

static void free_entry (IconEntry *icon)
{
	free (icon->colors);
	free (icon->and);
	free (icon->xor);
	free (icon);
}

static void write_icon (FILE *f, GList *entries)
{
	IconEntry *icon;
	GList *entry;
	unsigned char bytes[4];
	unsigned short words[4];
	unsigned int dwords[6];
	int type;
	int n_entries;
	int offset;
	int size;

	if (((IconEntry *)entries->data)->hot_x > -1)
		type = 2;
	else 
		type = 1;
	n_entries = g_list_length (entries);

	/* header */
	words[0] = 0;
	words[1] = type;
	words[2] = n_entries;
	write16 (f, words, 3);
	
	offset = 6 + 16 * n_entries;

	for (entry = entries; entry; entry = entry->next) {
		icon = (IconEntry *)entry->data;
		size = 40 + icon->height * (icon->and_rowstride + icon->xor_rowstride);
		
		/* directory entry */
		bytes[0] = icon->width;
		bytes[1] = icon->height;
		bytes[2] = icon->n_colors;
		bytes[3] = 0;
		write8 (f, bytes, 4);
		if (type == 1) {
			words[0] = 1;
			words[1] = icon->depth;
		}
		else {
			words[0] = icon->hot_x;
			words[1] = icon->hot_y;
		}
		write16 (f, words, 2);
		dwords[0] = size;
		dwords[1] = offset;
		write32 (f, dwords, 2);

		offset += size;
	}

	for (entry = entries; entry; entry = entry->next) {
		icon = (IconEntry *)entry->data;

		/* bitmap header */
		dwords[0] = 40;
		dwords[1] = icon->width;
		dwords[2] = icon->height * 2;
		write32 (f, dwords, 3);
		words[0] = 1;
		words[1] = icon->depth;
		write16 (f, words, 2);
		dwords[0] = 0;
		dwords[1] = 0;
		dwords[2] = 0;
		dwords[3] = 0;
		dwords[4] = 0;
		dwords[5] = 0;
		write32 (f, dwords, 6);

		/* image data */
		write8 (f, icon->xor, icon->xor_rowstride * icon->height);
		write8 (f, icon->and, icon->and_rowstride * icon->height);
	}
}




static int g_pixbuf_ico_image_save (FILE *f, GPixbuf *pixbuf)
{
	int hot_x, hot_y;
	IconEntry *icon;
	GList *entries = NULL;

	/* support only single-image ICOs for now */
	icon = (IconEntry*)malloc(sizeof(IconEntry));
	icon->width = pixbuf->width;
	icon->height = pixbuf->height;
	icon->depth = pixbuf->has_alpha ? 32 : 24;
	hot_x = -1;
	hot_y = -1;

	if (!fill_entry (icon, pixbuf, hot_x, hot_y)) {
		free_entry (icon);
		return -1;
	}

	entries = g_list_append (entries, icon); 
	write_icon (f, entries);

	g_list_foreach (entries, (FUNC)free_entry, NULL);
	g_list_free (entries);

	return 0;
}


typedef struct _TiffSaveContent{
        char *buffer;
        unsigned int allocated;
        unsigned int used;
        unsigned int pos;
} TiffSaveContext;

static tsize_t tiff_save_read (thandle_t handle, tdata_t buf, tsize_t size)
{
        return -1;
}

static tsize_t tiff_save_write (thandle_t handle, tdata_t buf, tsize_t size)
{
        TiffSaveContext *context = (TiffSaveContext *)handle;

        /* Modify buffer length */
        if (context->pos + size > context->used)
                context->used = context->pos + size;

        /* Realloc */
        if (context->used > context->allocated) {
                context->buffer = realloc(context->buffer, context->pos + size);
                context->allocated = context->used;
        }

        /* Now copy the data */
        memcpy (context->buffer + context->pos, buf, size);

        /* Update pos */
        context->pos += size;

        return size;
}

static toff_t tiff_save_seek (thandle_t handle, toff_t offset, int whence)
{
        TiffSaveContext *context = (TiffSaveContext *)handle;

        switch (whence) {
        case SEEK_SET:
                context->pos = offset;
                break;
        case SEEK_CUR:
                context->pos += offset;
                break;
        case SEEK_END:
                context->pos = context->used + offset;
                break;
        default:
                return -1;
        }
        return context->pos;
}

static int tiff_save_close (thandle_t context)
{
        return 0;
}

static toff_t tiff_save_size (thandle_t handle)
{
        return -1;
}

static TiffSaveContext *create_save_context (void)
{
    TiffSaveContext *context;

    context = (TiffSaveContext *)malloc(sizeof(TiffSaveContext));
    context->buffer = NULL;
    context->allocated = 0;
    context->used = 0;
    context->pos = 0;

   	return context;
}

static void free_save_context (TiffSaveContext *context)
{
	free (context->buffer);
    free (context);
}

static int g_pixbuf_tiff_image_save(FILE * f, GPixbuf *pixbuf)
{
    TIFF *tiff;
    int width, height, rowstride;
    unsigned char *pixels;
    int has_alpha;
    unsigned short alpha_samples[1] = { EXTRASAMPLE_UNASSALPHA };
    int y;
    TiffSaveContext *context;
    int retval;
    unsigned char *icc_profile = NULL;
    unsigned long icc_profile_size = 0;

    context = create_save_context ();
    tiff = TIFFClientOpen ("libtiff-pixbuf", "w", context,  
                            tiff_save_read, tiff_save_write, 
                            tiff_save_seek, tiff_save_close, 
                            tiff_save_size, 
                            NULL, NULL);

    if (!tiff) {
        free_save_context (context);
        return -1;
   	}

    rowstride = pixbuf->rowstride;
    pixels = pixbuf->pixels ;

    has_alpha = pixbuf->has_alpha ;

    height = pixbuf->height ;
    width = pixbuf->width ;

    TIFFSetField (tiff, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField (tiff, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField (tiff, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField (tiff, TIFFTAG_SAMPLESPERPIXEL, has_alpha ? 4 : 3);
    TIFFSetField (tiff, TIFFTAG_ROWSPERSTRIP, height);

    if (has_alpha)
    	TIFFSetField (tiff, TIFFTAG_EXTRASAMPLES, 1, alpha_samples);

    TIFFSetField (tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
   	TIFFSetField (tiff, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);        
    TIFFSetField (tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    if (icc_profile != NULL)
    	TIFFSetField (tiff, TIFFTAG_ICCPROFILE, icc_profile_size, icc_profile);

    for (y = 0; y < height; y++) {
    	if (TIFFWriteScanline (tiff, pixels + y * rowstride, y, 0) == -1)
        	break;
        }

    TIFFClose (tiff);

    /* Now call the callback */
   	retval = save_to_file_cb(context->buffer, context->used, f);

    free (icc_profile);
    free_save_context (context);
    return retval;
}
