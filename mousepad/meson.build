libmousepad_sources = [
  'mousepad-application.c',
  'mousepad-application.h',
  'mousepad-close-button.c',
  'mousepad-close-button.h',
  'mousepad-dialogs.c',
  'mousepad-dialogs.h',
  'mousepad-document.c',
  'mousepad-document.h',
  'mousepad-encoding-dialog.c',
  'mousepad-encoding-dialog.h',
  'mousepad-encoding.c',
  'mousepad-encoding.h',
  'mousepad-file.c',
  'mousepad-file.h',
  'mousepad-history.c',
  'mousepad-history.h',
  'mousepad-plugin-provider.c',
  'mousepad-plugin-provider.h',
  'mousepad-plugin.c',
  'mousepad-plugin.h',
  'mousepad-prefs-dialog.c',
  'mousepad-prefs-dialog.h',
  'mousepad-print.c',
  'mousepad-print.h',
  'mousepad-private.h',
  'mousepad-replace-dialog.c',
  'mousepad-replace-dialog.h',
  'mousepad-search-bar.c',
  'mousepad-search-bar.h',
  'mousepad-settings-store.c',
  'mousepad-settings-store.h',
  'mousepad-settings.c',
  'mousepad-settings.h',
  'mousepad-statusbar.c',
  'mousepad-statusbar.h',
  'mousepad-util.c',
  'mousepad-util.h',
  'mousepad-view.c',
  'mousepad-view.h',
  'mousepad-window.c',
  'mousepad-window.h',
]

libmousepad_sources += gnome.genmarshal(
  'mousepad-marshal',
  sources: 'mousepad-marshal.list',
  prefix: '_mousepad_marshal',
  internal: true,
  install_header: false,
)

libmousepad_sources += gnome.compile_resources(
  'mousepad-resources',
  'resources' / 'mousepad.gresource.xml',
  source_dir: 'resources',
)

c_args = [
  '-DG_LOG_DOMAIN="@0@"'.format('Mousepad'),
  '-DMOUSEPAD_GSETTINGS_SCHEMA_DIR="@0@"'.format(gio_schemasdir),
  '-DMOUSEPAD_PLUGIN_DIRECTORY="@0@"'.format(mousepad_plugin_directory),
]
if get_option('keyfile-settings')
  c_args += '-DMOUSEPAD_SETTINGS_KEYFILE_BACKEND=1'
endif

mousepad_so_version = '0'
mousepad_version = '@0@.0.0'.format(mousepad_so_version)

libmousepad = library(
  'mousepad',
  libmousepad_sources,
  sources: xfce_revision_h,
  version: mousepad_version,
  soversion: mousepad_so_version,
  c_args: c_args,
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gmodule,
    gtk,
    gtksourceview,
    libm,
  ],
  install: true,
)

executable(
  'mousepad',
  [
    'main.c',
  ],
  sources: xfce_revision_h,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('Mousepad'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libintl,
  ],
  link_with: [
    libmousepad,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

install_data('org.xfce.mousepad.gschema.xml', install_dir: gio_schemasdir)
