#!/bin/bash

# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA  02111-1307  USA



#
# Functions common to the whole script: they modify only global variables (script variables)
# in addition to their own local variables
#
printerr ()
{
  local prefix

  case $1 in
  'error')
    prefix='\033[1;31mError: \033[0m'
    shift
  ;;
  'warning')
    prefix='\033[1;33mWarning: \033[0m'
    shift
  ;;
  'message')
    prefix='\033[1;32mMessage: \033[0m'
    shift
  ;;
  *)
    prefix=''
  ;;
  esac

  printf '%b%s\n' "$prefix" "$1" >&2
}

duperr ()
{
  if ((quiet)); then
    tee /dev/null
  else
    tee >(cat >&2)
  fi
}

cleanup ()
{
  local f

  for f in "${tempfiles[@]}"; do
    [ -f "$f" ] && rm "$f"
  done

  [ -f "$temp_logfile" ] && rm "$temp_logfile"

  [ "$(ps -o comm --no-headers $gdbus_pid)" = 'gdbus' ] && kill $gdbus_pid
}

abort ()
{
  local -a files

  printf '\n%s\n' '*** Aborted ***' | duperr

  case $1 in
  'running')
    echo 'A previous Mousepad instance did not terminate'
  ;;
  'file')
    echo 'Creation of temporary file failed'
  ;;
  *)
    echo "$1"
  ;;
  esac | duperr

  # see if some backup files were kept
  shopt -s nullglob
  files=("$logdir/"*.bak)
  [ -n "${files[*]}" ] \
    && echo "Some '.bak' files to restore Mousepad configuration were kept in $logdir" | duperr

  cleanup

  exit 1
}

filter ()
{
  if [ -z "${ignored[*]}" ]; then
    cat
  else
    grep -E -v "${ignored[@]}"
  fi
}

indent ()
{
  sed 's/^/  /'
}



#
# Functions common only to test functions: they can (but should not) modify local variables
# of these test functions in addition to their own local variables
#
log_and_run_mousepad ()
{
  ((n_cmds++))
  echo "Command $n_cmds: $mousepad $*" | duperr

  if [ "$1" = '--quit' ]; then
    "$mousepad" --quit 2> >(filter | indent) 1>/dev/null &
    $timeout pwait -x mousepad 2> >(indent)
  else
    "$mousepad" "$@" 2> >(filter >"$temp_logfile") 1>/dev/null &
  fi
}

kill_mousepad ()
{
  local pid

  # use a subshell to disable kill builtin only locally
  if pid=$(pgrep -x mousepad); then
    (
      enable -n kill
      kill --timeout 5000 KILL -- $pid
    )
    return 1
  else
    return 0
  fi
}

purge_logs ()
{
  while IFS='' read -r; do
    if [[ $REPLY == Command:* ]]; then
      ((n_cmds++))
      echo "Command $n_cmds:${REPLY#Command:}" | duperr
    else
      echo "  $REPLY"
      ((n_warns == 0 || warnings[-1] != n_cmds)) && warnings[n_warns++]=$n_cmds
    fi
  done <"$temp_logfile"

  : >"$temp_logfile"
}

log_results ()
{
  purge_logs
  ((n_warns != 0 && warnings[-1] == n_cmds)) && printf '%s\n\n' "Exit code: $1"

  (($1 == 0)) || {
    errors[n_errs++]=$n_cmds
    ((n_warns != 0 && warnings[-1] == n_cmds)) || printf '%s\n\n' "Exit code: $1"
  }
}

shopt -s nullglob extglob
session_restore_cleanup ()
{
  local -a files

  ((session_restore)) || return 0

  gsettings reset org.xfce.mousepad.state.application session

  files=("${XDG_DATA_HOME:-$HOME/.local/share}/Mousepad/autosave-"+([0-9]))
  [ -z "${files[*]}" ] || rm "${files[@]}"
}
