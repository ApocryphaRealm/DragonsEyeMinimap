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
   elements changed to `RGBA` (fill `#0A0A0B` at 55% opacity, outline `#F5F2E9` fully opaque).

The two SVGs here are that intermediate, human-editable step - not the final binary. The
compiled result is `Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/!assets/MinimapArt.swf`
in the packaged mod, not checked into this repo (binary Scaleform output, same as the rest of
`Interface/` never being tracked here either - see the main `PORT-NOTES.md`).

Version 1.0.0, tagged `frame-reskin-v1.0.0` - versioned independently of this repo's main
`vX.Y.Z` tags, since it's a separate deliverable from the SMF settings port itself.
