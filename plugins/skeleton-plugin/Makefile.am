if MOUSEPAD_PLUGIN_SKELETON

mousepad_plugindir = \
	$(libdir)/mousepad/plugins

mousepad_plugin_LTLIBRARIES = \
	libmousepad-plugin-skeleton.la

libmousepad_plugin_skeleton_la_SOURCES = \
	skeleton-plugin.c \
	skeleton-plugin.h \
	mousepad-plugin-skeleton.c

libmousepad_plugin_skeleton_la_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/plugins \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins \
	-DG_LOG_DOMAIN=\"mousepad-plugin-skeleton\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(SKELETON_DEPENDENCY_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	$(PLATFORM_CPPFLAGS)

libmousepad_plugin_skeleton_la_LDFLAGS = \
	-avoid-version \
	-export-dynamic \
	-module \
	$(PLATFORM_LDFLAGS)

libmousepad_plugin_skeleton_la_LIBADD = \
	$(top_builddir)/mousepad/libmousepad.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS) \
	$(SKELETON_DEPENDENCY_LIBS)

libmousepad_plugin_skeleton_la_DEPENDENCIES = \
	$(top_builddir)/mousepad/libmousepad.la

gsettings_SCHEMAS = org.xfce.mousepad.plugins.skeleton.gschema.xml

EXTRA_DIST = \
	$(gsettings_SCHEMAS)

@GSETTINGS_RULES@

endif
