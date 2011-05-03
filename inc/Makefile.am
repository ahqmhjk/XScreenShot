libxssincludedir = $(includedir)/xss
libxssinclude_HEADERS = g_def.h g_pixbuf.h transform.h crosshair.xbm crosshair_mask.xbm

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(libxssincludedir)

uninstall:
	@rm -rf $(libxssincludedir)
