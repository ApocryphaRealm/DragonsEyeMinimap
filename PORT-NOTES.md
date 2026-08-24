# Port notes — INI-only settings → SKSE Menu Framework

**Version 1.6.2.** This is a fork of
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
| `include/Settings.h` | Added `Save()`, `RestoreDefaults()`, `Reload()` and `GetIniPath()`. Position is `uAnchor`/`fOffsetX`/`fOffsetY`; the control-tip strings and the `fHoldDownToControlSecs`/`fDelayToHideControlsSecs` timings are gone. |
| `source/Settings.cpp` | Captures the compiled-in values as defaults before reading the INI, and can write or re-read every setting. Reads go through a null-safe helper rather than dereferencing the collection directly. |
| `include/MiniMap.h`, `source/MiniMap.cpp` | Added `ApplyDisplaySettings()`, `ApplyShapeSetting()`, `IsReady()`, `GetMapZoom()`/`SetMapZoom()`/`ToggleZoomPreset()`. The tap-to-hide/hold-to-pan control mode, the control-tip Scaleform calls, and the platform-button plumbing that fed them are removed entirely. |
| `source/Controls.cpp` | Rewritten around two dedicated keys (hide, zoom toggle) instead of the map-control binding's tap/hold behaviour. |
| `source/MessageListeners.cpp` | Calls `UI::Register()` on `kPostPostLoad`. |
| `CMakeLists.txt` | Globs are `CONFIGURE_DEPENDS`; the auto-deploy copy only runs if the game folder exists. |
| `cmake/ports/commonlibsse-ng/portfile.cmake` | Fetches CommonLibVR over git instead of a GitHub tarball. |
| `include/Hooks.h`, `source/Hooks.cpp` | The `RefreshPlatform` and `MenuOpenHandler::CanProcess` hooks are removed - both existed only to serve the control mode. |

### Why the port file changed

The upstream port pinned a SHA512 of GitHub's generated `.tar.gz`. GitHub re-compresses
those archives over time, so the recorded hash eventually stops matching and the port fails
to download — which is exactly what happened here. Fetching the same pinned commit over git
keeps the pin while letting git verify the content, so it cannot rot the same way.

### Things worth knowing about the implementation

**The framework renders on the wrong thread.** SKSE Menu Framework draws from the
renderer's present hook. Talking to Scaleform from there would race the game, so every
widget that touches the minimap queues its work through `SKSE::GetTaskInterface()` and runs
it on the main thread. That includes `Save()`/`Reload()`/`RestoreDefaults()`, since they
drive the game's own `INISettingCollection`, and the three control-tip strings did too in an
earlier version - reassigning a `std::string` frees its old buffer, and the main thread was
handing that buffer to Scaleform as `.c_str()`.

**Positioning is computed in the parent's coordinate space, not the clip's own.** The AS2
`Minimap` function positions the clip by running a stage coordinate through `globalToLocal`
called on *itself* - mapping stage space into the clip's own local space - and then assigns
that into `_x`/`_y`, which are in the *parent's* space. Mixing those two spaces means the
function's output depends on where the clip already is: calling it twice does not put the
clip in the same place twice, and worse, its "screen edge" is not actually the screen edge in
the space `_x`/`_y` live in. An earlier version of this port built anchor arithmetic on top
of that function's output and got both wrong: the map walked off screen as sliders moved, and
even freshly measured, a "top right corner" reliably sat half off screen.

The fix was to stop using that function for anything but the initial layout, and instead:
convert a stage pixel into the parent's space by calling `globalToLocal` **on the parent**
(`Minimap::StageToParent`) - the same idiom the mod's own `LocalMap.as` already uses in the
other direction (`_parent.localToGlobal`) to report the map's extents back to C++; and ask
`getBounds(parent)` for the artwork's box **in the parent's space** (`GetArtBoundsInParent`),
re-measured on every apply rather than once at construction, since the map's contents are not
even attached yet when the clip is first built. `ApplyDisplaySettings()` then moves the clip
by the difference between where the chosen corner's edge is and where it should be - a
delta, not an absolute position, so it does not matter where the registration point sits
inside the artwork. The same screen-corner conversion also drives the on-screen clamp that
keeps scale from pushing the map off screen, and `SetLocalMapExtents`/`InitMap` is re-invoked
after every move, because it is normally computed once and would otherwise go stale the first
time the clip is repositioned or rescaled.

**The zoom toggle is deterministic, not distance-based.** The two zoom presets are meant to
be alternated between, so the toggle flips a remembered on/off flag rather than jumping to
"whichever preset the camera is currently further from" - the latter can pick the same target
twice in a row, or flip unpredictably, once the player has manually scrolled the map zoom to
a third value between the two. The camera's `zoom` units are not documented anywhere in
CommonLibSSE-NG or in this codebase, which is also why the presets are set from the menu with
"Set to current" rather than typed as numbers.

**Binding a key accepts any pressed frame, not only the first one.** `RE::ButtonEvent::IsDown()`
requires `HeldDuration() == 0.0F` exactly - the very first frame of a press. SKSE Menu
Framework's contract for when it delivers events to a registered `InputEventCallback`, and
with which frame, is undocumented, so the "Bind" buttons accept `IsPressed()` (any nonzero
value) instead. Capturing only once is handled by clearing the pending bind target after the
first accepted press, not by requiring a particular frame.

**The menu refuses to run against an old framework.** The vendored header calls the
framework's exported cimgui functions (`igSliderFloat`, `igCheckbox`, …) through function
pointers, without null-checking them. Version 1/2 of the framework does not export those —
verified by dumping the exports of the DLL bundled with the old SDK — so every widget call
would jump through a null pointer. `UI::HasRequiredExports()` probes for each export this
page uses and declines to register if any is missing, logging why. The minimap itself keeps
working; you just get no menu.

**The Controls clip starts visible by default, and had to be told to stay hidden.**
Removing every call into it (1.6.1) was not enough on its own: frame 1 of its timeline is the
label the AS2 side calls "show", and a MovieClip always begins on frame 1 with no script
needed to put it there. Upstream relied on `FoldControls()`/`ShowControls()` running before
the player ever saw it, and `HideControlsAfter()` fading it back out - with neither running
any more, the clip sat on screen showing whatever text was authored into it, for as long as
the minimap existed. `InitLocalMap()` now sets `Controls._visible = false` directly, once,
rather than going through the `"show"`/`"fadeOut"` frame labels or their animation.

**The tap-to-hide/hold-to-pan control mode is gone, along with the control-tip prompts and
platform-button Scaleform plumbing that supported it.** `ProcessThumbstick`/`ProcessMouseMove`
are no longer overridden - `RE::MenuEventHandler`'s own default (`return false`) is exactly
what is needed now that nothing pans the camera. `RefreshPlatform()`, the hand-written
`ControlMap__GetButtonNameFromUserEvent`, and two SKSE hooks (`HUDMenu::RefreshPlatform`,
`MenuOpenHandler::CanProcess`, the latter existing only to defer the gamepad Wait button
during control mode) went with it, since nothing else used any of them. The `Controls` clip
and its artwork still exist inside the shipped `.swf` - this repo has no Flash tooling to
rebuild it, and none is needed, since nothing calls `ShowControls`/`FoldControls`/
`UnfoldControls`/`HideControlsAfter` any more, so that clip simply never appears.

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
