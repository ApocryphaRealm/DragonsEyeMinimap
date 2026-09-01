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

## 1.5.9 - 2026-08-31 - untested

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

