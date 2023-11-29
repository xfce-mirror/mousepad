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



test_non_gui ()
{
  local -a cmd
  local    out
  local -i r

  # exit if ever the previous mousepad instance didn't terminate
  [ -n "$(pgrep -x mousepad)" ] && abort 'running'

  # log and run the mousepad command
  cmd=("$mousepad" "$@")
  ((n_cmds++))
  echo "Command $n_cmds: ${cmd[*]}" | duperr

  out=$($timeout "${cmd[@]}" 2> >(filter) 1>/dev/null)
  r=$?

  # log results
  [ -z "$out" ] || {
    warnings[n_warns++]=$n_cmds
    sed 's/^/  /' <<<"$out"
    printf '%s\n\n' "Exit code: $r"
  }

  ((r == 0)) || {
    errors[n_errs++]=$n_cmds
    [ -n "$out" ] || printf '%s\n\n' "Exit code: $r"
  }
}

test_simple_gui ()
{
  local -i r=0

  # exit if ever the previous mousepad instance didn't terminate
  [ -n "$(pgrep -x mousepad)" ] && abort 'running'

  # log and run the mousepad command
  temp_logfile=$(mktemp) || abort 'file'
  log_and_run_mousepad "$@" || r=$?

  # try to quit gracefully, testing `mousepad --quit` in various contexts by the way
  ((r == 0)) && {
    if [[ $* == *--disable-server* ]]; then
      # wait for the test plugin to run the quit command internally
      $timeout $pidwait -x mousepad 2> >(indent)
    elif $timeout grep -q -x -F "$idle" 2> >(indent); then
      # purge the logs and run the quit command
      purge_logs
      log_and_run_mousepad --quit || r=$?
    fi
  }

  # send KILL signal if needed
  kill_mousepad || r=$?

  # log results
  log_results "$r"

  # cleanup
  rm "$temp_logfile"
  session_restore_cleanup
}

test_gsettings ()
{
  local    bak schema key value
  local -a schemas keys values
  local -i r=0 n

  # exit if ever the previous mousepad instance didn't terminate
  [ -n "$(pgrep -x mousepad)" ] && abort 'running'

  # store current configuration and mark all settings as manually set, so that
  # reseting them afterwards will always trigger the corresponding mousepad handlers
  bak="$logdir/$script_name.gsettings.bak"
  : >"$bak" || abort 'file'
  gsettings list-recursively org.xfce.mousepad | sort >"$bak"
  gsettings list-schemas | grep -xE 'org\.xfce\.mousepad\.plugins\.[^.]+' | sort \
    | xargs gsettings list-recursively | sort >>"$bak"

  while read schema key value; do
    schemas+=("$schema")
    keys+=("$key")
    values+=("$value")
    gsettings set "$schema" "$key" "$value"
  done <"$bak"

  # log and run the mousepad command
  temp_logfile=$(mktemp) || abort 'file'
  log_and_run_mousepad "$@" || r=$?

  # a working mousepad is a prerequisite here
  ((r == 0)) && $timeout grep -q -x -F "$idle" 2> >(indent) && {
    for n in ${!schemas[*]}; do
      # purge the logs and run the gsettings command
      purge_logs
      ((n_cmds++))
      echo "Command $n_cmds: gsettings reset ${schemas[n]} ${keys[n]}" | duperr
      gsettings reset "${schemas[n]}" "${keys[n]}"

      # give mousepad some time to handle the setting change
      sleep 1
    done

    # purge the logs and run the quit command
    purge_logs
    log_and_run_mousepad --quit || r=$?
  }

  # send KILL signal if needed
  kill_mousepad || r=$?

  # restore the settings
  for n in ${!schemas[*]}; do
    gsettings set "${schemas[n]}" "${keys[n]}" "${values[n]}"
  done

  # log results
  log_results "$r"

  # cleanup
  rm "$temp_logfile" "$bak"
  session_restore_cleanup
}

test_actions ()
{
  local    type=$1
  local -a cmd
  local -i r=0

  # exit if ever the previous mousepad instance didn't terminate
  [ -n "$(pgrep -x mousepad)" ] && abort 'running'

  # check action type
  [[ $type == --@(off-menu|file|edit|search|view|document|help) ]] \
    || abort "${FUNCNAME[0]}(): Wrong argument '$type'"

  # log and run the mousepad command
  shift
  temp_logfile=$(mktemp) || abort 'file'
  log_and_run_mousepad "$@" || r=$?

  # a working mousepad is a prerequisite here
  ((r == 0)) && $timeout grep -q -x -F "$idle" 2> >(indent) && {
    # run the set of actions
    gdbus call --session --dest 'org.xfce.mousepad' --object-path '/org/xfce/mousepad' \
      --method 'org.gtk.Actions.Activate' --timeout 60 \
        'mousepad-test-plugin.window-actions' "[<'${type:2}'>]" '{}' >/dev/null

    # purge the logs and run the quit command
    purge_logs
    log_and_run_mousepad --quit || r=$?
  }

  # send KILL signal if needed
  kill_mousepad || r=$?

  # log results
  log_results "$r"

  # cleanup
  rm "$temp_logfile"
  session_restore_cleanup
}
