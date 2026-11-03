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

See [meson.build](https://gitlab.xfce.org/apps/mousepad/-/blob/master/meson.build) for the list of required and optional dependencies and their minimum versions.

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
