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
}

abort ()
{
  local files

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
  if [ -z "$ignored" ]; then
    cat
  else
    grep -E -v "${ignored[@]}"
  fi
}

purge_logs ()
{
  [ -s "$1" ] && {
    warnings[n_warns++]=$n_cmds
    sed 's/^/  /' <"$1"
    : >"$1"
  }
}



#
# Functions common only to test functions: they can modify local variables of these test
# functions in addition to their own local variables
#
log_and_run_mousepad ()
{
  local out

  ((n_cmds++))
  if [ "$1" = '--quit' ]; then
    echo "Command $n_cmds: $mousepad --quit" | duperr
    $timeout pwait -x mousepad 2>&1 &
    pid=$!
    $timeout "$mousepad" --quit 2>&1 &
  else
    out=$1
    shift
    echo "Command $n_cmds: $mousepad $*" | duperr
    "$mousepad" "$@" 2> >(filter >"$out") 1>/dev/null &
    pid=$!
  fi
}

wait_and_term_mousepad ()
{
  $timeout pwait -x mousepad 2>&1 &
  pid=$!
  pkill -x mousepad &
}

wait_and_kill_mousepad ()
{
  local -i r pid=$1

  wait $pid
  r=$?
  ((r >= 124)) && {
    $timeout pwait -x mousepad 2>&1 &
    pid=$!
    pkill -9 -x mousepad &
    wait $pid
    r=$?
  }

  return $r
}

log_results ()
{
  [ -s "$1" ] && {
    warnings[n_warns++]=$n_cmds
    sed 's/^/  /' <"$1"
    printf '%s\n\n' "Exit code: $2"
  }

  (($2 != 0)) && {
    errors[n_errs++]=$n_cmds
    [ ! -s "$1" ] && printf '%s\n\n' "Exit code: $2"
  }
}
