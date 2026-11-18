#!/usr/bin/env python3
"""Link the integration test with the exact object files of a Ninja build."""
import pathlib
import shlex
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
build = pathlib.Path(sys.argv[1]).resolve()
target = build / 'smoke-test' / 'multimonitor-smoke'
target.parent.mkdir(exist_ok=True)
objects = sorted((build / 'src/CMakeFiles/flameshot.dir').rglob('*.o'))
objects = [str(item) for item in objects if item.name != 'main.cpp.o']
if not objects:
    raise SystemExit('Build the flameshot target first')
flags = shlex.split(subprocess.check_output([
    'pkg-config', '--cflags', '--libs', 'Qt6Widgets', 'Qt6Svg',
    'Qt6Network', 'Qt6DBus'], text=True))
command = ['c++', '-std=c++20', '-fPIC', '-O2', '-DDISABLE_UPDATE_CHECKER',
           '-DUSE_PORTABLE_CONFIG', '-DUSE_KDSINGLEAPPLICATION=1', '-I' + str(root / 'src'),
           '-I' + str(build), str(root / 'tests/multimonitor_smoke.cpp'),
           *objects,
           str(build / '_deps/qtcolorwidgets-build/libQtColorWidgets.a'),
           str(build / '_deps/kdsingleapplication-build/src/libkdsingleapplication-qt6.a'),
           *flags, '-o', str(target)]
subprocess.run(command, check=True)
(target.parent / 'flameshot.ini').write_text('''[General]
disabledTrayIcon=true
showHelp=false
showQuitPrompt=false
showAbortNotification=false
showDesktopNotification=false
showStartupLaunchMessage=false
autoCloseIdleDaemon=false
saveLastRegion=false
startupLaunch=false
drawColor=#00ff00
drawThickness=4
''')
print(target)
