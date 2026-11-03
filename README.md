[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/apps/mousepad/-/blob/master/COPYING)

# mousepad

Mousepad is a simple text editor for the [Xfce](https://www.xfce.org) desktop environment.

Mousepad aims to be an easy-to-use and fast editor. Our target is an
editor for quickly editing text files, not a development environment or an
editor with a huge bunch of plugins. On the other hand we try to use the latest
GTK features available, which means that if GTK adds something new in a major
release that is useful for the editor, we will likely bump the GTK dependency
and integrate this new feature in Mousepad.

----

### Homepage

[Mousepad documentation](https://docs.xfce.org/apps/mousepad/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/apps/mousepad/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Required Packages

Mousepad depends on the following packages:

* [GLib](https://gitlab.gnome.org/GNOME/glib) >= 2.52.0 (including GIO and GModule)
* [GTK](https://www.gtk.org) >= 3.22.0
* [GtkSourceView](https://gitlab.gnome.org/GNOME/gtksourceview) >= 3.24.0 or >= 4.0.0
* [polkit](https://gitlab.freedesktop.org/polkit/polkit)

### Plugins

Mousepad optionally depends on the following packages:

* [gspell](https://gitlab.gnome.org/GNOME/gspell) >= 1.6.0
* [libxfce4ui](https://gitlab.xfce.org/xfce/libxfce4ui) >= 4.17.5

### Build Requirements

* [Meson](https://mesonbuild.com) and [Ninja](https://ninja-build.org)
* A C compiler and `pkg-config`
* [GNU gettext](https://www.gnu.org/software/gettext/) tools
* GLib build tools

### Source Code Repository

[Mousepad source code](https://gitlab.xfce.org/apps/mousepad)

### Download a Release Tarball

[Mousepad archive](https://archive.xfce.org/src/apps/mousepad)
    or
[Mousepad tags](https://gitlab.xfce.org/apps/mousepad/-/tags)

### Installation

From source code repository:

    % git clone https://gitlab.xfce.org/apps/mousepad.git
    % cd mousepad
    % meson setup build
    % meson compile -C build
    % meson install -C build

From release tarball:

    % tar xf mousepad-<version>.tar.xz
    % cd mousepad-<version>
    % meson setup build
    % meson compile -C build
    % meson install -C build

From [Flathub](https://flathub.org/apps/org.xfce.mousepad):

    % flatpak install flathub org.xfce.mousepad

### Uninstallation

From source code repository and release tarball:

    % ninja uninstall -C build

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/apps/mousepad/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.
