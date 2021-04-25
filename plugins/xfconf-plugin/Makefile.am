if MOUSEPAD_PLUGIN_XFCONF

mousepad_plugindir = \
	$(libdir)/mousepad/plugins

mousepad_plugin_LTLIBRARIES = \
	libmousepad-plugin-xfconf.la

libmousepad_plugin_xfconf_la_SOURCES = \
	xfconf-plugin.c \
	xfconf-plugin.h \
	mousepad-plugin-xfconf.c

libmousepad_plugin_xfconf_la_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/plugins \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins \
	-DG_LOG_DOMAIN=\"mousepad-plugin-xfconf\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	$(PLATFORM_CPPFLAGS)

libmousepad_plugin_xfconf_la_LDFLAGS = \
	-avoid-version \
	-export-dynamic \
	-module \
	$(PLATFORM_LDFLAGS)

libmousepad_plugin_xfconf_la_LIBADD = \
	$(top_builddir)/mousepad/libmousepad.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS) \
	$(XFCONF_LIBS)

libmousepad_plugin_xfconf_la_DEPENDENCIES = \
	$(top_builddir)/mousepad/libmousepad.la

endif
