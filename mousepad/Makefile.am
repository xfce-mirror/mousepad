AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DBINDIR=\"$(bindir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DG_LOG_DOMAIN=\"Mousepad\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	mousepad

mousepad_built_sources = \
	mousepad-marshal.c \
	mousepad-marshal.h

mousepad_SOURCES = \
	$(mousepad_built_sources) \
	main.c \
	mousepad-application.c \
	mousepad-application.h \
	mousepad-close-button.c \
	mousepad-close-button.h \
	mousepad-dialogs.c \
	mousepad-dialogs.h \
	mousepad-document.c \
	mousepad-document.h \
	mousepad-encoding.c \
	mousepad-encoding.h \
	mousepad-encoding-dialog.c \
	mousepad-encoding-dialog.h \
	mousepad-file.c \
	mousepad-file.h \
	mousepad-prefs-dialog.c \
	mousepad-prefs-dialog.h \
	mousepad-print.c \
	mousepad-print.h \
	mousepad-private.h \
	mousepad-replace-dialog.c \
	mousepad-replace-dialog.h \
	mousepad-resources.c \
	mousepad-search-bar.c \
	mousepad-search-bar.h \
	mousepad-settings.c \
	mousepad-settings.h \
	mousepad-settings-store.c \
	mousepad-settings-store.h \
	mousepad-statusbar.c \
	mousepad-statusbar.h \
	mousepad-view.c \
	mousepad-view.h \
	mousepad-util.c \
	mousepad-util.h \
	mousepad-window.c \
	mousepad-window.h

mousepad_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(GSPELL_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	$(XFCONF_CFLAGS) \
	-DMOUSEPAD_GSETTINGS_SCHEMA_DIR=\""$(datadir)/glib-2.0/schemas"\" \
	-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_60

mousepad_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

mousepad_LDADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS)	\
	$(GTHREAD_LIBS) \
	$(GTKSOURCEVIEW_LIBS) \
	$(GSPELL_LIBS) \
	$(XFCONF_LIBS)

if WITH_KEYFILE_SETTINGS
mousepad_CFLAGS += \
	-DMOUSEPAD_SETTINGS_KEYFILE_BACKEND=1
endif

if MAINTAINER_MODE
DISTCLEANFILES = \
	$(mousepad_built_sources) \
	mousepad-resources.c

BUILT_SOURCES = \
	$(mousepad_built_sources) \
	mousepad-resources.c

mousepad_resource_deps = $(shell glib-compile-resources --generate-dependencies --sourcedir $(srcdir)/resources --sourcedir $(builddir)/resources $(builddir)/resources/mousepad.gresource.xml)

mousepad_resources_c_deps = $(builddir)/resources/mousepad.gresource.xml $(mousepad_resource_deps)

mousepad-resources.c: $(mousepad_resources_c_deps)
	$(AM_V_GEN) glib-compile-resources \
		--target="$@" \
		--sourcedir="$(srcdir)/resources" \
		--sourcedir="$(builddir)/resources" \
		--generate-source "$(builddir)/resources/mousepad.gresource.xml"

mousepad-marshal.h: mousepad-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --header --prefix=_mousepad_marshal $< > $@

mousepad-marshal.c: mousepad-marshal.list Makefile
	$(AM_V_GEN) echo "#include <mousepad/mousepad-marshal.h>" > $@ \
	&& glib-genmarshal --body --prefix=_mousepad_marshal $< >> $@
endif

EXTRA_DIST = \
	$(mousepad_resource_deps) \
	resources/mousepad.gresource.xml \
	mousepad-marshal.list \
	org.xfce.mousepad.gschema.xml

gsettings_SCHEMAS = org.xfce.mousepad.gschema.xml
@GSETTINGS_RULES@

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
