# Third-party notices

## Norden UI — bundled "Frameless" theme

Dragon's Eye Minimap ships one theme:

    Interface/DragonsEyeMinimapThemes/Frameless.swf

That file is **Norden UI**'s `!assets/MinimapArt.swf` by **Nithog**, included unmodified:
https://www.nexusmods.com/skyrimspecialedition/mods/166086

A correction to what this file said when it was first written (2026-09-15): that SWF is NOT the
frame artwork. It is the shared minimap ASSET LIBRARY - map markers, the vision cone, and the
frame art exported as the symbols `LocalMapBackgroundSquared` and `LocalMapBackgroundRound`, which
`Minimap.swf` pulls in by name with `ImportAssets2`. Symbols exported for import are never placed
on the file's own root timeline, and the theme system loads a theme with `loadMovie`, which renders
only what the root draws. Loaded as a theme it therefore shows no frame at all - the minimap keeps
its markers and loses its border. That frameless look is the reason it is shipped and named as it
is (the owner, 2026-09-15: "I actually kind of like the look of it"), not an accident left in
place.

It is included under Norden UI's stated asset-use permission, read from the mod page on
2026-09-15:

> **Asset use permission** — "You are allowed to use the assets in this file without permission
> as long as you credit me"

and the author's notes on the same page:

> "For assets created by me, they have open permissions for modifying or redistributing.
> However, you may not sell any asset from this mod under any circumstances, but you may earn
> Donation Points for your hard work."

Dragon's Eye Minimap is free and is not sold, which satisfies that condition.

### The two "Norden UI" themes are MODIFIED copies

    Interface/DragonsEyeMinimapThemes/Norden UI.swf
    Interface/DragonsEyeMinimapThemes/Norden UI Round.swf

Both are that same Norden UI file with exactly ONE tag added: a root-level `PlaceObject2` placing
the sprite `LocalMapBackgroundSquared` (355) or `LocalMapBackgroundRound` (357) at depth 1, so the
frame draws when the theme system loads the file with `loadMovie`. No artwork was altered, added or
removed - every shape, sprite and export is byte-identical, verified by tag census (12,405 tags in,
12,406 out; 273 shapes and 287 exports unchanged).

Norden UI's permissions cover this explicitly, read from the mod page on 2026-09-15:

> **Modification permission** - "You are allowed to modify my files and release bug fixes or
> improve on the features so long as you credit me as the original creator"

Nithog is credited here, in the theme README, and on the mod's Nexus page.

### On the third-party caveat

Norden UI's permissions also carry a general notice that "some assets in this file belong to
other authors". That notice is not specific to the minimap frame. Before including the file we
compared it against Dragon's Eye Minimap's own artwork: of 270 and 273 exported shapes
respectively, only 2 were byte-identical, so the Norden frame is substantially its own artwork
rather than a repackage of ours, and the Norden UI page carries no third-party attribution for
minimap art.

If Nithog, or any other rights holder, asks for this file to be removed, it will be removed
immediately and without argument.
