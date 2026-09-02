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
