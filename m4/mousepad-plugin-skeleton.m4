dnl This program is free software; you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License as published by the Free
dnl Software Foundation; either version 2 of the License, or (at your option)
dnl any later version.
dnl
dnl This program is distributed in the hope that it will be useful, but WITHOUT
dnl ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
dnl FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
dnl more details.
dnl
dnl You should have received a copy of the GNU General Public License along with
dnl this program; if not, write to the Free Software Foundation, Inc., 59 Temple
dnl Place, Suite 330, Boston, MA  02111-1307  USA



dnl MOUSEPAD_PLUGIN_SKELETON()
dnl
dnl Check whether to build and install the skeleton plugin.
dnl
AC_DEFUN([MOUSEPAD_PLUGIN_SKELETON],
[
AC_ARG_ENABLE([plugin-skeleton], [AS_HELP_STRING([--disable-plugin-skeleton], [Don't build the skeleton plugin])],
  [ac_mousepad_plugin_skeleton=$enableval], [ac_mousepad_plugin_skeleton=yes])
if test x"$ac_mousepad_plugin_skeleton" = x"yes"; then
  XDT_CHECK_PACKAGE([SKELETON_DEPENDENCY], [skeleton-dependency], [0.0.1], [], [ac_mousepad_plugin_skeleton=no])
else
  ac_mousepad_plugin_skeleton=no
fi

AC_MSG_CHECKING([whether to build the skeleton plugin])
AM_CONDITIONAL([MOUSEPAD_PLUGIN_SKELETON], [test x"$ac_mousepad_plugin_skeleton" = x"yes"])
AC_MSG_RESULT([$ac_mousepad_plugin_skeleton])
])
