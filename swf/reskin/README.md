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

the author's design note: the fill should be genuinely solid black at maximum opacity, not the
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

## The squared frame had no interior at all (1.0.3)

the author reported that the round minimap shape was properly dark but the squared one was not.
The cause was in this file's own step 2 above, and it had been there since 1.0.0: replacing
the Nordic corner engravings with "a plain rectangular ring" produced *literally* a ring.
`335-squared-frame.svg`'s black path was an outer rectangle plus an inner rectangle at
`4,4`-`548.35,548.35` under `fill-rule="evenodd"`, which fills only the 4px band between
them and leaves the interior empty - so the squared minimap had no dark background behind
it. In the compiled SWF the same thing appeared as a second `StyleChangeRecord` moving to
`80,80` followed by four `10887`-twip edges wound opposite to the outer square, cancelling
it out.

The round shape never had this problem (a ray from its centre crosses its paths an odd
number of times, so the disc fills), which is exactly why only the square looked wrong.

This also explains why the 1.0.1 opacity work and the 1.0.2 solid-black change appeared to
do nothing on the squared shape: both were correct, but they were only ever recolouring a
4-pixel band.

Fixed by dropping the inner subpath entirely, in both `335-squared-frame.svg` (now a single
closed rectangle, filled, with the white outline left as its own separate stroked path -
the same construction the round shape uses) and the compiled `MinimapArt.swf` via the
`-swf2xml`/`-xml2swf` round-trip this file already documents. Only shape 335 was touched;
337 was verified byte-identical afterwards, and 335 keeps its `DefineShape3` tag, its solid
black `RGBA` fill and its `#F5F2E9` outline. Edge-record count for 335 went from 13 to 9.

Version 1.0.3, tagged `frame-reskin-v1.0.3` - versioned independently of this repo's main
`vX.Y.Z` tags, since it's a separate deliverable from the SMF settings port itself.
