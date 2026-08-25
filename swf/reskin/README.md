# Frame reskin — Untarnished UI theme

Source for the "Dragon's Eye Minimap - Frame Reskin Preview" package: a recolor of the
minimap's own compiled frame art toward
[Untarnished UI](https://www.nexusmods.com/skyrimspecialedition/mods/75188)'s palette.

Unlike the rest of this repo, this isn't produced from `Minimap.fla`/`MinimapArt.fla` - the
original mod's Flash project isn't editable here (no Adobe Flash/Animate, and re-authoring
from the `.fla` would need the original ActionScript animation timeline this project never
had access to). Instead, this is a direct binary edit of the shipped `MinimapArt.swf`, using
[JPEXS FFDec](https://github.com/jindrapetrik/jpexs-decompiler) (free/open-source):

1. `-export shape` pulled the two frame shapes (`LocalMapBackgroundSquared`/`Round`, real
   character IDs 335 and 337 in the shipped `MinimapArt.swf`) out as editable SVG.
2. `335-squared-frame.svg` was hand-edited to drop the ornamental Nordic corner engravings
   entirely, replaced with a plain rectangular ring (matching the round frame's already-plain
   style) — `337-round-frame.svg` needed no geometry change, only a recolor.
3. `-importShapes` brought both back into a copy of `MinimapArt.swf` — but FFDec's shape
   importer keeps a shape's original tag version, silently dropping the SVGs' `fill-opacity`
   (the original tags are SWF1 `DefineShape`, RGB-only, no alpha channel).
4. To get real translucency, both shapes were hand-upgraded from `DefineShape` to
   `DefineShape3` (RGBA) by round-tripping through FFDec's `-swf2xml`/`-xml2swf` and editing
   the two shapes' `<item type="DefineShapeTag">` → `DefineShape3Tag`, with their `RGB` color
   elements changed to `RGBA` (fill `#0A0A0B`, outline `#F5F2E9` fully opaque).

The two SVGs here are that intermediate, human-editable step - not the final binary. The
compiled result is `Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/!assets/MinimapArt.swf`
in the packaged mod, not checked into this repo (binary Scaleform output, same as the rest of
`Interface/` never being tracked here either - see the main `PORT-NOTES.md`).

## Fill opacity and HUD Opacity (1.0.1)

The fill's own baked alpha is now `1.0` (fully solid `#0A0A0B`), not the `0.55` it shipped
with in 1.0.0. This shape (`LocalMapBackgroundSquared`/`Round`, addressed at runtime as
`BackgroundArtSquare`/`BackgroundArtCircle`) is the same clip `Minimap::ApplyBackgroundOpacity()`
now drives every frame from SkyrimPrefs.ini's HUD Opacity slider (`fHUDOpacity:Display`,
0.0-1.0, mapped straight to the clip's `_alpha`, 0-100) - see `PORT-NOTES.md`. With the fill
baked fully opaque, that clip alpha is the *only* thing scaling it, so the background (and,
since fill and outline are one shape, the frame ring with it) is solid black at max HUD
opacity and fully invisible at minimum, with every value in between tracking the slider
linearly - rather than a fixed, baked-in translucency the slider had no effect on at all in
1.0.0.

## Solid black, not near-black (1.0.2)

Liam's design note: the fill should be genuinely solid black at maximum opacity, not the
`#0A0A0B` 1.0.0/1.0.1 shipped with (a near-black that read as very dark grey up close, not
true black - upstream's own original fill was already `#000000`, so this brings the reskin
back in line with that). The live-rendered minimap content (the actual 3D scene texture)
renders on top of this fill/background, so changing it carries no risk of hiding or being
hidden by the map itself. Changed the fill color in both `335-squared-frame.svg`/
`337-round-frame.svg` and the compiled `DefineShape3` fill color (`red=10 green=10 blue=11`
→ `red=0 green=0 blue=0`) via the same FFDec `-swf2xml`/`-xml2swf` round-trip this file
already documents, starting from the already-solid-opaque 1.0.1 asset rather than
re-deriving from the SVGs (only the fill color changed, not geometry or outline). Verified
by round-tripping the edited SWF back through `-swf2xml` and confirming the new color stuck.
The outline (`#F5F2E9`) and the fully-opaque baked alpha from 1.0.1 are unchanged.

Version 1.0.2, tagged `frame-reskin-v1.0.2` - versioned independently of this repo's main
`vX.Y.Z` tags, since it's a separate deliverable from the SMF settings port itself.
