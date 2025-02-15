project(
  'mousepad',
  'c',
version : '0.6.3-dev',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.57.0',
  default_options : ['c_std=gnu11', 'buildtype=debugoptimized', 'warning_level=2']
)

project_namespace = 'apps'
pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()
mousepad_plugin_directory = get_option('prefix') / get_option('libdir') / 'mousepad' / 'plugins'

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')
fs = import('fs')

dependency_versions = {
  'glib': '>= 2.56.2',
  'gtk': '>= 3.22.0',
  'gtksourceview3': '>= 3.24.0',
  'gtksourceview4': '>= 4.0.0',

  'polkit': '>= 0.102',

  'gspell': '>= 1.6.0',
  'libxfce4ui': '>= 4.17.5',
}

glib = dependency('glib-2.0', version: dependency_versions['glib'])
gio = dependency('gio-2.0', version: dependency_versions['glib'])
gmodule = dependency('gmodule-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
gtksourceview = dependency('gtksourceview-4', version: dependency_versions['gtksourceview4'], required: get_option('gtksourceview4'))
if not gtksourceview.found()
  gtksourceview = dependency('gtksourceview-3.0', version: dependency_versions['gtksourceview3'])
endif

polkit = dependency('polkit-gobject-1', version: dependency_versions['polkit'], required: get_option('polkit'))

enable_plugin = {}
plugin_deps = {}
base_deps = [glib, gtk, gtksourceview]

deps = base_deps
deps += dependency('gspell-1', version: dependency_versions['gspell'], required: get_option('gspell-plugin'))
plugin_deps += {'gspell': deps}
enable_plugin += {'gspell': deps[-1].found()}

deps = base_deps
deps += dependency('libxfce4ui-2', version: dependency_versions['libxfce4ui'], required: get_option('shortcuts-plugin'))
if deps[-1].found()
  deps += dependency('libxfce4kbd-private-3', version: dependency_versions['libxfce4ui'], required: get_option('shortcuts-plugin'))
endif
plugin_deps += {'shortcuts': deps}
enable_plugin += {'shortcuts': deps[-1].found()}

plugin_deps += {'test': base_deps}
enable_plugin += {'test': not get_option('test-plugin').disabled()}

gio_schemasdir = gio.get_variable(
  pkgconfig: 'schemasdir',
  pkgconfig_define: ['datadir', get_option('prefix') / get_option('datadir')]
)

feature_cflags = []

if cc.has_function('bind_textdomain_codeset')
  feature_cflags += '-DHAVE_BIND_TEXTDOMAIN_CODESET=1'
  libintl = dependency('', required: false)
else
  libintl = cc.find_library('intl', required: false)
  if libintl.found() and cc.has_function('bind_textdomain_codeset', dependencies: libintl)
    feature_cflags += '-DHAVE_BIND_TEXTDOMAIN_CODESET=1'
  endif
endif

if cc.has_function('realpath')
  feature_cflags += '-DHAVE_REALPATH=1'
endif

headers = [
  'errno.h',
  'locale.h',
  'math.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.underscorify().to_upper())
  endif
endforeach

libm = cc.find_library('m', required: true)

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Wno-missing-field-initializers',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 'minsize']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

desktop_files = [
  'org.xfce.mousepad-settings.desktop.in',
  'org.xfce.mousepad.desktop.in',
]
foreach file : desktop_files
  i18n.merge_file(
    input: file,
    output: fs.stem(file),
    po_dir: 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
  )
endforeach

i18n.merge_file(
  input: 'org.xfce.mousepad.appdata.xml.in',
  output: 'org.xfce.mousepad.appdata.xml',
  po_dir: 'po',
  type: 'xml',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'metainfo',
)

if polkit.found()
  policy_in = configure_file(
    configuration: configuration_data({
      'bindir': get_option('prefix') / get_option('bindir'),
    }),
    input: 'org.xfce.mousepad.policy.in.in',
    output: 'org.xfce.mousepad.policy.in',
    install: false,
  )

  i18n.merge_file(
    input: policy_in,
    output: 'org.xfce.mousepad.policy',
    po_dir: 'po',
    type: 'xml',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'polkit-1' / 'actions',
  )
endif

subdir('icons')
subdir('mousepad')
subdir('plugins')
subdir('po')

gnome.post_install(glib_compile_schemas: true)
