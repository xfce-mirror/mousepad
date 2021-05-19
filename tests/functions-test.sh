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
  [ -n "$out" ] && {
    warnings[n_warns++]=$n_cmds
    sed 's/^/  /' <<<"$out"
    printf '%s\n\n' "Exit code: $r"
  }

  ((r != 0)) && {
    errors[n_errs++]=$n_cmds
    [ -z "$out" ] && printf '%s\n\n' "Exit code: $r"
  }
}

test_simple_gui ()
{
  local    out
  local -i r pid id

  # exit if ever the previous mousepad instance didn't terminate
  [ -n "$(pgrep -x mousepad)" ] && abort 'running'

  # log and run the mousepad command
  out=$(mktemp) || abort 'file'
  log_and_run_mousepad "$out" "$@"

  # wait for the window to appear on screen, meaning mousepad is idle
  id=$($timeout xdotool search --sync --onlyvisible --pid $pid | head -1) 2>&1

  # quit as gracefully as possible, testing `mousepad --quit` in various contexts by the way
  if ((id != 0)); then
    if [[ $* != *--disable-server* ]]; then
      # purge the logs and run the quit command
      purge_logs "$out"
      log_and_run_mousepad --quit
    else
      $timeout pwait -x mousepad 2>&1 &
      pid=$!
      wmctrl -i -c $id &
    fi
  else
    wait_and_term_mousepad
  fi

  # send KILL signal if needed
  wait_and_kill_mousepad $pid
  r=$?

  # log results
  log_results "$out" "$r"

  # cleanup
  rm "$out"
}

test_gsettings ()
{
  local    out bak schema key value
  local -a schemas keys values
  local -i r pid id n

  # exit if ever the previous mousepad instance didn't terminate
  [ -n "$(pgrep -x mousepad)" ] && abort 'running'

  # store current configuration and mark all settings as manually set, so that
  # reseting them afterwards will always trigger the corresponding mousepad handlers
  bak="$logdir/$script_name.gsettings.bak"
  : >"$bak" || abort 'file'
  gsettings list-recursively org.xfce.mousepad | sort >"$bak"

  while read schema key value; do
    schemas+=("$schema")
    keys+=("$key")
    values+=("$value")
    gsettings set "$schema" "$key" "$value"
  done <"$bak"

  # log and run the mousepad command
  out=$(mktemp) || abort 'file'
  log_and_run_mousepad "$out" "$@"

  # wait for the window to appear on screen, meaning mousepad is idle
  id=$($timeout xdotool search --sync --onlyvisible --pid $pid | head -1) 2>&1

  # a working mousepad is a prerequisite here
  if ((id != 0)); then
    for n in ${!schemas[*]}; do
      # purge the logs and run the gsettings command
      purge_logs "$out"
      ((n_cmds++))
      echo "Command $n_cmds: gsettings reset ${schemas[n]} ${keys[n]}" | duperr
      gsettings reset "${schemas[n]}" "${keys[n]}"

      # give mousepad some time to handle the setting change
      sleep 1
    done

    # purge the logs and run the quit command
    purge_logs "$out"
    log_and_run_mousepad --quit
  else
    wait_and_term_mousepad
  fi

  # send KILL signal if needed
  wait_and_kill_mousepad $pid
  r=$?

  # restore the settings
  for n in ${!schemas[*]}; do
    gsettings set "${schemas[n]}" "${keys[n]}" "${values[n]}"
  done

  # log results
  log_results "$out" "$r"

  # cleanup
  rm "$out" "$bak"
}

# About the string variables 'included' 'excluded' and 'extrawins', and the array variables
# 'prereqs' 'postprocs' and 'param_actions' below:
# * 'included' and 'excluded' are ERE passed to awk to filter the action list provided by gdbus,
#   so remember to double-escape characters when needed, e.g. '\\.' for a litteral dot.
# * 'extrawins' is an ERE passed to bash to know which actions generate an extra window to be
#   closed, so do not double-escape characters here, e.g. '\.' for a litteral dot.
# * 'prereqs' and 'postprocs' are pairs (action - prerequisites/postprocesses), where:
#   * 'action' is an action name
#   * 'prerequisites/postprocesses' are sequences of actions separated by '\n', to be run
#     before/after the above action during the test process.
#   Example: prereqs=('edit.paste' 'edit.select-all\nedit.copy')
# * 'param_actions' are pairs (action - param), where:
#   * 'action' is an action name
#   * 'param' is the parameter to pass to the above action (typically for radio actions)
#   Example: 'document.filetype' "<'sh'>"
# NB: actions are sorted at the output of `gdbus | awk`, so the above array variables must be
#     pre-sorted (they are browsed only once).
test_actions ()
{
  local    out type=$1 included excluded extrawins action param
  local -a cmd prereqs postprocs param_actions actions
  local -i r pid main_id id m=0 n

  # exit if ever the previous mousepad instance didn't terminate
  [ -n "$(pgrep -x mousepad)" ] && abort 'running'

  # log and run the mousepad command
  shift
  out=$(mktemp) || abort 'file'
  log_and_run_mousepad "$out" "$@"

  # wait for the window to appear on screen, meaning mousepad is idle
  main_id=$($timeout xdotool search --sync --onlyvisible --pid $pid | head -1) 2>&1

  # a working mousepad is a prerequisite here
  if ((main_id != 0)); then
    case $type in
    '--off-menu')
      included='font-size'
      excluded='^[^.]+\\.'
      extrawins='$^'
    ;;
    '--file')
      included='^file\\.'
      excluded='\\.(new-from-template\\.new|open-recent\\.new|close-window)$'
      extrawins='\.(open|open-recent\.clear-history|save-as|print|new-window|detach-tab)$'
      prereqs=('file.close-tab' 'file.new' 'file.detach-tab' 'file.new')
      postprocs=('file.new' 'file.close-tab')
    ;;
    '--edit')
      included='^edit\\.'
      excluded='\\.(paste-from-history|select-all|undo)$'
      prereqs=(
        'edit.copy' 'edit.select-all' 'edit.cut' 'edit.select-all'
        'edit.paste' 'edit.select-all\nedit.copy'
        'edit.paste-special.paste-as-column' 'edit.select-all\nedit.copy' 'edit.redo' 'edit.undo'
      )
      postprocs=(
        'edit.convert.spaces-to-tabs' 'edit.undo' 'edit.convert.tabs-to-spaces' 'edit.undo'
        'edit.cut' 'edit.undo' 'edit.delete-selection' 'edit.undo' 'edit.redo' 'file.save'
      )
      extrawins='$^'

      # we need the view to be focused for some actions, but don't abort if that fails
      xdotool windowfocus $main_id 2>&1
    ;;
    '--search')
      included='^search\\.'
      excluded='$^'
      extrawins='\.(find-and-replace|go-to)$'
    ;;
    '--view')
      included='^view\\.|bar-visible$'
      excluded='$^'
      postprocs=(
        'preferences.window.menubar-visible' 'preferences.window.menubar-visible'
        'preferences.window.statusbar-visible' 'preferences.window.statusbar-visible'
        'preferences.window.toolbar-visible' 'preferences.window.toolbar-visible'
        'view.fullscreen' 'view.fullscreen'
      )
      extrawins='\.select-font$'
    ;;
    '--document')
      included='^document'
      excluded='$^'
      prereqs=('document' 'file.new')
      postprocs=(
        'document.go-to-tab' 'document.go-to-tab'
        'document.viewer-mode' 'document.viewer-mode'
        'document.write-unicode-bom' 'document.write-unicode-bom\nfile.save'
      )
      extrawins='\.tab\.tab-size$'
      param_actions=(
        'document.filetype' "<'sh'>" 'document.go-to-tab' '<1>' 'document.go-to-tab' '<0>'
        'document.line-ending' '<1>' 'document.tab.tab-size' '<0>'
      )
    ;;
    '--help')
      included='^help\\.'
      excluded='\\.contents$'
      extrawins='\.about$'
    ;;
    *)
      # try to quit gracefully and abort
      "$mousepad" --quit
      abort "${FUNCNAME[0]}(): Wrong argument '$type'"
    ;;
    esac

    # filter action list
    mapfile -t actions < <(
      gdbus call --session --dest org.xfce.mousepad --object-path /org/xfce/mousepad/window/1 \
      --method org.gtk.Actions.List | sed "s/(\['\|'\],)//g; s/', '/\n/g" | sort \
      | awk -v inc="$included" -v exc="$excluded" -v _pre="${prereqs[*]}" -v _post="${postprocs[*]}" '
        BEGIN { split (_pre, pre, "[ ]"); split (_post, post, "[ ]"); m = n = 1 }
        ($0 ~ inc && $0 !~ exc) {
          if ($0 == pre[m]) { print pre[++m]; m++ }
          print
          if ($0 == post[n]) { print post[++n]; n++ }
        }'
    )

    for action in "${actions[@]}"; do
      # purge the logs and run the gdbus command
      purge_logs "$out"
      ((n_cmds++))
      param=''
      [ "${param_actions[m]}" = "$action" ] && {
        param=${param_actions[++m]}
        ((m++))
      }

      echo "Command $n_cmds: gdbus call ... $action \"[$param]\" '{}'" | duperr
      gdbus call --session --dest org.xfce.mousepad --object-path /org/xfce/mousepad/window/1 \
        --method org.gtk.Actions.Activate "$action" "[$param]" '{}' >/dev/null &

      # wait for the end of the action when possible
      timeout 3 pwait -x gdbus

      # when needed, wait for the new window (main or dialog) to appear on screen and close it
      [[ $action =~ $extrawins ]] && {
        for ((n = 1; ; n++)); do
          id=$(xdotool search --sync --onlyvisible --pid $pid 2>/dev/null | tail -1)
          ((id != main_id && id != 0)) && {
            wmctrl -i -c $id
            break
          }

          ((n == 10)) && abort "${FUNCNAME[0]}(): Waiting time exceeded"
          sleep 1
        done
      }
    done

    # purge the logs and run the quit command
    purge_logs "$out"
    log_and_run_mousepad --quit
  else
    wait_and_term_mousepad
  fi

  # send KILL signal if needed
  wait_and_kill_mousepad $pid
  r=$?

  # log results
  log_results "$out" "$r"

  # cleanup
  rm "$out"
}
