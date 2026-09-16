Dragon's Eye Minimap - frame themes
===================================

A theme is a SWF that DRAWS THE MINIMAP FRAME. Put one in this folder and it appears in the
"Frame theme" dropdown on the mod's settings page on the next game start. Picking it replaces the
minimap's frame artwork; picking "Built-in frame" puts the mod's own artwork back.

  Data/Interface/DragonsEyeMinimapThemes/YourFrame.swf   ->  shows up as "YourFrame"

WHAT THE FILE HAS TO BE

  * A SWF whose ROOT TIMELINE draws the frame. It is loaded with ActionScript's loadMovie into
    the clip that holds the frame art, so whatever the root draws is what appears. A SWF that only
    exports symbols to a library will show nothing - the drawing has to be on the root.
  * Authored around the same size and registration as the frame it replaces, since the mod
    measures the loaded artwork to position and scale the map. Wildly different proportions will
    still work, but the map will be sized to them.
  * Both frame shapes use the same theme. The mod loads your file for whichever shape is
    currently selected, so a theme that only suits one shape is best drawn to suit both.

THE OTHER WAY: OVERWRITING THE DEFAULT

A theme and a frame reskin are the same kind of file, delivered two ways. Instead of adding to
this folder you can overwrite the mod's own artwork:

  Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/!assets/MinimapArt.swf

That changes the built-in frame outright for everyone with your mod installed, with no dropdown
entry and no setting - which is how existing frame reskins for this mod already work. They are
unaffected by the theme system and keep working exactly as before.

Themes are not versioned or validated by the mod. If a file will not load, the frame simply stays
as it was and the mod's log says which path it tried.

BUNDLED THEMES

  "Norden UI"        -  the minimap frame from Norden UI by Nithog, square. This is his own frame
                        artwork, taken from that mod's !assets/MinimapArt.swf.
  "Norden UI Round"  -  the same, using his round frame instead. Pick this one if the minimap shape
                        is set to round.
  "Frameless"        -  no border at all: the map and its markers with nothing drawn around them.

  Pick any of them from the "Frame theme" dropdown on the mod's settings page, and press Save -
  the choice is written to sTheme under [Display] and is not kept unless you save it. Choosing
  "Built-in frame" puts this mod's own artwork back.

  Credit and the licence terms for the Norden UI artwork are in THIRD_PARTY_NOTICES.md.

WHY "FRAMELESS" IS THAT FILE

  It is Norden UI's !assets/MinimapArt.swf included unmodified. That file is the shared asset
  library - markers, vision cone, and the frame art exported as LocalMapBackgroundSquared and
  LocalMapBackgroundRound for Minimap.swf to import by name. Nothing in it is placed on its own
  root timeline, and a theme is loaded with loadMovie, which draws only the root - so it renders
  the map with no border at all. That is an accident of how the file is built, but it looks good,
  so it ships as a theme in its own right.

  The two "Norden UI" themes are that same file with ONE tag added: a root PlaceObject2 that places
  the frame sprite, so the artwork actually draws. That is the whole difference.
