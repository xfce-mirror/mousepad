plugins = [
  'gspell',
  'shortcuts',
  'test',
]

foreach plugin : plugins
  if enable_plugin[plugin]
    plugin_dir = '@0@-plugin'.format(plugin)
    module_plugin = 'mousepad-plugin-@0@'.format(plugin)
    plugin_sources = [
      plugin_dir / '@0@-plugin.c'.format(plugin),
      plugin_dir / '@0@-plugin.h'.format(plugin),
      plugin_dir / '@0@.c'.format(module_plugin),
    ]

    shared_module(
      module_plugin,
      plugin_sources,
      sources: xfce_revision_h,
      gnu_symbol_visibility: 'hidden',
      c_args: [
        '-DG_LOG_DOMAIN="@0@"'.format(module_plugin),
      ],
      include_directories: include_directories('..'),
      dependencies: plugin_deps[plugin],
      link_with: libmousepad,
      install: true,
      install_dir: mousepad_plugin_directory,
    )

    gschema_file = plugin_dir / 'org.xfce.mousepad.plugins.@0@.gschema.xml'.format(plugin)
    if fs.is_file(gschema_file)
      install_data(gschema_file, install_dir: gio_schemasdir)
    endif
  endif
endforeach
