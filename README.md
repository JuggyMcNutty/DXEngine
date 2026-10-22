## DXEngine

Deus Ex's engine, a hard fork of [Surreal Engine](https://github.com/dpjudas/SurrealEngine), the
open-source Unreal Engine 1 reimplementation.

Targeting only Deus Ex.

### You need your own copy of Deus Ex (version 1112fm); none of the game's files are included!

- **Build Targets**: Linux on x86_64 (X11, Wayland, SDL3 or SDL2) and on aarch64 with any GPU
  (Snapdragon, Mali, PowerVR); Windows 10 (1903) or newer on x86_64 (SDL3 or Win32). It runs on
  x86_64 and on the Smart Pro's PowerVR, and Windows' under Wine
  ([the ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#engine-ports)).
- **Deus Ex as the original plays it**: Our goal is a playable build as close to 1:1 with the
  original engine, its scripting and the rest as possible.

## DXEngine and Surreal Engine

The two differ at the core ([how it is kept](docs/ENGINE.md#how-it-is-kept)).

DXEngine's docs and tools are in [`docs/`](docs/) and [`tools/`](tools/).

- [`docs/ENGINE.md`](docs/ENGINE.md): how the engine is kept, run and profiled, and
  [what it changes](docs/ENGINE.md#what-it-changes-from-surreal-engine).
- [`docs/NATIVES.md`](docs/NATIVES.md): differences from the original.
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md): scripted runs of both engines,
  debugging a crash, proving a change.

## Building and running

Linux builds are made and tested here, and Windows' cross-built by llvm-mingw and tested under
Proton's wine.

### The easy way: Port Ex Machina

[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina) fetches the engine, builds it
with the launcher for each of its ports, and stages, runs or deploys the result: follow
[its quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start).

### Building it on its own

You need:

- a C++20 compiler (GCC 10 or later), CMake 3.15 or later and pkg-config;
- the development files of OpenAL, SDL3 or SDL2, and X11 (with Xi);
- optionally Wayland's (waylandpp, xkbcommon), EGL, dbus, GLib and fontconfig, used when found.

On Arch these are among the packages
[Port Ex Machina's dependencies](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#dependencies)
list. Then:

```sh
git clone https://github.com/JuggyMcNutty/DXEngine.git
cd DXEngine
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target DXEngine -j"$(nproc)"
```

Its SDL window is SDL3's when SDL3 is found, else SDL2's: either has DXEngine's gamepad support,
which its X11 and Wayland windows lack, and is the window when `DXE_DISPLAY_BACKEND`
names it ([the gamepad](docs/ENGINE.md#running-on-our-devices)). The build leaves two files
in `build/` that go together:

- `DXEngine`, the engine;
- `DXEngine.pk3`, its resources (the UI's fonts among them), found beside the executable.

### Running it

Give it the game's folder, or start it from the game's `System` folder:

```sh
build/DXEngine /path/to/deusex
```

It draws with Vulkan unless its settings name another device, so it needs the GPU's Vulkan
driver. Where to go next:

- [Running it](docs/ENGINE.md#running-it): the rest of the command line -- the log on
  stderr, a map to start on, a dedicated server, the inis.
- [Settings and environment](docs/ENGINE.md#settings-and-environment): the settings file
  and the variables the engine reads, the window backend a pad needs among them.

### For advanced users

The build's own switches:

| Switch | Default | What it does |
|---|---|---|
| `ENABLE_SDL2` | `ON` | The SDL2 window backend, built when SDL3's is not. |
| `ENABLE_SDL3` | `ON` | The SDL3 backend: when SDL3 is found, it is built in place of SDL2's. |
| `ENABLE_X11`, `ENABLE_WAYLAND` | `ON` | The desktop's window backends; off for a device with neither. |
| `DXE_SDL2_BORDERLESS_FULLSCREEN` | `OFF` | SDL2's fullscreen as a borderless window, for a vendor SDL2 that deadlocks in `SDL_SetWindowFullscreen`. |
| `ZIPDIR_EXECUTABLE` | unset | A host-built `zipdir` to pack `DXEngine.pk3` with when cross-compiling. |

- **A port's build** is an initial cache (`cmake -C`), `ports/<id>/engine.cmake` on the port's
  launcher branch: the desktop's toolchain and switches
  ([linux-x86_64's](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/ports/linux-x86_64/engine.cmake)),
  or a cross build's
  ([trimui-smartpro's](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/engine.cmake)).
  Port Ex Machina's `scripts/engine.sh build <port>` builds with it
  ([a port's files](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#what-a-port-is)).
- **Debugging**: a build with symbols, or with AddressSanitizer
  ([crashes](docs/DEVELOPMENT.md#crashes)).
- **Profiling and validation**: [measuring it](docs/ENGINE.md#measuring-it).
- **Kept, but neither built nor tested here**: MSVC's own, the natvis and the crash reporter's
  Windows part (under MinGW a stub). The D3D11 device is built for Windows, and offered by no
  launcher ([where it stands](docs/NATIVES.md)).

## License

DXEngine is zlib-licensed, an altered version of Surreal Engine, whose licence and those of the
third-party code it carries are in [`LICENSE.md`](LICENSE.md). Deus Ex belongs to its owners; this
project is not affiliated with or endorsed by them.
