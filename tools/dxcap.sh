#!/usr/bin/env bash
# Scripted runs of both engines -- the original game under Proton and
# DXEngine -- each driven by a console class of the DXCapture package
# (tools/dxcap), named in a private ini made from the game's own for every
# run: the game's inis are never touched. docs/DEVELOPMENT.md, scripted runs.
#
#   tools/dxcap.sh setup                      build/dxcap: the SDK's UCC among links to the game's files
#   tools/dxcap.sh compile                    tools/dxcap -> build/dxcap/System/DXCapture.u (UCC, under Proton),
#                                             its test sounds, textures, mesh and song written by
#                                             tools/dxcap/testsounds.py, testtextures.py, testmeshes.py
#                                             and testmusic.py; DXCapTwin.u a copy of it (CastConsole)
#   tools/dxcap.sh original <console> [<secs>] the original, from its menu map (or DXCAP_START's URL)
#   tools/dxcap.sh dxe <console> <map> [<secs>]  DXEngine's linux-x86_64 build, straight into <map>
#                                                  (or joining a server: <map> an address, as 127.0.0.1:7790;
#                                                  <map> - its own start, no URL given, as the original's);
#                                                  <console> - keeps the stock console, and DXCAP_TIMELINE=<file>
#                                                  drives the run (tools/dxcap/timelines)
#   tools/dxcap.sh prove <map>                DXEngine's proving run: shots at 20 s and 60 s, exit at 65 s
#   tools/dxcap.sh live <address> [<secs>]    DXEngine joining a live server with the stock console its
#                                                  game wants, driven by a timeline (DXEngine's --timeline):
#                                                  DXCAP_TIMELINE=<file>, else JoinConsole's walk
#   tools/dxcap.sh dedicated <original|dxe> <actor> <map> [<secs>]
#                                                  either engine as a dedicated server on <map>, drawing no
#                                                  frame, driven by DXCapture.<actor> among its ServerActors
#
# The workspace is Port Ex Machina's: this clone sits in its repositories'
# parent folder ($DX_ROOT, or the one the clone is in), with the game, the SDK
# and the builds beside it. A run's shots and log land in
# build/dxcap/runs/<engine>-<console>-<date>-<time>/ (a folder of its own:
# with the time alone, a run at the time of day of an earlier day's wrote
# into that run's folder, their files mixed).
# The original runs in this container, never on the host, under the Proton
# build's own wine, in a Wine prefix of its own, build/dxcap/prefix (made on
# first use), never IDA's: a prefix's Wine desktop is on the display of the
# program that started its wineserver, and IDA's headless server starts one
# on the desktop's, where the game's first window fails with BadWindow.
# DXCAP_PREFIX and DXCAP_PROTON (default "Proton-CachyOS Latest") pick another
# prefix and the Proton under ~/.local/share/Steam/compatibilitytools.d.
# Both engines draw on the desktop's X display, Xwayland ($DISPLAY, or
# DXCAP_DISPLAY), each in a 1280x720 window (DXCAP_WINDOW=<w>x<h> another
# size), DXEngine through its window's X11 backend: one X server and the GPU
# for both. DXCAP_FULLSCREEN=1 runs both full screen at that size, the
# display's own: nothing switches the display, and the original's grabber
# reads the whole frame. DXCAP_RENDERER=D3D draws the original through D3DDrv, the game's
# own renderer, instead of OpenGLDrv: the same picture, gamma ramp and all,
# though the grabber can miss its shots (docs/DEVELOPMENT.md, shots).
# DXCAP_AUDIO=1 gives DXEngine real audio; it is silent otherwise.
# DXCAP_RECORD=1 sends either engine's audio to a private sink instead of the
# speakers and records it into the run's audio.wav, with the music off, or on
# with DXCAP_MUSIC=1 (tools/dxcap/sound.py reads it). DXCAP_HRTF=1 has a dxe
# run's recording made through OpenAL Soft's HRTF, as OpenAL Soft turns it on
# by itself on headphones (its stereo-encoding).
# DXCAP_UPLINK=<host>:<port> has a run's server announce itself there -- a
# master on this machine (tools/dxcap/fakemaster.py), its uplink's
# DoUplink set --, where it otherwise announces itself nowhere. DXCAP_PORT=<port> has a run's server
# listen there ([URL] Port), as for tools/dxcap/netrelay.py between it
# and a client opening 7790. DXCAP_STATS=<password> has a run's server log
# world stats (bWorldLog) and its player hold that world stats password, so a
# join carries the player's checksum (the server logs the login's URL).
# DXCAP_PRECACHE=1 gives a run's ini D3DDrv's UsePrecache on: the frame after
# a map's load shows PRECACHING.
# DXCAP_CACHEEXT=<ext> gives a run's ini that [Core.System] CacheExt, the
# extension its download cache looks packages up by.
# DXCAP_INI="<Section>:<Key>=<Value> ..." sets each key in its section of a
# run's ini, adding it, or the section, where there is none.
# DXCAP_SERVERPKGS=<dir> has a run's server find packages in <dir> too, each
# named in its ServerPackages, so that a client, whose paths lack <dir>,
# downloads them.
# DXCAP_MAPS=<dir> has a run find the maps in <dir> too, by their names
# (tools/dxcap/clientonly.py's): the game's Maps are its own.
# DXCAP_BACKEND=SDL3 gives a dxe run DXWidgets' SDL3 window, the one
# linux-x86_64's launchers start its engine with, in place of its X11 one;
# DXCAP_BACKEND=SDL2 its SDL2 window, linux-aarch64's and the Smart Pro's
# launchers', in a build with SDL2's (DXCAP_ENGINE). A timeline that drives
# the window's virtual pad has the run see that pad alone.
# DXCAP_BACKEND=Wayland gives a dxe run the engine's own Wayland window, a
# dev build's (ENABLE_WAYLAND, DXCAP_ENGINE), on the desktop's compositor
# (WAYLAND_DISPLAY), where an input method types into it;
# DXCAP_BACKEND=SDL3-Wayland its SDL3 window through SDL's Wayland driver
# there, as linux-x86_64's launchers start it on a Wayland desktop.
# DXCAP_USERINI=<file> gives a run that User.ini in place of the game's
# (IniConsole's keys given twice). DXCAP_USERINI_FIRST=1 names the run's inis
# USERINI= then INI= on the original's command line, and gives DXEngine the
# two so by --cmdline in place of --ini and --userini: each engine reads INI=
# by its own name only, not within USERINI= (dx-reverse-info cli-flags.md).
# DXCAP_INI_CASE=1 gives the original's INI= and USERINI=, and DXEngine's by
# --cmdline, their names in capitals, the files on disk as they are: each
# engine finds them in any case, as Windows' file system does.
# DXCAP_READONLY=1 makes the run's two inis read-only once made, every write
# bit off (chmod a-w), so that the original's open for writing is refused
# under Wine as on Windows (ReadOnlyConsole): their SHA-256 sums before and
# after the run go into its readonly-before.txt and readonly-after.txt, the
# same when neither was written, and the owner's write bit comes back after.
# DXCAP_KILL=1 ends an original run at its time limit by a kill, not
# taskkill's close, so the run shows what the game wrote before any exit (a
# dxe run is always killed at its limit); either run's two inis, as it left
# them, then go into its ini-after.ini and userini-after.ini
# (IniFlushConsole, WindowSizeConsole). The original's log is written only
# as it exits: a killed original run has none.
# DXCAP_FLAGS="<flags>" puts the original's flags (-norunaway) on its command
# line, and on DXEngine's by --cmdline.
# DXCAP_STDIN=<script> makes what the script prints, as it runs, a dxe run's
# standard input, a background job's /dev/null otherwise: a dedicated
# server's typed lines (TypedServer with tools/dxcap/typedlines.sh).
# DXCAP_CONFIG="<key>=<value> ..." gives the run's console class those keys
# in its section of the ini, and DXCAP_MISSION_MAP=<map> its TargetMap, the
# map it travels to: an original run starts at its menu map, a dxe run at
# the map it is given (each console's keys: docs/DEVELOPMENT.md, scripted
# runs).
# DXCAP_START=<url> starts an original run there, not at its menu map: its
# command line's first word, which the original takes only with -hax0r
# (?loadgame=9 for StartConsole, a run started on slot 9's save, and for
# CensusConsole with Load=False).
# DXCAP_ENGINE=<binary> runs another build of DXEngine than linux-x86_64's
# (an ASan build). A Windows build's DXEngine.exe (windows-x86_64's) runs
# under the original's wine, in its prefix, its files by Wine's names for
# them, in its SDL3 window (DXCAP_BACKEND=Win32: its own), its settings by a
# --userdir of the run's own (DXCAP_SETTINGS) whose shots the run collects;
# DXCAP_DXVK=1 draws its D3D11 through Proton's DXVK in place of Wine's own.
# DXCAP_RECORD and DXCAP_PERF are for Linux builds. DXCAP_MEMLOG=1 logs
# either engine's resident memory each second into the run's memlog.txt:
# seconds since its process started, then MB.
# DXCAP_SETTINGS=<file> gives a dxe run that Settings.json in place of
# ~/.config/DXEngine's, through a HOME of the run's own (the caches stay
# the usual ones). DXCAP_USERDIR=<dir> gives a dxe run the engine's
# --userdir: Settings.json, its log, its shots and a class's own config file
# there. DXCAP_NOVSYNC=1 has Mesa's GL draw either engine without
# waiting for the display (vblank_mode=0), whatever the engine asks: the
# original's OpenGLDrv asks nothing and gets the driver's vsync.
# DXCAP_FPS=<n> holds either engine to n frames a second:
# MangoHud's limiter, its display off (mangohud, and lib32-mangohud for the
# original's 32-bit process); not the software device in its X11 window,
# whose present (XShmPutImage) MangoHud does not hook. DXCAP_BURST=1 samples
# either engine's window frame by frame with tools/dxcap/burst.py into the
# run's burst.csv, the frames LightConsole's Burst marks (an original run's in
# grab.py's place, whose five grabs a second would only slow it). DXCAP_PERF=1 samples
# the engine's main thread with the parent folder's deps/perf
# (tools/host-tools.sh) into the run's perf.data, on the wall clock, with
# the process's maps.txt; tools/perf/frame-report.py reads them against
# PerfConsole's windows.
set -euo pipefail
# A die inside $(...) ends the run too (prove's cmd_dxe): without this, bash
# clears errexit in a command substitution
shopt -s inherit_errexit
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DX_ROOT="${DX_ROOT:-$(cd "$HERE/../.." && pwd)}"
say() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

# An interrupt ends what the script started too: the jobs it backgrounded
# start with SIGINT ignored and timeout puts the engine in a process group
# of its own, so a Ctrl-C ended the script alone, the engine running to its
# limit and grab.py until killed by hand.
GRAB_PID=""
on_interrupt() {
    trap - INT TERM
    [ -z "$GRAB_PID" ] || kill "$GRAB_PID" 2>/dev/null || true
    pkill -f "^DeusEx.exe" 2>/dev/null || true
    pkill -x DXEngine 2>/dev/null || true
    pkill -x DXEngine.exe 2>/dev/null || true
    rec_stop 2>/dev/null || true
    exit 130
}
trap on_interrupt INT TERM

GAME="$DX_ROOT/gamefiles"
CAP="$DX_ROOT/build/dxcap"
SDK="$DX_ROOT/reference/ReleaseSDK1112f/System"
# Absolute: the engine is started from gamefiles, where a relative path
# named another file (timeout's 127 in engine.log, the run exiting 0)
ENGINE_BIN="$(realpath -m "${DXCAP_ENGINE:-$DX_ROOT/build/linux-x86_64/engine/DXEngine}")"
# A Windows build of DXEngine, run under wine (DXCAP_ENGINE).
windows_engine() { [[ "$ENGINE_BIN" == *.exe ]]; }

usage() { sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d; s/^# \{0,1\}//'; exit 2; }
# Every command but help needs the game.
need_game() { [ -d "$GAME/System" ] || die "no game install at $GAME"; }

# The recording: a null sink that the game's stream goes to (PULSE_SINK),
# and parecord on its monitor, detached so it outlives the call.
rec_start() {
    local wav="$1"
    pactl list short sinks | grep -q '[[:space:]]dxcap[[:space:]]' ||
        pactl load-module module-null-sink sink_name=dxcap sink_properties=device.description=dxcap > "$CAP/sink.module"
    setsid nohup parecord --device=dxcap.monitor --file-format=wav "$wav" > /dev/null 2>&1 < /dev/null &
    echo $! > "$CAP/rec.pid"
    sleep 1
}

rec_stop() {
    # Not an && list: as the last command of a function called at the end
    # of an || list, errexit is live, and a kill that failed ended the script
    # there with no message, the sink loaded and the shots uncollected
    if [ -f "$CAP/rec.pid" ]; then
        kill "$(cat "$CAP/rec.pid")" 2>/dev/null || true
    fi
    rm -f "$CAP/rec.pid"
    sleep 1
    if [ -f "$CAP/sink.module" ]; then
        pactl unload-module "$(cat "$CAP/sink.module")" || true
        rm -f "$CAP/sink.module"
    fi
}

# DXCAP_NOVSYNC: Mesa's vblank_mode. DXCAP_FPS: MangoHud preloaded into the
# engine's process, its limiter on and its display off; the dynamic linker
# takes lib32's copy for the original's 32-bit wine process ($LIB).
# Checked in the main shell before a run, lib32's too for the original
# (fps_check original), whose process would otherwise run uncapped: fps_env
# runs in the engine's background subshell, where a die ended that subshell
# alone and the run went on without the game, exiting 0.
fps_check() {
    [ -n "${DXCAP_FPS:-}" ] || return 0
    [ -f /usr/lib/mangohud/libMangoHud_shim.so ] || die "DXCAP_FPS needs MangoHud (mangohud)"
    [ "${1:-}" != original ] || [ -f /usr/lib32/mangohud/libMangoHud_shim.so ] ||
        die "DXCAP_FPS needs lib32-mangohud for the original's 32-bit process"
}

fps_env() {
    [ "${DXCAP_NOVSYNC:-0}" != 1 ] || export vblank_mode=0
    [ -n "${DXCAP_FPS:-}" ] || return 0
    [ -f /usr/lib/mangohud/libMangoHud_shim.so ] || die "DXCAP_FPS needs MangoHud (mangohud, and lib32-mangohud for the original)"
    export MANGOHUD=1 MANGOHUD_CONFIG="no_display,fps_limit=$DXCAP_FPS"
    export LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD:}/usr/\$LIB/mangohud/libMangoHud_shim.so"
}

# DXCAP_PERF: the engine's main thread (its process's first) sampled until it
# exits, its samples stamped with the wall clock (-k realtime) so that they
# can be cut to PerfConsole's windows; its cycles and instructions counted
# each 100 ms (stat.csv, from the wall-clock time in stat.start), which a
# CPU's clock speed does not change; the process's maps once its libraries
# are in, for the original's DLLs. Prints perf's pids, for perf_wait.
PERF="$DX_ROOT/deps/perf/usr/bin/perf"
perf_attach() {
    local pid="$1" dir="$2"
    [ -n "$pid" ] || { say "perf: no engine process to record"; return 0; }
    LD_LIBRARY_PATH="$DX_ROOT/deps/perf/usr/lib" "$PERF" record -q -e cpu-clock -t "$pid" -k realtime -F 999 \
        -o "$dir/perf.data" > "$dir/perf.log" 2>&1 &
    local ppid=$!
    date +%s.%N > "$dir/stat.start"
    LD_LIBRARY_PATH="$DX_ROOT/deps/perf/usr/lib" "$PERF" stat -e cycles:u,instructions:u -t "$pid" -I 100 -x , \
        -o "$dir/stat.csv" > /dev/null 2>&1 &
    local spid=$!
    ( sleep 20; cp "/proc/$pid/maps" "$dir/maps.txt" 2>/dev/null || true ) > /dev/null 2>&1 &
    printf '%s %s\n' "$ppid" "$spid"
}
# perf_wait <pids>: until perf has written its files (not this shell's
# children: perf_attach runs in a command substitution).
perf_wait() {
    local p
    for p in "$@"; do
        while kill -0 "$p" 2>/dev/null; do sleep 0.2; done
    done
}
need_perf() {
    [ "${DXCAP_PERF:-0}" != 1 ] || [ -x "$PERF" ] || die "DXCAP_PERF needs perf in $DX_ROOT/deps -- tools/host-tools.sh"
}

# The prefix's Windows programs -- the original and the SDK's UCC -- run
# here with the Proton build's own wine, as dx-reverse-info's tools/ida/idalib-mcp.sh runs IDA.
WINE="$HOME/.local/share/Steam/compatibilitytools.d/${DXCAP_PROTON:-Proton-CachyOS Latest}/files/bin/wine"
PREFIX="$(realpath -m "${DXCAP_PREFIX:-$CAP/prefix}")" # wine takes only an absolute WINEPREFIX
XDISPLAY="${DXCAP_DISPLAY:-${DISPLAY:-:0}}"
WINDOW="${DXCAP_WINDOW:-1280x720}"
[[ "$WINDOW" =~ ^[1-9][0-9]*x[1-9][0-9]*$ ]] || die "DXCAP_WINDOW is <width>x<height>, not $WINDOW"
WINDOW_X="${WINDOW%x*}"
WINDOW_Y="${WINDOW#*x}"
need_display() {
    DISPLAY="$XDISPLAY" xdotool getdisplaygeometry > /dev/null 2>&1 ||
        die "no X display at $XDISPLAY (the desktop's Xwayland; xdotool is needed too)"
}
# Wine gives a window the focus only once it can grab the pointer itself
# (winex11's set_focus tries every 10 ms): while another X client holds a
# pointer grab, the original stops at its first focus change, in its message
# pump, its window black. A held grab: an ImageMagick import given a window
# that had gone, waiting for a click on another (tools/dxcap/grab.py).
need_free_pointer() {
    DISPLAY="$XDISPLAY" python3 - <<'EOF' && return 0
import ctypes, sys
x = ctypes.cdll.LoadLibrary('libX11.so.6')
x.XOpenDisplay.restype = ctypes.c_void_p
x.XOpenDisplay.argtypes = [ctypes.c_char_p]
x.XDefaultRootWindow.restype = ctypes.c_ulong
x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
x.XGrabPointer.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_uint,
                           ctypes.c_int, ctypes.c_int, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong]
x.XUngrabPointer.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x.XCloseDisplay.argtypes = [ctypes.c_void_p]
d = x.XOpenDisplay(None)
if not d:
    sys.exit(2)
# GrabSuccess is 0, AlreadyGrabbed 1.
grabbed = x.XGrabPointer(d, x.XDefaultRootWindow(d), 0, 0, 1, 1, 0, 0, 0)
if grabbed == 0:
    x.XUngrabPointer(d, 0)
x.XCloseDisplay(d)
sys.exit(1 if grabbed else 0)
EOF
    die "another X client holds the pointer grabbed on $XDISPLAY: the original would stop at its first focus change. An import waiting for a click? $(pgrep -a -x import | tr '\n' ' ')"
}
wine_run() {
    env WINEPREFIX="$PREFIX" WINEDEBUG=-all \
        WINEDLLOVERRIDES="winemenubuilder.exe=d" "$WINE" "$@"
}

# The original at its time limit: closed by Windows' own taskkill, which
# ends it with its exit written (its log is written only as it exits), and
# killed only when that has not ended it within 10 s. The caller's $secs and
# $i say how long it ran.
stop_original() {
    if pgrep -f "^DeusEx.exe" > /dev/null && [ "${DXCAP_KILL:-0}" = 1 ]; then
        kill -KILL $(pgrep -f "^DeusEx.exe") 2>/dev/null || true
        echo "killed at $secs s (DXCAP_KILL)"
    elif pgrep -f "^DeusEx.exe" > /dev/null; then
        wine_run taskkill /im DeusEx.exe > /dev/null 2>&1 || true
        local j=0
        while [ $j -lt 10 ] && pgrep -f "^DeusEx.exe" > /dev/null; do sleep 1; j=$((j+1)); done
        if pgrep -f "^DeusEx.exe" > /dev/null; then
            kill $(pgrep -f "^DeusEx.exe")
            echo "killed after $secs s"
        else
            echo "closed after $secs s"
        fi
    else
        echo "exited after $i s"
    fi
}

# The prefix is made on first use, by the same wine's wineboot on the
# desktop's X display, without Mono and Gecko, which
# the game and UCC do not need. Wine's wined3d, which D3DDrv's DirectDraw
# runs on, imports vkd3d's libraries, which Proton's own prefix has and a
# bare wineboot's lacks: without them ddraw.dll does not load, and the game
# falls back from D3DDrv to SoftDrv (its log: "DirectDraw not installed").
# So they are copied in from the Proton build's lib/vkd3d when missing.
ensure_prefix() {
    [ -f "$PREFIX/system.reg" ] || make_prefix
    ln -sfn "$CAP" "$PREFIX/dosdevices/x:"   # original_inis' drive
    local vkd3d="${WINE%/bin/wine}/lib/vkd3d" arch dir f
    for arch in x86_64:system32 i386:syswow64; do
        dir="$PREFIX/drive_c/windows/${arch#*:}"
        for f in "$vkd3d/${arch%%:*}-windows"/libvkd3d*.dll; do
            [ -f "$f" ] || die "no vkd3d libraries in $vkd3d"
            [ -f "$dir/${f##*/}" ] || cp "$f" "$dir/"
        done
    done
}

make_prefix() {
    need_display
    say "making the Wine prefix $PREFIX"
    mkdir -p "$(dirname "$PREFIX")"
    env -u WAYLAND_DISPLAY DISPLAY="$XDISPLAY" WINEPREFIX="$PREFIX" WINEDEBUG=-all \
        WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" "$WINE" wineboot -i > /dev/null 2>&1 || true
    env WINEPREFIX="$PREFIX" "${WINE%/wine}/wineserver" -w || true
    [ -f "$PREFIX/system.reg" ] || die "wineboot made no prefix at $PREFIX"
}

# Wine's view of an absolute path: Z: is the root.
winpath() { printf 'Z:%s' "${1//\//\\}"; }
# A path as DXEngine names it: Wine's name for a Windows build.
enginepath() { if windows_engine; then winpath "$1"; else printf '%s' "$1"; fi; }
# A path's last part in capitals with DXCAP_INI_CASE=1, the rest as it is.
ini_case() { if [ "${DXCAP_INI_CASE:-0}" = 1 ]; then printf '%s/%s' "${1%[/\\]*}" "$(printf '%s' "${1##*[/\\]}" | tr '[:lower:]' '[:upper:]')"; else printf '%s' "$1"; fi; }
# original_inis <ini> <userini>: the original's INI= and USERINI= into the
# caller's inis, by DXCAP_INI_CASE and DXCAP_USERINI_FIRST, each through the
# prefix's drive X:, the harness's folder, both files in its System: Wine
# quotes an argument that holds a space before its INI=, and the original's
# Parse cuts the value at the space (Core.dll 0x10146c4e). The whole folder,
# not System alone: a program started in a drive's root (UCC in System)
# finds nothing by its ..\System paths.
original_inis() {
    local ini userini
    ini="$(ini_case "X:/System/${1##*/}")"
    userini="$(ini_case "X:/System/${2##*/}")"
    inis=("INI=${ini//\//\\}" "USERINI=${userini//\//\\}")
    [ "${DXCAP_USERINI_FIRST:-0}" != 1 ] || inis=("${inis[1]}" "${inis[0]}")
}

# DXCAP_DXVK: Proton's DXVK, D3D11 on Vulkan, in the prefix beside Wine's
# own d3d11 and dxgi, which stay the default: a run takes DXVK's by its
# WINEDLLOVERRIDES.
dxvk_ready() {
    local dxvk="${WINE%/bin/wine}/lib/wine/dxvk/x86_64-windows" dir="$PREFIX/drive_c/windows/system32" f
    for f in d3d11.dll dxgi.dll; do
        [ -f "$dxvk/$f" ] || die "no DXVK $f in $dxvk"
        cmp -s "$dxvk/$f" "$dir/$f" || cp "$dxvk/$f" "$dir/$f"
    done
}

# crlf <file>: standard input into <file>, its lines ended as Windows ends them.
crlf() { sed 's/$/\r/' > "$1"; }

# LocalizeConsole's two folders, made afresh, each with .int files of the
# harness's own: build/dxcap/mods/first/System, with a DeusEx.int giving one
# of the game's keys, a DXCapture.int giving the console's Greeting and
# DXCapFirst.int; mods/second/System, with DXCapSecond.int, DXCapWide.int
# (UTF-16) and DXCapLatin.int (Latin-1); DXCapFirst and DXCapSecond each
# registering an object for the console and giving a text. Prints the paths
# that name them first in an <engine>'s run, as a mod's ini names its
# folders: the first by its absolute path, the second by one relative to
# the engine's System folder (the original's view's, DXEngine's game's).
localize_folders() {
    local engine="$1" first="$CAP/mods/first/System" second="$CAP/mods/second/System" system
    rm -rf "$CAP/mods"
    mkdir -p "$first" "$second"
    crlf "$first/DeusEx.int" <<'EOF'
[DeusExPlayer]
TooHeavyToLift="the first folder's DeusEx.int"
EOF
    crlf "$first/DXCapture.int" <<'EOF'
[LocalizeConsole]
Greeting="the first folder's DXCapture.int"
EOF
    crlf "$first/DXCapFirst.int" <<'EOF'
[Public]
Object=(Name=DXCapFirst.Registered,Class=Class,MetaClass=DXCapture.LocalizeConsole,Description="registered, by the first folder")

[Strings]
Hello="the first folder's DXCapFirst.int"
EOF
    crlf "$second/DXCapSecond.int" <<'EOF'
[Public]
Object=(Name=DXCapSecond.Registered,Class=Class,MetaClass=DXCapture.LocalizeConsole,Description="registered by the second folder")

[Strings]
Hello="the second folder's DXCapSecond.int"
EOF
    # DXCapWide.int as Notepad's "Unicode" saves one, UTF-16 with its
    # byte-order mark, holding characters past Latin-1; DXCapLatin.int a byte
    # a character, holding Latin-1's e acute
    { printf '\xff\xfe'; printf '[Strings]\r\nHello="wide \xce\xa9\xe2\x84\xa2\xc3\xa9"\r\n' | iconv -f UTF-8 -t UTF-16LE; } > "$second/DXCapWide.int"
    printf '[Strings]\r\nHello="caf\xe9"\r\n' > "$second/DXCapLatin.int"
    if [ "$engine" = original ]; then
        system="$CAP/game/System"
    else
        system="$GAME/System"
    fi
    if [ "$engine" = original ] || windows_engine; then
        printf '%s\\*.u\n' "$(winpath "$first")"
    else
        printf '%s/*.u\n' "$first"
    fi
    printf '%s\\*.u\n' "$(realpath -m -s --relative-to="$system" "$second" | tr / '\\')"
}

# make_ini <original|dxe> <console> <out>: the game's DeusEx.ini with the
# console class, the package's path, the run's window, the music off when
# recording (but with DXCAP_MUSIC=1), and for the original the OpenGL
# renderer (DXCAP_RENDERER=D3D: D3DDrv), whose window its frames are grabbed
# from; for LocalizeConsole its folders first in the paths.
make_ini() {
    local engine="$1" console="$2" out="$3" path mods="" winpaths=0
    # A read-only one a DXCAP_READONLY run left behind made afresh
    rm -f "$out"
    if [ "$engine" = original ] || windows_engine; then winpaths=1; fi
    if [ "$winpaths" = 1 ]; then
        path="$(winpath "$CAP/System")\\*.u"
    else
        path="$CAP/System/*.u"
    fi
    [ "$console" != LocalizeConsole ] || mods="$(localize_folders "$engine")"
    DXCAP_LOCALIZE_PATHS="$mods" DXCAP_WINPATHS="$winpaths" python3 - "$GAME/System/DeusEx.ini" "$out" "$engine" "$console" "$path" "$WINDOW_X" "$WINDOW_Y" <<'EOF'
import os, re, sys
src, out, engine, console, path, window_x, window_y = sys.argv[1:]
s = open(src, encoding='latin1', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
def put(key, value):
    global s
    s, n = re.subn(r'^%s=.*$' % re.escape(key), lambda m: '%s=%s' % (key, value), s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no %s= in %s' % (key, src))
# A live run keeps the stock console: a server's game drops any other.
if console:
    put('Console', 'DXCapture.' + console)
s, n = re.subn(r'^(Paths=\.\.\\System\\\*\.u)\r?$', lambda m: m.group(1) + nl + 'Paths=' + path, s, count=1, flags=re.M)
if n != 1:
    sys.exit('no Paths=..\\System\\*.u in ' + src)
# LocalizeConsole's folders before the game's own, as a mod's are.
mods = ''.join('Paths=%s%s' % (p, nl) for p in os.environ.get('DXCAP_LOCALIZE_PATHS', '').split('\n') if p)
if mods:
    s, n = re.subn(r'^(Paths=)', lambda m: mods + m.group(1), s, count=1, flags=re.M)
if os.environ.get('DXCAP_RECORD') == '1' and os.environ.get('DXCAP_MUSIC') != '1':
    put('MusicVolume', '0')
# The Join Internet screen asks a live master server: the game's names
# GameSpy's, closed.
put('MasterServerAddress', 'master.333networks.com')
# A run's server never tells the master servers about itself -- only, with
# DXCAP_UPLINK=<host>:<port>, a master on this machine (fakemaster.py).
uplink = os.environ.get('DXCAP_UPLINK', '')
first = [True]
def one_uplink(m):
    if not uplink or not first[0]:
        return ''
    first[0] = False
    host, port = uplink.rsplit(':', 1)
    return 'ServerActors=IpServer.UdpServerUplink DoUplink=True MasterServerAddress=%s MasterServerPort=%s%s' % (host, port, nl)
s, n = re.subn(r'^ServerActors=IpServer\.UdpServerUplink.*\r?\n', one_uplink, s, flags=re.M)
if n == 0:
    sys.exit('no ServerActors=IpServer.UdpServerUplink in ' + src)
# A run's server listens on DXCAP_PORT's port.
if os.environ.get('DXCAP_PORT'):
    put('Port', os.environ['DXCAP_PORT'])
# A run's render device precaches after each map's load with DXCAP_PRECACHE=1.
if os.environ.get('DXCAP_PRECACHE') == '1':
    put('UsePrecache', 'True')
# A run's download cache names its packages by DXCAP_CACHEEXT's extension.
if os.environ.get('DXCAP_CACHEEXT'):
    put('CacheExt', os.environ['DXCAP_CACHEEXT'])
# DXCAP_INI="<Section>:<Key>=<Value> ...": each key set in its section, added
# at the section's end where it has none, the section at the file's end
for item in os.environ.get('DXCAP_INI', '').split():
    sect, _, kv = item.partition(':')
    key, eq, value = kv.partition('=')
    if not sect or not key or not eq:
        sys.exit('DXCAP_INI: %s is not <Section>:<Key>=<Value>' % item)
    m = re.search(r'^\[%s\]\r?$' % re.escape(sect), s, flags=re.M | re.I)
    if not m:
        s = s.rstrip('\r\n') + nl + nl + '[%s]%s%s=%s%s' % (sect, nl, key, value, nl)
        continue
    start = m.end()
    nxt = re.search(r'^\[', s[start:], flags=re.M)
    end = start + nxt.start() if nxt else len(s)
    body, n = re.subn(r'^%s=.*?(?=\r?$)' % re.escape(key), lambda _: '%s=%s' % (key, value), s[start:end], count=1, flags=re.M | re.I)
    if n == 0:
        body = body.rstrip('\r\n') + nl + '%s=%s%s' % (key, value, nl) + (nl if nxt else '')
    s = s[:start] + body + s[end:]
# A run's server offers the packages of DXCAP_SERVERPKGS's folder, found
# there by its paths.
serverpkgs = os.environ.get('DXCAP_SERVERPKGS', '')
if serverpkgs:
    import glob
    files = sorted(glob.glob(os.path.join(serverpkgs, '*.u*')))
    if not files:
        sys.exit('no packages in ' + serverpkgs)
    exts = sorted(set(os.path.splitext(f)[1] for f in files))
    folder = os.path.abspath(serverpkgs)
    if os.environ.get('DXCAP_WINPATHS') == '1':
        folder = 'Z:' + folder.replace('/', '\\')
        lines = ''.join('Paths=%s\\*%s%s' % (folder, e, nl) for e in exts)
    else:
        lines = ''.join('Paths=%s/*%s%s' % (folder, e, nl) for e in exts)
    s, n = re.subn(r'^(Paths=\.\.\\System\\\*\.u\r?\n)', lambda m: m.group(1) + lines, s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no Paths=..\\System\\*.u in ' + src)
    names = ''.join('ServerPackages=%s%s' % (os.path.splitext(os.path.basename(f))[0], nl) for f in files)
    s, n = re.subn(r'^\[DeusEx\.DeusExGameEngine\]\r?\n', lambda m: m.group(0) + names, s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no [DeusEx.DeusExGameEngine] in ' + src)
# A run finds the maps of DXCAP_MAPS's folder by its paths, as a map of
# the game's.
maps = os.environ.get('DXCAP_MAPS', '')
if maps:
    folder = os.path.abspath(maps)
    if os.environ.get('DXCAP_WINPATHS') == '1':
        line = 'Paths=Z:%s\\*.dx%s' % (folder.replace('/', '\\'), nl)
    else:
        line = 'Paths=%s/*.dx%s' % (folder, nl)
    s, n = re.subn(r'^(Paths=\.\.\\System\\\*\.u\r?\n)', lambda m: m.group(1) + line, s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no Paths=..\\System\\*.u in ' + src)
# A run's server logs world stats only with DXCAP_STATS.
if os.environ.get('DXCAP_STATS'):
    s, n = re.subn(r'^\[Engine\.GameInfo\]\r?\n', lambda m: m.group(0) + 'bWorldLog=True' + nl, s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no [Engine.GameInfo] in ' + src)
# Both engines take the game's settings from it; both run in a window of
# the same size, or with DXCAP_FULLSCREEN=1 full screen at that size, the
# display's own, so that nothing switches it and the grabber reads all of it.
put('WindowedViewportX', window_x)
put('WindowedViewportY', window_y)
if os.environ.get('DXCAP_FULLSCREEN') == '1':
    put('FullscreenViewportX', window_x)
    put('FullscreenViewportY', window_y)
    put('StartupFullscreen', 'True')
else:
    put('StartupFullscreen', 'False')
# A game that has run: an install never started keeps FirstRun 0, and the
# original's launcher then opens its first-run wizard, which waits for clicks
put('FirstRun', '1100')
if engine == 'original':
    if os.environ.get('DXCAP_RENDERER', '') == 'D3D':
        put('GameRenderDevice', 'D3DDrv.D3DRenderDevice')
    else:
        put('GameRenderDevice', 'OpenGLDrv.OpenGLRenderDevice')
else:
    # DXEngine's device is the settings file's (DXCAP_SETTINGS), never the
    # game ini's SoftDrv: its renderer settings D3DDrv's, as the stock ini's
    put('GameRenderDevice', 'D3DDrv.D3DRenderDevice')
open(out, 'w', encoding='latin1', newline='').write(s)
EOF
}

# console_keys <console>: the run's keys for its console class's section,
# [DXCapture.<console>]: DXCAP_MISSION_MAP's TargetMap and DXCAP_CONFIG's
# "<key>=<value> ...".
console_keys() {
    local kv
    [ -n "${DXCAP_MISSION_MAP:-}${DXCAP_CONFIG:-}" ] || return 0
    printf '[DXCapture.%s]\n' "$1"
    [ -z "${DXCAP_MISSION_MAP:-}" ] || printf 'TargetMap=%s\n' "$DXCAP_MISSION_MAP"
    for kv in ${DXCAP_CONFIG:-}; do
        [[ "$kv" == ?*=* ]] || die "DXCAP_CONFIG is \"<key>=<value> ...\", not $kv"
        printf '%s\n' "$kv"
    done
}

# user_ini <out>: the game's User.ini, or DXCAP_USERINI's file in its place,
# with DXCAP_STATS's world stats password for the player.
user_ini() {
    local src="${DXCAP_USERINI:-$GAME/System/User.ini}"
    [ -f "$src" ] || die "no user ini $src"
    rm -f "$1"
    cp "$src" "$1"
    [ -n "${DXCAP_STATS:-}" ] || return 0
    python3 - "$1" "$DXCAP_STATS" <<'EOF'
import re, sys
path, secret = sys.argv[1:]
s = open(path, encoding='latin1', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
# The game's User.ini has both keys, empty: set in place, as a second
# key's value would not count in the original (its last one does).
for key, value in (('ngWorldSecret', secret), ('ngSecretSet', 'True')):
    s, n = re.subn(r'^%s=[^\r\n]*' % key, lambda m: '%s=%s' % (key, value), s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no %s= in %s' % (key, path))
open(path, 'w', encoding='latin1', newline='').write(s)
EOF
}

# DXCAP_READONLY=1: readonly_before <dir> <ini>... takes every write bit off
# the run's inis, their sums into the run's readonly-before.txt;
# readonly_after <dir> <ini>... takes their sums after the run, says whether
# either was written and gives the owner's write bit back.
readonly_before() {
    [ "${DXCAP_READONLY:-0}" = 1 ] || return 0
    local dir="$1"
    shift
    mkdir -p "$dir"
    chmod a-w "$@"
    sha256sum "$@" > "$dir/readonly-before.txt"
}
readonly_after() {
    [ "${DXCAP_READONLY:-0}" = 1 ] || return 0
    local dir="$1"
    shift
    sha256sum "$@" > "$dir/readonly-after.txt"
    chmod u+w "$@"
    if cmp -s "$dir/readonly-before.txt" "$dir/readonly-after.txt"; then
        say "read-only inis: neither written"
    else
        say "read-only inis: written ($dir/readonly-after.txt)"
    fi
}

# DXCAP_KILL=1: ini_after <dir> <ini> <userini> copies the run's two inis,
# as the run left them, into its ini-after.ini and userini-after.ini.
ini_after() {
    [ "${DXCAP_KILL:-0}" = 1 ] || return 0
    mkdir -p "$1"
    cp "$2" "$1/ini-after.ini"
    cp "$3" "$1/userini-after.ini"
}

cmd_setup() {
    [ -f "$SDK/UCC.exe" ] || die "no SDK at $SDK (reference/ReleaseSDK1112f)"
    mkdir -p "$CAP/System" "$CAP/DXCapture" "$CAP/runs"
    local f d
    for f in "$GAME"/System/*.u "$GAME"/System/*.dll "$GAME"/System/*.int; do
        ln -sfn "$f" "$CAP/System/$(basename "$f")"
    done
    # The SDK's UCC with the Core and Window it was built against.
    for f in UCC.exe Core.dll Window.dll; do
        rm -f "$CAP/System/$f"
        cp "$SDK/$f" "$CAP/System/$f"
    done
    for d in Textures Sounds Music Maps; do
        ln -sfn "$GAME/$d" "$CAP/$d"
    done
    ln -sfn "$HERE/dxcap/Classes" "$CAP/DXCapture/Classes"
    # UCC wants both UCC.ini and DeusEx.ini, and the defaults beside them:
    # the game's, with the package to compile last.
    ln -sfn "$GAME/System/Default.ini" "$CAP/System/Default.ini"
    ln -sfn "$GAME/System/DefUser.ini" "$CAP/System/DefUser.ini"
    python3 - "$GAME/System/DeusEx.ini" "$CAP/System/UCC.ini" <<'EOF'
import re, sys
s = open(sys.argv[1], encoding='latin1', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
s, n = re.subn(r'^(EditPackages=DeusEx)\r?$', lambda m: m.group(1) + nl + 'EditPackages=DXCapture', s, count=1, flags=re.M)
if n != 1:
    sys.exit('no EditPackages=DeusEx in ' + sys.argv[1])
open(sys.argv[2], 'w', encoding='latin1', newline='').write(s)
EOF
    cp "$CAP/System/UCC.ini" "$CAP/System/DeusEx.ini"
    cp "$GAME/System/User.ini" "$CAP/System/User.ini"
    ensure_prefix

    say "build/dxcap is ready -- tools/dxcap.sh compile next"
}

cmd_compile() {
    [ -f "$CAP/System/UCC.exe" ] || die "tools/dxcap.sh setup first"
    ensure_prefix
    rm -f "$CAP/System/DXCapture.u" "$CAP/System/DXCapTwin.u"
    python3 "$HERE/dxcap/testsounds.py" "$CAP/DXCapture/Sounds" || die "testsounds.py failed"
    python3 "$HERE/dxcap/testtextures.py" "$CAP/DXCapture/Textures" || die "testtextures.py failed"
    python3 "$HERE/dxcap/testmeshes.py" "$CAP/DXCapture/Models" || die "testmeshes.py failed"
    python3 "$HERE/dxcap/testmusic.py" "$CAP/DXCapture/Music" || die "testmusic.py failed"
    (cd "$CAP/System" && wine_run UCC.exe make > "$CAP/ucc.out" 2>&1) || true
    grep -E "Error|error\(s\)" "$CAP/System/UCC.log" >&2 || true
    [ -f "$CAP/System/DXCapture.u" ] || die "UCC made no DXCapture.u -- build/dxcap/System/UCC.log"
    # CastConsole's twin: the package again under another name, each class
    # in it one of a DXCapture class's name and not it
    cp "$CAP/System/DXCapture.u" "$CAP/System/DXCapTwin.u"
    say "compiled: build/dxcap/System/DXCapture.u"
}

# server_actor <class> <ini>: DXCapture.<class> the first of the game
# engine's ServerActors, which a server spawns once its level listens.
server_actor() {
    python3 - "$2" "DXCapture.$1" <<'EOF'
import re, sys
path, cls = sys.argv[1:]
s = open(path, encoding='latin1', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
s, n = re.subn(r'^\[DeusEx\.DeusExGameEngine\]\r?\n', lambda m: m.group(0) + 'ServerActors=' + cls + nl, s, count=1, flags=re.M)
if n != 1:
    sys.exit('no [DeusEx.DeusExGameEngine] in ' + path)
open(path, 'w', encoding='latin1', newline='').write(s)
EOF
}

# Shots the game writes into its System folder, or with --userdir into that
# folder's Screenshots, but not ones that were already there before the run.
# A dxe run refuses to start over stray shots: the engine writes the first
# free ShotNNNN.bmp while the consoles number their marks from 0, so a shot
# left behind shifted a run's numbers and shots.py paired them with the
# original's wrong ones without a word.
shots_before() { (cd "$GAME/System" && ls Shot*.bmp 2>/dev/null) || true; }
shots_in() { [ -d "$1" ] && (cd "$1" && ls Shot*.bmp 2>/dev/null) || true; }
no_stray_shots() {
    local stray; stray="$(shots_before)"
    [ -z "$stray" ] || die "stray shots in $GAME/System, which would shift this run's numbers: $(printf '%s ' $stray)-- move them away"
    [ -z "${DXCAP_USERDIR:-}" ] || stray="$(shots_in "$(realpath -m "$DXCAP_USERDIR")/Screenshots")"
    [ -z "$stray" ] || die "stray shots in $DXCAP_USERDIR/Screenshots, which would shift this run's numbers: $(printf '%s ' $stray)-- move them away"
}

collect() {
    local dir="$1" before="$2" own="${3:-}" f
    mkdir -p "$dir"
    for f in $( (cd "$GAME/System" && ls Shot*.bmp 2>/dev/null) || true); do
        if ! printf '%s\n' "$before" | grep -qx "$f"; then
            mv "$GAME/System/$f" "$dir/$f"
        fi
    done
    # A run's own --userdir (a Windows build's settings, or DXCAP_USERDIR):
    # every shot in its Screenshots the run's, the folder checked empty of
    # shots before
    [ -n "$own" ] || [ -z "${DXCAP_USERDIR:-}" ] || own="$(realpath -m "$DXCAP_USERDIR")"
    [ -z "$own" ] || for f in "$own"/Screenshots/Shot*.bmp; do
        [ ! -f "$f" ] || mv "$f" "$dir/"
    done
}

# The original runs in a view of the game, build/dxcap/game, made afresh for
# each run: the game's folders linked, and its System folder's files, but for
# what the game writes there -- its log, Running.ini, shots -- and any file
# with no extension. The original takes a package's bare name in its folder
# before any of its paths (appFindPackageFile), so a file with no extension
# beside DeusEx.exe -- the recreated launcher's DeusEx -- stands in for the
# DeusEx package. What the game writes stays in the view. Its DeusEx.exe is
# GOG's where goggamefiles/ holds it: Steam's is packed and starts only under
# Steam's client ("Failed to find Steam"), and GOG's is the same build over
# the same DLLs, byte for byte.
GOG_EXE="$DX_ROOT/goggamefiles/System/DeusEx.exe"
make_view() {
    local view="$CAP/game" f name d
    rm -rf "$view"
    mkdir -p "$view/System"
    for f in "$GAME"/System/*; do
        [ -f "$f" ] || continue
        name="${f##*/}"
        case "$name" in
            *.*) ;;
            *) continue ;;
        esac
        case "$name" in
            *.log|*.bmp|*.i64|Running.ini) continue ;;
        esac
        if [ "$name" = DeusEx.exe ] && [ -f "$GOG_EXE" ]; then
            f="$GOG_EXE"
        fi
        ln -s "$f" "$view/System/$name"
    done
    for d in "$GAME"/*/; do
        d="${d%/}"
        [ "${d##*/}" = System ] || ln -s "$d" "$view/${d##*/}"
    done
}

# One original at a time: a second, but a server, forwards its command line
# to the running one (dx-reverse-info's launch-flow.md), and grab.py would
# see two windows of the name.
need_no_original() {
    ! pgrep -f "^DeusEx.exe" > /dev/null || die "an original runs already (pid $(pgrep -f "^DeusEx.exe" | paste -sd ' '))"
}

cmd_original() {
    local console="${1:?console class}" secs="${2:-120}"
    [ -f "$CAP/System/DXCapture.u" ] || die "tools/dxcap.sh compile first"
    need_no_original
    command -v import >/dev/null || die "ImageMagick's import is needed to grab the original's window"
    need_display
    need_free_pointer
    need_perf
    fps_check original
    ensure_prefix
    local ini="$CAP/System/Original.ini" userini="$CAP/System/OriginalUser.ini"
    local dir="$CAP/runs/original-$console-$(date +%Y%m%d-%H%M%S)"
    make_ini original "$console" "$ini"
    user_ini "$userini"
    # A console that travels to a named map (the sweep's MissionConsole):
    # the game starts at the menu map, so the map comes through the ini.
    console_keys "$console" >> "$ini"
    make_view
    mkdir -p "$dir"
    readonly_before "$dir" "$ini" "$userini"

    local gpid=""
    if [ "${DXCAP_BURST:-0}" = 1 ]; then
        python3 "$HERE/dxcap/burst.py" grab "$dir/burst.csv" "$XDISPLAY" '^Deus Ex$' "$WINDOW_X" "$WINDOW_Y" 2> "$dir/burst.log" &
    else
        python3 "$HERE/dxcap/grab.py" "$dir" "$XDISPLAY" '^Deus Ex$' "$WINDOW_X" "$WINDOW_Y" 2> "$dir/grab.log" &
    fi
    gpid=$!
    GRAB_PID=$gpid

    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_start "$dir/audio.wav"
    # At the menu map, or at DXCAP_START's URL, which -hax0r has the original
    # take from its command line; each console class ends its run with EXIT.
    local inis=()
    original_inis "$ini" "$userini"
    original_process "$dir" "$secs" "${DXCAP_START:-DX.dx}" ${DXCAP_START:+-hax0r} "${inis[@]}"
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_stop
    readonly_after "$dir" "$ini" "$userini"
    ini_after "$dir" "$ini" "$userini"

    kill "$gpid" 2>/dev/null || true
    wait "$gpid" 2>/dev/null || true
    GRAB_PID=""
    # The game's own shots, black here, stay in the view; the grabbed frames
    # stand for them.
    local f
    for f in "$dir"/Shot*.ppm; do
        [ -f "$f" ] || continue
        magick "$f" "${f%.ppm}.png" && rm -f "$f"
    done
    cp "$CAP/game/System/DeusEx.log" "$dir/DeusEx.log" 2>/dev/null || true
    say "original: $dir"
    # A renderer that fails to start is replaced by SoftDrv without a word
    # on the screen: its frames would pass for D3DDrv's.
    if grep -a -q "Bound to SoftDrv" "$dir/DeusEx.log" 2>/dev/null; then
        die "the game fell back to SoftDrv (its renderer did not start): $dir/DeusEx.log"
    fi
}

# original_process <dir> <secs> <args...>: the original started with <args>
# and DXCAP_FLAGS, straight on the desktop's X display (no Wine desktop),
# under DXCAP_FPS's limiter and DXCAP_PERF's and DXCAP_MEMLOG's watchers;
# waited for until it exits or <secs> run out, then only its own process
# stopped.
original_process() {
    local dir="$1" secs="$2"
    shift 2
    (
        unset WAYLAND_DISPLAY
        export DISPLAY="$XDISPLAY"
        [ "${DXCAP_RECORD:-0}" != 1 ] || export PULSE_SINK=dxcap
        cd "$CAP/game/System"
        (fps_env; wine_run DeusEx.exe "$@" ${DXCAP_FLAGS:-} > "$dir/wine.log" 2>&1) &
        i=0
        while [ $i -lt 60 ] && ! pgrep -f "^DeusEx.exe" > /dev/null; do sleep 1; i=$((i+1)); done
        perfpid=""
        [ "${DXCAP_PERF:-0}" != 1 ] || perfpid="$(perf_attach "$(pgrep -f "^DeusEx.exe" | head -1)" "$dir")"
        if [ "${DXCAP_MEMLOG:-0}" = 1 ] && pgrep -f "^DeusEx.exe" > /dev/null; then
            memlog "$(pgrep -f "^DeusEx.exe" | head -1)" "$dir/memlog.txt" &
        fi
        i=0
        while [ $i -lt "$secs" ] && pgrep -f "^DeusEx.exe" > /dev/null; do sleep 1; i=$((i+1)); done
        stop_original
        sleep 3
        [ -z "$perfpid" ] || perf_wait $perfpid
    ) || true
}

# memlog <pid> <file> [child]: the process's resident memory (/proc's statm)
# each second, as "<seconds> <MB>", until it exits; with child, its first
# child's (the engine under timeout).
memlog() {
    local watch="$1" out="$2" pid="" t=0 pages
    local page; page="$(getconf PAGESIZE)"
    [ "${3:-}" = child ] || pid="$watch"
    : > "$out"
    while kill -0 "$watch" 2>/dev/null; do
        [ -n "$pid" ] || pid="$(pgrep -P "$watch" | head -1)" || true
        if [ -n "$pid" ] && [ -r "/proc/$pid/statm" ]; then
            read -r _ pages _ < "/proc/$pid/statm" || break
            awk -v t="$t" -v p="$pages" -v s="$page" 'BEGIN { printf "%d %.1f\n", t, p * s / 1048576 }' >> "$out"
        fi
        sleep 1
        t=$((t + 1))
    done
}

# A dxe run into <map> or a server's address, its log into <dir>; any
# further arguments go to the engine.
run_dxe() {
    local dir="$1" ini="$2" userini="$3" map="$4" secs="$5"
    shift 5
    no_stray_shots
    local before; before="$(shots_before)"
    mkdir -p "$dir"
    need_perf
    fps_check
    local settings=""
    if [ -n "${DXCAP_SETTINGS:-}" ]; then
        settings="$(realpath "$DXCAP_SETTINGS")"
        [ -f "$settings" ] || die "no settings file $DXCAP_SETTINGS"
    fi
    # With DXCAP_USERDIR the folder's own Settings.json is the run's: the
    # pair silently dropped DXCAP_SETTINGS on a Linux build
    [ -z "$settings" ] || [ -z "${DXCAP_USERDIR:-}" ] || die "DXCAP_SETTINGS with DXCAP_USERDIR: the folder's own Settings.json is the run's; put the settings there"
    need_display
    # The engine's own folder: DXCAP_USERDIR's, or for a Windows build's
    # settings (DXCAP_SETTINGS) one of the run's own.
    local userdir=() own="" overrides="winemenubuilder.exe=d"
    [ -z "${DXCAP_USERDIR:-}" ] || userdir=(--userdir="$(enginepath "$(realpath -m "$DXCAP_USERDIR")")")
    if windows_engine; then
        [ "${DXCAP_RECORD:-0}" != 1 ] && [ "${DXCAP_PERF:-0}" != 1 ] || die "DXCAP_RECORD and DXCAP_PERF are for Linux builds"
        if [ -n "$settings" ]; then
            [ -z "${DXCAP_USERDIR:-}" ] || die "a Windows build's DXCAP_SETTINGS is its own --userdir's: not with DXCAP_USERDIR"
            own="$dir/userdir"
            mkdir -p "$own"
            cp "$settings" "$own/Settings.json"
            userdir=(--userdir="$(winpath "$own")")
        fi
        ensure_prefix
        if [ "${DXCAP_DXVK:-0}" = 1 ]; then
            dxvk_ready
            overrides="$overrides;d3d11,dxgi=n,b"
        fi
    fi
    # The engine's own options, a timeline's file by the engine's name for it.
    local args=() a
    for a in "$@"; do
        case "$a" in
            --timeline=*) args+=(--timeline="$(enginepath "${a#--timeline=}")") ;;
            *) args+=("$a") ;;
        esac
    done
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_start "$dir/audio.wav"
    # DXCAP_BURST: the engine's window, by its name, sampled frame by frame
    local bpid=""
    if [ "${DXCAP_BURST:-0}" = 1 ]; then
        python3 "$HERE/dxcap/burst.py" grab "$dir/burst.csv" "$XDISPLAY" ' - DXEngine$' "$WINDOW_X" "$WINDOW_Y" 2> "$dir/burst.log" &
        bpid=$!
        GRAB_PID=$bpid
    fi
    # DXCAP_STDIN's script, by its path before the run's cd
    local stdin=""
    if [ -n "${DXCAP_STDIN:-}" ]; then
        stdin="$(realpath "$DXCAP_STDIN")"
        [ -f "$stdin" ] || die "no script $DXCAP_STDIN"
    fi
    local rc=0
    (
        cd "$GAME"
        # The desktop's X display through the window's X11 backend, where
        # the original draws through Wine's; DXCAP_BACKEND=SDL3 or SDL2 its
        # SDL one, the launchers', on the same display. A Windows build's
        # window is SDL3's (or its own), through Wine's X11 driver: Windows'
        # SDL has no driver to name. DXCAP_BACKEND=Wayland: the engine's
        # own Wayland window on the desktop's compositor, the first the
        # engine starts with no backend named.
        if [ "${DXCAP_BACKEND:-}" = Wayland ] && ! windows_engine; then
            [ -n "${WAYLAND_DISPLAY:-}" ] || die "DXCAP_BACKEND=Wayland: no WAYLAND_DISPLAY (the desktop's compositor)"
            unset DXE_DISPLAY_BACKEND SDL_VIDEODRIVER
        elif [ "${DXCAP_BACKEND:-}" = SDL3-Wayland ] && ! windows_engine; then
            [ -n "${WAYLAND_DISPLAY:-}" ] || die "DXCAP_BACKEND=SDL3-Wayland: no WAYLAND_DISPLAY (the desktop's compositor)"
            export DXE_DISPLAY_BACKEND=SDL3 SDL_VIDEODRIVER=wayland
        else
            unset WAYLAND_DISPLAY
            export DISPLAY="$XDISPLAY" DXE_DISPLAY_BACKEND="${DXCAP_BACKEND:-X11}" SDL_VIDEODRIVER=x11
            if windows_engine; then
                export DXE_DISPLAY_BACKEND="${DXCAP_BACKEND:-SDL3}"
                unset SDL_VIDEODRIVER
            fi
        fi
        # A timeline's virtual pad alone (its vendor and product): no pad of
        # the machine's in its place.
        if [ -f "$dir/timeline.txt" ] &&
            grep -qE '^[[:space:]]*(start|game)[[:space:]]+[0-9.]+[[:space:]]+pad(axis)?[[:space:]]' "$dir/timeline.txt"; then
            export SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x1209/0xd0d1
        fi
        if [ "${DXCAP_RECORD:-0}" = 1 ]; then
            printf '[general]\ndrivers = pulse\n' > "$CAP/alsoft-pulse.conf"
            [ "${DXCAP_HRTF:-0}" != 1 ] || printf 'stereo-encoding = hrtf\n' >> "$CAP/alsoft-pulse.conf"
            export ALSOFT_CONF="$CAP/alsoft-pulse.conf" PULSE_SINK=dxcap
        elif windows_engine && [ "${DXCAP_AUDIO:-0}" != 1 ]; then
            # OpenAL Soft on Windows reads no file by a Unix name.
            export ALSOFT_DRIVERS=null
        elif [ "${DXCAP_AUDIO:-0}" != 1 ]; then
            printf '[general]\ndrivers = null\n' > "$CAP/alsoft-null.conf"
            export ALSOFT_CONF="$CAP/alsoft-null.conf"
        fi
        # The run's own Settings.json, where the engine looks for it
        # ($HOME/.config/DXEngine); the caches stay where they are.
        if [ -n "$settings" ] && ! windows_engine; then
            mkdir -p "$dir/home/.config/DXEngine"
            cp "$settings" "$dir/home/.config/DXEngine/Settings.json"
            export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
            export HOME="$dir/home"
        fi
        fps_env
        local e_ini e_userini run=("$ENGINE_BIN")
        e_ini="$(enginepath "$ini")"
        e_userini="$(enginepath "$userini")"
        inis=(--ini="$e_ini" --userini="$e_userini")
        # Quoted: Parse takes a value with a space whole
        [ "${DXCAP_INI_CASE:-0}" != 1 ] || inis=(--cmdline="INI=\"$(ini_case "$e_ini")\" USERINI=\"$(ini_case "$e_userini")\"")
        [ "${DXCAP_USERINI_FIRST:-0}" != 1 ] || inis=(--cmdline="USERINI=\"$(ini_case "$e_userini")\" INI=\"$(ini_case "$e_ini")\"")
        if [ -n "${DXCAP_FLAGS:-}" ]; then
            if [[ "${inis[0]}" == --cmdline=* ]]; then inis=("${inis[0]} $DXCAP_FLAGS"); else inis+=(--cmdline="$DXCAP_FLAGS"); fi
        fi
        ! windows_engine || run=(env WINEPREFIX="$PREFIX" WINEDEBUG=-all WINEDLLOVERRIDES="$overrides" "$WINE" "$ENGINE_BIN")
        local others; others="$(pgrep -x DXEngine.exe || true)"
        # A map of - is no URL: the engine's own start (LocalMapConsole)
        local url=(--url="$map")
        [ "$map" != - ] || url=()
        if [ -n "$stdin" ]; then
            timeout -s KILL "$secs" "${run[@]}" --no-launcher "$(enginepath "$GAME")" "${inis[@]}" "${url[@]}" "${userdir[@]}" "${args[@]}" > "$dir/engine.log" 2>&1 < <(sh "$stdin") &
        else
            timeout -s KILL "$secs" "${run[@]}" --no-launcher "$(enginepath "$GAME")" "${inis[@]}" "${url[@]}" "${userdir[@]}" "${args[@]}" > "$dir/engine.log" 2>&1 &
        fi
        local tpid=$! mpid="" perfpid="" epid="" wpid="" i=0
        # Under wine, timeout's child is start.exe, which starts the engine
        # as a process of its own, passes on its exit code, and leaves it
        # running when killed: the engine's process is the new DXEngine.exe.
        if windows_engine; then
            # As long as the run itself (a prefix updating after a Proton
            # update starts it slowly); a miss is said, the engine then
            # outliving the run once timeout kills start.exe
            while [ $i -lt $((secs * 10)) ] && [ -z "$wpid" ] && kill -0 "$tpid" 2>/dev/null; do
                wpid="$(pgrep -x DXEngine.exe | grep -vxF -e "${others:-none}" | head -1)" || true
                [ -n "$wpid" ] || sleep 0.1
                i=$((i + 1))
            done
            [ -n "$wpid" ] || ! kill -0 "$tpid" 2>/dev/null || say "dxe: DXEngine.exe not seen in $((i / 10)) s; it may outlive the run"
        fi
        if [ "${DXCAP_MEMLOG:-0}" = 1 ] && [ -n "$wpid" ]; then
            memlog "$wpid" "$dir/memlog.txt" &
            mpid=$!
        elif [ "${DXCAP_MEMLOG:-0}" = 1 ]; then
            memlog "$tpid" "$dir/memlog.txt" child &
            mpid=$!
        fi
        if [ "${DXCAP_PERF:-0}" = 1 ]; then
            while [ $i -lt 50 ] && [ -z "$epid" ]; do
                epid="$(pgrep -P "$tpid" | head -1)" || true
                [ -n "$epid" ] || sleep 0.1
                i=$((i + 1))
            done
            perfpid="$(perf_attach "$epid" "$dir")"
        fi
        local erc=0
        wait "$tpid" || erc=$?
        [ -z "$wpid" ] || kill -KILL "$wpid" 2>/dev/null || true
        [ -z "$mpid" ] || kill "$mpid" 2>/dev/null || true
        [ -z "$perfpid" ] || perf_wait $perfpid
        exit "$erc"
    ) || rc=$?
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_stop
    if [ -n "$bpid" ]; then
        kill "$bpid" 2>/dev/null || true
        wait "$bpid" 2>/dev/null || true
        GRAB_PID=""
    fi
    collect "$dir" "$before" "$own"
    printf '%s\n' "$rc" > "$dir/exit"
    say "dxe: $dir (exit $rc)"
    # A GPU device that does not start gives way to the software device:
    # its frames would pass for the GPU device's.
    if grep -a -q "the software device draws in its place" "$dir/engine.log" 2>/dev/null; then
        die "the engine fell back to the software device (its device did not start): $dir/engine.log"
    fi
    printf '%s\n' "$dir"
}

cmd_dxe() {
    local console="${1:?console class}" map="${2:?map}" secs="${3:-120}"
    [ -f "$CAP/System/DXCapture.u" ] || die "tools/dxcap.sh compile first"
    [ -x "$ENGINE_BIN" ] || die "no engine build at $ENGINE_BIN"
    local ini="$CAP/System/DXE.ini" userini="$CAP/System/DXEUser.ini"
    local name="$console"
    [ "$console" != - ] || { console=""; name=stock; }
    local dir="$CAP/runs/dxe-$name-$(date +%Y%m%d-%H%M%S)"
    make_ini dxe "$console" "$ini"
    user_ini "$userini"
    [ -z "$console" ] || console_keys "$console" >> "$ini"
    local args=()
    if [ -n "${DXCAP_TIMELINE:-}" ]; then
        mkdir -p "$dir"
        cp "$DXCAP_TIMELINE" "$dir/timeline.txt"
        args=(--timeline="$dir/timeline.txt")
    fi
    readonly_before "$dir" "$ini" "$userini"
    run_dxe "$dir" "$ini" "$userini" "$map" "$secs" "${args[@]}"
    readonly_after "$dir" "$ini" "$userini"
    ini_after "$dir" "$ini" "$userini"
}

# DXEngine on a live server: the stock console, and a timeline for what a
# console class would do -- by default JoinConsole's walk, the player's
# place and the others' logged each second of the game (DXLIVE lines).
cmd_live() {
    local address="${1:?server address}" secs="${2:-120}"
    [ -x "$ENGINE_BIN" ] || die "no engine build at $ENGINE_BIN"
    local ini="$CAP/System/DXE.ini" userini="$CAP/System/DXEUser.ini"
    local dir="$CAP/runs/dxe-live-$(date +%Y%m%d-%H%M%S)"
    make_ini dxe "" "$ini"
    user_ini "$userini"
    mkdir -p "$dir"
    local timeline="$dir/timeline.txt"
    if [ -n "${DXCAP_TIMELINE:-}" ]; then
        cp "$DXCAP_TIMELINE" "$timeline"
    else
        cat > "$timeline" <<'EOF'
# JoinConsole's walk: once in the game the player stands 5 s, walks forward
# 5 s with the key held (W, MoveForward in the game's User.ini) and stands
# again; shots at the stops; the run exits 25 s into the game.
game 5 shot
game 5 press W
game 10 release W
game 11 shot
game 25 exit
EOF
    fi
    run_dxe "$dir" "$ini" "$userini" "$address" "$secs" --timeline="$timeline"
}

cmd_prove() {
    local map="${1:?map}" dir shots=0 f mean bad=0
    dir="$(cmd_dxe ProveConsole "$map" 90 | tail -1)"
    [ "$(cat "$dir/exit")" = 0 ] || { say "the run did not exit cleanly: $dir/engine.log"; bad=1; }
    for f in "$dir"/Shot*.bmp; do
        [ -f "$f" ] || continue
        shots=$((shots + 1))
        mean="$(magick "$f" -format '%[fx:mean]' info:)"
        say "$(basename "$f"): mean brightness $mean"
        awk -v m="$mean" 'BEGIN { exit !(m < 0.01) }' && { say "  -- nearly black"; bad=1; }
    done
    [ "$shots" = 2 ] || { say "expected 2 shots, got $shots"; bad=1; }
    grep -E "Script error|DXEngine error|Unimplemented" "$dir/engine.log" | sort | uniq -c >&2 || true
    [ "$bad" = 0 ] || die "proving run failed: $dir"
    say "proving run clean: $dir"
}

# Either engine as a dedicated server on <map>, which draws no frame: a
# server runs no console, so DXCapture.<actor>, the first of its
# ServerActors, drives the run and ends it (exit). The original's command
# line is "<map> -server", its first word the start URL (dx-reverse-info
# cli-flags.md); DXEngine's, --server. Waited for until it exits or the time
# runs out; its log lands in the run's folder.
cmd_dedicated() {
    local engine="${1:?original or dxe}" actor="${2:?server actor class}" map="${3:?map}" secs="${4:-120}"
    [ -f "$CAP/System/DXCapture.u" ] || die "tools/dxcap.sh compile first"
    local ini userini dir
    case "$engine" in
        original)
            need_no_original
            need_display
            need_perf
            fps_check original
            ensure_prefix
            ini="$CAP/System/Original.ini" userini="$CAP/System/OriginalUser.ini"
            dir="$CAP/runs/original-dedicated-$actor-$(date +%Y%m%d-%H%M%S)"
            make_ini original "" "$ini"
            server_actor "$actor" "$ini"
            user_ini "$userini"
            make_view
            mkdir -p "$dir"
            local inis=()
            original_inis "$ini" "$userini"
            original_process "$dir" "$secs" "$map" -server "${inis[@]}"
            cp "$CAP/game/System/DeusEx.log" "$dir/DeusEx.log" 2>/dev/null || true
            say "original: $dir"
            ;;
        dxe)
            ini="$CAP/System/DXE.ini" userini="$CAP/System/DXEUser.ini"
            dir="$CAP/runs/dxe-dedicated-$actor-$(date +%Y%m%d-%H%M%S)"
            make_ini dxe "" "$ini"
            server_actor "$actor" "$ini"
            user_ini "$userini"
            run_dxe "$dir" "$ini" "$userini" "$map" "$secs" --server
            ;;
        *) die "dedicated takes original or dxe, not $engine" ;;
    esac
}

cmd="${1:-help}"; shift || true
case "$cmd" in
    setup)    need_game; cmd_setup ;;
    compile)  need_game; cmd_compile ;;
    original) need_game; cmd_original "$@" ;;
    dxe)      need_game; cmd_dxe "$@" ;;
    prove)    need_game; cmd_prove "$@" ;;
    live)     need_game; cmd_live "$@" ;;
    dedicated) need_game; cmd_dedicated "$@" ;;
    -h|--help|help) usage ;;
    *)        die "unknown command '$cmd' (tools/dxcap.sh help)" ;;
esac
