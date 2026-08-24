# Port notes — INI-only settings → SKSE Menu Framework

**Version 1.5.0.** This is a fork of
[alexsylex/DragonsEyeMinimap](https://github.com/alexsylex/DragonsEyeMinimap) 1.1.0 that adds an in-game settings page driven by
[SKSE Menu Framework 3](https://github.com/QTR-Modding/SKSE-Menu-Framework-3), so the minimap
can be configured while the game is running instead of only through `DragonsEyeMinimap.ini`
at startup.

The upstream `README.md` and git history are untouched.

## What changed from upstream

| File | Change |
| --- | --- |
| `include/SKSEMenuFramework.h` | Vendored, unmodified, from SKSE Menu Framework 3.13. Self-contained: it reaches the framework through `GetProcAddress`, so nothing has to be linked and ImGui is not a build dependency. |
| `include/UI.h`, `source/UI.cpp` | New. The settings page, the registration, and the live-apply entry point. |
| `include/Settings.h` | Control tips became `std::string` so the menu can edit them. Added `Save()`, `RestoreDefaults()` and `GetIniPath()`. |
| `source/Settings.cpp` | Captures the compiled-in values as defaults before reading the INI, and can write every setting back. |
| `include/MiniMap.h`, `source/MiniMap.cpp` | Added `ApplyDisplaySettings()`, `ApplyShapeSetting()` and `IsReady()`. The constructor now remembers the clip's unscaled size. |
| `source/Controls.cpp` | `.c_str()` on the control tips, following the `std::string` change. |
| `source/MessageListeners.cpp` | Calls `UI::Register()` on `kPostPostLoad`. |
| `CMakeLists.txt` | Globs are `CONFIGURE_DEPENDS`; the auto-deploy copy only runs if the game folder exists. |
| `cmake/ports/commonlibsse-ng/portfile.cmake` | Fetches CommonLibVR over git instead of a GitHub tarball. |

### Why the port file changed

The upstream port pinned a SHA512 of GitHub's generated `.tar.gz`. GitHub re-compresses
those archives over time, so the recorded hash eventually stops matching and the port fails
to download — which is exactly what happened here. Fetching the same pinned commit over git
keeps the pin while letting git verify the content, so it cannot rot the same way.

### Three things worth knowing about the implementation

**The framework renders on the wrong thread.** SKSE Menu Framework draws from the
renderer's present hook. Talking to Scaleform from there would race the game, so every
widget that touches the minimap queues its work through `SKSE::GetTaskInterface()` and runs
it on the main thread.

**Re-positioning had to become idempotent.** The AS2 `Minimap` function positions the clip
by running a stage coordinate through `globalToLocal` — which is relative to the clip's own
transform — and assigning the result to `_x`/`_y`. Its output therefore depends on where the
clip already is, so calling it twice does not put the clip in the same place twice: each
call walks it further from where it started. Called once at construction that is invisible,
but driving it from a slider sent the minimap off screen within a few ticks, and putting the
slider back did not bring it home, because the position was a running total rather than a
function of the setting. `ApplyDisplaySettings()` now restores the authored `_x`, `_y`,
`_width` and `_height` before each call, so the function always sees the state it saw when
the minimap was built. The constructor calls the same function, so the initial layout and
every later change cannot drift apart.

**Scale had to stop compounding.** Upstream applied `fScale` once, in the constructor, by
multiplying the clip's current width. Re-applying that at runtime would multiply the already
scaled size. The constructor now records the unscaled `baseWidth`/`baseHeight`, and
`ApplyDisplaySettings()` resets to those, re-runs the AS2 `Minimap` function, then scales —
the same order the constructor uses, which matters because that function converts a stage
coordinate through the clip's own transform.

**The menu refuses to run against an old framework.** The vendored header calls the
framework's exported cimgui functions (`igSliderFloat`, `igCheckbox`, …) through function
pointers, without null-checking them. Version 1/2 of the framework does not export those —
verified by dumping the exports of the DLL bundled with the old SDK — so every widget call
would jump through a null pointer. `UI::HasRequiredExports()` probes for each export this
page uses and declines to register if any is missing, logging why. The minimap itself keeps
working; you just get no menu.

## Building

You need two things, and neither has to be installed anywhere in particular:

- **Visual Studio** with the *Desktop development with C++* workload. The Community
  edition is fine. CMake and Ninja come with it, so they do not need installing separately.
- **vcpkg** — `git clone https://github.com/microsoft/vcpkg` anywhere, then run
  `bootstrap-vcpkg.bat` inside it.

```
set VCPKG_ROOT=C:\path\to\vcpkg
configure.bat   # first run builds CommonLibSSE-NG; takes a few minutes
build.bat
```

`VCPKG_ROOT` is the only thing you have to tell the build about, and setting it
permanently (System Properties → Environment Variables, or `setx VCPKG_ROOT ...`) means
you never have to again. Visual Studio is located with `vswhere`
by `find-msvc.bat`, which also puts the CMake and Ninja that ship with it on `PATH`, so
nothing has a machine-specific path baked into it. Output lands in
`build/relwithdebinfo-se-only/DragonsEyeMinimap.dll`.

The configured preset is `build-relwithdebinfo-se-only`, i.e. Skyrim SE/AE only. Use
`build-relwithdebinfo-all` instead if you want a DLL that also loads in VR.

## Licence

MIT, as upstream. See `LICENSE`.
