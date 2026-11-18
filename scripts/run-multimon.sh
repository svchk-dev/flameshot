#!/usr/bin/env bash
set -euo pipefail
script_path=$(readlink -f -- "${BASH_SOURCE[0]}")
bundle_dir=$(dirname -- "$script_path")
binary="$bundle_dir/bin/flameshot"
unit=flameshot-multimon.service
service=org.flameshot.FlameshotMultiMonitor

has_owner() {
    local reply
    reply=$(gdbus call --session --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.NameHasOwner "$1")
    [[ "$reply" == '(true,)' ]]
}

start_daemon() {
    if has_owner "$service"; then return; fi
    if [[ ! -x "$binary" ]]; then
        printf 'Build the MultiMonitor bundle first.\n' >&2
        exit 1
    fi
    if [[ ! -s "$bundle_dir/bin/flameshot.ini" ]]; then
        python3 - "$bundle_dir/bin/flameshot.ini" <<'PY'
import configparser, os, pathlib, sys
config = configparser.ConfigParser(interpolation=None)
config.optionxform = str
config.read(pathlib.Path(os.environ.get('XDG_CONFIG_HOME', str(pathlib.Path.home() / '.config'))) / 'flameshot/flameshot.ini')
if not config.has_section('General'):
    config.add_section('General')
for key in ('checkForUpdates', 'ignoreUpdateToVersion'):
    config.remove_option('General', key)
for key in ('startupLaunch', 'showStartupLaunchMessage', 'autoCloseIdleDaemon'):
    config.set('General', key, 'false')
with open(sys.argv[1], 'w') as stream:
    config.write(stream, space_around_delimiters=False)
PY
    fi
    systemd-run --user --collect --unit="$unit" \
        --setenv=QT_QPA_PLATFORM=xcb "$binary"
    for attempt in {1..40}; do
        if has_owner "$service"; then return; fi
        sleep 0.1
    done
    printf 'MultiMonitor did not start. Check: journalctl --user -u %s\n' "$unit" >&2
    exit 1
}

action=${1:-start}
case "$action" in
    start) start_daemon ;;
    gui|capture)
        start_daemon
        shift
        exec "$binary" gui "$@"
        ;;
    stop) systemctl --user stop "$unit" ;;
    restore)
        if [[ ! -x /usr/bin/flameshot ]]; then
            printf 'The standard Flameshot package is not installed.\n' >&2
            exit 1
        fi
        systemctl --user stop "$unit" 2>/dev/null || true
        if ! has_owner org.flameshot.Flameshot; then
            systemd-run --user --collect --unit=flameshot-stock /usr/bin/flameshot
        fi
        ;;
    status) systemctl --user status "$unit" --no-pager ;;
    *) exec "$binary" "$@" ;;
esac

