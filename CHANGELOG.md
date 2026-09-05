# DragonsEyeMinimap-SMF - changelog

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

<!-- VERSIONING-RULES -->
> **Versioning rules (CLAUDE.md rules 6 and 48 - identical for mods and documents):**
> * `X.Y.Z`. A change increments the THIRD number. At `.9` the MINOR rolls: `1.0.9 -> 1.1.0`;
>   `1.0.10` never exists.
> * The next number is **LAST WORKING + 1**. A failed, scratch or untested test build does NOT
>   consume its number - the next attempt at the same step REUSES it.
> * Numbers are assigned by the tooling, never by hand: mods via `version-ledger.ps1 -Action next`
>   then `set-version.ps1`; governed documents via `docs-pipeline.ps1 -Action bump`; the rules via
>   `rules-version.ps1 -Action bump`. If a number was typed by hand, it is wrong until the tool
>   agrees.

## 1.6.4 - 2026-09-04 - working

> Observed on Apostasy Test Build (SE 1.5.97) 2026-09-04 23:53: "Loading DragonsEyeMinimap
> 1.6.4.0", HUD patches loaded, the minimap drew in Whiterun after a `coc` from the main menu, no
> crash log. The AE 1.6.1130+ fix below is by construction (the reference is gone); it has not
> been run on an AE game at the time of writing. **Run on SME (AE 1.6.1170, SKSE 2.2.6) 2026-09-05
> 00:2x:** loaded, HUD patches applied, local map allocated, registered in Apocrypha Menu Framework
> next to Local Map Upgrade, `coc whiterun` from the main menu, zero crash logs - the first AE
> 1.6.1130+ run of this mod that gets past start-up. (The AE-instance crashes that had blocked
> this were the 1.7-format ContentCatalog.txt, not Engine Fixes.)

### Fixed
- AE 1.6.1130 / 1.6.1170 / 1.6.1179 COULD NOT START THE GAME WITH THIS MOD INSTALLED. The
  minimap object carried a reference to the engine's `localMapMargin` float through Address
  Library ID 189820, and that ID does not exist in any database from 1.6.1130 on - so on those
  games CommonLib stopped the game at startup with "Failed to find the id within the address
  library: 189820". The value was never read anywhere in the mod; the reference is gone. SE
  1.5.97 was never affected (its ID 234438 exists).
- THE WORLD PICTURE IS NO LONGER DRAWN OFF A HALF-BUILT SCENE. Two identical crashes on
  2026-09-04 (`RenderOffScreen -> CullJobDescriptor::Cull`, 1.5-3 s after a console `coc` from
  the main menu) came from culling a scene-graph child that did not exist yet: the settle timer
  had expired while cells were still streaming in. The redraw is now also gated on the world
  itself - the current interior or every loaded exterior grid cell attached with its 3D built,
  the sky cell attached, and the shadow-scene children the off-screen render walks being real
  pointers. Anything short of that keeps the last picture and restarts the settle window from
  the moment the world became whole; the off-screen render also refuses to hand the culler a
  portal-shared node that is not a real pointer. Logged once per transition ("World not steady:
  <reason>" / "steady again").

## 1.6.3 - 2026-09-02 - working

> Observed running on the full Apostasy list (SE 1.5.97) on 2026-09-02: the map drew in Solitude,
> Riverwood and Falkreath, the markers sat correctly on it, the redraw gate held on loads and only
> on loads, and the author confirmed in play that the fast-motion lag is gone - **with ENB frame
> generation still enabled**, which rules the interpolation layer out as the cause and puts
> both defects squarely on this mod. **The theme
> replacement path below was NOT exercised** - no theme SWF was installed, so the mod ran on its
> built-in frame throughout.


### Added
- WHEN THE WORLD PICTURE IS REDRAWN is now under the player's control, in a new `[Rendering]`
  section. The picture inside the frame is the world, drawn through the game's own local-map
  path, and drawing it walks the engine's live scene graph - so the mod now decides when that is
  allowed to happen instead of doing it on every single frame no matter what the game is doing.
- `bSkipWhileWorldSettles` (ON by default) holds the redraw off while the world is not steady:
  during a loading screen, and for `iSettleMs` (default 1500) after a LOAD - a worldspace change,
  a move between an interior and an exterior, or the player being moved further in one frame than
  anything can travel. The map keeps showing the last picture it drew for that fraction of a
  second, which is what the vanilla local map does anyway.
- Deliberately NOT the player's parent cell. The first cut of this used it, and in an exterior
  that pointer changes every time the player crosses a cell boundary at a run - so the picture
  froze for a second and a half every few seconds of running while the markers kept moving.
  Caught in play immediately: "there definitely seems to be some kind of lag going on with the map
  when fast motion happens though it's not every time" (2026-09-02). Verified after the fix: six
  cell-boundary crossings produced no hold at all, and a `coc` to another worldspace produced
  exactly one.
- `iRedrawIntervalMs` (default 0, meaning every frame) sets a minimum gap between redraws. Raise
  it to have the engine's scene graph walked far less often - at 250 that is four times a second
  instead of sixty, and the picture steps rather than glides. The frame, the markers, the compass
  ring and the player arrow keep updating every frame either way.

### Fixed
- MARKERS NO LONGER LAG THE MAP WHEN THE PLAYER TURNS QUICKLY. Reported 2026-09-02: "if you walk
  around in circles quickly the markers on the map seem to detach from the map rotation and then
  they resync and start turning at the same time again."
- The cause was an ordering one, not a rotation one. The engine places the markers from the
  local-map camera, and the mod placed them in `AdvanceMovie` while the camera was only updated
  later in the same frame, in `PreDisplay` - so every marker was positioned with the PREVIOUS
  frame's camera while the picture underneath it was drawn with the current one. A fixed
  one-frame lag is invisible at walking pace and obvious when spinning on the spot, and it
  vanishes the moment the turn stops, which is exactly what was described.
- The camera update now happens in `AdvanceMovie`, immediately before the markers are placed;
  `PreDisplay` only draws. Nothing about panning, zoom or the rotation maths changed.

### Why
- A user reported trees rendering deformed straight after `coc` / `cow` (2026-09-02). The
  off-screen render is the one thing this mod does to the game's own scene graph, and to do it
  it briefly unhides the object-LOD root, turns node fade off and clears terrain render passes -
  state that belongs to the frame the player is looking at. Doing that on the frames when the
  engine is streaming a cell in is the worst possible moment, and it was happening on every
  frame with no guard at all. The deformation has NOT been reproduced here (see
  `2. Mod Types/bug reports/.../2026-09-02 - trees deformed after coc-cow/findings.md`), so this
  ships as a safeguard on a plausible mechanism, not as a confirmed fix.
- There is no way to draw a live local map without that path - it is the same code the vanilla
  local map runs - so the honest remedy is to not run it at the moments it can do harm, and to
  let anyone who suspects it turn the rate right down.

### Changed
- THEME SYSTEM REWORKED: a theme now REPLACES the minimap's frame artwork instead of recolouring
  it. The project owner's correction, 2026-09-02: "it's not supposed to change the color of the
  frame it's supposed to be used for introducing new frames to replace the current one." The 1.6.0
  system gave each theme a uFrameColor and applied it as an AS2 Color.setTransform MULTIPLY, so a
  theme could only ever be a recolour of the one frame - four colours of the same shape rather
  than four frames.
- A theme is now a SWF that draws the frame, loaded into the art clip with loadMovie. Themes live
  in Data/Interface/DragonsEyeMinimapThemes, because that is where Scaleform's file opener
  resolves loadMovie paths - the old SKSE/Plugins/DragonsEyeMinimap/themes location is not
  reachable from ActionScript.
- A theme file and a frame-reskin mod are now the same artifact delivered two ways: drop the SWF
  in the themes folder to switch it in game, or overwrite MinimapArt.swf to change the built-in
  frame outright. EXISTING RESKINS ARE UNAFFECTED and keep working exactly as before - they
  replace the default art, which is what "Built-in frame" shows.
- loadMovie is asynchronous, so selecting a theme re-runs the artwork measurement that positions
  and scales the map, using the existing pendingReapplyFrames mechanism.

### Removed
- uFrameTint:Display and the tint code path, along with the four colour-only themes that shipped
  in 1.6.0 (Vanilla White, Untarnished Ivory, Ebony Gold, Njord Ice). They cannot be converted:
  they carried a colour, and a theme is now artwork. An INI still naming one of them falls back
  to the built-in frame and says so in the log.

### Note for theme authors
- The SWF's ROOT TIMELINE must draw the frame. A SWF that only exports symbols to a library will
  load and show nothing. Authored size and registration decide how the map is positioned and
  scaled, since the mod measures the loaded artwork. Documented in the shipped themes folder.

## 1.6.2 - 2026-09-02 - working

### Added
- SHOW LOCATION NAME toggle (`bShowLocationName:Display`, on by default), with a checkbox on the
  settings page. A compatibility switch for non-English games, requested 2026-09-02: the title
  under the map is drawn with the game's own interface font, so a localisation whose glyphs that
  font does not carry, or whose location names are longer than the space allows, can show missing
  characters or text running past the frame. The mod cannot change another language's font, so it
  lets those players switch the title off and keep the map. Turning it off blanks the title rather
  than skipping the update, because the same call is what clears the PREVIOUS location - skipping
  it would freeze whatever text was last on screen.

### Changed
- The map now OPENS at the zoomed-in preset. It never applied a zoom at startup at all: SetMapZoom
  was only ever called from the toggle key, so the map opened at whatever the game's camera
  happened to be at and the first key press jumped it. The toggle state is marked accordingly, so
  the next press goes to the other preset instead of appearing to do nothing. No new setting -
  fZoomZoomedIn was already the configurable value.
- New default zoom presets: 0 and 1, the two extremes, replacing 0.25 and 0.75.

### Internal
- `dist/DragonsEyeMinimap.ini` is now the version-controlled source of truth for the shipped INI.
  It had only ever been carried forward from the previous package, with nothing tying it to the
  compiled defaults - a fragile way to satisfy rule 16. All 27 registered settings verified present.

## 1.6.1 - 2026-09-02 - working

### Fixed
- TWEEN MENU FLICKER. Tapping Tab made the tween menu open and close again one frame later, a
  steady 13.8ms cycle, and the minimap appeared to flicker because it was faithfully following a
  menu that was itself opening and closing. `Minimap::InputHandler::CanProcess` returned true for
  EVERY input event whenever no menu was open, which put this handler into MenuControls' dispatch
  for keys the mod has no interest in, Tab among them. It now claims only the hide key, the zoom
  key, the configured gamepad button, and mouse/thumbstick input while hold-to-pan is active.
  Every keybind is unchanged.
- `CanProcess` no longer calls `MenuControls::RemoveHandler(this)`. That ran from inside a
  callback MenuControls invokes while walking its own handler list, mutating the container
  mid-iteration. Returning false is all that is needed to decline input.

### How it was found
Five earlier attempts failed because each assumed a VISIBILITY bug - HUD mode flags, forced
_visible in the SWF's ActionScript, making menu mode authoritative, an entire rebuild from HUD
element to standalone menu, and removing the mid-dispatch RemoveHandler. It was input throughout.
A bisect against real keypresses settled it:

    handler not registered at all        no flicker   gaps 366-812ms
    registered, CanProcess always false  no flicker   gaps 463-2028ms
    registered, CanProcess broadly true  FLICKER      gaps 13.8ms

So presence in the handler list is harmless; claiming events the mod does not own is not.

### Scope
This release is v1.6.0 plus ONE changed file, `source/Controls.cpp`. The standalone-menu rebuild
explored during the same session is NOT included: it caused off-axis rendering and lost the
compass, and it was never needed for this bug. It is preserved on a git stash and in
`_dem-standalone-rebuild-backup-20260902`, with its plan in `4. plans/dem-standalone-menu/`.

## 1.6.0 - 2026-09-01 - working

### Added
- MINIMAP THEMES (author, 2026-09-01: a theme folder for the mod and a dropdown to select
  from installed themes - themes recolour the MINIMAP frame, not the compass): a Theme
  dropdown in the settings page's Display section, fed from
  Data/SKSE/Plugins/DragonsEyeMinimap/themes/*.ini (drop a file in, never overwrite; new
  files picked up on the next game start). A theme carries uFrameColor, applied to the
  frame art as an AS2 Color.setTransform MULTIPLY so the art's shading survives,
  change-detected per frame and re-applied across shape switches. The selection (sTheme)
  and the tint (uFrameTint) persist under [Display]. Ships four themes: Vanilla White
  (untinted), Untarnished Ivory, Ebony Gold, Njord Ice.

## 1.5.9 - 2026-09-01 - working

### Added
- COMPASS SETTINGS on the settings page (author request, 2026-08-31: players must be able to
  switch the compass off entirely): a Compass section with live toggles for the ring, the
  quest pointer and metric units, saved to the INI with the rest.

### Fixed
- HOLD-TO-PAN NEVER PANNED (author pad report, 2026-09-01) - mouse and controller both:
  Advance() marks a camera update every frame, and the update path zeroed the camera
  translation "to recenter after a cell change" before consuming it - wiping the pan offsets
  the input handlers had just written. The recenter now skips while hold-to-pan is active,
  which also recenters on release, vanilla-style.
- ALL KEYBINDS AND CONTROLLER BINDS DEAD (author playtest, 2026-08-31: "the hide and zoom are
  not working anymore", "none of the controller binds actually work"): RE::MenuEventHandler's
  `registered` field is engine-managed and carries NO initializer, so a freshly constructed
  input handler held uninitialized memory there. When the garbage read as true, the
  registration guard skipped MenuControls::AddHandler forever and every key and controller
  input was stillborn - a heisenbug that flipped with the 1.5.8 binary layout (1.5.7 in the
  field happened to read false). The constructor now starts it false explicitly. Verified by
  key-splice gate: hide toggles on every tap, also after menus; zoom cycles presets.

### Changed
- Controller hold-to-pan default button is now LEFT BUMPER (0x0100), replacing R3: holding a
  shoulder button while steering the right stick is natural; clicking-and-holding the stick
  you are steering is not (the author, 2026-09-01).
- COMPASS RESTYLED to match the original separate-widget design (design decision, 2026-08-31:
  the built-in compass must look like the old add-on's screenshots). The old widget drew in
  SCREEN pixels while the built-in ring draws in stage pixels (~2.5x on a 3200-wide screen),
  so every element was rendering ~2.5x too fat. Defaults converted to stage units: ring 0.8
  (was 2), gap 2.4 (was 6), pointer 8 (was 20), labels 8 (was 18), ticks 3.2/1.6 (was 8/4),
  cardinal letters riding the ring (offset 4.8, was 14), backing disc at ring radius +
  thickness (was + 2x). The distance readout uses the old widget's bearing-aware clearing
  formula, and the above/below mark is its small triangle glyph beside the number instead of
  a text caret.
- Circles drawn with 64 lineTo segments (ring strip 48) so the ring reads smooth at
  screenshot scale, matching the old widget's 96-segment ImGui circles.

## 1.5.8 - 2026-08-31 - working

### Added
- COMPASS BUILT IN (design decision, 2026-08-30: "the compass feature should be built in to DEM
  not AMF"): a compass ring with cardinal letters and a dark backing disc in the minimap's corner
  while the map is hidden, and the vanilla-style notched quest pointer riding the ring (or the
  visible map's inscribed circle) with a distance readout (CNO conventions, feet by default) and
  an above/below mark past +-840 units. Drawn at RUNTIME into an own clip on the HUD root via the
  ActionScript drawing API (Local Map Upgrade's border technique) - no framework HUD API, no SWF
  recompile. INI-only settings ([Compass], every piece toggleable); the settings-page section
  follows. Ported from the parked DragonsEyePointers reference.
- CONTROLLER: the same tap/hold scheme on iPanHoldGamepadButton (default R3, 0x0080; 0 disables) - tap toggles hide/show, holding hands the RIGHT stick to the map for panning (the author: 'the panning feature should be keyed to the right stick on controller'); the left stick keeps moving the player; looking returns to the camera on release.
- HOLD-TO-PAN restored on the hide key (the author: "hold to pan and press to hide using the same button"): a tap (shorter than fHoldToPanSecs, default 0.25 s) still toggles hide/show on release; holding the key hands the mouse to the map - move pans, wheel zooms at the game's own local-map speeds - and returns looking/wheel-zoom to the camera on release or when a menu opens. Mirrors the original mod's control scheme from its source (heldDownSecs + ControlMap::ToggleControls), on this port's existing K key.
- DEM_GetMinimapStageRect C export: the minimap artwork's rect in stage pixels, stage size, anchored corner and shown state, published after every positioning pass - for the Dragon's Eye Pointers add-on (quest pointer + compass ring widget that follows the map's corner).

### Changed
- Controller hide/pan button is OFF by default and behind a settings-page switch ('Controller: tap to hide, hold to pan'), with the XInput button mask editable beside it (the author: gamepad buttons are used for other things and PC players remap via Steam Input). bGamepadHideButton / iPanHoldGamepadButton in the INI.
- The MAIN mod zip now ships the .pdb debug symbols beside the DLL (packaging decision,
  2026-08-31), so users' Crash Logger resolves this mod's stack frames out of the box. The
  separate Debug Symbols upload is retired - the main download is all users need.

### Fixed
- DEM_GetMinimapStageRect kept the last rect measured while the map was SHOWN - while hidden the artwork measures about half size (103 vs 189 stage px), which had shrunk the pointer add-on's disc to half the map. A never-shown session still publishes the hidden measurement.
- The settings menu now registers with Apocrypha Menu Framework (AMF), the parallel framework
  these mods ship with. It previously resolved the framework only by the stock name
  "SKSEMenuFramework", which AMF is deliberately NOT named; the VFS alias under that name is
  refused at load and unloaded by SKSE, so `GetModuleHandleW` returned null and the plugin
  logged "SKSE Menu Framework does not export AddSectionItem". The vendored consumer header now
  prefers AMF's real module (ApocryphaMenuFramework), falling back to stock SMF, so both
  registration and the in-menu drawing calls reach the one live framework instance.
- COMPASS INVISIBLE: the INI reader parsed integers in base 10 only, so hex colour values like
  `uRingColor=0xE0E0E0` silently read as 0 - every compass element drew BLACK on the black
  backing disc and the whole feature looked like an empty plate. Numeric INI values now parse
  with base auto-detection (`0x` hex and decimal both work).
- COMPASS DRAWING: the AS2 `curveTo` call never renders in the HUD movie on this stack, so the
  disc and ring are now drawn as 32-segment `lineTo` circles and the ring/ticks as filled quad
  segments (moveTo/lineTo/beginFill only). Labels use the device-font fallback
  (`embedFonts=false`) because runtime-created text fields carry no authored font embed.

## 1.5.7 - 2026-08-27 - working

### Fixed
- Settings reload returned compiled defaults instead of the saved values. Save() wrote the INI with plain file I/O, but Reload() read it back through INISettingCollection, which uses the Win32 profile APIs that PrivateProfileRedirector caches - so a reload was served the values from game start. Verified end to end: the file on disk held uAnchor=0 and fOffsetXTopLeft=148 while the reload applied anchor 1 / offset (0,0) / scale 0.5.

### Changed
- The plugin's INI is never handed to the Win32 profile API at all. Init and Reload no longer call INISettingCollection::ReadFromFile on it, so PrivateProfileRedirector never caches the file and cannot flush a stale copy over it on game-save or exit. Behaviour is now identical whether or not the Redirector is installed.
### Known
- Every screenshot in the project up to 2026-08-27 was taken with the Frame Reskin ENABLED (modlist line 2), so the published banners show an optional add-on's appearance rather than what the main mod ships. Before building a banner from any capture, check modlist.txt for an enabled reskin - the two looks are close enough to pass unnoticed side by side.
- The Frame Reskin Preview add-on is NOT a palette shift: 52,378 bytes differ from the default art, the body is 718 bytes SHORTER, and the frame colour records are unchanged at #FFFFFF/#969696. Most likely it REMOVES the corner ornaments rather than recolouring - which fits its own description's 'no ornament' but makes its 'off-white hairline' claim doubtful. Unconfirmed; the vanilla capture run settles it.
- The DEFAULT frame art has corner ornaments that protrude beyond the map edge (the author, observed in game). They are geometry inside BackgroundArtSquared, not a named symbol, so they cannot be found by searching the SWF symbol table. Any frame-detection crop that finds the bright rectangle will CLIP them, because the rectangle is the stroke, not the artwork bounds - this silently broke the banner plate extractor for vanilla captures.
- The FRAME art lives in Minimap.swf (BackgroundArtSquared, BackgroundArtCircle, LocalMapBackgroundSquared, LocalMapBackgroundRound, backgroundArtMask, SetShape, SHAPE_ROUND, SHAPE_SQUARED). MinimapArt.swf is MARKER art (WoodMillMarker, TownMarker, hIconClip) despite the name suggesting otherwise. Anything touching the frame must go to Minimap.swf.
- The 1.5.6 package shipped a DLL built 2026-08-26 22:18, before the fixes committed at 03:04 the next morning - so the readable-path and reserved-key fixes were absent from the binary despite the version claiming them. 1.5.7 is built from current source; build, package and installed DLL all hash to 62E5E56B50DD1993.

---

<!-- reconstructed-history -->
> **Everything below was rebuilt from `version-ledger.json` on 2026-08-27**, not written
> at the time of the change. It carries only what the ledger recorded, so it is thinner
> than a real entry and may omit changes the ledger never captured. Everything ABOVE this
> line was written as the change was made.

## 1.5.6 - 2026-08-27 - working

### Changed
- 2026-08-27 05:23-05:28 Apostasy SE 1.5.97: loaded as 1.5.6.0, Infinity UI registered, SURVIVED kDataLoaded (the point 1.5.7-1.5.10 died at), stage 1280x720, display settled at _x 482.25 _y -466.4 after 9 re-applies, visibility settled after 1 re-assertion + 60 stable frames. Settings page verified live: anchor 1->0, scale 0.5->0.67, offset (0,0)->(148,0), saved to INI, reloaded and re-applied. No crash, no error, no warning.

## 1.5.4 - 2026-08-27 - working

### Changed
- 22:21 run: loaded, keybind fixes tested in game

## 1.5.1 - 2026-08-27 - working

### Changed
- published on Nexus as MAIN; in use by reporters

