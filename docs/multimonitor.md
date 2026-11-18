# Flameshot 15 MultiMonitor

Experimental Linux fork based on upstream **v15.0.0**, commit
`f4f2e4e879f0e9b4cb7efb59f6b3f44ed05b1843`.

The tray's **Take Screenshot** action and `gui` command immediately open a
continuous selection across all monitors. Explicit `screen -n N -e` requests
retain upstream's single-monitor behavior.

The image is captured through the desktop screenshot portal. The selection
overlay runs through **XWayland** (`QT_QPA_PLATFORM=xcb`) and uses a frameless
window covering the union of the monitor geometries rather than native
single-monitor fullscreen.

The fork uses a separate D-Bus service, process lock, application/cache name,
and portable configuration (`bin/flameshot.ini`). Its tray icon has a cyan
**2** badge. It can run beside the installed `/usr/bin/flameshot`.

## Build on Ubuntu

Install `g++ cmake ninja-build qt6-base-dev qt6-tools-dev qt6-tools-dev-tools
qt6-svg-dev libx11-dev`, then run:

```sh
bash scripts/build-multimon.sh /absolute/path/to/flameshot-15-multimon
```

CMake downloads the upstream-pinned QtColorWidgets and KDSingleApplication
dependencies on the first build. The script creates a user-owned bundle and
does not install or replace a system package. It disables the upstream update
checker because official binaries do not contain this patch.

## Run and return to the standard version

```sh
/absolute/path/to/flameshot-15-multimon/flameshot-multimon start
/absolute/path/to/flameshot-15-multimon/flameshot-multimon gui
/absolute/path/to/flameshot-15-multimon/flameshot-multimon restore
```

`start` adds the experimental tray icon. `gui` opens the common selection.
`restore` stops only the experimental service and ensures that the system
Flameshot is running. No autostart entries or existing hotkeys are changed.
On the first start, the bundle copies the regular configuration; subsequent
changes apply only to the bundle's copy.

## Scope and verification

The target setup is GNOME Wayland with two adjacent monitors at the same scale.
Mixed per-monitor scaling, rotation, and other compositors need separate
validation. Screenshots can contain the empty space between irregularly
arranged monitors. XWayland and a working screenshot portal are required.

`tests/multimonitor_smoke.cpp` exercises the actual CaptureWidget against the
current monitors using events delivered only inside the test process. It checks
the desktop-sized overlay, region selection and pixel correspondence on each
monitor and across their boundary, drawing, saving, and clipboard transfer.
After building with the script above, run:

```sh
python3 tests/build_multimonitor_smoke.py build-multimon
build-multimon/smoke-test/multimonitor-smoke /tmp/flameshot-multimon-test
```

The test briefly displays the overlay and replaces the clipboard with a test
capture. It does not move the system pointer or inject system-wide input.
Do not publish screenshots or local configuration files with source changes.

