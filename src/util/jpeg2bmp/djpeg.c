/*
 * jpeg2bmp.c
 *
 * Copyright (C) HJK.
 */

#include <unistd.h>
#include <stdlib.h>
#include "cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */

typedef struct _bmp_dest_struct_{
  struct djpeg_dest_struct pub;	/* public fields */
  boolean is_os2;		/* saves the OS2 format request flag */
  jvirt_sarray_ptr whole_image;	/* needed to reverse row order */
  JDIMENSION data_width;	/* JSAMPLEs per row */
  JDIMENSION row_width;		/* physical width of one row in the BMP file */
  int pad_bytes;		/* number of padding bytes needed per row */
  JDIMENSION cur_output_row;	/* next row# to write to virtual array */
} bmp_dest_struct;

typedef bmp_dest_struct * bmp_dest_ptr;


/* Forward declarations */
static void write_colormap JPP((j_decompress_ptr cinfo, bmp_dest_ptr dest, int map_colors, int map_entry_size));
static void put_pixel_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo, JDIMENSION rows_supplied);
static void put_gray_rows(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo, JDIMENSION rows_supplied);
static void start_output_bmp(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo);
static void write_bmp_header(j_decompress_ptr cinfo, bmp_dest_ptr dest);
static void write_os2_header(j_decompress_ptr cinfo, bmp_dest_ptr dest);
static void write_colormap(j_decompress_ptr cinfo, bmp_dest_ptr dest, int map_colors, int map_entry_size);
static void finish_output_bmp(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo);
static djpeg_dest_ptr jinit_write_bmp(j_decompress_ptr cinfo, boolean is_os2);

int jpg2bmp(const char *in, const char *out)
{
  int isWin = 1;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;

  int file_index;
  djpeg_dest_ptr dest_mgr = NULL;
  FILE * input_file;
  FILE * output_file;
  JDIMENSION num_scanlines;

  /* Initialize the JPEG decompression object with default error handling. */
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);

  if ((input_file = fopen(in, "rb")) == NULL) {
      exit(EXIT_FAILURE);
  }

  if ((output_file = fopen(out, "wb")) == NULL) {
     exit(EXIT_FAILURE);
  }

  /* Specify data source for decompression */
  jpeg_stdio_src(&cinfo, input_file);

  /* Read file header, set default decompression parameters */
  (void) jpeg_read_header(&cinfo, TRUE);

  if (isWin)
     dest_mgr = jinit_write_bmp(&cinfo, FALSE);
  else
     dest_mgr = jinit_write_bmp(&cinfo, TRUE);

   dest_mgr->output_file = output_file;

  /* Start decompressor */
  (void) jpeg_start_decompress(&cinfo);

  /* Write output file header */
  (*dest_mgr->start_output) (&cinfo, dest_mgr);

  /* Process data */

  while (cinfo.output_scanline < cinfo.output_height) {
    num_scanlines = jpeg_read_scanlines(&cinfo, dest_mgr->buffer,
					dest_mgr->buffer_height);
    (*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, num_scanlines);
  }


  /* Finish decompression and release memory.
   * I must do it in this order because output module has allocated memory
   * of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
   */
  (*dest_mgr->finish_output) (&cinfo, dest_mgr);
  (void) jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  /* Close files, if we opened them */
  if (input_file != stdin)
    fclose(input_file);
  if (output_file != stdout)
    fclose(output_file);

  return 0;			/* suppress no-return-value warnings */
}

/*
 * Write the colormap.
 * Windows uses BGR0 map entries; OS/2 uses BGR entries.
 */

static void write_colormap(j_decompress_ptr cinfo, bmp_dest_ptr dest, int map_colors, int map_entry_size)
{
  	JSAMPARRAY colormap = cinfo->colormap;
  	int num_colors = cinfo->actual_number_of_colors;
  	FILE * outfile = dest->pub.output_file;
  	int i;

  	if (colormap != NULL) {
    	if (cinfo->out_color_components == 3) {
      /* Normal case with RGB colormap */
      		for (i = 0; i < num_colors; i++) {
				putc(GETJSAMPLE(colormap[2][i]), outfile);
				putc(GETJSAMPLE(colormap[1][i]), outfile);
				putc(GETJSAMPLE(colormap[0][i]), outfile);
				if (map_entry_size == 4)
	  				putc(0, outfile);
      		}
    	} 
		else {
      /* Grayscale colormap (only happens with grayscale quantization) */
	      for (i = 0; i < num_colors; i++) {
			putc(GETJSAMPLE(colormap[0][i]), outfile);
			putc(GETJSAMPLE(colormap[0][i]), outfile);
			putc(GETJSAMPLE(colormap[0][i]), outfile);
			if (map_entry_size == 4)
	  			putc(0, outfile);
      		}
    	}
  	} 
	else {
    /* If no colormap, must be grayscale data.  Generate a linear "map". */
    	for (i = 0; i < 256; i++) {
      		putc(i, outfile);
      		putc(i, outfile);
      		putc(i, outfile);
      		if (map_entry_size == 4)
				putc(0, outfile);
    	}
  	}
  /* Pad colormap with zeros to ensure specified number of colormap entries */ 
  	if (i > map_colors)
    	ERREXIT1(cinfo, JERR_TOO_MANY_COLORS, i);
  	for (; i < map_colors; i++) {
    	putc(0, outfile);
    	putc(0, outfile);
    	putc(0, outfile);
    	if (map_entry_size == 4)
      		putc(0, outfile);
  	}
}
/*
 * Write some pixel data.
 * In this module rows_supplied will always be 1.
 */

static void put_pixel_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo, JDIMENSION rows_supplied)
/* This version is for writing 24-bit pixels */
{
  bmp_dest_ptr dest = (bmp_dest_ptr) dinfo;
  JSAMPARRAY image_ptr;
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  int pad;

  /* Access next row in virtual array */
  image_ptr = (*cinfo->mem->access_virt_sarray)((j_common_ptr) cinfo, dest->whole_image, dest->cur_output_row, (JDIMENSION) 1, TRUE);
  dest->cur_output_row++;

  /* Transfer data.  Note destination values must be in BGR order
   * (even though Microsoft's own documents say the opposite).
   */
  inptr = dest->pub.buffer[0];
  outptr = image_ptr[0];
  for (col = cinfo->output_width; col > 0; col--) {
    outptr[2] = *inptr++;	/* can omit GETJSAMPLE() safely */
    outptr[1] = *inptr++;
    outptr[0] = *inptr++;
    outptr += 3;
  }

  /* Zero out the pad bytes. */
  pad = dest->pad_bytes;
  while (--pad >= 0)
    *outptr++ = 0;
}

/* This version is for grayscale OR quantized color output */
static void put_gray_rows(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo, JDIMENSION rows_supplied)
{
  bmp_dest_ptr dest = (bmp_dest_ptr) dinfo;
  JSAMPARRAY image_ptr;
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  int pad;

  /* Access next row in virtual array */
  image_ptr = (*cinfo->mem->access_virt_sarray)((j_common_ptr) cinfo, dest->whole_image, dest->cur_output_row, (JDIMENSION) 1, TRUE);
  dest->cur_output_row++;

  /* Transfer data. */
  inptr = dest->pub.buffer[0];
  outptr = image_ptr[0];
  for (col = cinfo->output_width; col > 0; col--) {
    *outptr++ = *inptr++;	/* can omit GETJSAMPLE() safely */
  }

  /* Zero out the pad bytes. */
  pad = dest->pad_bytes;
  while (--pad >= 0)
    *outptr++ = 0;
}


/*
 * Startup: normally writes the file header.
 * In this module we may as well postpone everything until finish_output.
 */

static void start_output_bmp(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  /* no work here */
}


/*
 * Finish up at the end of the file.
 *
 * Here is where we really output the BMP file.
 *
 * First, routines to write the Windows and OS/2 variants of the file header.
 */

static void write_bmp_header(j_decompress_ptr cinfo, bmp_dest_ptr dest)
/* Write a Windows-style BMP file header, including colormap if needed */
{
	char bmpfileheader[14];
	char bmpinfoheader[40];
#define PUT_2B(array,offset,value)  \
	(array[offset] = (char) ((value) & 0xFF), \
	 array[offset+1] = (char) (((value) >> 8) & 0xFF))
#define PUT_4B(array,offset,value)  \
	(array[offset] = (char) ((value) & 0xFF), \
	 array[offset+1] = (char) (((value) >> 8) & 0xFF), \
	 array[offset+2] = (char) (((value) >> 16) & 0xFF), \
	 array[offset+3] = (char) (((value) >> 24) & 0xFF))
	
	INT32 headersize, bfSize;
	int bits_per_pixel, cmap_entries;

  /* Compute colormap size and total file size */
	if (cinfo->out_color_space == JCS_RGB) {
    	if (cinfo->quantize_colors) {
      /* Colormapped RGB */
      		bits_per_pixel = 8;
      		cmap_entries = 256;
    	} 
		else {
      	/* Unquantized, full color RGB */
      		bits_per_pixel = 24;
      		cmap_entries = 0;
    	}
  	} 
	else {
    /* Grayscale output.  We need to fake a 256-entry colormap. */
    	bits_per_pixel = 8;
    	cmap_entries = 256;
  	}
  /* File size */
  	headersize = 14 + 40 + cmap_entries * 4; /* Header and colormap */
  	bfSize = headersize + (INT32) dest->row_width * (INT32) cinfo->output_height;
  
  /* Set unused fields of header to 0 */
  	MEMZERO(bmpfileheader, SIZEOF(bmpfileheader));
  	MEMZERO(bmpinfoheader, SIZEOF(bmpinfoheader));

  /* Fill the file header */
  	bmpfileheader[0] = 0x42;	/* first 2 bytes are ASCII 'B', 'M' */
  	bmpfileheader[1] = 0x4D;
  	PUT_4B(bmpfileheader, 2, bfSize); /* bfSize */
  /* we leave bfReserved1 & bfReserved2 = 0 */
  	PUT_4B(bmpfileheader, 10, headersize); /* bfOffBits */

  /* Fill the info header (Microsoft calls this a BITMAPINFOHEADER) */
  	PUT_2B(bmpinfoheader, 0, 40);	/* biSize */
  	PUT_4B(bmpinfoheader, 4, cinfo->output_width); /* biWidth */
  	PUT_4B(bmpinfoheader, 8, cinfo->output_height); /* biHeight */
  	PUT_2B(bmpinfoheader, 12, 1);	/* biPlanes - must be 1 */
  	PUT_2B(bmpinfoheader, 14, bits_per_pixel); /* biBitCount */
  /* we leave biCompression = 0, for none */
  /* we leave biSizeImage = 0; this is correct for uncompressed data */
  	if (cinfo->density_unit == 2) { /* if have density in dots/cm, then */
    	PUT_4B(bmpinfoheader, 24, (INT32) (cinfo->X_density*100)); /* XPels/M */
    	PUT_4B(bmpinfoheader, 28, (INT32) (cinfo->Y_density*100)); /* XPels/M */
  	}
  	PUT_2B(bmpinfoheader, 32, cmap_entries); /* biClrUsed */
  	/* we leave biClrImportant = 0 */

  	if (JFWRITE(dest->pub.output_file, bmpfileheader, 14) != (size_t) 14)
    	ERREXIT(cinfo, JERR_FILE_WRITE);
  	if (JFWRITE(dest->pub.output_file, bmpinfoheader, 40) != (size_t) 40)
    	ERREXIT(cinfo, JERR_FILE_WRITE);

  	if (cmap_entries > 0)
    	write_colormap(cinfo, dest, cmap_entries, 4);
}


/* Write an OS2-style BMP file header, including colormap if needed */
static void write_os2_header(j_decompress_ptr cinfo, bmp_dest_ptr dest)
{
  	char bmpfileheader[14];
  	char bmpcoreheader[12];
  	INT32 headersize, bfSize;
  	int bits_per_pixel, cmap_entries;

  	/* Compute colormap size and total file size */
  	if (cinfo->out_color_space == JCS_RGB) {
    	if (cinfo->quantize_colors) {
      	/* Colormapped RGB */
      		bits_per_pixel = 8;
      		cmap_entries = 256;
    	} 
		else {
      	/* Unquantized, full color RGB */
      		bits_per_pixel = 24;
      		cmap_entries = 0;
    	}
  	} 
	else {
    /* Grayscale output.  We need to fake a 256-entry colormap. */
    	bits_per_pixel = 8;
    	cmap_entries = 256;
  	}
  	/* File size */
  	headersize = 14 + 12 + cmap_entries * 3; /* Header and colormap */
  	bfSize = headersize + (INT32) dest->row_width * (INT32) cinfo->output_height;
  
  	/* Set unused fields of header to 0 */
  	MEMZERO(bmpfileheader, SIZEOF(bmpfileheader));
  	MEMZERO(bmpcoreheader, SIZEOF(bmpcoreheader));

  	/* Fill the file header */
  	bmpfileheader[0] = 0x42;	/* first 2 bytes are ASCII 'B', 'M' */
  	bmpfileheader[1] = 0x4D;
  	PUT_4B(bmpfileheader, 2, bfSize); /* bfSize */
  	/* we leave bfReserved1 & bfReserved2 = 0 */
  	PUT_4B(bmpfileheader, 10, headersize); /* bfOffBits */

  	/* Fill the info header (Microsoft calls this a BITMAPCOREHEADER) */
  	PUT_2B(bmpcoreheader, 0, 12);	/* bcSize */
  	PUT_2B(bmpcoreheader, 4, cinfo->output_width); /* bcWidth */
  	PUT_2B(bmpcoreheader, 6, cinfo->output_height); /* bcHeight */
  	PUT_2B(bmpcoreheader, 8, 1);	/* bcPlanes - must be 1 */
  	PUT_2B(bmpcoreheader, 10, bits_per_pixel); /* bcBitCount */

  	if (JFWRITE(dest->pub.output_file, bmpfileheader, 14) != (size_t) 14)
    	ERREXIT(cinfo, JERR_FILE_WRITE);
  	if (JFWRITE(dest->pub.output_file, bmpcoreheader, 12) != (size_t) 12)
    	ERREXIT(cinfo, JERR_FILE_WRITE);

  	if (cmap_entries > 0)
    	write_colormap(cinfo, dest, cmap_entries, 3);
}

static void finish_output_bmp(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  	bmp_dest_ptr dest = (bmp_dest_ptr) dinfo;
  	register FILE * outfile = dest->pub.output_file;
  	JSAMPARRAY image_ptr;
  	register JSAMPROW data_ptr;
  	JDIMENSION row;
  	register JDIMENSION col;
  	cd_progress_ptr progress = (cd_progress_ptr) cinfo->progress;

  /* Write the header and colormap */
  	if (dest->is_os2)
    	write_os2_header(cinfo, dest);
  	else
    	write_bmp_header(cinfo, dest);

  /* Write the file body from our virtual array */
  	for (row = cinfo->output_height; row > 0; row--) {
    	if (progress != NULL) {
      		progress->pub.pass_counter = (long) (cinfo->output_height - row);
      		progress->pub.pass_limit = (long) cinfo->output_height;
      		(*progress->pub.progress_monitor) ((j_common_ptr) cinfo);
    	}
    	image_ptr = (*cinfo->mem->access_virt_sarray)((j_common_ptr) cinfo, dest->whole_image, row-1, (JDIMENSION) 1, FALSE);
    	data_ptr = image_ptr[0];
    	for (col = dest->row_width; col > 0; col--) {
      		putc(GETJSAMPLE(*data_ptr), outfile);
      		data_ptr++;
    	}
  	}
  	if (progress != NULL)
    	progress->completed_extra_passes++;
	
  /* Make sure we wrote the output file OK */
  	fflush(outfile);
  	if (ferror(outfile))
    	ERREXIT(cinfo, JERR_FILE_WRITE);
}


/*
 * The module selection routine for BMP format output.
 */

static djpeg_dest_ptr jinit_write_bmp(j_decompress_ptr cinfo, boolean is_os2)
{
  	bmp_dest_ptr dest;
  	JDIMENSION row_width;

  /* Create module interface object, fill in method pointers */
  	dest = (bmp_dest_ptr)((*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_IMAGE, sizeof(bmp_dest_struct)));
  	dest->pub.start_output = start_output_bmp;
  	dest->pub.finish_output = finish_output_bmp;
  	dest->is_os2 = is_os2;

  	if (cinfo->out_color_space == JCS_GRAYSCALE) {
    	dest->pub.put_pixel_rows = put_gray_rows;
  	} 
	else if (cinfo->out_color_space == JCS_RGB) {
    	if (cinfo->quantize_colors)
      		dest->pub.put_pixel_rows = put_gray_rows;
    	else
      		dest->pub.put_pixel_rows = put_pixel_rows;
  	} 
	else {
    	ERREXIT(cinfo, JERR_BMP_COLORSPACE);
  	}

  /* Calculate output image dimensions so we can allocate space */
  	jpeg_calc_output_dimensions(cinfo);

  /* Determine width of rows in the BMP file (padded to 4-byte boundary). */
  	row_width = cinfo->output_width * cinfo->output_components;
  	dest->data_width = row_width;
  	while ((row_width & 3) != 0) row_width++;
  	dest->row_width = row_width;
  	dest->pad_bytes = (int) (row_width - dest->data_width);

  /* Allocate space for inversion array, prepare for write pass */
  	dest->whole_image = (*cinfo->mem->request_virt_sarray)((j_common_ptr) cinfo, JPOOL_IMAGE, FALSE, row_width, cinfo->output_height, (JDIMENSION) 1);
  	dest->cur_output_row = 0;
  	if (cinfo->progress != NULL) {
    	cd_progress_ptr progress = (cd_progress_ptr) cinfo->progress;
    	progress->total_extra_passes++; /* count file input as separate pass */
  	}

  /* Create decompressor output buffer. */
  	dest->pub.buffer = (*cinfo->mem->alloc_sarray)((j_common_ptr) cinfo, JPOOL_IMAGE, row_width, (JDIMENSION) 1);
  	dest->pub.buffer_height = 1;

  	return (djpeg_dest_ptr) dest;
}
