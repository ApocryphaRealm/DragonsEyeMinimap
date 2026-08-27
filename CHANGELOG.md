# DragonsEyeMinimap-SMF - changelog

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

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

## 1.5.11 - 2026-08-27 - scratch

### Changed
- worked, but 1.5.6 rebuilt to isolate the flicker fix - no new change, so no number earned

## 1.5.10 - 2026-08-27 - failed

### Known
- Every screenshot in the project up to 2026-08-27 was taken with the Frame Reskin ENABLED (modlist line 2), so the published banners show an optional add-on's appearance rather than what the main mod ships. Before building a banner from any capture, check modlist.txt for an enabled reskin - the two looks are close enough to pass unnoticed side by side.
- The Frame Reskin Preview add-on is NOT a palette shift: 52,378 bytes differ from the default art, the body is 718 bytes SHORTER, and the frame colour records are unchanged at #FFFFFF/#969696. Most likely it REMOVES the corner ornaments rather than recolouring - which fits its own description's 'no ornament' but makes its 'off-white hairline' claim doubtful. Unconfirmed; the vanilla capture run settles it.
- The DEFAULT frame art has corner ornaments that protrude beyond the map edge (the author, observed in game). They are geometry inside BackgroundArtSquared, not a named symbol, so they cannot be found by searching the SWF symbol table. Any frame-detection crop that finds the bright rectangle will CLIP them, because the rectangle is the stroke, not the artwork bounds - this silently broke the banner plate extractor for vanilla captures.
- The FRAME art lives in Minimap.swf (BackgroundArtSquared, BackgroundArtCircle, LocalMapBackgroundSquared, LocalMapBackgroundRound, backgroundArtMask, SetShape, SHAPE_ROUND, SHAPE_SQUARED). MinimapArt.swf is MARKER art (WoodMillMarker, TownMarker, hIconClip) despite the name suggesting otherwise. Anything touching the frame must go to Minimap.swf.
- crashed at kDataLoaded - ActorValueGenerator+002429D; number reclaimed

## 1.5.9 - 2026-08-27 - failed

### Known
- Every screenshot in the project up to 2026-08-27 was taken with the Frame Reskin ENABLED (modlist line 2), so the published banners show an optional add-on's appearance rather than what the main mod ships. Before building a banner from any capture, check modlist.txt for an enabled reskin - the two looks are close enough to pass unnoticed side by side.
- The Frame Reskin Preview add-on is NOT a palette shift: 52,378 bytes differ from the default art, the body is 718 bytes SHORTER, and the frame colour records are unchanged at #FFFFFF/#969696. Most likely it REMOVES the corner ornaments rather than recolouring - which fits its own description's 'no ornament' but makes its 'off-white hairline' claim doubtful. Unconfirmed; the vanilla capture run settles it.
- The DEFAULT frame art has corner ornaments that protrude beyond the map edge (the author, observed in game). They are geometry inside BackgroundArtSquared, not a named symbol, so they cannot be found by searching the SWF symbol table. Any frame-detection crop that finds the bright rectangle will CLIP them, because the rectangle is the stroke, not the artwork bounds - this silently broke the banner plate extractor for vanilla captures.
- The FRAME art lives in Minimap.swf (BackgroundArtSquared, BackgroundArtCircle, LocalMapBackgroundSquared, LocalMapBackgroundRound, backgroundArtMask, SetShape, SHAPE_ROUND, SHAPE_SQUARED). MinimapArt.swf is MARKER art (WoodMillMarker, TownMarker, hIconClip) despite the name suggesting otherwise. Anything touching the frame must go to Minimap.swf.
- crashed at kDataLoaded - ActorValueGenerator+002429D; number reclaimed

## 1.5.8 - 2026-08-27 - failed

### Known
- Every screenshot in the project up to 2026-08-27 was taken with the Frame Reskin ENABLED (modlist line 2), so the published banners show an optional add-on's appearance rather than what the main mod ships. Before building a banner from any capture, check modlist.txt for an enabled reskin - the two looks are close enough to pass unnoticed side by side.
- The Frame Reskin Preview add-on is NOT a palette shift: 52,378 bytes differ from the default art, the body is 718 bytes SHORTER, and the frame colour records are unchanged at #FFFFFF/#969696. Most likely it REMOVES the corner ornaments rather than recolouring - which fits its own description's 'no ornament' but makes its 'off-white hairline' claim doubtful. Unconfirmed; the vanilla capture run settles it.
- The DEFAULT frame art has corner ornaments that protrude beyond the map edge (the author, observed in game). They are geometry inside BackgroundArtSquared, not a named symbol, so they cannot be found by searching the SWF symbol table. Any frame-detection crop that finds the bright rectangle will CLIP them, because the rectangle is the stroke, not the artwork bounds - this silently broke the banner plate extractor for vanilla captures.
- The FRAME art lives in Minimap.swf (BackgroundArtSquared, BackgroundArtCircle, LocalMapBackgroundSquared, LocalMapBackgroundRound, backgroundArtMask, SetShape, SHAPE_ROUND, SHAPE_SQUARED). MinimapArt.swf is MARKER art (WoodMillMarker, TownMarker, hIconClip) despite the name suggesting otherwise. Anything touching the frame must go to Minimap.swf.
- crashed at kDataLoaded - ActorValueGenerator+002429D; number reclaimed

## 1.5.6 - 2026-08-27 - working

### Changed
- 2026-08-27 05:23-05:28 Apostasy SE 1.5.97: loaded as 1.5.6.0, Infinity UI registered, SURVIVED kDataLoaded (the point 1.5.7-1.5.10 died at), stage 1280x720, display settled at _x 482.25 _y -466.4 after 9 re-applies, visibility settled after 1 re-assertion + 60 stable frames. Settings page verified live: anchor 1->0, scale 0.5->0.67, offset (0,0)->(148,0), saved to INI, reloaded and re-applied. No crash, no error, no warning.

## 1.5.4 - 2026-08-27 - working

### Changed
- 22:21 run: loaded, keybind fixes tested in game

## 1.5.1 - 2026-08-27 - working

### Changed
- published on Nexus as MAIN; in use by reporters

