plugindir = $(libdir)/mousepad/plugins/helloworld

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_builddir)

plugin_DATA = helloworld.plugin
plugin_LTLIBRARIES = libhelloworld.la

libhelloworld_la_SOURCES = \
  helloworld-configurable.c \
  helloworld-configurable.h \
  helloworld-plugin.c \
  helloworld-plugin.h

libhelloworld_la_CFLAGS = $(AM_CPPFLAGS) $(LIBPEAS_CFLAGS) $(LIBPEASGTK_CFLAGS)
libhelloworld_la_LDFLAGS = -module -avoid-version -no-undefined
libhelloworld_la_LIBADD = $(LIBPEAS_LIBS) $(LIBPEASGTK_LIBS) \
	$(top_builddir)/mousepad/libmousepad.la

EXTRA_DIST = $(plugin_DATA)
