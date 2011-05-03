##SUBDIRS = util
lib_LTLIBRARIES = libxss.la
libxss_la_SOURCES = util/list/list.h \
					util/list/list.c \
					util/jpeg2bmp/cdjpeg.h \
					util/jpeg2bmp/cderror.h \
				 	util/jpeg2bmp/jerror.h \
					util/jpeg2bmp/djpeg.c  \
					util/bmp_png/common.h \
					util/bmp_png/bmphed.h \
					util/bmp_png/common.c \
					util/bmp_png/bmp_png.h \
					util/bmp_png/bmp2png.c \
					util/bmp_png/png2bmp.c \
					g_save.c \
		    		pixbuf.c \
		    		shot.c
##libxss_la_LIBADD = util/libutil.la
INCLUDES = -I$(top_srcdir)/inc -I$(top_srcdir)/src/util/list
LIBS += -lX11 -ljpeg -lpng -ltiff

