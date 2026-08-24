# Port notes — INI-only settings → SKSE Menu Framework

**Version 1.6.6.** This is a fork of
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

**The location title repositions itself with the corner.** `LocationName` and
`ClearedHint` are ordinary TextField children of the same `MapClip` the minimap art lives in,
positioned wherever the FLA happened to author them - which was fine while the minimap only
ever sat in one place, but not once it can anchor to any corner: a title authored above the
map is the first thing clipped by the screen edge when the map is pinned to the bottom of the
screen. `Minimap::ApplyTitlePosition()` measures the authored layout once - the gap between
the title and whichever map edge it started nearer to, the combined height of both fields, and
each field's offset within that group, all read from `_y`/`_height` rather than assumed - and
from then on always computes an absolute target for the group: below the map when the anchor
is a top corner, above it when the anchor is a bottom corner. Like the minimap's own position,
it is idempotent by construction - every call derives its answer from the fixed authored
layout and the current anchor, never from wherever the fields already are, so repeated calls
cannot drift. It runs at the end of `ApplyDisplaySettings()`, and `InitLocalMap()` calls that
again once `localMap_` exists, since the first call happens at construction before
`LocationName`/`ClearedHint`/`LocalMapHolder` can be reached at all.

**The screen-corner offset is per corner, not shared.** `fOffsetX`/`fOffsetY` were a single
pair applying to whichever corner was selected, so nudging the map while testing one corner
carried that same nudge into whatever corner was tried next - which reads exactly like "the
map won't reach the edge" the moment you switch corners with a nonzero offset still set from
the last one. `display::offsetX`/`offsetY` are now `std::array<float, kAnchorCount>`, indexed
by `AnchorIndex()`, with the same per-corner-key-in-the-INI pattern (`fOffsetXTopLeft`,
`fOffsetYBottomRight`, and so on) used for the edge-margin approach tried and reverted in
1.5.4/1.5.6 - reused here because it is the same actual requirement, now sitting on top of the
corrected 1.6.1 positioning math rather than the broken pre-1.6.1 arithmetic. The menu's
Offset X/Y sliders always edit whichever corner is currently selected, with a line underneath
stating which that is.

**The on-screen position clamp is removed entirely.** It was meant purely as a safety net
against fScale growing large enough to push the map off screen, but that job was already
being done elsewhere: GetMaxScale() caps fScale, in the menu and again when applied, to
whatever keeps the artwork within a quarter of the screen - a size that cannot need rescuing
from going off screen at any anchor. The clamp was pure redundancy for that purpose, and it
turned out to also be actively wrong: corners on one side would not reach the true screen
edge even at a (0, 0) offset, which is inconsistent with the clamp's own math (the unclamped
target and the clamp bound are the same value when offset is 0, so the clamp should have been
a no-op there) - something in it was misbehaving beyond what that reasoning accounts for.
Rather than keep chasing an intermittent, per-corner discrepancy in a code path that is not
actually load-bearing, it is gone. The offset now IS the position, unconditionally - including
being able to push the artwork off screen if that is what is set, which is the trade a pure
safety net makes by existing at all.

**The very first position, at startup, could still be wrong until something re-applied it.**
ApplyDisplaySettings() runs twice during setup - once from the constructor, before the map's
children exist at all, and once from InitLocalMap(), in the same frame as InitMap()/SetShape().
Neither of those lets Scaleform actually render a frame in between, and in practice that
appears to have been enough to occasionally leave the very first position wrong - self-healing
the moment anything (changing the corner, for instance) re-applied it later, once actually
rendered frames had passed. `pendingInitialReapply` asks for one more application, the next
time `Advance()` runs for real - i.e. after a frame has actually happened - rather than relying
on either of the two same-frame applies during setup being reliable.

**The artwork's measured bounds were inflated by the hidden Controls clip, in a way that
explained the per-corner offset asymmetry a real user found empirically.** `getBounds()`
measures a clip's full render extent regardless of `_visible` - Flash does not exclude hidden
children from it. `Controls`, permanently hidden since 1.6.2, is still a child of the tree
`GetArtBoundsInParent()` was measuring as a whole, so its authored space was folded into
`artLeft`/`artTop`/`artRight`/`artBottom` even though nothing of it is drawn. The corrections
that were needed to compensate - roughly -69 on the left corners, +30 on the bottom ones, small
adjustments on the right and top ones - are the signature of one element sitting below and to
the left of the map artwork, exactly where a "hold to control/tap to hide" prompt with buttons
would be authored. A resolution bug would have misplaced all four corners by proportional
amounts; this pattern (same correction shared by both left corners, same one shared by both
bottom corners, small corrections on the other two sides) does not fit that shape at all.

Fixed by measuring `BackgroundArtSquare`/`BackgroundArtCircle` - whichever matches the current
shape - instead of the whole clip, once `localMap_` exists to reach it. That clip is exactly
the visible map frame, with no Controls artwork anywhere inside it. Before `localMap_` exists
(the constructor's very first call, before `LocalMap` has even been constructed) there is
nothing more precise to measure yet, so the whole clip's bounds remain a fallback for that one
early call; `pendingInitialReapply` (1.6.5) already re-applies once real frames have passed,
which is when the precise measurement takes over.

Deliberately not fixed by hardcoding the four corrections a player found as new per-corner
defaults: if the underlying measurement was wrong, baking in numbers discovered against that
wrong measurement would double-correct once the measurement itself is fixed. The corner+offset
system was already resolution-independent by construction - it reads Stage.width/Stage.height
live on every apply - so a (0, 0) offset landing flush at any resolution, once the actual
measurement bug is fixed, was the more durable answer than tuning to one player's screen.

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
